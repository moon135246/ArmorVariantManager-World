#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
namespace worlddiff {
constexpr uint32_t kMagic=0x34304457,kVersion=104,kMaxRows=8192;
constexpr wchar_t kMappingPrefix[]=L"Local\\WorldDiff104_";
struct alignas(8) LiveRow {
 volatile LONG ready;
 char modelPath[256];
 uint32_t meshIndex, materialId, groupId;
 uint64_t resource, query;
 char materialName[128];
 uint32_t materialHash,meshCount,materialCount,lod;
 volatile LONG hide; // 0 = follow game's visibility, 1 = hide
 volatile LONG hits, hiddenHits, lastTick, originalVisible;
};
struct alignas(8) SharedState {
 uint32_t magic,version,pid,rowCapacity;
 volatile LONG ready,enabled,rowCount,overflow;
 volatile LONG heartbeat,lastError;
 char message[256];
 LiveRow rows[kMaxRows];
};
static_assert(sizeof(LiveRow)==456);
}
