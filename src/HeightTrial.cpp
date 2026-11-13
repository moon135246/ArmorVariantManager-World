#include "HeightTrial.h"
#include "Overlay.h"
#include "State.h"
#include "safetyhook.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

namespace worlddiff::height_trial {
namespace {
std::atomic<uintptr_t> target{};
std::atomic<uintptr_t> bodyResource{};
std::atomic<float> applied{};
SafetyHookMid fullPose, partialPose;

// Both pose loops have just computed a fresh world matrix. The leg IK targets
// are parentless too. Preserve their displacement relative to the hips.
void Adjust(SafetyHookContext& context) {
    const float cm=applied.load(std::memory_order_relaxed);
    if (!cm || context.rdi != target.load(std::memory_order_relaxed)) return;
    __try {
        auto actor = context.rdi;
        auto joint = context.rbx;
        if (*reinterpret_cast<unsigned char*>(joint + 0xa4) != 255 ||
            *reinterpret_cast<uintptr_t*>(joint + 0xb0) != actor) return;
        auto model = *reinterpret_cast<uintptr_t*>(actor + 0x2a0);
        if (!model || model!=bodyResource.load(std::memory_order_relaxed)) return;
        auto& y = *reinterpret_cast<float*>(joint + 0x54);
        if (std::isfinite(y)) y += cm;
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
void FullPose(SafetyHookContext& context) { if (context.rsi) Adjust(context); }
void PartialPose(SafetyHookContext& context) { if (context.rbp) Adjust(context); }
}
void Install() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto full=base+0x223b9e7, partial=base+0x223bcea;
    constexpr unsigned char fullSignature[]={0x48,0x8b,0xd3,0x48,0x8b,0xcf,0xff,0xd6,0x45,0x84,0xff,0x74,0x15,0x48,0x8b,0x4b,0x08};
    constexpr unsigned char partialSignature[]={0x48,0x8b,0xd3,0x48,0x8b,0xcf,0xff,0xd5,0x45,0x84,0xed,0x74,0x15,0x48,0x8b,0x4b,0x08};
    if(!memcmp(full-8,fullSignature,sizeof(fullSignature)) && !memcmp(partial-8,partialSignature,sizeof(partialSignature))) {
        fullPose=safetyhook::create_mid(full,FullPose);
        partialPose=safetyhook::create_mid(partial,PartialPose);
    }
    RuntimeLog("Height trial: world joint callbacks %s",fullPose && partialPose?"ready":"unavailable");
}
void Tick() {
    auto& state=app::Get();
    std::lock_guard lock(state.mutex);
    const auto& model=state.outfit;
    const auto height=app::WornEquipmentHeight(model);
    if (!height.enabled || !fullPose || !partialPose || !model.resources[1]) {
        applied=0;
        target=0;
        return;
    }
    if (target.load()!=model.entity || bodyResource.load()!=model.resources[1]) applied=0;
    bodyResource=model.resources[1];
    target=model.entity;
    applied=height.tenthsCm/10.f;
}
}
