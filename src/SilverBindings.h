#pragma once
#include "Dx12DissolveDraw.h"

namespace worlddiff::silver {
// D3D12 command lists have independent binding state, including when a list
// is recorded on a different thread after a reset.
struct MaterialBindings {
    ID3D12RootSignature* root{};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE,4> tables{};
    std::array<ID3D12DescriptorHeap*,2> heaps{};
    std::array<dissolve::BoundHeap,2> bounds{};
    bool recording{};
    unsigned knownTables{};
    void SetRoot(ID3D12RootSignature* next) {
        if(root!=next){
            if(root)knownTables=15;
            root=next;tables={};
        }
    }
};
bool Snapshot(ID3D12GraphicsCommandList*,MaterialBindings&);
void OnRoot(ID3D12GraphicsCommandList*,ID3D12RootSignature*);
void OnTable(ID3D12GraphicsCommandList*,UINT,D3D12_GPU_DESCRIPTOR_HANDLE);
void OnHeaps(ID3D12GraphicsCommandList*,UINT,ID3D12DescriptorHeap*const*);
void Invalidate(ID3D12GraphicsCommandList*);
void AfterIndirect(ID3D12GraphicsCommandList*,bool bundle);
}
