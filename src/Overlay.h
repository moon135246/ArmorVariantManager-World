#pragma once
#include "Protocol.h"
#include <filesystem>
namespace worlddiff {
void RuntimeLog(const char*,...);
SharedState* RuntimeState();
std::filesystem::path PluginDirectory();
namespace overlay {
bool Install();
void DrawPanel();
bool Visible();void SetVisible(bool);
const char* BackendStatus();
void WorkerTick();
}
}
