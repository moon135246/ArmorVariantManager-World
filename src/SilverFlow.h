#pragma once
#include "Dx12SceneDraw.h"
#include <filesystem>
namespace worlddiff::overlay { bool ObserveMaterialList(ID3D12GraphicsCommandList*); }

namespace worlddiff::silver {
using RootCreate = HRESULT (STDMETHODCALLTYPE*)(ID3D12Device*,UINT,const void*,SIZE_T,REFIID,void**);
using PipelineCreate = HRESULT (STDMETHODCALLTYPE*)(ID3D12Device*,const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,REFIID,void**);
extern RootCreate originalRoot;
extern PipelineCreate originalPipeline;
using LibraryLoad=HRESULT (STDMETHODCALLTYPE*)(ID3D12PipelineLibrary*,LPCWSTR,const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,REFIID,void**);
extern LibraryLoad originalLibrary;
HRESULT STDMETHODCALLTYPE LoadGraphics(ID3D12PipelineLibrary*,LPCWSTR,const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreateRoot(ID3D12Device*,UINT,const void*,SIZE_T,REFIID,void**);
HRESULT STDMETHODCALLTYPE CreatePipeline(ID3D12Device*,const D3D12_GRAPHICS_PIPELINE_STATE_DESC*,REFIID,void**);
void PrepareModels(const std::filesystem::path& nativePC);
void SetDevice(ID3D12Device*);
unsigned Stage(ID3D12PipelineState*,dissolve::Style style=dissolve::Style::Silver,bool uv=false);
bool CanBind(const dissolve::SceneDraw&);
bool Draw(const dissolve::SceneDraw&,const ModelPolicy&,float hidden,bool pixelPath,bool uvPath,const dissolve::Transition* move);
}
