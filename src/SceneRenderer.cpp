#include "SceneRenderer.h"
#include "GamePresentSource.h"
#include "Overlay.h"
#ifdef WORLDDIFF_SILVER_FLOW
#include "SilverFlow.h"
#endif
#include "safetyhook.hpp"
#ifdef WORLDDIFF_DISSOLVE_PROBE
#include "DissolveProbe.h"
#endif
#ifdef WORLDDIFF_HANDOFF_TRIAL
#include "SceneHandoff.h"
#endif

namespace worlddiff::scene_renderer {
namespace {
struct Frame {
    std::shared_ptr<const RenderPolicies> policies;
    std::array<D3D12_GPU_VIRTUAL_ADDRESS,5> views{};
    DWORD tick{};
};
std::atomic<std::shared_ptr<const Frame>> currentFrame;
std::array<std::atomic<UINT64>,5> localViews{};
std::atomic<bool> rendererInstalled{};
bool ReadView(const ModelPolicy& p,D3D12_GPU_VIRTUAL_ADDRESS& view) {
    __try {
        if (!p.context || !p.resource || dissolve::SceneField<uintptr_t>(p.context,0x2a0)!=p.resource) return false;
        const auto object=dissolve::SceneField<uintptr_t>(p.resource,0x108);
        if(!object)return false;
        const auto buffer=dissolve::SceneField<uintptr_t>(object,0x60);
        if(!buffer)return false;
        view=dissolve::SceneField<UINT64>(buffer,0x40);
        return view!=0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool Recent(DWORD now,DWORD marker,DWORD age) { return marker && now-marker<age; }

struct Submission {
    unsigned indices;
    uint64_t frame=0;
    unsigned cut=0;
};

bool RecordCoverage(const dissolve::SceneDraw& h,const ModelPolicy& live,const Frame& f,Submission& out) {
    const auto* sampled=f.policies->Find(h.binding.model);
    if (!live.observations || !sampled || sampled->observations!=live.observations ||
        h.mesh>=live.hidden.size() || h.mesh>=sampled->hidden.size())return false;
    auto& observation=live.observations[h.mesh];
#ifdef WORLDDIFF_SILVER_FLOW
    const bool bindingRecords=overlay::ObserveMaterialList(h.commands);
    const auto kind=silver::Stage(h.pipeline);
    auto stages=observation.silverStages.load(std::memory_order_relaxed);
    if(!(stages&kind))stages=observation.silverStages.fetch_or(kind,std::memory_order_relaxed)|kind;
    const auto amberKind=silver::Stage(h.pipeline,dissolve::Style::Amber);
    auto amberStages=observation.amberStages.load(std::memory_order_relaxed);
    if(!(amberStages&amberKind))amberStages=observation.amberStages.fetch_or(amberKind,std::memory_order_relaxed)|amberKind;
    const auto uvKinds=silver::Stage(h.pipeline,dissolve::Style::Silver,true) |
                       (silver::Stage(h.pipeline,dissolve::Style::Amber,true)<<4);
    auto uvStages=observation.uvStages.load(std::memory_order_relaxed);
    if((uvStages&uvKinds)!=uvKinds)uvStages=observation.uvStages.fetch_or(uvKinds,std::memory_order_relaxed)|uvKinds;
    auto ready=observation.silverBindings.load(std::memory_order_relaxed);
    if(bindingRecords && kind!=4 && !(stages&4) && !(ready&kind) && silver::CanBind(h)) {
        ready=observation.silverBindings.fetch_or(kind,std::memory_order_relaxed)|kind;
        if(ready==3)RuntimeLog("Silver pixel: material ready model=%s row=%u",live.path.c_str(),h.mesh);
    }
#endif
    if(!observation.seen.load(std::memory_order_relaxed))observation.seen.store(true,std::memory_order_relaxed);
    const DWORD now=GetTickCount();
    if (Recent(now,observation.controllerTick.load(std::memory_order_relaxed),250))return false;
    const auto* move=sampled->TransitionFor(h.mesh);
    // A query can belong to the next policy before Present publishes it. Use
    // the old endpoint for the rest of the current submission window.
    if(!move && !live.TransitionFor(h.mesh) &&
       !Recent(now,observation.admittedTick.load(std::memory_order_relaxed),250))return false;
    const float hidden=move?move->At(f.tick):static_cast<float>(sampled->hidden[h.mesh]);
#ifdef WORLDDIFF_SILVER_FLOW
    const auto styleStages=move && move->style!=dissolve::Style::Silver?amberStages:stages;
    const auto uvStyleStages=move && move->style!=dissolve::Style::Silver?uvStages>>4:uvStages&15;
    if(silver::Draw(h,live,hidden,bindingRecords && styleStages==3 && ready==3,
                    bindingRecords && uvStyleStages==3 && ready==3,move)) {
        out={h.draw.IndexCountPerInstance,f.tick,0};
        return true;
    }
#endif
    auto spans=dissolve::NativeCoverage(h.draw.IndexCountPerInstance,hidden,move && move->keepSuffix);
    out={spans.indices,f.tick,move && move->keepSuffix?h.draw.IndexCountPerInstance-spans.indices:spans.indices};
    if(spans.indices==h.draw.IndexCountPerInstance)return false;
    for(unsigned i=0;i<spans.used;++i) {
        const auto s=spans.ranges[i];
        h.commands->DrawIndexedInstanced(s.count,1,h.draw.StartIndexLocation+s.first,h.draw.BaseVertexLocation,0);
    }
    return true;
}
}
void NextFrame(bool dx12) {
    const auto policies=renderPolicies.load(std::memory_order_acquire);
    const auto tick=GetTickCount();
    if (!dx12 || !policies || !policies->enabled || !policies->dissolveEnabled || tick-policies->tick>=1500) {
        for(auto& v:localViews)v.store(0,std::memory_order_relaxed);
        currentFrame.store({},std::memory_order_release);return;
    }
    auto next=std::make_shared<Frame>();
    next->policies=policies;next->tick=tick;
    for(const auto& p:policies->models)if(p.local&&!p.preview&&p.part<next->views.size())ReadView(p,next->views[p.part]);
    for(size_t i=0;i<localViews.size();++i)localViews[i].store(next->views[i],std::memory_order_relaxed);
    currentFrame.store(std::move(next),std::memory_order_release);
}
bool Admit(const ModelPolicy& owner,unsigned row,bool shown,bool resolved,bool controllerWins) {
    if (!rendererInstalled.load(std::memory_order_acquire) || !owner.observations || row>=owner.hidden.size())return resolved;
    auto& observed=owner.observations[row];
    const auto now=GetTickCount();
    if(controllerWins) { observed.controllerTick.store(now,std::memory_order_relaxed);return resolved; }
    if(!shown || resolved || !owner.hidden[row] || !observed.seen.load(std::memory_order_relaxed) ||
       Recent(now,observed.controllerTick.load(std::memory_order_relaxed),250))return resolved;
    const auto f=currentFrame.load(std::memory_order_acquire);
    if(!f || owner.part>=f->views.size() || !f->views[owner.part] || now-f->tick>=250)return resolved;
    const auto* sampled=f->policies->Find(owner.context);
    if(!sampled || sampled->observations!=owner.observations)return resolved;
    const auto* move=owner.TransitionFor(row);
    if(!move || move->Finished(now))return resolved;
    observed.admittedTick.store(now,std::memory_order_relaxed);
    return true;
}
namespace {
// The list is in RCX after the renderer flushes pending resource barriers.
// Both paths into this basic block have already selected the same list.
constexpr uintptr_t drawSetup = 0x259d740 + 0xa3;
constexpr unsigned char drawCode[] = {
    0x41,0xb8,1,0,0,0, 0x8b,0x47,0x48, 0x44,0x8b,0x4f,0x44, 0x8b,0x57,0x40,
    0x4c,0x8b,0x11, 0xc7,0x44,0x24,0x28,0,0,0,0, 0x89,0x44,0x24,0x20, 0x41,0xff,0x52,0x68
};
constexpr unsigned char epilogue[] = {0x48,0x8b,0x5c,0x24,0x40,0x48,0x8b,0x74,0x24,0x48};
uintptr_t resume{};
safetyhook::MidHook hook;
#ifdef WORLDDIFF_DISSOLVE_PROBE
std::atomic<uint64_t> window{};
std::atomic<unsigned> visits{}, dropped{};
uint64_t serial{};
struct Record {
    DWORD tick;
    dissolve::SceneDraw hit;
    Submission submitted;
};
std::mutex recordsMutex;
std::vector<Record> records, batch;
FILE* output{};
constexpr size_t capacity = 4096;

#endif

void AtDraw(safetyhook::Context& ctx) {
    const auto backend=dissolve::SceneField<uintptr_t>(ctx.rsi,8);
    const auto address=dissolve::SceneField<UINT64>(backend,0x88);
    bool relevant=address && std::any_of(localViews.begin(),localViews.end(),
                           [address](const auto& v){return v.load(std::memory_order_relaxed)==address;});
#ifdef WORLDDIFF_DISSOLVE_PROBE
    const auto session=window.load(std::memory_order_acquire);
    if(session){visits.fetch_add(1,std::memory_order_relaxed);relevant=true;}
#endif
#ifdef WORLDDIFF_HANDOFF_TRIAL
    relevant=relevant || scene_handoff::MightDraw(ctx.rdi);
#endif
    if(!relevant)return;
    auto f=currentFrame.load(std::memory_order_acquire);
    const DWORD now=GetTickCount();
    auto policies=renderPolicies.load(std::memory_order_acquire);
    if(!policies || !policies->enabled || now-policies->tick>=1500)return;
    dissolve::SceneDraw hit{};
    if(!dissolve::ReadSceneDraw(ctx.rcx,backend,ctx.rbx,ctx.rdi,*policies,hit))return;
    Submission submitted{hit.draw.IndexCountPerInstance};
    bool took=false;
#ifdef WORLDDIFF_HANDOFF_TRIAL
    if(scene_handoff::Requested()) {
        scene_handoff::Submission trial{hit.draw.IndexCountPerInstance};
        took=scene_handoff::Submit(hit,*policies,trial);
        submitted={trial.indices,trial.frame,trial.cut};
    } else
#endif
    if(f && now-f->tick<250 && policies->dissolveEnabled) {
        const auto* p=policies->Find(hit.binding.model);
        if(p)took=RecordCoverage(hit,*p,*f,submitted);
    }
    if(took)ctx.rip=resume;
#ifdef WORLDDIFF_DISSOLVE_PROBE
    if(!session)return;
    std::unique_lock lock(recordsMutex,std::try_to_lock);
    if(!lock || records.size()==capacity){dropped.fetch_add(1,std::memory_order_relaxed);return;}
    if(session==window.load(std::memory_order_relaxed))records.push_back({now,hit,submitted});
#endif
}
bool SetUp() {
    const auto begin = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + drawSetup;
    if (!present_source::Matches(reinterpret_cast<void*>(begin), drawCode) ||
        !present_source::Matches(reinterpret_cast<void*>(begin + sizeof(drawCode)), epilogue)) return false;
    auto candidate = safetyhook::MidHook::create(reinterpret_cast<void*>(begin), AtDraw,
                                                  safetyhook::MidHook::StartDisabled);
    if (!candidate || candidate->original_bytes().size() > sizeof(drawCode)) return false;
    resume = begin + sizeof(drawCode);
    hook = std::move(*candidate);
    return true;
}

#ifdef WORLDDIFF_DISSOLVE_PROBE
void Flush() {
    {
        std::lock_guard lock(recordsMutex);
        records.swap(batch);
    }
    for (const auto& r : batch) {
        const auto& h = r.hit;
        fprintf(output, "%u,%llx,%u,%u,%llx,%u,%u,%u,%d,%p,%p,%p,%u,%llx,%u,%llu,%u\n",
                r.tick, static_cast<unsigned long long>(h.binding.model), h.part, h.mesh,
                h.binding.indices.BufferLocation, h.binding.indices.SizeInBytes, h.draw.StartIndexLocation,
                h.draw.IndexCountPerInstance, h.draw.BaseVertexLocation, static_cast<void*>(h.commands),
                static_cast<void*>(h.pipeline), static_cast<void*>(h.root), h.colors,
                static_cast<unsigned long long>(h.depthView),r.submitted.indices,r.submitted.frame,r.submitted.cut);
    }
    batch.clear();
    fflush(output);
}
#endif
}

void Tick([[maybe_unused]] const std::filesystem::path& folder) {
#ifdef WORLDDIFF_SILVER_FLOW
    silver::PrepareModels(folder.parent_path().parent_path());
#endif
    bool wanted=bool(currentFrame.load(std::memory_order_acquire));
#ifdef WORLDDIFF_DISSOLVE_PROBE
    const bool capture=dissolve_probe::active.load(std::memory_order_acquire);
    wanted=wanted || capture;
#endif
#ifdef WORLDDIFF_HANDOFF_TRIAL
    wanted=wanted || scene_handoff::Requested();
#endif
    static bool unsupported=false;
    if(wanted && !rendererInstalled.load() && !unsupported) {
        if((!hook && !SetUp()) || !hook.enable()) {
            RuntimeLog("DX12 mesh dissolve unavailable for current renderer");
            unsupported=true;
        } else rendererInstalled.store(true,std::memory_order_release);
    }
    if(!wanted && rendererInstalled.load() && hook.disable())rendererInstalled.store(false,std::memory_order_release);
#ifdef WORLDDIFF_DISSOLVE_PROBE
    static bool previous = false;
    const bool begin = capture && !previous;
    previous = capture;
    if (begin) {
        const auto path = folder / (L"dissolve-scene-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                    std::to_wstring(dissolve_probe::started) + L".csv");
        if (_wfopen_s(&output, path.c_str(), L"wb") != 0) return;
        fputs("tick,instance,part,mesh,ib,ib_bytes,first_index,index_count,base_vertex,list,pso,root,color_targets,depth_view,submitted_indices,handoff_frame,cut_indices\n", output);
        {
            std::lock_guard lock(recordsMutex);
            records.reserve(capacity);
            batch.reserve(capacity);
        }
        visits = 0;
        dropped = 0;
        window.store(++serial, std::memory_order_release);
    }
    if (!output) return;
    if (!capture) {
        window.store(0, std::memory_order_release);
    }
    Flush();
    if (!capture) {
        fprintf(output, "# visited=%u,dropped=%u\n", visits.load(), dropped.load());
        fclose(output);
        output = nullptr;
    }
#endif
}
} // namespace worlddiff::scene_renderer
