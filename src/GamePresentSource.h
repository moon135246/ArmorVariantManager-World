#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <cstdint>
#include <cstring>

namespace worlddiff::present_source {
// Steam 15.23.00 (421810). Validated against the running executable, not
// offsets into DXGI or a third-party DLL. A mismatch leaves the overlay inert.
inline constexpr uintptr_t kDx12Call = 0x23AA1EA;
inline constexpr uintptr_t kDx11Call = 0x23AA337;
inline constexpr unsigned char kDx12Signature[] = {
    0xFF,0x50,0x40,0xC6,0x83,0xD9,0x10,0,0,1,0x85,0xC0,0x75,0x1A,
    0x41,0xFF,0xC4,0x44,0x8D,0x78,1};
inline constexpr unsigned char kDx11Signature[] = {
    0xFF,0x50,0x40,0x8B,0xF0,0x85,0xC0,0x75,0x5F,0xFF,0xC3,
    0x3B,0x9F,0xA0,0x14,0,0};

template<size_t N> bool Matches(const void* address, const unsigned char (&bytes)[N]) {
    unsigned char actual[N]{};
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, actual, N, &got) &&
           got == N && memcmp(actual, bytes, N) == 0;
}

// At the DX12 Present call RBX is cD3DRender and R12 is its swapchain index.
// Read the exact pair used by this call; never choose the first DIRECT queue
// seen in the process, and never try to unwrap a third-party COM object.
inline ID3D12CommandQueue* QueueForCall(uintptr_t renderer, uintptr_t index, IDXGISwapChain* swap) {
    if (!renderer || !swap || index >= 8) return nullptr;
    __try {
        if (*(IDXGISwapChain**)(renderer + 0x1470 + index * 0x48) != swap) return nullptr;
        auto* queue = *(ID3D12CommandQueue**)(renderer + 0x20);
        if (!queue || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return nullptr;
        return queue; // borrowed, within the game's own Present call
    } __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
}
