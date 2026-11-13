#include "HeightTrial.h"
#include "Overlay.h"
#include "State.h"
#include "safetyhook.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <mutex>

namespace worlddiff::height_trial {
namespace {
std::atomic<uintptr_t> target{};
std::atomic<ULONGLONG> start{};
std::atomic<unsigned> frames{};
std::atomic<float> applied{};
std::mutex controlMutex;
bool enabled=false;
float draftCm=10.0f;
SafetyHookMid fullPose, partialPose;
constexpr auto outfit = "pl\\f_equip\\pl105_0000\\";
constexpr char bodyPath[] = "pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";

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
        if (!model || memcmp(reinterpret_cast<const char*>(model + 12),bodyPath,sizeof(bodyPath))) return;
        auto& y = *reinterpret_cast<float*>(joint + 0x54);
        if (std::isfinite(y)) { y += cm; ++frames; }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
void FullPose(SafetyHookContext& context) { if (context.rsi) Adjust(context); }
void PartialPose(SafetyHookContext& context) { if (context.rbp) Adjust(context); }
}
Controls GetControls() {
    std::lock_guard lock(controlMutex);
    return {target.load()!=0,enabled,draftCm};
}
void SetControls(bool on, float cm) {
    std::lock_guard lock(controlMutex);
    if(!target.load() || !std::isfinite(cm)) return;
    start=0;
    draftCm=std::round(std::clamp(cm,-10.0f,50.0f)*10.0f)/10.0f;
    enabled=on;
    applied=on?draftCm:0.0f;
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
    auto model=app::ReadPlayerOutfit();
    bool eligible=fullPose && partialPose && model.ready;
    for(const auto& path:model.paths) eligible=eligible && path.starts_with(outfit);
    auto actor=eligible?model.entity:0;
    std::lock_guard lock(controlMutex);
    if(target.load()!=actor) {
        applied=0; start=0; enabled=false; draftCm=10; target=actor;
    }
    // The local diagnostic retains the three-stage sequence from Beta.2.
    static const auto trigger=PluginDirectory()/L"WorldDiff"/L"height-sequence.test";
    std::error_code error;
    if(actor && std::filesystem::remove(trigger,error)) {
        frames=0; applied=0; enabled=false; draftCm=10;
        start=GetTickCount64(); RuntimeLog("Gait trial: start 0/+10/0");
    }
    auto begin=start.load();
    if(!begin) return;
    auto elapsed=GetTickCount64()-begin;
    enabled=elapsed>=8000 && elapsed<16000;
    applied=enabled?10.0f:0.0f;
    if(elapsed>=24000) { RuntimeLog("Gait trial: completed, adjusted roots=%u",frames.load()); start=0; }
}
}
