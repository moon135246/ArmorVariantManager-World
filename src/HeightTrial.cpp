#include "HeightTrial.h"
#include "Overlay.h"
#include "State.h"
#include "safetyhook.hpp"
#include <atomic>
#include <cmath>
#include <cstring>

namespace worlddiff::height_trial {
namespace {
std::atomic<uintptr_t> target{};
std::atomic<ULONGLONG> start{};
std::atomic<unsigned> frames{};
SafetyHookMid fullPose, partialPose;
constexpr auto outfit = "pl\\f_equip\\pl105_0000\\";
constexpr char bodyPath[] = "pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";

// Both 15.23 pose loops have just rebuilt this joint's world matrix. Children
// and attached models subsequently consume it. The local pose stays native.
void Adjust(SafetyHookContext& context) {
    if (context.rdi != target.load(std::memory_order_relaxed)) return;
    __try {
        auto actor = context.rdi;
        auto joint = context.rbx;
        if (joint != *reinterpret_cast<uintptr_t*>(actor + 0x4a8) ||
            *reinterpret_cast<uintptr_t*>(joint + 0xb0) != actor ||
            *reinterpret_cast<unsigned char*>(joint + 0xa4) != 255 || !Height()) return;
        auto model = *reinterpret_cast<uintptr_t*>(actor + 0x2a0);
        if (!model || memcmp(reinterpret_cast<const char*>(model + 12),bodyPath,sizeof(bodyPath))) return;
        auto& y = *reinterpret_cast<float*>(joint + 0x54);
        if (std::isfinite(y)) { y += 10.0f; ++frames; }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
void FullPose(SafetyHookContext& context) { if (context.rsi) Adjust(context); }
void PartialPose(SafetyHookContext& context) { if (context.rbp) Adjust(context); }
}
int Height() {
    auto begin=start.load();
    if (!begin) return 0;
    auto elapsed=GetTickCount64()-begin;
    return elapsed>=8000 && elapsed<16000 ? 10 : 0;
}
void StartSequence() { if(fullPose && partialPose && target.load()) { frames=0; start=GetTickCount64(); RuntimeLog("Height trial: start 0/+10/0, 8 seconds per phase"); } }
void Reset() { start=0; }
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
    bool eligible=model.ready;
    for(const auto& path:model.paths) eligible=eligible && path.starts_with(outfit);
    auto actor=eligible?model.entity:0;
    if(target.exchange(actor)!=actor) Reset();
    // Local test runner and the settings button start the same one-shot.
    static const auto trigger=PluginDirectory()/L"WorldDiff"/L"height-sequence.test";
    std::error_code error;
    if(actor && std::filesystem::remove(trigger,error)) StartSequence();
    auto begin=start.load();
    if(!begin) return;
    auto elapsed=GetTickCount64()-begin;
    if(elapsed>=24000) { RuntimeLog("Height trial: completed, adjusted updates=%u",frames.load()); Reset(); }
}
}
