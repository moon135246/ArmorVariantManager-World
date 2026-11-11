#include "SilverFlow.h"
#include "Overlay.h"
#include <wrl/client.h>
#include <cmath>
#include <fstream>
#include <mutex>
#include <map>
#include <numeric>

namespace worlddiff::silver {
using Microsoft::WRL::ComPtr;
bool RecordMaterial(const dissolve::SceneDraw&,D3D12_GPU_VIRTUAL_ADDRESS,const std::array<float,5>&,dissolve::Style);
bool RecordBands(const dissolve::SceneDraw&,UINT,UINT,UINT);
namespace {
struct Position { float height,ripple; uint32_t crystal; };
struct Row {
    UINT first{},count{};
    INT base{};
    UINT positionOffset{};
    float low{},inverseHeight{};
    std::vector<float> thresholds;
    UINT sortedFirst{};
};
struct Asset {
    ComPtr<ID3D12Resource> buffer;
    D3D12_INDEX_BUFFER_VIEW sorted{};
    std::vector<Row> rows;
};
struct Part {
    std::shared_ptr<MeshObservation[]> identity;
    std::shared_ptr<const Asset> asset;
};
using Parts=std::array<Part,5>;
std::atomic<std::shared_ptr<const Parts>> published;
std::mutex deviceMutex;
ComPtr<ID3D12Device> activeDevice;
constexpr size_t maxUploadBytes=384ull*1024*1024;
bool LiveRows(const ModelPolicy& model,const uint8_t* headers) {
    __try {
        for(UINT row=0;row<model.materials.size();++row) {
            const auto live=reinterpret_cast<const uint8_t*>(model.meshes)+80ull*row;
            const auto file=headers+80ull*row;
            // The loader translates the vertex-format ID and mutable visibility flags.
            if(memcmp(live+2,file+2,8) || memcmp(live+14,file+14,10) || memcmp(live+28,file+28,12))return false;
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
template<class T> T Field(const uint8_t* p,size_t offset) {
    T value;memcpy(&value,p+offset,sizeof(T));return value;
}
std::shared_ptr<Asset> Load(const std::filesystem::path& path,const ModelPolicy& model,ID3D12Device& device,size_t remaining) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)return {};
    auto size=file.tellg();
    if(size<320 || size>128*1024*1024)return {};
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),size))return {};
    const auto p=bytes.data();
    if(memcmp(p,"MOD\0\xed\0",6) || Field<uint16_t>(p,8)!=model.materials.size())return {};
    auto table=Field<uint64_t>(p,0x48),vb=Field<uint64_t>(p,0x50),ib=Field<uint64_t>(p,0x58);
    if(table<320||table>vb||vb>ib||ib>bytes.size()||model.materials.size()>(vb-table)/80 ||
       !LiveRows(model,p+table))return {};
    auto result=std::make_shared<Asset>();
    result->rows.resize(model.materials.size());
    std::vector<Position> positions;
    std::vector<uint16_t> sortedIndices;
    for(size_t i=0;i<result->rows.size();i++) {
        const auto h=p+table+80*i;
        const UINT vertices=Field<uint16_t>(h,2), stride=h[14];
        const UINT sub=Field<UINT>(h,16), first=Field<UINT>(h,28), count=Field<UINT>(h,32);
        const INT base=Field<INT>(h,36);
        const int64_t start=int64_t(sub)+base;
        if(start<0||stride<12||vertices<3||!count||count%3||uint64_t(first)+count>(bytes.size()-ib)/2)continue;
        const auto offset=vb+Field<UINT>(h,20)+uint64_t(start)*stride;
        if(offset>ib || uint64_t(vertices-1)*stride+12>ib-offset)continue;
        if((positions.size()+uint64_t(count)+3)*sizeof(Position)+(sortedIndices.size()+uint64_t(count))*sizeof(uint16_t)>remaining)return {};
        std::vector<Position> points(vertices);
        bool valid=true;
        for(UINT j=0;j<vertices;++j) {
            const auto pos=p+offset+size_t(j)*stride;
            const float x=Field<float>(pos,0),y=Field<float>(pos,4),z=Field<float>(pos,8);
            if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)){valid=false;break;}
            points[j]={y,.064f*std::sin(.075f*x+.038f*z)+.024f*std::sin(.33f*x+.09f*y+.23f*z),0};
        }
        if(!valid)continue;
        while(positions.size()%4)positions.push_back({});
        const auto before=positions.size();
        std::array<float,3> lower{FLT_MAX,FLT_MAX,FLT_MAX},upper{-FLT_MAX,-FLT_MAX,-FLT_MAX};
        for(UINT j=0;j<count;++j) {
            UINT index=Field<uint16_t>(p+ib,2ull*(uint64_t(first)+j));
            if(index<sub||index-sub>=vertices){valid=false;break;}
            const auto v=points[index-sub];positions.push_back(v);
            const auto pos=p+offset+size_t(index-sub)*stride;
            for(unsigned axis=0;axis<3;++axis) {
                float value=Field<float>(pos,4*axis);
                lower[axis]=std::min(lower[axis],value);upper[axis]=std::max(upper[axis],value);
            }
        }
        const float low=lower[1],high=upper[1];
        if(!valid){positions.resize(before);continue;}
        const float span=std::max({upper[0]-lower[0],high-low,upper[2]-lower[2]});
        if(!(span>0.00001f)||!std::isfinite(span)){positions.resize(before);continue;}
        for(UINT j=0;j<count;++j) {
            const UINT index=Field<uint16_t>(p+ib,2ull*(uint64_t(first)+j));
            const auto pos=p+offset+size_t(index-sub)*stride;
            uint32_t packed=0;
            for(unsigned axis=0;axis<3;++axis) {
                const float local=((Field<float>(pos,4*axis)-lower[axis])-.5f*(upper[axis]-lower[axis]))/span;
                packed|=uint32_t(std::lround(1023.f*std::clamp(local+.5f,0.f,1.f)))<<(axis*10);
            }
            positions[before+j].crystal=packed;
        }
        auto& row=result->rows[i];
        row.first=first;row.count=count;row.base=base;row.positionOffset=static_cast<UINT>(before);
        row.low=low;row.inverseHeight=high-low>.00001f?1.f/(high-low):0.f;row.sortedFirst=static_cast<UINT>(sortedIndices.size());
        std::vector<std::pair<float,UINT>> order;
        order.reserve(count/3);
        for(UINT j=0;j<count;j+=3) {
            float level=0;
            for(UINT k=0;k<3;++k) {
                const auto& v=positions[before+j+k];
                const float y=std::clamp((v.height-low)*row.inverseHeight,0.f,1.f);
                level+=std::clamp(y+4.f*y*(1.f-y)*v.ripple,0.f,1.f);
            }
            order.emplace_back(level/3.f,j);
        }
        std::sort(order.begin(),order.end());
        row.thresholds.reserve(order.size());
        for(const auto& [level,j]:order) {
            row.thresholds.push_back(level);
            for(UINT k=0;k<3;++k)sortedIndices.push_back(Field<uint16_t>(p+ib,2ull*(uint64_t(first)+j+k)));
        }
    }
    if(positions.empty())return {};
    D3D12_HEAP_PROPERTIES props{};props.Type=D3D12_HEAP_TYPE_UPLOAD;props.CreationNodeMask=props.VisibleNodeMask=1;
    const auto fieldBytes=positions.size()*sizeof(Position),indexBytes=sortedIndices.size()*sizeof(uint16_t);
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=fieldBytes+indexBytes;
    desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if(FAILED(device.CreateCommittedResource(&props,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,
           nullptr,IID_PPV_ARGS(&result->buffer))))return {};
    void* data{};D3D12_RANGE empty{};
    if(FAILED(result->buffer->Map(0,&empty,&data)))return {};
    memcpy(data,positions.data(),fieldBytes);
    memcpy(static_cast<uint8_t*>(data)+fieldBytes,sortedIndices.data(),indexBytes);
    result->buffer->Unmap(0,nullptr);
    result->sorted={result->buffer->GetGPUVirtualAddress()+fieldBytes,static_cast<UINT>(indexBytes),DXGI_FORMAT_R16_UINT};
    return result;
}
}
void SetDevice(ID3D12Device* device) {
    std::lock_guard lock(deviceMutex);activeDevice=device;
}
void PrepareModels(const std::filesystem::path& nativePC) {
    auto policies=renderPolicies.load();
    if(!policies||!policies->enabled||!policies->dissolveEnabled) {published.store({});return;}
    ComPtr<ID3D12Device> device;
    {std::lock_guard lock(deviceMutex);device=activeDevice;}
    if(!device)return;
    static size_t allocated=0;
    // Immutable uploads may still be referenced by the game's queues after an
    // outfit change. Reuse by device, asset revision and layout; bound residency.
    using Key=std::tuple<ID3D12Device*,std::filesystem::path,std::filesystem::file_time_type,uint64_t>;
    static std::map<Key,std::shared_ptr<const Asset>> cache;
    const auto previous=published.load();
    auto next=std::make_shared<Parts>();
    bool changed=!previous;
    for(const auto& p:policies->models) {
        if(!p.local||p.preview||p.part>=next->size()||!p.observations)continue;
        auto& part=(*next)[p.part];
        if(previous&&(*previous)[p.part].identity==p.observations) {part=(*previous)[p.part];continue;}
        changed=true;part.identity=p.observations;
        auto rel=std::filesystem::path(std::u8string(p.path.begin(),p.path.end()));
        if(rel.empty()||rel.has_root_path()||*rel.begin()!=L"pl")continue;
        if(std::find(rel.begin(),rel.end(),L"..")!=rel.end())continue;
        auto path=(nativePC/rel).replace_extension(L".mod3");
        std::error_code ec;auto revision=std::filesystem::last_write_time(path,ec);
        if(ec)continue;
        const Key key{device.Get(),path,revision,p.geometrySignature};
        if(auto it=cache.find(key);it!=cache.end()) {part.asset=it->second;continue;}
        auto asset=Load(path,p,*device.Get(),maxUploadBytes-allocated);
        if(asset) {
            allocated+=static_cast<size_t>(asset->buffer->GetDesc().Width);
            part.asset=asset;cache.emplace(key,asset);
            RuntimeLog("Silver flow: geometry ready part %u, %zu rows (%s)",p.part,asset->rows.size(),p.path.c_str());
        } else RuntimeLog("Silver flow: native coverage for part %u (%s)",p.part,p.path.c_str());
    }
    if(previous)for(size_t i=0;i<next->size();++i)changed|=(*next)[i].identity!=(*previous)[i].identity;
    if(changed)published.store(next);
}
bool Draw(const dissolve::SceneDraw& hit,const ModelPolicy& model,float hidden,bool pixelPath,bool uvPath,const dissolve::Transition* move) {
    auto parts=published.load();
    if(!parts || hit.part>=parts->size() || !(hidden>0.f && hidden<1.f))return false;
    const auto& p=(*parts)[hit.part];
    if(p.identity!=model.observations)return false;
    const auto phase=1.f-hidden;
    const auto style=move?move->style:dissolve::DefaultStyle;
    const auto* local=p.asset && hit.mesh<p.asset->rows.size()?&p.asset->rows[hit.mesh]:nullptr;
    if(local && (!local->count || local->count!=hit.draw.IndexCountPerInstance ||
       local->first!=hit.draw.StartIndexLocation || local->base!=hit.draw.BaseVertexLocation ||
       (style==dissolve::Style::Silver && !local->inverseHeight)))local=nullptr;
    const auto addr=local?p.asset->buffer->GetGPUVirtualAddress()+uint64_t(local->positionOffset)*sizeof(Position):0;
    if((local?pixelPath:uvPath) && RecordMaterial(hit,addr,{phase,local?local->low:0.f,local?local->inverseHeight:1.f,
                     .035f,style==dissolve::Style::Ink?1.f:-1.f},style)) {
        if(hit.colors && move && model.observations[hit.mesh].silverColorTransition.exchange(move->changedAt)!=move->changedAt)
            RuntimeLog("%s: color draw model=%s part=%u row=%u begin=%u from=%.4f to=%.4f hidden=%.4f source=%s",
                       style==dissolve::Style::Ink?"Ink bloom":style==dissolve::Style::Amber?"Amber crystal":"Silver moon",model.path.c_str(),hit.part,hit.mesh,move->changedAt,move->from,move->to,hidden,local?"mod3":"material-uv");
        return true;
    }
    if(!local || style!=dissolve::Style::Silver)return false; // Retain generic coverage when the tessellated material is unavailable.
    const auto& row=*local;
    auto cut=[&](float t) {
        return 3*static_cast<UINT>(std::lower_bound(row.thresholds.begin(),row.thresholds.end(),t)-row.thresholds.begin());
    };
    const UINT count=cut(phase);
    if(count) {
        hit.commands->IASetIndexBuffer(&p.asset->sorted);
        hit.commands->DrawIndexedInstanced(count,1,row.sortedFirst,hit.draw.BaseVertexLocation,0);
        const bool lit=RecordBands(hit,row.sortedFirst+cut(phase-.027f),row.sortedFirst+cut(phase-.010f),row.sortedFirst+count);
        hit.commands->IASetIndexBuffer(&hit.binding.indices);
        static std::atomic_uint colorReports{},plainReports{};
        if(hit.colors && (lit?colorReports:plainReports).fetch_add(1)<4)
            RuntimeLog("Silver flow: spatial %s part=%u row=%u",lit?"silver band":"original material",hit.part,hit.mesh);
    }
    return true;
}
}
