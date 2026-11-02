// ===========================================================================
//  Dx12Backend -- WorldDiff 的 DirectX 12 渲染后端
// ---------------------------------------------------------------------------
//  为什么需要它：MHW 的 graphics_option.ini 里 DirectX12Enable=On 时，
//  交换链属于 ID3D12Device，DX11 的 ImGui 后端拿设备/后备缓冲都会得到
//  E_NOINTERFACE。所以必须给 DX12 单独一条渲染路径。
//
//  与 DX11 路径的关键差别（DX12 什么都不自动做）：
//    * 后备缓冲的 RTV 要自己建（描述符堆）
//    * 每帧要把后备缓冲 PRESENT -> RENDER_TARGET -> PRESENT 手动转换
//    * 命令要用自己的命令队列提交，并用围栏等待上一帧画完再复用分配器
//    * ImGui 的字体纹理 SRV 由我们分配（1.92 要求提供 alloc/free 回调）
// ===========================================================================
#pragma once

struct IDXGISwapChain;
struct ImDrawData;
struct ID3D12CommandQueue;

namespace dx12 {

// 日志出口，由宿主（WorldDiff.cpp）实现
void HostLog(const char* fmt, ...);

// 把抓到的「游戏自己的命令队列」交给后端。
// 必须在它上面提交覆盖层绘制：用自己的队列会和游戏对后备缓冲的写入无序，
// 实测会导致游戏在自家代码里 AV 崩溃（异常码 0xc0000005，错误模块 MonsterHunterWorld.exe）。
void SetGameQueue(ID3D12CommandQueue* queue);
bool QueueReady();

// 判定这个交换链是不是 DX12 的
bool IsDx12Swapchain(IDXGISwapChain* swap);

// 建 DX12 对象并初始化 ImGui 的 DX12 后端（ImGui 上下文与 Win32 后端须已就绪）
bool Init(IDXGISwapChain* swap);

// 拆掉后端并释放所有 DX12 对象（交换链变化时用）
void Shutdown();

void NewFrame();
void Render(ImDrawData* drawData);

// ResizeBuffers 前后必须分两步：之前放掉对后备缓冲的引用，之后重建 RTV。
void ReleaseBackBuffers();
bool RecreateBackBuffers(IDXGISwapChain* swap);

// 给日志用的一句话状态
const char* Status();

}  // namespace dx12
