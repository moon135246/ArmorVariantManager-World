#include "SilverBindings.h"
#include <wrl/client.h>
#include <atomic>
#include <new>

namespace worlddiff::silver {
namespace {
using Microsoft::WRL::ComPtr;
const GUID recordedBindings={0x86da5617,0x2e02,0x4fbe,{0x99,0x53,0x0a,0x99,0x8d,0xc4,0x2a,0x59}};
class OwnedBindings final : public IUnknown {
    std::atomic<ULONG> refs{1};
public:
    MaterialBindings state;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;
        *out=nullptr;
        if(id!=__uuidof(IUnknown))return E_NOINTERFACE;
        *out=static_cast<IUnknown*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override {auto n=--refs;if(!n)delete this;return n;}
};
MaterialBindings* Current(ID3D12GraphicsCommandList* cmd) {
    thread_local ComPtr<ID3D12GraphicsCommandList> lastList;
    thread_local ComPtr<OwnedBindings> data;
    if(lastList.Get()!=cmd) {
        data.Reset();lastList=cmd;
        if(cmd->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT) {
            UINT size=sizeof(IUnknown*);
            if(FAILED(cmd->GetPrivateData(recordedBindings,&size,data.GetAddressOf()))) {
                data.Attach(new(std::nothrow) OwnedBindings);
                if(data && FAILED(cmd->SetPrivateDataInterface(recordedBindings,data.Get())))data.Reset();
            }
        }
    }
    return data?&data->state:nullptr;
}
}
bool Snapshot(ID3D12GraphicsCommandList* cmd,MaterialBindings& result) {
    auto* state=Current(cmd);if(!state||!state->recording||state->knownTables!=15)return false;
    result=*state;return true;
}
void Invalidate(ID3D12GraphicsCommandList* cmd) {
    if(auto* s=Current(cmd)){*s={};s->recording=true;s->knownTables=15;}
}
void AfterIndirect(ID3D12GraphicsCommandList* cmd,bool bundle) {
    if(auto* s=Current(cmd)) {
        s->tables={};s->knownTables=0;
        if(bundle)s->root=nullptr;
    }
}
void OnRoot(ID3D12GraphicsCommandList* cmd,ID3D12RootSignature* root) {
    if(auto* s=Current(cmd))s->SetRoot(root);
}
void OnTable(ID3D12GraphicsCommandList* cmd,UINT slot,D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    if(slot<4)if(auto* s=Current(cmd)){s->tables[slot]=handle;s->knownTables|=1u<<slot;}
}
void OnHeaps(ID3D12GraphicsCommandList* cmd,UINT count,ID3D12DescriptorHeap*const* heaps) {
    auto* s=Current(cmd);if(!s)return;
    std::array<ID3D12DescriptorHeap*,2> bound{};
    std::array<D3D12_DESCRIPTOR_HEAP_DESC,2> descriptions{};
    for(UINT i=0;i<count;++i) {
        const auto desc=heaps[i]->GetDesc();
        if(desc.Type>D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)continue;
        bound[desc.Type]=heaps[i];descriptions[desc.Type]=desc;
    }
    if(s->heaps==bound)return;
    s->tables={};s->knownTables=15;s->heaps=bound;s->bounds={};
    ComPtr<ID3D12Device> device;
    if(FAILED(cmd->GetDevice(IID_PPV_ARGS(&device))))return;
    for(unsigned i=0;i<2;++i)if(bound[i]) {
        const auto& d=descriptions[i];
        s->bounds[i]={bound[i]->GetGPUDescriptorHandleForHeapStart(),d.NumDescriptors,
                      device->GetDescriptorHandleIncrementSize(d.Type)};
    }
}
}
