#ifndef NOMINMAX
#define NOMINMAX
#endif
#define DIRECTINPUT_VERSION 0x0800
#include "Overlay.h"
#include "State.h"
#include "NativeInput.h"
#include "Dx12Backend.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "safetyhook.hpp"
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dinput.h>
#include <atomic>
#include <cstdarg>
#include <fstream>
#include <mutex>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace dx12 {void HostLog(const char* fmt,...){char b[1800];va_list a;va_start(a,fmt);vsnprintf_s(b,sizeof(b),_TRUNCATE,fmt,a);va_end(a);worlddiff::RuntimeLog("%s",b);}}
namespace worlddiff::overlay {
namespace {
std::recursive_mutex renderMutex;
std::atomic<bool> open{false},deviceLost{false},inputRequested{false},inputInstalled{false};
std::atomic<ID3D12Device*> expectedDevice{nullptr};
thread_local bool ownInput=false;
HWND window=nullptr;WNDPROC originalWnd=nullptr;IDXGISwapChain* bound=nullptr;
bool commonReady=false,ready=false,is12=false;DWORD firstTick=0;
bool autoOpened=false;std::atomic<DWORD> messageToggleAt{0};
ID3D11Device* device11=nullptr;ID3D11DeviceContext* context11=nullptr;ID3D11RenderTargetView* rtv=nullptr;
using PresentFn=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT);
using ResizeFn=HRESULT(WINAPI*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using ExecuteFn=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList*const*);
PresentFn originalPresent=nullptr;ResizeFn originalResize=nullptr;ExecuteFn originalExecute=nullptr;
using AsyncFn=SHORT(WINAPI*)(int);using KeyboardFn=BOOL(WINAPI*)(PBYTE);using RawFn=UINT(WINAPI*)(HRAWINPUT,UINT,LPVOID,PUINT,UINT);
AsyncFn originalAsync=nullptr;KeyboardFn originalKeyboard=nullptr;RawFn originalRaw=nullptr;
using CursorPosFn=BOOL(WINAPI*)(int,int);using ClipFn=BOOL(WINAPI*)(const RECT*);
CursorPosFn originalCursorPos=nullptr,originalPhysicalCursor=nullptr;ClipFn originalClip=nullptr;
std::mutex clipMutex;RECT lastGameClip{};bool hasGameClip=false;std::atomic<uint64_t> blockedWarps{0};
bool OwnInput(){return ownInput||NativeInputThread();}
BOOL WINAPI CursorFilter(int x,int y){if(open&&!OwnInput()){blockedWarps++;return TRUE;}return originalCursorPos(x,y);}
BOOL WINAPI PhysicalCursorFilter(int x,int y){if(open&&!OwnInput()){blockedWarps++;return TRUE;}return originalPhysicalCursor(x,y);}
BOOL WINAPI ClipFilter(const RECT* rect){if(!OwnInput()){std::lock_guard lock(clipMutex);hasGameClip=rect!=nullptr;if(rect)lastGameClip=*rect;}if(open&&!OwnInput()){if(GetForegroundWindow()==window||NativeInputOwnWindow(GetForegroundWindow()))return originalClip(nullptr);return TRUE;}return originalClip(rect);}
using DeviceStateFn=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPVOID);
using DeviceDataFn=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
DeviceStateFn originalDeviceState=nullptr;DeviceDataFn originalDeviceData=nullptr;
template<class F> bool Hook(void* address,void* detour,F& original){auto made=safetyhook::InlineHook::create(address,detour,safetyhook::InlineHook::StartDisabled);if(!made)return false;auto* hook=new safetyhook::InlineHook(std::move(*made));original=hook->original<F>();if(!hook->enable()){delete hook;original=nullptr;return false;}return true;}
SHORT WINAPI KeyFilter(int key){SHORT v=originalAsync?originalAsync(key):0;return open&&!OwnInput()?0:v;}
BOOL WINAPI KeyboardFilter(PBYTE keys){BOOL v=originalKeyboard?originalKeyboard(keys):FALSE;if(v&&keys&&open&&!OwnInput())memset(keys,0,256);return v;}
UINT WINAPI RawFilter(HRAWINPUT h,UINT command,LPVOID data,PUINT size,UINT header){UINT n=originalRaw?originalRaw(h,command,data,size,header):(UINT)-1;if(open&&!OwnInput()&&command==RID_INPUT&&data&&n!=(UINT)-1&&n>=sizeof(RAWINPUTHEADER)){auto* input=(RAWINPUT*)data;if(input->header.dwType==RIM_TYPEMOUSE&&n>=sizeof(RAWINPUTHEADER)+sizeof(RAWMOUSE))memset(&input->data.mouse,0,sizeof(RAWMOUSE));else if(input->header.dwType==RIM_TYPEKEYBOARD&&n>=sizeof(RAWINPUTHEADER)+sizeof(RAWKEYBOARD))memset(&input->data.keyboard,0,sizeof(RAWKEYBOARD));}return n;}
HRESULT STDMETHODCALLTYPE DeviceStateFilter(IDirectInputDevice8W* self,DWORD bytes,LPVOID data){HRESULT hr=originalDeviceState(self,bytes,data);if(SUCCEEDED(hr)&&open&&!OwnInput()&&data)memset(data,0,bytes);return hr;}
HRESULT STDMETHODCALLTYPE DeviceDataFilter(IDirectInputDevice8W* self,DWORD stride,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags){HRESULT hr=originalDeviceData(self,stride,data,count,flags);if(SUCCEEDED(hr)&&open&&!OwnInput()&&count)*count=0;return hr;}
bool Key(int key){bool before=ownInput;ownInput=true;SHORT v=originalAsync?originalAsync(key):GetAsyncKeyState(key);ownInput=before;return (v&0x8000)!=0;}
void InstallInput(){
 if(inputInstalled.exchange(true))return;
 HMODULE user=GetModuleHandleW(L"user32.dll");bool a=Hook((void*)GetProcAddress(user,"GetAsyncKeyState"),(void*)KeyFilter,originalAsync);bool b=Hook((void*)GetProcAddress(user,"GetKeyboardState"),(void*)KeyboardFilter,originalKeyboard);bool c=Hook((void*)GetProcAddress(user,"GetRawInputData"),(void*)RawFilter,originalRaw);
 auto cursor=(void*)GetProcAddress(user,"SetCursorPos");bool warp=Hook(cursor,(void*)CursorFilter,originalCursorPos);auto physical=(void*)GetProcAddress(user,"SetPhysicalCursorPos");if(physical&&physical!=cursor)Hook(physical,(void*)PhysicalCursorFilter,originalPhysicalCursor);bool clip=Hook((void*)GetProcAddress(user,"ClipCursor"),(void*)ClipFilter,originalClip);
 bool d=false,e=false;IDirectInput8W* di=nullptr;IDirectInputDevice8W* keyboard=nullptr;
 if(SUCCEEDED(DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,(void**)&di,nullptr))&&di){if(SUCCEEDED(di->CreateDevice(GUID_SysKeyboard,&keyboard,nullptr))&&keyboard){auto** vt=*(void***)keyboard;d=Hook(vt[9],(void*)DeviceStateFilter,originalDeviceState);e=Hook(vt[10],(void*)DeviceDataFilter,originalDeviceData);keyboard->Release();}di->Release();}
 RuntimeLog("Input capture installed: async=%d keyboard=%d raw=%d direct-state=%d direct-buffer=%d cursor-warp=%d clip=%d",a,b,c,d,e,warp,clip);
}
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
 std::lock_guard lock(renderMutex);
 int menuKey=VK_F9;{std::lock_guard stateLock(app::Get().mutex);menuKey=app::Get().data.menuKey;}
 if(msg==WM_KEYDOWN&&w==(WPARAM)menuKey&&!(l&(1LL<<30))&&!Key(VK_CONTROL)){
  DWORD now=GetTickCount();if((DWORD)(now-messageToggleAt.load())>250){SetVisible(!open);messageToggleAt=now;}return 0;
 }
 if(commonReady&&open){
  ownInput=true;LRESULT handled=ImGui_ImplWin32_WndProcHandler(hwnd,msg,w,l);ownInput=false;
  if(msg==WM_KEYDOWN&&w==VK_ESCAPE&&!ImGui::GetIO().WantTextInput){SetVisible(false);return 0;}
  if(msg==WM_SETCURSOR){SetCursor(nullptr);return TRUE;}
  if(msg==WM_INPUT)return DefWindowProcW(hwnd,msg,w,l);
  if(handled||(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)||(msg>=WM_KEYFIRST&&msg<=WM_KEYLAST))return 0;
 }
 return originalWnd?CallWindowProcW(originalWnd,hwnd,msg,w,l):DefWindowProcW(hwnd,msg,w,l);
}
void CreateRtv(IDXGISwapChain* swap){if(rtv){rtv->Release();rtv=nullptr;}ID3D11Texture2D* back=nullptr;if(SUCCEEDED(swap->GetBuffer(0,IID_PPV_ARGS(&back)))){device11->CreateRenderTargetView(back,nullptr,&rtv);back->Release();}}
bool Common(HWND hwnd){
 if(commonReady)return true;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.ConfigWindowsMoveFromTitleBarOnly=true;
 wchar_t windows[32768];GetWindowsDirectoryW(windows,32768);auto font=std::filesystem::path(windows)/L"Fonts"/L"msyh.ttc";static std::string fontName=font.string();
 if(!io.Fonts->AddFontFromFileTTF(fontName.c_str(),22))io.Fonts->AddFontDefault();
 ImGui::StyleColorsDark();auto& st=ImGui::GetStyle();st.WindowRounding=12;st.ChildRounding=8;st.FrameRounding=6;st.PopupRounding=8;st.GrabRounding=5;st.WindowBorderSize=1;st.ChildBorderSize=1;st.FramePadding={9,6};st.ItemSpacing={10,8};st.ScrollbarSize=14;
 auto* co=st.Colors;co[ImGuiCol_WindowBg]={.078f,.094f,.125f,.97f};co[ImGuiCol_ChildBg]={.122f,.149f,.188f,.98f};co[ImGuiCol_Border]={.24f,.29f,.36f,.8f};co[ImGuiCol_Text]={.945f,.957f,.972f,1};co[ImGuiCol_TextDisabled]={.60f,.66f,.72f,1};co[ImGuiCol_FrameBg]={.145f,.18f,.227f,1};co[ImGuiCol_Button]={.13f,.30f,.35f,1};co[ImGuiCol_ButtonHovered]={.18f,.45f,.51f,1};co[ImGuiCol_ButtonActive]={.23f,.62f,.68f,1};co[ImGuiCol_Header]={.13f,.30f,.35f,1};co[ImGuiCol_HeaderHovered]={.18f,.45f,.51f,1};co[ImGuiCol_HeaderActive]={.23f,.55f,.61f,1};co[ImGuiCol_CheckMark]={.33f,.72f,.78f,1};co[ImGuiCol_SliderGrab]=co[ImGuiCol_CheckMark];co[ImGuiCol_TabSelected]={.13f,.30f,.35f,1};
 if(!ImGui_ImplWin32_Init(hwnd)){ImGui::DestroyContext();return false;}commonReady=true;return true;
}
void Teardown(){
 if(ready){if(is12)dx12::Shutdown();else{ImGui_ImplDX11_Shutdown();if(rtv){rtv->Release();rtv=nullptr;}if(context11){context11->Release();context11=nullptr;}if(device11){device11->Release();device11=nullptr;}}}
 if(commonReady){ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();commonReady=false;}ready=false;bound=nullptr;
}
bool Initialize(IDXGISwapChain* swap){
 DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(swap->GetDesc(&desc))||!desc.OutputWindow)return false;DWORD pid=0;GetWindowThreadProcessId(desc.OutputWindow,&pid);if(pid!=GetCurrentProcessId()||!IsWindowVisible(desc.OutputWindow))return false;
 ID3D12Device* d12=nullptr;is12=SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&d12)))&&d12;
 if(is12){if(!expectedDevice.load())expectedDevice=d12;d12->Release();if(!dx12::QueueReady())return false;}
 window=desc.OutputWindow;if(!Common(window))return false;
 if(is12){if(!dx12::Init(swap))return false;}else{if(FAILED(swap->GetDevice(IID_PPV_ARGS(&device11))))return false;device11->GetImmediateContext(&context11);if(!ImGui_ImplDX11_Init(device11,context11))return false;CreateRtv(swap);}
 if(!originalWnd)originalWnd=(WNDPROC)SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)WndProc);
 ready=true;bound=swap;firstTick=GetTickCount();NativeInputInitialize(window);RuntimeLog("In-game UI ready: %s hwnd=%p",is12?"DX12":"DX11",window);
 return true;
}
HRESULT WINAPI Present(IDXGISwapChain* swap,UINT sync,UINT flags){
 if(flags&DXGI_PRESENT_TEST)return originalPresent(swap,sync,flags);
 {
  std::lock_guard lock(renderMutex);
  if(!deviceLost){
   if(ready&&swap!=bound){DXGI_SWAP_CHAIN_DESC d{};if(SUCCEEDED(swap->GetDesc(&d))&&d.OutputWindow==window)Teardown();}
   if(!ready)Initialize(swap);
   if(ready&&swap==bound){
    int key;{std::lock_guard stateLock(app::Get().mutex);key=app::Get().data.menuKey;}
    static bool previous=false,panicPrevious=false;bool down=Key(key)&&!Key(VK_CONTROL);if(down&&!previous&&GetForegroundWindow()==window&&(DWORD)(GetTickCount()-messageToggleAt.load())>250){SetVisible(!open);messageToggleAt=GetTickCount();}previous=down;
    if(!autoOpened&&RuntimeState()&&RuntimeState()->rowCount>0){std::lock_guard stateLock(app::Get().mutex);autoOpened=true;if(app::Get().data.autoOpen)SetVisible(true);}
    bool panic=Key(VK_CONTROL)&&Key(VK_F9)&&(GetForegroundWindow()==window||NativeInputOwnWindow(GetForegroundWindow()));if(panic&&!panicPrevious){std::lock_guard stateLock(app::Get().mutex);app::RestoreAll();}panicPrevious=panic;
    ownInput=true;if(is12)dx12::NewFrame();else ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ownInput=false;
    auto& io=ImGui::GetIO();io.MouseDrawCursor=open;ImGui::NewFrame();
    NativeInputBeginFrame();if(open)DrawPanel();else if((DWORD)(GetTickCount()-firstTick)<9000){ImGui::SetNextWindowPos({26,28});ImGui::SetNextWindowBgAlpha(.84f);ImGui::Begin("##WorldDiffToast",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);ImGui::TextColored({.33f,.72f,.78f,1},"世界版差分管理器 1.0.7 已加载");ImGui::Text("F%d 打开面板  |  Ctrl+F9 暂停控制",key-VK_F1+1);ImGui::End();}NativeInputEndFrame();
    ImGui::Render();if(ImGui::GetDrawData()->CmdListsCount){if(is12)dx12::Render(ImGui::GetDrawData());else if(context11&&rtv){ID3D11RenderTargetView* before=nullptr;ID3D11DepthStencilView* depth=nullptr;context11->OMGetRenderTargets(1,&before,&depth);context11->OMSetRenderTargets(1,&rtv,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());context11->OMSetRenderTargets(1,&before,depth);if(before)before->Release();if(depth)depth->Release();}}
   }
  }
 }
 HRESULT hr=originalPresent(swap,sync,flags);if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET){deviceLost=true;SetVisible(false);RuntimeLog("Device lost: 0x%08X. Overlay stopped safely.",(unsigned)hr);}return hr;
}
HRESULT WINAPI Resize(IDXGISwapChain* swap,UINT count,UINT w,UINT h,DXGI_FORMAT fmt,UINT flags){std::lock_guard lock(renderMutex);if(ready&&swap==bound){if(is12)dx12::ReleaseBackBuffers();else if(rtv){rtv->Release();rtv=nullptr;}}HRESULT hr=originalResize(swap,count,w,h,fmt,flags);if(ready&&swap==bound&&SUCCEEDED(hr)){if(is12)dx12::RecreateBackBuffers(swap);else CreateRtv(swap);}return hr;}
void STDMETHODCALLTYPE Execute(ID3D12CommandQueue* q,UINT n,ID3D12CommandList*const* lists){
 if(q&&!dx12::QueueReady()&&q->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT){ID3D12Device* d=nullptr;if(SUCCEEDED(q->GetDevice(IID_PPV_ARGS(&d)))){if(d==expectedDevice.load())dx12::SetGameQueue(q);d->Release();}}
 originalExecute(q,n,lists);
}
}
bool Visible(){return open.load();}
void SetVisible(bool show){bool old=open.exchange(show);if(show)inputRequested=true;if(old!=show){if(show){ownInput=true;if(originalClip)originalClip(nullptr);else ClipCursor(nullptr);ownInput=false;}else{NativeInputClose();std::lock_guard lock(clipMutex);if(originalClip)originalClip(hasGameClip&&GetForegroundWindow()==window?&lastGameClip:nullptr);}RuntimeLog("Panel %s (blocked cursor warps=%llu)",show?"opened":"closed",blockedWarps.load());}}
const char* BackendStatus(){return ready?(is12?"DirectX 12 · 游戏命令队列":"DirectX 11"):deviceLost?"设备已重置，面板停止":"等待游戏渲染";}
void WorkerTick(){
 if(inputRequested&&!inputInstalled)InstallInput();
 static std::string last;std::ifstream f(PluginDirectory()/L"WorldDiff"/L"WorldDiff.control",std::ios::binary);std::string command;std::getline(f,command);if(command.empty()||command==last)return;last=command;
 if(command.rfind("open",0)==0)SetVisible(true);else if(command.rfind("close",0)==0)SetVisible(false);else if(command.rfind("reload",0)==0){std::lock_guard lock(app::Get().mutex);app::Load();}else if(command.rfind("save",0)==0){std::lock_guard lock(app::Get().mutex);app::Save();}
}
bool Install(){
 WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WorldDiff104Dummy";RegisterClassW(&wc);HWND dummy=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,8,8,nullptr,nullptr,wc.hInstance,nullptr);if(!dummy)return false;
 DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=dummy;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
 ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;IDXGISwapChain* swap=nullptr;D3D_FEATURE_LEVEL level{};
 HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&dev,&level,&ctx);if(FAILED(hr))hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&dev,&level,&ctx);
 bool p=false,r=false,e=false;if(SUCCEEDED(hr)&&swap){void** vt=*(void***)swap;p=Hook(vt[8],(void*)Present,originalPresent);r=Hook(vt[13],(void*)Resize,originalResize);}
 ID3D12Device* d12=nullptr;ID3D12CommandQueue* q=nullptr;if(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12)))){D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;if(SUCCEEDED(d12->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)))){void** vt=*(void***)q;e=Hook(vt[10],(void*)Execute,originalExecute);q->Release();}d12->Release();}
 if(swap)swap->Release();if(ctx)ctx->Release();if(dev)dev->Release();DestroyWindow(dummy);RuntimeLog("Overlay hooks: Present=%d Resize=%d Queue=%d",p,r,e);return p&&r;
}
}
