#pragma once
#include "Dx12SceneDraw.h"
#include "NativeCoverage.h"
#include <filesystem>

namespace worlddiff::scene_renderer {
void Tick(const std::filesystem::path&);
void NextFrame(bool dx12);
bool Admit(const ModelPolicy& owner,unsigned row,bool shown,bool resolved,bool controllerWins);
}
