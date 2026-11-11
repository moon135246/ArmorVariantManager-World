#pragma once
#include "DissolveStyle.h"
#include <cmath>
#include <cstdint>
#include <optional>

namespace worlddiff::dissolve {
enum class DrawRoute { skip, original, clipped };

// A phase is supplied only when the renderer has prepared this instance and
// mesh in all its required passes. Both visibility inputs include arbitration.
inline DrawRoute RouteMesh(bool visibleWhenShown,bool finalVisible,std::optional<float> preparedHidden) {
    if(!preparedHidden || !std::isfinite(*preparedHidden) || *preparedHidden<0.f || *preparedHidden>1.f)
        return finalVisible?DrawRoute::original:DrawRoute::skip;
    if(!visibleWhenShown)return DrawRoute::skip;
    if(*preparedHidden==0.f)return DrawRoute::original;
    if(*preparedHidden==1.f)return DrawRoute::skip;
    return DrawRoute::clipped;
}
// Hidden fraction is the shader's progression: zero displays the original mesh.
struct Transition {
    std::uint32_t mesh = 0, changedAt = 0, delayMs = 0, durationMs = 0;
    float from = 0, to = 0;
    bool keepSuffix = false;
    Style style=DefaultStyle;

    float At(std::uint32_t frameTime) const {
        if (static_cast<std::int32_t>(frameTime-changedAt)<0) return from;
        const auto elapsed = frameTime - changedAt;
        if (elapsed < delayMs) return from;
        if (elapsed - delayMs >= durationMs) return to;
        const float t = static_cast<float>(elapsed - delayMs) / durationMs;
        return from + (to - from) * t * t * (3.0f - 2.0f * t);
    }
    bool Finished(std::uint32_t now) const {
        return now - changedAt >= delayMs + durationMs;
    }
};

inline Transition Retarget(std::uint32_t mesh, std::uint32_t now, float from,
                           bool hide, bool pairedIncoming) {
    const float to = hide ? 1.0f : 0.0f;
    const float distance = std::abs(to - from);
    const auto duration = static_cast<std::uint32_t>(std::ceil(distance * (hide ? 400.f : 300.f)));
    return {mesh, now, pairedIncoming && !hide && from == 1.f ? 280u : 0u,
            duration, from, to};
}

// Matched rows keep the same reference geometry when a handoff reverses.
// Only use this partition while at least one row contributes to the pair.
inline float HandoffBoundary(float firstHidden,float secondHidden) {
    const float firstWeight=1.f-firstHidden,secondWeight=1.f-secondHidden;
    if(secondWeight<=0.f)return 0.f;
    if(firstWeight<=0.f)return 1.f;
    return secondWeight/(firstWeight+secondWeight);
}

// A complementary partition has one position. Retarget the current sweep
// directly; deriving it from two delayed fades confines motion to their overlap.
struct PairSweep {
    double startMs=0.,durationMs=0.;
    float from=0.f,to=0.f;

    float At(double now) const {
        if (durationMs<=0. || now>=startMs+durationMs) return to;
        if (now<=startMs) return from;
        const float t=static_cast<float>((now-startMs)/durationMs);
        return from+(to-from)*t*t*(3.f-2.f*t);
    }
    void Set(bool secondVisible,double now,double fullDurationMs,bool reset) {
        const float target=secondVisible?1.f:0.f;
        if(reset) { from=to=target;startMs=now;durationMs=0.;return; }
        if(target==to)return;
        from=At(now);to=target;startMs=now;durationMs=std::abs(to-from)*fullDurationMs;
    }
};
} // namespace worlddiff::dissolve
