#pragma once
#include "RenderPolicy.h"
#include "Dx12DissolveDraw.h"
#include <cstring>

namespace worlddiff::dissolve {
// Borrowed only until the native indexed draw returns (Steam 421810).
struct SceneDraw {
    ID3D12GraphicsCommandList* commands{};
    ID3D12PipelineState* pipeline{};
    ID3D12RootSignature* root{};
    D3D12_DRAW_INDEXED_ARGUMENTS draw{};
    MeshDrawBinding binding{};
    uintptr_t depthView{};
    unsigned colors{}, part{}, mesh{};
};

template<typename T> inline T SceneField(uintptr_t address, size_t offset) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address + offset), sizeof(value));
    return value;
}

// Resolve CBModel from the actual material's VS mapping. The engine constant
// ID is shared, while the shader register can vary by material.
inline bool ReadSceneDraw(uintptr_t list, uintptr_t backend, uintptr_t worker, uintptr_t packet,
                          const RenderPolicies& policies, SceneDraw& result) {
    __try {
        if (!list || !backend || !packet || !worker || !policies.enabled ||
            SceneField<uintptr_t>(backend, 0x20) != list ||
            SceneField<uintptr_t>(backend, 0x158) != packet ||
            (SceneField<unsigned char>(packet, 0) & 31) != 1) return false;
        const auto material = SceneField<uintptr_t>(packet, 0x20);
        const auto shaderSet = SceneField<uintptr_t>(material, 8);
        const auto vs = SceneField<uintptr_t>(shaderSet, 0);
        const auto map = SceneField<uintptr_t>(vs, 0);
        const auto count = SceneField<uint16_t>(vs, 8);
        const auto start = SceneField<uint16_t>(vs, 10);
        if (!map || count > 256 || start > 1024) return false;
        uintptr_t owner = 0;
        for (unsigned i = 0; i < count; ++i) {
            const auto entry = SceneField<uint32_t>(map, 4 * (start + i));
            if ((entry & 15) || ((entry >> 12) & 4095) != 51) continue;
            if(owner)continue; // repeated registration of the same constant
            const auto cb = SceneField<uintptr_t>(worker, 0x1860 + 8 * 51);
            if (!cb) return false;
            owner = SceneField<uintptr_t>(cb, 160);
            if (!owner) return false;
        }
        const auto* p = owner ? policies.Find(owner) : nullptr;
        if (!p || !p->local || p->preview || !p->actor || p->actor != policies.localEntity ||
            p->part >= 5 || localModelContexts[p->part].load(std::memory_order_relaxed) != owner ||
            !p->resource || !p->meshes ||
            SceneField<uintptr_t>(owner, 0x2a0) != p->resource ||
            SceneField<uintptr_t>(p->resource, 0xc8) != p->meshes ||
            SceneField<uint32_t>(p->resource, 0xd0) != p->materials.size()) return false;

        const auto view = SceneField<D3D12_INDEX_BUFFER_VIEW>(backend, 0x88);
        const auto indexObject = SceneField<uintptr_t>(p->resource, 0x108);
        const auto buffer = SceneField<uintptr_t>(indexObject, 0x60);
        if (!buffer || !view.BufferLocation || view.Format != DXGI_FORMAT_R16_UINT ||
            view.BufferLocation != SceneField<UINT64>(buffer, 0x40) ||
            view.SizeInBytes != SceneField<UINT>(buffer, 0x10)) return false;
        const MeshIndexRange range{SceneField<UINT>(packet, 0x44), SceneField<UINT>(packet, 0x40),
                                   SceneField<INT>(packet, 0x48)};
        if (!range.count || range.count % 3 || UINT64(range.first) + range.count > view.SizeInBytes / 2) return false;
        unsigned matched = UINT_MAX;
        for (unsigned i = 0; i < p->materials.size(); ++i) {
            const auto row = p->meshes + size_t(i) * 80;
            if (range.first != SceneField<UINT>(row, 0x1c) || range.count != SceneField<UINT>(row, 0x20) ||
                range.base != SceneField<INT>(row, 0x24)) continue;
            if (matched != UINT_MAX || p->materials[i] != SceneField<uint16_t>(row, 6)) return false;
            matched = i;
        }
        if (matched == UINT_MAX) return false;
        const auto targets = SceneField<uintptr_t>(packet, 0x10);
        SceneDraw hit{};
        hit.commands = reinterpret_cast<ID3D12GraphicsCommandList*>(list);
        hit.pipeline = SceneField<ID3D12PipelineState*>(backend, 0x108);
        hit.root = SceneField<ID3D12RootSignature*>(backend, 0x110);
        if (!hit.root || !hit.pipeline) return false;
        hit.draw = {range.count, 1, range.first, range.base, 0};
        hit.binding = {owner, view};
        hit.depthView = SceneField<uintptr_t>(targets, 0x40);
        hit.colors = SceneField<unsigned char>(targets, 0x48) & 7;
        hit.part = p->part;
        hit.mesh = matched;
        result = hit;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace worlddiff::dissolve
