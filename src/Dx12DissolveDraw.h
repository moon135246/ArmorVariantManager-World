#pragma once
#include <d3d12.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace worlddiff::dissolve {

// The two captured MHW roots contain descriptor tables only. Keep the
// original heaps bound and reapply their tables after each signature switch.
struct TableRequirement {
    D3D12_DESCRIPTOR_HEAP_TYPE heap;
    UINT size;
};
inline bool DescribeTables(const D3D12_ROOT_SIGNATURE_DESC1& root,std::vector<TableRequirement>& result) {
    result.clear();
    if(!root.NumParameters || !root.pParameters)return false;
    std::vector<TableRequirement> tables;
    for(UINT i=0;i<root.NumParameters;i++) {
        const auto& p=root.pParameters[i];
        if(p.ParameterType!=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)return false;
        const auto& table=p.DescriptorTable;
        if(!table.NumDescriptorRanges || !table.pDescriptorRanges)return false;
        bool sampler=table.pDescriptorRanges[0].RangeType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        UINT end=0,size=0;
        for(UINT j=0;j<table.NumDescriptorRanges;j++) {
            const auto& range=table.pDescriptorRanges[j];
            if((range.RangeType==D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)!=sampler)return false;
            UINT start=range.OffsetInDescriptorsFromTableStart==D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND?
                       end:range.OffsetInDescriptorsFromTableStart;
            if(!range.NumDescriptors || range.NumDescriptors==UINT_MAX || range.NumDescriptors>UINT_MAX-start)return false;
            end=start+range.NumDescriptors;
            if(end>size)size=end;
        }
        tables.push_back({sampler?D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,size});
    }
    result=std::move(tables);
    return true;
}

struct BoundHeap {
    D3D12_GPU_DESCRIPTOR_HANDLE start{};
    UINT count=0,stride=0;
    bool Contains(D3D12_GPU_DESCRIPTOR_HANDLE handle,UINT size) const {
        if(!start.ptr || !stride || !size || handle.ptr<start.ptr)return false;
        const UINT64 offset=handle.ptr-start.ptr;
        const UINT64 slot=offset/stride;
        return !(offset%stride) && slot<count && size<=count-slot;
    }
};
struct GraphicsTables {
    ID3D12RootSignature* root;
    ID3D12PipelineState* pipeline;
    std::span<const D3D12_GPU_DESCRIPTOR_HANDLE> handles;
    bool Complete(std::span<const TableRequirement> required,const BoundHeap& resources,const BoundHeap& samplers) const {
        if(!root || !pipeline || required.empty() || handles.size()!=required.size())return false;
        for(size_t i=0;i<required.size();i++) {
            const auto& heap=required[i].heap==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER?samplers:resources;
            if(!heap.Contains(handles[i],required[i].size))return false;
        }
        return true;
    }
    void Bind(ID3D12GraphicsCommandList& cmd,ID3D12RootSignature& signature) const {
        cmd.SetGraphicsRootSignature(&signature);
        for(UINT i=0;i<handles.size();i++)cmd.SetGraphicsRootDescriptorTable(i,handles[i]);
    }
};

struct MeshIndexRange {
    UINT first, count;
    INT base;
    bool operator==(const MeshIndexRange&) const = default;
};

struct MeshDrawBinding {
    std::uintptr_t model;
    D3D12_INDEX_BUFFER_VIEW indices;
};

// Prepare once from the local model and its MOD3 rows. Use only for that
// model's lifetime and the command recording in which its view was verified.
class MeshDrawTarget {
    MeshDrawBinding expected;
    MeshIndexRange range;
    MeshDrawTarget(MeshDrawBinding binding, MeshIndexRange row) : expected(binding), range(row) {}
public:
    static std::optional<MeshDrawTarget> Prepare(MeshDrawBinding binding,
                                                std::span<const MeshIndexRange> rows, size_t target) {
        if (!binding.model || !binding.indices.BufferLocation || target >= rows.size()) return {};
        const auto& view = binding.indices;
        const auto row = rows[target];
        const UINT stride = view.Format == DXGI_FORMAT_R16_UINT ? 2u :
                            view.Format == DXGI_FORMAT_R32_UINT ? 4u : 0u;
        if (!stride || view.BufferLocation % stride || !row.count || row.count % 3 ||
            static_cast<UINT64>(row.first) + row.count > view.SizeInBytes / stride ||
            std::count(rows.begin(), rows.end(), row) != 1) return {};
        return MeshDrawTarget(binding, row);
    }

    // Draw-side model comes from CBModel.iUnitAddress. The index view and
    // arguments must be those bound for this native draw.
    bool Matches(MeshDrawBinding bound, const D3D12_DRAW_INDEXED_ARGUMENTS& draw) const {
        return expected.model == bound.model &&
               expected.indices.BufferLocation == bound.indices.BufferLocation &&
               expected.indices.SizeInBytes == bound.indices.SizeInBytes &&
               expected.indices.Format == bound.indices.Format &&
               draw.InstanceCount == 1 && draw.StartInstanceLocation == 0 &&
               range == MeshIndexRange{draw.StartIndexLocation, draw.IndexCountPerInstance, draw.BaseVertexLocation};
    }
};

// Borrowed for one command-list recording. The caller owns the PSO,
// triangle-position buffer and descriptors until the GPU fence completes.
// The matching extended root signature and original material bindings must
// already be active. Colour, depth-only, mask and edge passes use the same
// indexed arguments and field, including the original signed base vertex.
struct IndexedPass {
    D3D12_DRAW_INDEXED_ARGUMENTS draw;
    D3D12_GPU_VIRTUAL_ADDRESS positions;
    UINT constantsRoot;
    UINT positionsRoot;
    std::array<float,5> field;

    void Record(ID3D12GraphicsCommandList& commands, ID3D12PipelineState& pipeline) const {
        commands.SetPipelineState(&pipeline);
        commands.SetGraphicsRoot32BitConstants(constantsRoot,static_cast<UINT>(field.size()),field.data(),0);
        commands.SetGraphicsRootShaderResourceView(positionsRoot,positions);
        commands.DrawIndexedInstanced(draw.IndexCountPerInstance,draw.InstanceCount,draw.StartIndexLocation,
                                      draw.BaseVertexLocation,draw.StartInstanceLocation);
    }

    // For the original material's colour or depth draw. IA, targets, heaps,
    // stencil reference and blend factor are consumed in place. False means
    // the original caller still owns the draw and no commands were recorded.
    bool TryRecordMaterial(ID3D12GraphicsCommandList& cmd,const MeshDrawTarget& target,MeshDrawBinding bound,
                           const GraphicsTables& native,
                           std::span<const TableRequirement> required,const BoundHeap& resources,const BoundHeap& samplers,
                           ID3D12RootSignature& extended,ID3D12PipelineState& clipped) const {
        if(!target.Matches(bound,draw) || !native.Complete(required,resources,samplers) || !positions ||
           constantsRoot!=required.size() || positionsRoot!=constantsRoot+1)return false;
        native.Bind(cmd,extended);
        Record(cmd,clipped);
        native.Bind(cmd,*native.root);
        cmd.SetPipelineState(native.pipeline);
        return true;
    }
};
} // namespace worlddiff::dissolve
