#ifdef WORLDDIFF_DISSOLVE_PROBE
#include "SceneHandoff.h"
#include "IndexedHandoff.h"
#include "Overlay.h"
#include <wrl/client.h>
#include <mutex>
#include <set>
#include <tuple>
#include <chrono>
#include <sstream>

namespace worlddiff::scene_handoff {
namespace {
using Microsoft::WRL::ComPtr;
using Stage = std::tuple<unsigned, uintptr_t, uintptr_t, uintptr_t>;
struct Trial {
    ModelPolicy owner;
    dissolve::IndexedHandoff mesh;
    // Publish the immutable index buffer and verified stages together. The
    // first incoming draw independently checks its actual bindings.
    std::mutex mutex;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12Resource> upload;
    D3D12_INDEX_BUFFER_VIEW sorted{}, original{};
    std::array<std::set<Stage>,2> stages;
    std::set<Stage> expectedStages;
    std::atomic<bool> ready{}, failed{};
    size_t slot{};
    std::array<dissolve::PairSweep,2> sweeps;
};
struct Selection {
    std::vector<std::unique_ptr<Trial>> pairs;
    std::map<std::pair<uintptr_t,unsigned>,Trial*> byRow;
    std::set<std::pair<unsigned,unsigned>> ranges;
    std::atomic<unsigned> sweepMs{900};
    Trial* Find(uintptr_t owner,unsigned row) const {
        const auto it=byRow.find({owner,row});
        return it==byRow.end()?nullptr:it->second;
    }
};
struct Slice { std::array<unsigned,2> cuts{}; std::array<bool,2> moving{}; bool valid=false; };
struct Frame {
    std::shared_ptr<const RenderPolicies> policies;
    std::vector<Slice> slices;
    DWORD tick;
    uint64_t number;
};
std::unique_ptr<Selection> storage;
std::atomic<Selection*> active{};
std::atomic<std::shared_ptr<const Frame>> frame;

bool HeadersMatch(const ModelPolicy& owner, const dissolve::IndexedHandoff& mesh) {
    __try {
        for (unsigned side=0;side<2;++side) {
            const unsigned row=mesh.rows[side];
            if (row>=owner.materials.size()) return false;
            const auto* live=reinterpret_cast<const unsigned char*>(owner.meshes+size_t(row)*80);
            const auto* file=mesh.headers[side].data();
            // Counts, material, LOD, vertex format, offsets and draw range.
            if (!dissolve::MatchesHandoffRow(live,file)) return false;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool CreateIndices(Trial& t) {
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask=heap.VisibleNodeMask=1;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=t.mesh.indices.size()*sizeof(uint16_t);
    desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
    desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(t.device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&t.upload)))) return false;
    void* mapped=nullptr;
    D3D12_RANGE noRead{0,0};
    if (FAILED(t.upload->Map(0,&noRead,&mapped))) return false;
    std::memcpy(mapped,t.mesh.indices.data(),static_cast<size_t>(desc.Width));
    t.upload->Unmap(0,nullptr);
    t.sorted={t.upload->GetGPUVirtualAddress(),static_cast<UINT>(desc.Width),DXGI_FORMAT_R16_UINT};
    return t.sorted.BufferLocation!=0;
}

void Observe(Trial& t,unsigned side,const dissolve::SceneDraw& hit) {
    if (t.ready.load(std::memory_order_acquire) || t.failed.load()) return;
    std::unique_lock lock(t.mutex,std::try_to_lock);
    if (!lock || t.ready.load(std::memory_order_relaxed) || t.failed.load()) return;
    if (!t.device) {
        if (FAILED(hit.commands->GetDevice(IID_PPV_ARGS(&t.device)))) { t.failed=true; return; }
        t.original=hit.binding.indices;
    }
    if (hit.binding.indices.BufferLocation!=t.original.BufferLocation ||
        hit.binding.indices.SizeInBytes!=t.original.SizeInBytes || !hit.depthView ||
        (hit.colors!=0 && hit.colors!=6)) { t.failed=true; return; }
    t.stages[side].emplace(hit.colors,hit.depthView,reinterpret_cast<uintptr_t>(hit.pipeline),
                           reinterpret_cast<uintptr_t>(hit.root));
    if (t.stages[side].size()>3) t.failed=true;
}

void Prepare(Trial& t) {
    if (t.ready.load() || t.failed.load()) return;
    std::lock_guard lock(t.mutex);
    if (!t.device) return;
    if (!t.upload && !CreateIndices(t)) {
        t.failed=true; RuntimeLog("Handoff: index buffer creation failed"); return;
    }
    // Preparing from the currently displayed side avoids consuming the
    // player's first switch as a warm-up. Neither hidden row is force-drawn.
    const unsigned source=t.stages[0].size()==3?0u:1u;
    const auto& seen=t.stages[source];
    const auto& other=t.stages[1-source];
    if(seen.size()!=3)return;
    if(!std::includes(seen.begin(),seen.end(),other.begin(),other.end())) {
        t.failed=true;RuntimeLog("Handoff: incompatible stages for rows %u/%u",t.mesh.rows[0],t.mesh.rows[1]);return;
    }
    std::set<uintptr_t> depthOnly;
    for (const auto& s:seen) if (std::get<0>(s)==0) depthOnly.insert(std::get<1>(s));
    if (depthOnly.size()!=2 || std::count_if(seen.begin(),seen.end(),[](const Stage& s){return std::get<0>(s)==6;})!=1) {
        t.failed=true; return;
    }
    t.expectedStages=seen;
    t.ready.store(true,std::memory_order_release);
    RuntimeLog("Handoff ready: part %u rows %u/%u from initial row %u; other stages observed=%zu",
               t.owner.part,t.mesh.rows[0],t.mesh.rows[1],t.mesh.rows[source],other.size());
}
}

bool Requested() { return active.load(std::memory_order_acquire)!=nullptr; }

void Tick(const std::filesystem::path& folder,const std::filesystem::path& gameRoot) {
    const auto path=folder/L"dissolve-handoff.test";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path,ec)) {
        active.store(nullptr,std::memory_order_release); frame.store({}); return;
    }
    if (!storage) {
        std::ifstream file(path);
        auto snapshot=renderPolicies.load();
        if(!snapshot)return;
        auto next=std::make_unique<Selection>();
        std::string line;
        while(std::getline(file,line)) {
            if(line.empty())continue;
            std::istringstream settings(line);
            std::string model;
            unsigned part{},a{},b{};
            if (!(settings>>part>>a>>b>>model) || part>=5 || a==b) return;
            auto* p=snapshot->Find(localModelContexts[part].load());
            if (!p || !p->local || p->preview || p->path!=model || p->part!=part) return;
            if(next->Find(p->context,a) || next->Find(p->context,b))return;
            auto t=std::make_unique<Trial>();
            t->owner=*p;
            t->slot=next->pairs.size();
            auto asset=gameRoot/L"nativePC"/std::u8string(p->path.begin(),p->path.end());
            asset.replace_extension(L".mod3");
            if (!dissolve::LoadHandoff(asset,{a,b},t->mesh) || !HeadersMatch(*p,t->mesh)) {
                t->failed=true; RuntimeLog("Handoff rejected file geometry or live layout: part %u rows %u/%u",part,a,b);
            } else {
                for(unsigned first:t->mesh.first)next->ranges.emplace(first,static_cast<unsigned>(t->mesh.thresholds.size())*3);
            }
            for(unsigned row:{a,b})next->byRow.emplace(std::pair{p->context,row},t.get());
            next->pairs.push_back(std::move(t));
        }
        if(next->pairs.empty())return;
        RuntimeLog("Handoff prepared %zu pairs; waiting for both presets",next->pairs.size());
        storage=std::move(next);
    }
    static std::filesystem::file_time_type lastEdit{};
    const auto edit=std::filesystem::last_write_time(path,ec);
    if (!ec && edit!=lastEdit) {
        lastEdit=edit;
        std::ifstream settings(path);
        unsigned part{},a{},b{},ms=900;
        std::string model;
        const auto& first=*storage->pairs.front();
        if (settings>>part>>a>>b>>model && model==first.owner.path && part==first.owner.part &&
            a==first.mesh.rows[0] && b==first.mesh.rows[1]) {
            unsigned value{};
            std::string rest;
            std::getline(settings,rest);
            if(std::istringstream(rest)>>value)ms=std::clamp(value,100u,5000u);
            storage->sweepMs.store(ms);
            RuntimeLog("Handoff paired sweep: %u ms",ms);
        }
    }
    active.store(storage.get(),std::memory_order_release);
    for(auto& t:storage->pairs)Prepare(*t);
}

void NextFrame() {
    auto* selection=active.load(std::memory_order_acquire);
    if (!selection) return;
    auto policies=renderPolicies.load(std::memory_order_acquire);
    const DWORD now=GetTickCount();
    if(!policies || !policies->enabled || !policies->worldDiffPriority || now-policies->tick>=1500) {
        frame.store({}); return;
    }
    const double timeMs=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto previous=frame.load(std::memory_order_acquire);
    std::vector<Slice> slices(selection->pairs.size());
    for(auto& t:selection->pairs) {
        auto rows=t->mesh.rows;
        auto* p=policies->Find(t->owner.context);
        if(t->failed.load() || !t->ready.load(std::memory_order_acquire) || !p || !SameDissolveInstance(t->owner,*p) ||
           p->hidden.size()<=std::max(rows[0],rows[1]))continue;
        const bool reset=!previous || now-previous->tick>=250 || !previous->slices[t->slot].valid;
        auto& slice=slices[t->slot];
        slice.valid=true;
        // A preset swap drives both cuts together. A single show or hide
        // drives that row alone, even when both rows end up visible/hidden.
        const unsigned duration=selection->sweepMs.load();
        for(unsigned side=0;side<2;++side) {
            auto& sweep=t->sweeps[side];
            sweep.Set(side?!p->hidden[rows[side]]:p->hidden[rows[side]]!=0,timeMs,duration,reset);
            slice.cuts[side]=t->mesh.ShapeCut(sweep.At(timeMs));
            slice.moving[side]=timeMs<sweep.startMs+sweep.durationMs;
        }
    }
    static uint64_t sequence=0; // Present is serialized by the overlay.
    frame.store(std::make_shared<const Frame>(Frame{std::move(policies),std::move(slices),now,++sequence}),std::memory_order_release);
}

bool Admit(const ModelPolicy& policy,unsigned row,bool shown,bool resolved) {
    auto* selection=active.load(std::memory_order_acquire);
    auto* t=selection?selection->Find(policy.context,row):nullptr;
    if (!t) return resolved;
    const auto f=frame.load(std::memory_order_acquire);
    if (!f || !f->slices[t->slot].valid || t->failed.load() || GetTickCount()-f->tick>=250 ||
        !SameDissolveInstance(policy,t->owner)) return resolved;
    const auto* sampled=f->policies->Find(policy.context);
    if (!sampled || !SameDissolveInstance(*sampled,policy)) return resolved;
    if (!f->slices[t->slot].moving[row==t->mesh.rows[1]] && !policy.TransitionFor(row) && !sampled->TransitionFor(row) &&
        sampled->hidden[row]==policy.hidden[row]) return resolved;
    // Admit both original rows during the handoff. The recorder selects
    // complementary subsets; settle back to the ordinary visibility query.
    return shown;
}

bool MightDraw(uintptr_t packet) {
    const auto* selection=active.load(std::memory_order_acquire);
    if (!selection) return false;
    const auto first=dissolve::SceneField<unsigned>(packet,0x44);
    return selection->ranges.contains({first,dissolve::SceneField<unsigned>(packet,0x40)});
}

bool Submit(const dissolve::SceneDraw& h,const RenderPolicies& policies,Submission& submitted) {
    auto* selection=active.load(std::memory_order_acquire);
    auto* t=selection?selection->Find(h.binding.model,h.mesh):nullptr;
    if (!t) return false;
    const auto* p=policies.Find(h.binding.model);
    if (!p || !policies.worldDiffPriority || !SameDissolveInstance(t->owner,*p)) return false;
    const unsigned side=h.mesh==t->mesh.rows[1];
    if (h.draw.StartIndexLocation!=t->mesh.first[side] || h.draw.BaseVertexLocation!=t->mesh.base[side] ||
        h.draw.IndexCountPerInstance!=t->mesh.thresholds.size()*3) return false;
    if(t->failed.load()) {
        // A draw already admitted for an outgoing transition can arrive after
        // the trial fell back. Respect its final visibility on the scene list.
        if(!t->ready.load() || !p->hidden[h.mesh])return false;
        submitted.indices=0;return true;
    }
    Observe(*t,side,h);
    const auto f=frame.load(std::memory_order_acquire);
    if (!f || !f->slices[t->slot].valid || GetTickCount()-f->tick>=250 || !t->ready.load(std::memory_order_acquire)) return false;
    const Stage stage{h.colors,h.depthView,reinterpret_cast<uintptr_t>(h.pipeline),reinterpret_cast<uintptr_t>(h.root)};
    if (!t->expectedStages.contains(stage) || h.binding.indices.BufferLocation!=t->original.BufferLocation ||
        h.binding.indices.SizeInBytes!=t->original.SizeInBytes) {
        t->failed.store(true);
        submitted.indices=p->hidden[h.mesh]?0:h.draw.IndexCountPerInstance;
        return submitted.indices==0;
    }
    const unsigned total=h.draw.IndexCountPerInstance, cut=f->slices[t->slot].cuts[side];
    submitted={side?cut:total-cut,f->number,cut};
    if (submitted.indices==total) return false; // native order, including endpoints
    if (submitted.indices) {
        const unsigned offset=side?total:cut;
        h.commands->IASetIndexBuffer(&t->sorted);
        h.commands->DrawIndexedInstanced(submitted.indices,1,offset,h.draw.BaseVertexLocation,0);
        h.commands->IASetIndexBuffer(&h.binding.indices);
    }
    return true;
}
} // namespace worlddiff::scene_handoff
#endif
