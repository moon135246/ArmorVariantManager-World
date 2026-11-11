#pragma once
#include "Dx12SceneDraw.h"
#include <filesystem>

namespace worlddiff::scene_handoff {
// Development build. One explicitly selected pair owns immutable upload
// memory for the process lifetime, including command lists still in flight.
void Tick(const std::filesystem::path& folder, const std::filesystem::path& gameRoot);
bool Requested();
bool MightDraw(uintptr_t packet);
struct Submission { unsigned indices; uint64_t frame=0; unsigned cut=0; };
bool Submit(const dissolve::SceneDraw& hit, const RenderPolicies& policies, Submission& submitted);
bool Admit(const ModelPolicy& policy, unsigned row, bool shown, bool resolved);
void NextFrame();
} // namespace worlddiff::scene_handoff
