// Executes the real overlay dispatch and DX12 backend with an isolated swapchain.
// No game process, plugin installation or user configuration is accessed.
#include "../src/Overlay.cpp"
#include <wrl/client.h>
#include <thread>
#include <stdexcept>
#include <iostream>

namespace worlddiff {
void RuntimeLog(const char*,...) {}
SharedState* RuntimeState() { return nullptr; }
std::filesystem::path PluginDirectory() { return std::filesystem::temp_directory_path()/L"WorldDiff-render-fixture"; }
namespace overlay { void DrawPanel() {} }
}
using Microsoft::WRL::ComPtr;
using namespace worlddiff;
namespace ui=worlddiff::overlay;
unsigned checks=0;
void Check(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
void Hr(HRESULT hr,const char* why){Check(SUCCEEDED(hr),why);}

struct QueueProxy final : ID3D12CommandQueue {
 ID3D12CommandQueue* real; ID3D12Device* expected; ULONG refs=1;
 unsigned executes=0,deviceRequests=0; bool nullDevice=false,failDevice=false,mismatch=false;
 QueueProxy(ID3D12CommandQueue* q,ID3D12Device* d):real(q),expected(d){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override {
  if(!out)return E_POINTER;*out=nullptr;
  if(iid==__uuidof(IUnknown)||iid==__uuidof(ID3D12CommandQueue)){*out=this;AddRef();return S_OK;}
  return E_NOINTERFACE;
 }
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{return --refs;}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID g,UINT* n,void* p)override{return real->GetPrivateData(g,n,p);}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID g,UINT n,const void* p)override{return real->SetPrivateData(g,n,p);}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID g,const IUnknown* p)override{return real->SetPrivateDataInterface(g,p);}
 HRESULT STDMETHODCALLTYPE SetName(LPCWSTR n)override{return real->SetName(n);}
 HRESULT STDMETHODCALLTYPE GetDevice(REFIID iid,void** out)override{
  ++deviceRequests;*out=nullptr;if(failDevice)return E_FAIL;if(nullDevice)return S_OK;return real->GetDevice(iid,out);
 }
 void STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource* a,UINT b,const D3D12_TILED_RESOURCE_COORDINATE* c,const D3D12_TILE_REGION_SIZE* d,ID3D12Heap* e,UINT f,const D3D12_TILE_RANGE_FLAGS* g,const UINT* h,const UINT* i,D3D12_TILE_MAPPING_FLAGS j)override{real->UpdateTileMappings(a,b,c,d,e,f,g,h,i,j);}
 void STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource* a,const D3D12_TILED_RESOURCE_COORDINATE* b,ID3D12Resource* c,const D3D12_TILED_RESOURCE_COORDINATE* d,const D3D12_TILE_REGION_SIZE* e,D3D12_TILE_MAPPING_FLAGS f)override{real->CopyTileMappings(a,b,c,d,e,f);}
 void STDMETHODCALLTYPE ExecuteCommandLists(UINT n,ID3D12CommandList*const* lists)override{
  for(UINT i=0;i<n;++i){ComPtr<ID3D12Device> d;lists[i]->GetDevice(IID_PPV_ARGS(&d));if(d.Get()!=expected)mismatch=true;}
  ++executes;real->ExecuteCommandLists(n,lists);
 }
 void STDMETHODCALLTYPE SetMarker(UINT a,const void* b,UINT c)override{real->SetMarker(a,b,c);}
 void STDMETHODCALLTYPE BeginEvent(UINT a,const void* b,UINT c)override{real->BeginEvent(a,b,c);}
 void STDMETHODCALLTYPE EndEvent()override{real->EndEvent();}
 HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence* a,UINT64 b)override{return real->Signal(a,b);}
 HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence* a,UINT64 b)override{return real->Wait(a,b);}
 HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64* a)override{return real->GetTimestampFrequency(a);}
 HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64* a,UINT64* b)override{return real->GetClockCalibration(a,b);}
 D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc()override{return real->GetDesc();}
};

// Deliberately makes Swapchain::GetDevice unusable, while Queue::GetDevice
// remains valid. Regressing to the old lookup must fail these rendering tests.
struct SwapProxy final : IDXGISwapChain {
 IDXGISwapChain* real; ULONG refs=1; unsigned deviceRequests=0; UINT countOverride=0; bool failBuffer=false;
 explicit SwapProxy(IDXGISwapChain* s):real(s){}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{return real->QueryInterface(iid,out);}
 ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
 ULONG STDMETHODCALLTYPE Release()override{return --refs;}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID g,UINT n,const void* p)override{return real->SetPrivateData(g,n,p);}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID g,const IUnknown* p)override{return real->SetPrivateDataInterface(g,p);}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID g,UINT* n,void* p)override{return real->GetPrivateData(g,n,p);}
 HRESULT STDMETHODCALLTYPE GetParent(REFIID i,void** p)override{return real->GetParent(i,p);}
 HRESULT STDMETHODCALLTYPE GetDevice(REFIID,void** out)override{++deviceRequests;*out=nullptr;return E_NOINTERFACE;}
 HRESULT STDMETHODCALLTYPE Present(UINT a,UINT b)override{return real->Present(a,b);}
 HRESULT STDMETHODCALLTYPE GetBuffer(UINT n,REFIID i,void** p)override{if(failBuffer&&n==1){*p=nullptr;return E_FAIL;}return real->GetBuffer(n,i,p);}
 HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL a,IDXGIOutput* b)override{return real->SetFullscreenState(a,b);}
 HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* a,IDXGIOutput** b)override{return real->GetFullscreenState(a,b);}
 HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* d)override{auto h=real->GetDesc(d);if(SUCCEEDED(h)&&countOverride)d->BufferCount=countOverride;return h;}
 HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT a,UINT b,UINT c,DXGI_FORMAT d,UINT e)override{return real->ResizeBuffers(a,b,c,d,e);}
 HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* d)override{return real->ResizeTarget(d);}
 HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** p)override{return real->GetContainingOutput(p);}
 HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* p)override{return real->GetFrameStatistics(p);}
 HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* p)override{return real->GetLastPresentCount(p);}
};

thread_local unsigned forwards=0;
HRESULT WINAPI ForwardPresent(IDXGISwapChain*,UINT sync,UINT flags){++forwards;return (HRESULT)(0x1000+sync+flags);}
int main(){try{
 ComPtr<IDXGIFactory4> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
 ComPtr<IDXGIAdapter> adapter;Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"WARP adapter");
 ComPtr<ID3D12Device> device;Hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"fixture device");
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> realQueue;Hr(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&realQueue)),"fixture queue");
 QueueProxy queue(realQueue.Get(),device.Get());
 HWND hwnd=CreateWindowExW(0,L"STATIC",L"WorldDiff isolated render test",WS_POPUP,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
 Check(hwnd!=nullptr,"hidden test window");
 DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=320;desc.Height=240;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 ComPtr<IDXGISwapChain1> realSwap;Hr(factory->CreateSwapChainForHwnd(realQueue.Get(),hwnd,&desc,nullptr,nullptr,&realSwap),"fixture swapchain");
 SwapProxy swap(realSwap.Get());
 std::array<uintptr_t,0x1800/8> renderer{};
 renderer[0x20/8]=(uintptr_t)&queue;renderer[0x1470/8]=(uintptr_t)&swap;
 Check(present_source::QueueForCall((uintptr_t)renderer.data(),0,&swap)==&queue,"exact game pair");
 Check(!present_source::QueueForCall((uintptr_t)renderer.data(),1,&swap),"wrong swap index rejected");
 Check(!present_source::QueueForCall((uintptr_t)renderer.data(),8,&swap),"out of bounds rejected");
 Check(!present_source::QueueForCall(1,0,&swap),"unreadable renderer rejected");
 Check(!present_source::QueueForCall(0,0,&swap),"null renderer rejected");
 renderer[0x20/8]=0;Check(!present_source::QueueForCall((uintptr_t)renderer.data(),0,&swap),"null queue rejected");renderer[0x20/8]=(uintptr_t)&queue;
 Check(present_source::Matches(present_source::kDx12Signature,present_source::kDx12Signature),"known instruction signature");
 Check(!present_source::Matches((void*)1,present_source::kDx12Signature),"unreadable signature rejected");
 auto changed=std::to_array(present_source::kDx12Signature);changed[3]^=1;
 Check(!present_source::Matches(changed.data(),present_source::kDx12Signature),"changed signature rejected");
 void* table[9]{};table[8]=(void*)ForwardPresent;
 auto dispatch=[&](bool dx12Call,UINT sync){
  safetyhook::Context ctx{};ctx.rcx=(uintptr_t)&swap;ctx.rax=(uintptr_t)table;ctx.rbx=(uintptr_t)renderer.data();ctx.r12=0;
  if(dx12Call)ui::Capture12(ctx);else ui::Capture11(ctx);
  auto fn=(ui::PresentFn)((void**)ctx.rax)[8];return fn(&swap,sync,DXGI_PRESENT_TEST);
 };
 Check(dispatch(true,2)==0x1003&&forwards==1,"DX12 forwards exactly once and preserves HRESULT");
 Check(dispatch(false,3)==0x1004&&forwards==2,"DX11 forwards exactly once and preserves HRESULT");
 bool threadOk=false;std::thread worker([&]{threadOk=dispatch(true,5)==0x1006&&forwards==1;});worker.join();
 Check(threadOk&&forwards==2,"dispatch state is thread local");
 Check(queue.deviceRequests==0,"Present TEST creates no backend device resources");

 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={320,240};io.DeltaTime=1.f/60;io.Fonts->AddFontDefault();
 queue.nullDevice=true;Check(!dx12::Init(&swap,&queue),"S_OK with null device rejected");Check(queue.refs==1&&swap.refs==1,"null device cleanup");queue.nullDevice=false;
 queue.failDevice=true;Check(!dx12::Init(&swap,&queue),"failed queue device rejected");Check(queue.refs==1&&swap.refs==1,"failed device cleanup");queue.failDevice=false;
 swap.countOverride=9;Check(!dx12::Init(&swap,&queue),"unsupported buffer count rejected");Check(queue.refs==1&&swap.refs==1,"descriptor failure cleanup");swap.countOverride=0;
 swap.failBuffer=true;Check(!dx12::Init(&swap,&queue),"partial buffer failure rejected");Check(queue.refs==1&&swap.refs==1,"partial GPU resource cleanup");swap.failBuffer=false;
 for(UINT cycle=0;cycle<3;++cycle){
  Hr(realSwap->ResizeBuffers(cycle==1?3:2,320+cycle*16,240,DXGI_FORMAT_R8G8B8A8_UNORM,0),"resize after full cleanup");
  Check(dx12::Init(&swap,&queue),"initialize with proxy queue and unrelated swap device");
  Check(dx12::Init(&swap,&queue),"idempotent initialize");Check(dx12::UsesQueue(&queue)&&!dx12::UsesQueue(realQueue.Get()),"keep exact proxy identity");
  for(int frame=0;frame<5;++frame){
   dx12::NewFrame();ImGui::NewFrame();ImGui::GetBackgroundDrawList()->AddRectFilled({2,2},{80,80},IM_COL32(255,80,40,255));ImGui::Render();dx12::Render(ImGui::GetDrawData());
   Hr(realSwap->Present(0,0),"present rendered fixture");
  }
  dx12::Shutdown();Check(queue.refs==1&&swap.refs==1,"shutdown releases exact held queue and swap");
 }
 Check(swap.deviceRequests==0,"never use Swapchain GetDevice for DX12 resources");
 Check(queue.executes>=15&&!queue.mismatch,"font upload and rendering use queue-compatible lists");
 dx12::Shutdown();Check(queue.refs==1&&swap.refs==1,"shutdown is idempotent");
 ImGui::DestroyContext();DestroyWindow(hwnd);
 std::cout<<"render compatibility checks: "<<checks<<" passed; proxy submissions="<<queue.executes<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<"\n";return 1;}}
