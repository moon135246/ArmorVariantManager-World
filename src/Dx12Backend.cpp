// ===========================================================================
//  Dx12Backend.cpp -- 见 Dx12Backend.h 的说明
// ===========================================================================
#include "Dx12Backend.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#include "imgui.h"
#include "imgui_impl_dx12.h"

namespace dx12 {
namespace {

// 由 WorldDiff.cpp 提供


constexpr UINT kFramesInFlight = 3;
constexpr UINT kSrvHeapSize = 128;
constexpr UINT kMaxBackBuffers = 8;

ID3D12Device* g_device = nullptr;
ID3D12CommandQueue* g_gameQueue = nullptr;  // 游戏自己的 DIRECT 队列（首选）
IDXGISwapChain3* g_swap = nullptr;
IDXGISwapChain* g_sourceSwap = nullptr;
ID3D12DescriptorHeap* g_rtvHeap = nullptr;
ID3D12DescriptorHeap* g_srvHeap = nullptr;
ID3D12GraphicsCommandList* g_cmdList = nullptr;
ID3D12CommandAllocator* g_alloc[kFramesInFlight] = {};
ID3D12Resource* g_backBuffer[kMaxBackBuffers] = {};
D3D12_CPU_DESCRIPTOR_HANDLE g_rtvHandle[kMaxBackBuffers] = {};
ID3D12Fence* g_fence = nullptr;
HANDLE g_fenceEvent = nullptr;
UINT64 g_fenceValue = 0;
UINT64 g_frameFence[kFramesInFlight] = {};
UINT g_frameIndex = 0;
UINT g_rtvSize = 0;
UINT g_srvSize = 0;
UINT g_bufferCount = 0;
DXGI_FORMAT g_format = DXGI_FORMAT_UNKNOWN;
bool g_ready = false;

// 提交与上传都优先用游戏的队列，避免跨队列无序写后备缓冲
ID3D12CommandQueue* Queue() { return g_gameQueue; }
char g_status[192] = "未初始化";

// SRV 描述符的分配器（给 ImGui 的纹理用）
std::vector<UINT> g_srvFree;
UINT g_srvNext = 0;

void SrvAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
              D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    UINT idx;
    if (!g_srvFree.empty()) {
        idx = g_srvFree.back();
        g_srvFree.pop_back();
    } else {
        idx = g_srvNext++;
    }
    cpu->ptr = g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)idx * g_srvSize;
    gpu->ptr = g_srvHeap->GetGPUDescriptorHandleForHeapStart().ptr + (UINT64)idx * g_srvSize;
}

void SrvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
             D3D12_GPU_DESCRIPTOR_HANDLE) {
    const SIZE_T base = g_srvHeap->GetCPUDescriptorHandleForHeapStart().ptr;
    if (cpu.ptr < base) return;
    g_srvFree.push_back((UINT)((cpu.ptr - base) / g_srvSize));
}

bool CreateBackBufferRtvs(IDXGISwapChain* swap) {
    for (UINT i = 0; i < g_bufferCount; ++i) {
        if (g_backBuffer[i]) {
            g_backBuffer[i]->Release();
            g_backBuffer[i] = nullptr;
        }
        if (FAILED(swap->GetBuffer(i, IID_PPV_ARGS(&g_backBuffer[i]))) || !g_backBuffer[i]) {
            HostLog("DX12: GetBuffer(%u) 失败", i);
            return false;
        }
        g_device->CreateRenderTargetView(g_backBuffer[i], nullptr, g_rtvHandle[i]);
    }
    return true;
}

}  // namespace

bool UsesQueue(ID3D12CommandQueue* queue) { return g_ready && queue == g_gameQueue; }

bool Init(IDXGISwapChain* swap, ID3D12CommandQueue* queue) {
    if (g_ready) return swap == g_sourceSwap && UsesQueue(queue);
    if (!swap || !queue || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return false;
    Shutdown(); // also cleans up partial initialization
    struct InitGuard { bool ok = false; ~InitGuard() { if (!ok) Shutdown(); } } guard;
    queue->AddRef();
    g_gameQueue = queue;
    swap->AddRef();
    g_sourceSwap = swap;
    // Swapchain::GetDevice may expose a raw device while the game's queue is
    // wrapped. Keep device, allocators and command lists in the queue's domain.
    if (FAILED(queue->GetDevice(IID_PPV_ARGS(&g_device))) || !g_device) {
        HostLog("DX12: 游戏队列未提供有效 ID3D12Device");
        return false;
    }
    if (FAILED(swap->QueryInterface(IID_PPV_ARGS(&g_swap))) || !g_swap) {
        HostLog("DX12: 拿不到 IDXGISwapChain3（无法查当前后备缓冲索引）");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap->GetDesc(&desc))) {
        HostLog("DX12: GetDesc 失败");
        return false;
    }
    g_format = desc.BufferDesc.Format;
    g_bufferCount = desc.BufferCount;
    if (!g_bufferCount || g_bufferCount > kMaxBackBuffers) {
        HostLog("DX12: 不支持的后备缓冲数量 %u", g_bufferCount); return false;
    }

    for (UINT i = 0; i < kFramesInFlight; ++i) {
        if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(&g_alloc[i]))) || !g_alloc[i]) {
            HostLog("DX12: CreateCommandAllocator 失败");
            return false;
        }
    }
    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_alloc[0],
                                           nullptr, IID_PPV_ARGS(&g_cmdList))) || !g_cmdList) {
        HostLog("DX12: CreateCommandList 失败");
        return false;
    }
    if (FAILED(g_cmdList->Close())) return false;

    D3D12_DESCRIPTOR_HEAP_DESC rh{};
    rh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rh.NumDescriptors = kMaxBackBuffers;
    if (FAILED(g_device->CreateDescriptorHeap(&rh, IID_PPV_ARGS(&g_rtvHeap))) || !g_rtvHeap) {
        HostLog("DX12: RTV 描述符堆创建失败");
        return false;
    }
    g_rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    {
        D3D12_CPU_DESCRIPTOR_HANDLE h = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < kMaxBackBuffers; ++i) {
            g_rtvHandle[i] = h;
            h.ptr += g_rtvSize;
        }
    }

    D3D12_DESCRIPTOR_HEAP_DESC sh{};
    sh.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    sh.NumDescriptors = kSrvHeapSize;
    sh.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(g_device->CreateDescriptorHeap(&sh, IID_PPV_ARGS(&g_srvHeap))) || !g_srvHeap) {
        HostLog("DX12: SRV 描述符堆创建失败");
        return false;
    }
    g_srvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))) || !g_fence) {
        HostLog("DX12: CreateFence 失败");
        return false;
    }
    g_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent) {
        HostLog("DX12: CreateEvent 失败");
        return false;
    }

    if (!CreateBackBufferRtvs(swap)) return false;

    ImGui_ImplDX12_InitInfo info{};
    info.Device = g_device;
    info.CommandQueue = Queue();
    info.NumFramesInFlight = (int)kFramesInFlight;
    info.RTVFormat = g_format;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.SrvDescriptorHeap = g_srvHeap;
    info.SrvDescriptorAllocFn = &SrvAlloc;
    info.SrvDescriptorFreeFn = &SrvFree;
    if (!ImGui_ImplDX12_Init(&info)) {
        HostLog("DX12: ImGui_ImplDX12_Init 失败");
        return false;
    }

    g_ready = true;
    guard.ok = true;
    snprintf(g_status, sizeof(g_status), "DX12 就绪（%u 个后备缓冲，格式 %d）",
             g_bufferCount, (int)g_format);
    HostLog("DX12: %s, device=%p queue=%p", g_status, g_device, g_gameQueue);
    return true;
}

void NewFrame() {
    if (g_ready) ImGui_ImplDX12_NewFrame();
}

void Render(ImDrawData* drawData) {
    if (!g_ready || !drawData || !g_swap) return;

    // 等上一轮我们自己提交的绘制完成，才能复用这个分配器
    if (g_fence->GetCompletedValue() < g_frameFence[g_frameIndex]) {
        if (FAILED(g_fence->SetEventOnCompletion(g_frameFence[g_frameIndex], g_fenceEvent)) ||
            WaitForSingleObject(g_fenceEvent, 1000) != WAIT_OBJECT_0) return;
    }

    const UINT idx = g_swap->GetCurrentBackBufferIndex();
    if (idx >= g_bufferCount || !g_backBuffer[idx]) return;

    if (FAILED(g_alloc[g_frameIndex]->Reset())) return;
    if (FAILED(g_cmdList->Reset(g_alloc[g_frameIndex], nullptr))) return;

    // 后备缓冲此刻处于 PRESENT 状态（DXGI 要求 Present 前必须如此），
    // 我们要往上面画，就必须手动转到 RENDER_TARGET，画完再转回去。
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_backBuffer[idx];
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_cmdList->ResourceBarrier(1, &barrier);

    g_cmdList->OMSetRenderTargets(1, &g_rtvHandle[idx], FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = {g_srvHeap};
    g_cmdList->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(drawData, g_cmdList);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_cmdList->ResourceBarrier(1, &barrier);

    if (FAILED(g_cmdList->Close())) return;

    ID3D12CommandList* lists[] = {g_cmdList};
    Queue()->ExecuteCommandLists(1, lists);
    Queue()->Signal(g_fence, ++g_fenceValue);
    g_frameFence[g_frameIndex] = g_fenceValue;
    g_frameIndex = (g_frameIndex + 1) % kFramesInFlight;
}

void ReleaseBackBuffers() {
    if (!g_ready) return;
    // 等所有在飞的绘制结束，再释放对后备缓冲的引用，否则 ResizeBuffers 会失败
    if (g_fence && Queue()) {
        Queue()->Signal(g_fence, ++g_fenceValue);
        if (SUCCEEDED(g_fence->SetEventOnCompletion(g_fenceValue, g_fenceEvent))) {
            WaitForSingleObject(g_fenceEvent, 2000);
        }
    }
    for (UINT i = 0; i < kMaxBackBuffers; ++i) {
        if (g_backBuffer[i]) {
            g_backBuffer[i]->Release();
            g_backBuffer[i] = nullptr;
        }
    }
    ImGui_ImplDX12_InvalidateDeviceObjects();
}

bool RecreateBackBuffers(IDXGISwapChain* swap) {
    if (!g_ready) return false;
    if (!CreateBackBufferRtvs(swap)) return false;
    return ImGui_ImplDX12_CreateDeviceObjects();
}

void Shutdown() {
    if (g_ready) {
        ReleaseBackBuffers();
        ImGui_ImplDX12_Shutdown();
    }
    g_ready = false;

    for (UINT i = 0; i < kMaxBackBuffers; ++i) {
        if (g_backBuffer[i]) { g_backBuffer[i]->Release(); g_backBuffer[i] = nullptr; }
    }
    for (UINT i = 0; i < kFramesInFlight; ++i) {
        if (g_alloc[i]) { g_alloc[i]->Release(); g_alloc[i] = nullptr; }
    }
    if (g_cmdList) { g_cmdList->Release(); g_cmdList = nullptr; }
    if (g_rtvHeap) { g_rtvHeap->Release(); g_rtvHeap = nullptr; }
    if (g_srvHeap) { g_srvHeap->Release(); g_srvHeap = nullptr; }
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_sourceSwap) { g_sourceSwap->Release(); g_sourceSwap = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
    if (g_gameQueue) { g_gameQueue->Release(); g_gameQueue = nullptr; }
    if (g_fence) { g_fence->Release(); g_fence = nullptr; }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = nullptr; }
    g_srvFree.clear();
    g_srvNext = 0;
    g_frameIndex = 0;
    g_fenceValue = 0;
    memset(g_frameFence, 0, sizeof(g_frameFence));
    snprintf(g_status, sizeof(g_status), "已关闭");
    g_bufferCount = 0;
}

const char* Status() { return g_status; }

}  // namespace dx12
