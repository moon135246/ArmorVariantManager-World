// Included by the in-game overlay's DX12 integration. These observers mirror
// only graphics descriptor bindings; each forwards the native call unchanged.
namespace {
using List=ID3D12GraphicsCommandList;
std::atomic<List*> firstMaterialList{};
std::atomic<bool> bindingSetupDone{};
std::array<void*,7> observedMethods{};
constexpr std::array<unsigned,7> bindingSlots{10,11,27,28,30,32,59};

HRESULT (STDMETHODCALLTYPE *listReset)(List*,ID3D12CommandAllocator*,ID3D12PipelineState*);
void (STDMETHODCALLTYPE *listClear)(List*,ID3D12PipelineState*);
void (STDMETHODCALLTYPE *listBundle)(List*,List*);
void (STDMETHODCALLTYPE *listHeaps)(List*,UINT,ID3D12DescriptorHeap*const*);
void (STDMETHODCALLTYPE *listRoot)(List*,ID3D12RootSignature*);
void (STDMETHODCALLTYPE *listTable)(List*,UINT,D3D12_GPU_DESCRIPTOR_HANDLE);
void (STDMETHODCALLTYPE *listIndirect)(List*,ID3D12CommandSignature*,UINT,ID3D12Resource*,UINT64,ID3D12Resource*,UINT64);

HRESULT STDMETHODCALLTYPE ResetBindings(List* list,ID3D12CommandAllocator* allocator,ID3D12PipelineState* pso) {
    auto hr=listReset(list,allocator,pso);if(SUCCEEDED(hr))silver::Invalidate(list);return hr;
}
void STDMETHODCALLTYPE ClearBindings(List* list,ID3D12PipelineState* pso) {
    listClear(list,pso);silver::Invalidate(list);
}
void STDMETHODCALLTYPE BundleBindings(List* list,List* bundle) {
    listBundle(list,bundle);silver::AfterIndirect(list,true);
}
void STDMETHODCALLTYPE HeapBindings(List* list,UINT count,ID3D12DescriptorHeap*const* heaps) {
    listHeaps(list,count,heaps);silver::OnHeaps(list,count,heaps);
}
void STDMETHODCALLTYPE RootBinding(List* list,ID3D12RootSignature* root) {
    listRoot(list,root);silver::OnRoot(list,root);
}
void STDMETHODCALLTYPE TableBinding(List* list,UINT slot,D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    listTable(list,slot,handle);silver::OnTable(list,slot,handle);
}
void STDMETHODCALLTYPE IndirectBindings(List* list,ID3D12CommandSignature* sig,UINT count,ID3D12Resource* args,
                                      UINT64 offset,ID3D12Resource* counts,UINT64 countOffset) {
    listIndirect(list,sig,count,args,offset,counts,countOffset);
    silver::AfterIndirect(list,false);
}
void StartMaterialBindingRecords() {
    if(bindingSetupDone.load(std::memory_order_acquire))return;
    auto* list=firstMaterialList.load(std::memory_order_acquire);
    if(!list)return;
    auto methods=*reinterpret_cast<void***>(list);
    const bool ok=Hook(methods[10],reinterpret_cast<void*>(ResetBindings),listReset) &&
                  Hook(methods[11],reinterpret_cast<void*>(ClearBindings),listClear) &&
                  Hook(methods[27],reinterpret_cast<void*>(BundleBindings),listBundle) &&
                  Hook(methods[28],reinterpret_cast<void*>(HeapBindings),listHeaps) &&
                  Hook(methods[30],reinterpret_cast<void*>(RootBinding),listRoot) &&
                  Hook(methods[32],reinterpret_cast<void*>(TableBinding),listTable) &&
                  Hook(methods[59],reinterpret_cast<void*>(IndirectBindings),listIndirect);
    if(ok)for(size_t i=0;i<bindingSlots.size();++i)observedMethods[i]=methods[bindingSlots[i]];
    bindingSetupDone.store(true,std::memory_order_release);
    list->Release();
    RuntimeLog("Silver pixel: native material table recording %s",ok?"ready":"unavailable");
}
}
bool ObserveMaterialList(ID3D12GraphicsCommandList* list) {
    if(!bindingSetupDone.load(std::memory_order_acquire)) {
        if(!firstMaterialList.load(std::memory_order_relaxed)) {
            list->AddRef();
            List* empty=nullptr;
            if(!firstMaterialList.compare_exchange_strong(empty,list))list->Release();
        }
        return false;
    }
    if(!observedMethods[0])return false;
    auto methods=*reinterpret_cast<void***>(list);
    thread_local void** last=nullptr;
    thread_local bool compatible=false;
    if(methods!=last) {
        compatible=true;last=methods;
        for(size_t i=0;i<bindingSlots.size();++i)compatible&=methods[bindingSlots[i]]==observedMethods[i];
    }
    return compatible;
}
