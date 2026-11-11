// Headless native DX12 proof. Two MOD3 rows share a material pixel shader.
// Only the target geometry stage clips triangles; the control row is drawn
// normally. This does not attach to the game or reproduce its shader bytecode.
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include "../src/DissolveTransition.h"
#include "../src/IndexedHandoff.h"
#include "../src/NativeCoverage.h"
#include "../src/Dx12DissolveDraw.h"

using Microsoft::WRL::ComPtr;
constexpr UINT side = 384;
constexpr DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
using Resource = ComPtr<ID3D12Resource>;
using Shader = ComPtr<ID3DBlob>;
using Pipeline = ComPtr<ID3D12PipelineState>;

void check(HRESULT result, const char* note) {
    if (FAILED(result)) {
        std::cerr << note << " HRESULT " << std::hex << result << std::dec << '\n';
        throw std::runtime_error(note);
    }
}
void require(bool okay, const char* note) { if (!okay) throw std::runtime_error(note); }

struct Vertex { float clip[3], local[3]; };
struct MeshPair {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t firstCount, secondCount;
    float low, invHeight;
};
MeshPair readPair(const std::filesystem::path& path) {
    struct Header { char magic[4]; uint32_t vertices, first, second; float low, invHeight; } h{};
    std::ifstream in(path, std::ios::binary);
    in.read(reinterpret_cast<char*>(&h), sizeof(h));
    require(in.good() && !std::memcmp(h.magic,"WDM1",4) && h.vertices && h.vertices<=131070 &&
            h.first && h.second && !(h.first%3) && !(h.second%3) &&
            uint64_t(h.first)+h.second <= 2000000 && std::isfinite(h.low) &&
            std::isfinite(h.invHeight) && h.invHeight>0, "invalid fixture header");
    MeshPair result{{},{},h.first,h.second,h.low,h.invHeight};
    result.vertices.resize(h.vertices); result.indices.resize(h.first+h.second);
    in.read(reinterpret_cast<char*>(result.vertices.data()), result.vertices.size()*sizeof(Vertex));
    in.read(reinterpret_cast<char*>(result.indices.data()), result.indices.size()*sizeof(uint32_t));
    require(in.good() && in.peek()==EOF && *std::max_element(result.indices.begin(),result.indices.end())<h.vertices,
            "invalid fixture payload");
    return result;
}

const char* materialShaders=R"(
cbuffer Cloth : register(b3) { float3 clothTint; };
struct V { float3 clipPos : POSITION; float3 localPos : TEXCOORD0; };
struct S { float4 pos : SV_Position; float3 localPos : TEXCOORD0; };
S VS(V i) { S o; o.pos=float4(i.clipPos,1); o.localPos=float3(i.localPos.xy,0); return o; }
float4 OpaquePS(S s) : SV_Target {
    float bands=.5+.5*sin(dot(s.localPos,float3(.5,.11,.23)));
    return float4(float3(.09+.12*bands,.25+.23*bands,.37+.16*bands)*clothTint,1);
}
float4 CutoutPS(S s) : SV_Target {
    clip(frac(s.localPos.x*.17+s.localPos.y*.09)-.25);
    float v=.5+.5*cos(dot(s.localPos,float3(.26,.17,.14)));
    return float4(float3(.12+.26*v,.35+.12*v,.12+.04*v)*clothTint,1);
}
)";
Shader compile(const char* entry, const char* profile, bool effect,bool amber=false,bool moon=false) {
    Shader shader, errors;
    const D3D_SHADER_MACRO moonField[]={{"SILVER_GAME_FIELD","1"},{nullptr,nullptr}};
    constexpr UINT flags=D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS;
    HRESULT result=effect
        ? D3DCompileFromFile(amber?L"src/shaders/AmberCrystal.hlsl":L"src/shaders/DissolveCoverage.hlsl",moon?moonField:nullptr,nullptr,entry,profile,
                             flags,0,&shader,&errors)
        : D3DCompile(materialShaders,std::strlen(materialShaders),"material fixture",nullptr,nullptr,
                     entry,profile,flags,0,&shader,&errors);
    if (FAILED(result) && errors) std::cerr << static_cast<const char*>(errors->GetBufferPointer());
    check(result,entry);
    return shader;
}
D3D12_SHADER_BYTECODE bytecode(const Shader& s) { return {s->GetBufferPointer(),s->GetBufferSize()}; }
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition={r,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};return b;
}
struct Pixels { std::vector<uint32_t> color,depth; };
enum class Pass { Native, MaskedMaterial, Coverage, Edge };

class Renderer {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> cmd;
    ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12RootSignature> root,originalRoot;
    ComPtr<ID3D12DescriptorHeap> materialHeap;
    Resource materialConstants;
    std::vector<worlddiff::dissolve::TableRequirement> requiredTables;
    ComPtr<ID3D12DescriptorHeap> rtv,dsv,readOnlyDsv,coverageRtv;
    Resource color,depth,readback,vbo,ibo,coverage,coverageBytes;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT maskFootprint{};
    struct RowGeometry { Resource positions; float low=0, invHeight=0; UINT count=0,first=0; };
    std::array<RowGeometry,2> geometry;
    D3D12_VERTEX_BUFFER_VIEW vb{};
    D3D12_INDEX_BUFFER_VIEW ib{};
    std::array<std::optional<worlddiff::dissolve::MeshDrawTarget>,2> targets;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,2> layout{};
    std::array<Pipeline,2> native,masked,coveragePass;
    Pipeline edge;
    Shader clipShader;
    Shader hull,domainShader;
    bool amber;
    bool ink;
    bool moon;
    uint64_t serial=0;
    const MeshPair& pair;

    Resource buffer(UINT64 bytes, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES hp{};hp.Type=heap;hp.CreationNodeMask=hp.VisibleNodeMask=1;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;
        d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Resource r;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"buffer");
        return r;
    }
    Resource upload(const void* data,UINT bytes) {
        auto r=buffer(bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped=nullptr;D3D12_RANGE noRead{0,0};check(r->Map(0,&noRead,&mapped),"map geometry");
        std::memcpy(mapped,data,bytes);r->Unmap(0,nullptr);return r;
    }
    Resource texture(DXGI_FORMAT fmt,D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=d.Height=side;
        d.DepthOrArraySize=d.MipLevels=1;d.Format=fmt;d.SampleDesc.Count=1;d.Flags=flags;
        D3D12_CLEAR_VALUE clear{};
        if(flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
            clear.Format=depthFormat;
            clear.DepthStencil.Depth=1;
        } else {
            clear.Format=fmt;
            clear.Color[3]=1;
            if(fmt==DXGI_FORMAT_R8_UNORM)clear.Color[0]=1;
        }
        Resource r;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,&clear,IID_PPV_ARGS(&r)),"texture");
        return r;
    }
    ComPtr<ID3D12DescriptorHeap> singleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type) {
        D3D12_DESCRIPTOR_HEAP_DESC d{};d.Type=type;d.NumDescriptors=1;
        if(type==D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)d.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ComPtr<ID3D12DescriptorHeap> heap;check(device->CreateDescriptorHeap(&d,IID_PPV_ARGS(&heap)),"descriptor heap");return heap;
    }
    Pipeline pipeline(const Shader& vs,const Shader& ps,Pass pass) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=pass==Pass::Native?originalRoot.Get():root.Get();d.VS=bytecode(vs);d.PS=bytecode(ps);
        if(pass!=Pass::Native) {
            if(amber){d.HS=bytecode(hull);d.DS=bytecode(domainShader);}
            else d.GS=bytecode(clipShader);
        }
        const D3D12_INPUT_ELEMENT_DESC inputs[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
        d.InputLayout={inputs,2};d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        if(amber && pass!=Pass::Native)d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        d.SampleMask=UINT_MAX;d.SampleDesc.Count=1;d.NumRenderTargets=1;d.RTVFormats[0]=colorFormat;d.DSVFormat=depthFormat;
        d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;d.RasterizerState.DepthClipEnable=TRUE;
        auto& b=d.BlendState.RenderTarget[0];b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;
        b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.LogicOp=D3D12_LOGIC_OP_NOOP;
        b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        if(pass==Pass::Coverage) {
            d.RTVFormats[0]=DXGI_FORMAT_R8_UNORM;
            b.BlendEnable=TRUE;b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ZERO;
        }
        if(pass==Pass::Edge) {
            b.BlendEnable=TRUE;
            b.SrcBlend=D3D12_BLEND_SRC_ALPHA;
            b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
            b.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_RED|D3D12_COLOR_WRITE_ENABLE_GREEN|D3D12_COLOR_WRITE_ENABLE_BLUE;
        }
        const bool readOnly=pass==Pass::Coverage || pass==Pass::Edge;
        auto& z=d.DepthStencilState;z.DepthEnable=TRUE;
        z.DepthFunc=readOnly?D3D12_COMPARISON_FUNC_EQUAL:D3D12_COMPARISON_FUNC_LESS_EQUAL;
        z.DepthWriteMask=readOnly?D3D12_DEPTH_WRITE_MASK_ZERO:D3D12_DEPTH_WRITE_MASK_ALL;
        Pipeline p;check(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&p)),"graphics PSO");return p;
    }
public:
    Renderer(const MeshPair& meshes,bool warp,worlddiff::dissolve::Style style=worlddiff::dissolve::Style::Silver,bool moonMotion=false):
        amber(style!=worlddiff::dissolve::Style::Silver),ink(style==worlddiff::dissolve::Style::Ink),moon(moonMotion),pair(meshes) {
        ComPtr<IDXGIFactory4> factory;check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI");
        ComPtr<IDXGIAdapter1> adapter;
        if (warp) check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"WARP adapter");
        check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"DX12 device");
        check(factory->EnumAdapterByLuid(device->GetAdapterLuid(),IID_PPV_ARGS(&adapter)),"identify device");
        DXGI_ADAPTER_DESC1 ad{};check(adapter->GetDesc1(&ad),"adapter description");
        std::wcout << L"DX12 adapter: " << ad.Description << L'\n';
        D3D12_COMMAND_QUEUE_DESC q{};check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");
        check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)),"allocator");
        check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&cmd)),"command list");
        check(cmd->Close(),"initial close");check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
        D3D12_ROOT_PARAMETER rp[5]{};
        D3D12_DESCRIPTOR_RANGE clothRange{D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1,3,0,0};
        rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp[0].DescriptorTable={1,&clothRange};rp[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rp[1].Constants={0,0,5};rp[1].ShaderVisibility=amber?D3D12_SHADER_VISIBILITY_ALL:D3D12_SHADER_VISIBILITY_GEOMETRY;
        rp[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        rp[2].Descriptor={0,0};rp[2].ShaderVisibility=rp[1].ShaderVisibility;
        rp[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        rp[3].Descriptor={1,0};rp[3].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        rp[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rp[4].Constants={1,0,2};rp[4].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC rd{1,rp,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        Shader blob;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,nullptr),"serialize material root");
        check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&originalRoot)),"material root");
        ComPtr<ID3D12VersionedRootSignatureDeserializer> reader;
        check(D3D12CreateVersionedRootSignatureDeserializer(blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&reader)),"read material root");
        const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* desc=nullptr;
        check(reader->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1,&desc),"material parameters");
        require(worlddiff::dissolve::DescribeTables(desc->Desc_1_1,requiredTables),"material table requirements");
        rd.NumParameters=5;blob.Reset();check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,nullptr),"serialize root");
        check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"root signature");
        std::array<float,64> tint{.93f,.97f,.89f};
        materialConstants=upload(tint.data(),sizeof(tint));
        materialHeap=singleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{materialConstants->GetGPUVirtualAddress(),sizeof(tint)};
        device->CreateConstantBufferView(&cbv,materialHeap->GetCPUDescriptorHandleForHeapStart());
        auto vs=compile("VS","vs_5_0",false);
        clipShader=compile("CoverageGS","gs_5_0",true,false,moon);
        if(amber){hull=compile("AmberHS","hs_5_0",true,true);domainShader=compile("AmberDS","ds_5_0",true,true);}
        for (int i=0;i<2;++i) {
            auto ps=compile(i?"CutoutPS":"OpaquePS","ps_5_0",false);
            native[i]=pipeline(vs,ps,Pass::Native);masked[i]=pipeline(vs,ps,Pass::MaskedMaterial);
            coveragePass[i]=pipeline(vs,ps,Pass::Coverage);
        }
        edge=pipeline(vs,compile(amber?"AmberAlbedoPS":"SilverEdgePS","ps_5_0",true,amber,moon),Pass::Edge);
        UINT vbytes=static_cast<UINT>(pair.vertices.size()*sizeof(Vertex)), ibytes=static_cast<UINT>(pair.indices.size()*4);
        vbo=upload(pair.vertices.data(),vbytes);ibo=upload(pair.indices.data(),ibytes);
        geometry[0].count=pair.firstCount;
        geometry[1].first=pair.firstCount;geometry[1].count=pair.secondCount;
        for(auto& row:geometry) {
            std::vector<std::array<float,3>> positions(row.count);
            float low=std::numeric_limits<float>::max(),high=std::numeric_limits<float>::lowest();
            for(UINT i=0;i<row.count;i++) {
                auto& pos=pair.vertices[pair.indices[row.first+i]].local;
                std::copy_n(pos,3,positions[i].begin());
                low=std::min(low,pos[1]);high=std::max(high,pos[1]);
            }
            row.low=low;row.invHeight=high>low?1.f/(high-low):0.f;
            if(amber || moon) {
                auto lo=positions.front(),hi=lo;
                for(const auto& v:positions)for(int a=0;a<3;++a){lo[a]=std::min(lo[a],v[a]);hi[a]=std::max(hi[a],v[a]);}
                const auto span=std::max({hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2],.00001f});
                for(auto& v:positions){
                    UINT packed{};
                    for(unsigned a=0;a<3;++a)packed|=UINT(std::lround(1023.f*std::clamp(((v[a]-lo[a])-.5f*(hi[a]-lo[a]))/span+.5f,0.f,1.f)))<<(10*a);
                    if(moon) v[0]=v[1]; // Raw bind height, as uploaded by SilverGeometry.
                    memcpy(v.data()+2,&packed,sizeof(packed));
                }
            }
            row.positions=upload(positions.data(),static_cast<UINT>(positions.size()*sizeof(positions[0])));
        }
        vb={vbo->GetGPUVirtualAddress(),vbytes,sizeof(Vertex)};ib={ibo->GetGPUVirtualAddress(),ibytes,DXGI_FORMAT_R32_UINT};
        using worlddiff::dissolve::MeshIndexRange;
        const worlddiff::dissolve::MeshDrawBinding model{reinterpret_cast<uintptr_t>(&pair),ib};
        const std::array<MeshIndexRange,2> rows{{
            {geometry[0].first,geometry[0].count,0},{geometry[1].first,geometry[1].count,0}
        }};
        for (size_t i=0;i<rows.size();++i) {
            targets[i]=worlddiff::dissolve::MeshDrawTarget::Prepare(model,rows,i);
            require(targets[i].has_value(),"valid indexed target rejected");
        }
        const std::array<MeshIndexRange,2> ambiguous{rows[0],rows[0]};
        require(!worlddiff::dissolve::MeshDrawTarget::Prepare(model,ambiguous,0),"accepted overlapping identities");
        const std::array<MeshIndexRange,1> pastEnd{{{UINT_MAX,3,0}}};
        require(!worlddiff::dissolve::MeshDrawTarget::Prepare(model,pastEnd,0),"accepted out-of-view range");
        D3D12_DRAW_INDEXED_ARGUMENTS batched{rows[0].count,2,rows[0].first,0,0};
        require(!targets[0]->Matches(model,batched),"accepted instance batch");
        batched.InstanceCount=1;
        batched.StartInstanceLocation=1;
        require(!targets[0]->Matches(model,batched),"accepted nonzero instance offset");
        batched.StartInstanceLocation=0;
        batched.BaseVertexLocation=-1;
        require(!targets[0]->Matches(model,batched),"accepted different signed base");
        color=texture(colorFormat,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
        depth=texture(DXGI_FORMAT_R32_TYPELESS,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,D3D12_RESOURCE_STATE_DEPTH_WRITE);
        coverage=texture(DXGI_FORMAT_R8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
        coverageRtv=singleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        device->CreateRenderTargetView(coverage.Get(),nullptr,coverageRtv->GetCPUDescriptorHandleForHeapStart());
        auto maskDesc=coverage->GetDesc();
        device->GetCopyableFootprints(&maskDesc,0,1,0,&maskFootprint,nullptr,nullptr,nullptr);
        coverageBytes=buffer(UINT64(maskFootprint.Footprint.RowPitch)*side,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        rtv=singleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);dsv=singleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        device->CreateRenderTargetView(color.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
        D3D12_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=depthFormat;dv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(depth.Get(),&dv,dsv->GetCPUDescriptorHandleForHeapStart());
        readOnlyDsv=singleDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        dv.Flags=D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        device->CreateDepthStencilView(depth.Get(),&dv,readOnlyDsv->GetCPUDescriptorHandleForHeapStart());
        auto cd=color->GetDesc(),dd=depth->GetDesc();UINT64 bytes=0,row=0;
        device->GetCopyableFootprints(&cd,0,1,0,&layout[0],nullptr,nullptr,&bytes);
        UINT64 depthOffset=(bytes+511)&~UINT64(511);
        device->GetCopyableFootprints(&dd,0,1,depthOffset,&layout[1],nullptr,&row,&bytes);
        require(row==side*4,"unexpected depth readback layout");
        readback=buffer(layout[1].Offset+UINT64(layout[1].Footprint.RowPitch)*side,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    }

    Pixels frame(int shader,float progress,bool siblingFirst,bool drawTarget=true,bool drawSibling=true,float siblingProgress=0,bool sharedBoundary=false,bool universal=false) {
        check(alloc->Reset(),"reset allocator");check(cmd->Reset(alloc.Get(),nullptr),"reset list");
        auto c=rtv->GetCPUDescriptorHandleForHeapStart(),z=dsv->GetCPUDescriptorHandleForHeapStart();
        cmd->OMSetRenderTargets(1,&c,FALSE,&z);
        const float bg[4]={0,0,0,1};cmd->ClearRenderTargetView(c,bg,0,nullptr);
        cmd->ClearDepthStencilView(z,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
        const D3D12_VIEWPORT vp{0,0,float(side),float(side),0,1};const D3D12_RECT rect{0,0,side,side};
        cmd->RSSetViewports(1,&vp);cmd->RSSetScissorRects(1,&rect);
        auto table=materialHeap->GetGPUDescriptorHandleForHeapStart();
        const worlddiff::dissolve::GraphicsTables original{originalRoot.Get(),native[shader].Get(),{&table,1}};
        const worlddiff::dissolve::BoundHeap resources{table,1,device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)};
        require(original.Complete(requiredTables,resources,{}),"valid material binding rejected");
        ID3D12DescriptorHeap* heaps[]={materialHeap.Get()};cmd->SetDescriptorHeaps(1,heaps);
        original.Bind(*cmd.Get(),*originalRoot.Get());cmd->SetPipelineState(native[shader].Get());
        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);cmd->IASetVertexBuffers(0,1,&vb);cmd->IASetIndexBuffer(&ib);
        const float phase[]={drawTarget?progress:1.f,drawSibling?siblingProgress:1.f};
        const std::array<int,2> order={siblingFirst?1:0,siblingFirst?0:1};
        auto draw=[&](const Pipeline& p,int slot){
            if(amber)cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
            const auto& row=geometry[slot];
            // Matched indexed triangles use the same undeformed positions for
            // classification. IA still fetches each body's own vertices.
            const auto& field=sharedBoundary?geometry[0]:row;
            const bool back=sharedBoundary && slot==1;
            worlddiff::dissolve::IndexedPass pass{
                {row.count,1,row.first,0,0},field.positions->GetGPUVirtualAddress(),1,2,
                {(amber || moon)?1.f-phase[slot]:sharedBoundary?progress:phase[slot],field.low,field.invHeight,.035f,amber?(ink?1.f:-1.f):back?-1.f:1.f}
            };
            if(p.Get()==masked[shader].Get()) {
                // Exercise refusal on the live command list before the accepted
                // draw. A bad table cannot be submitted or disturb native state.
                D3D12_GPU_DESCRIPTOR_HANDLE pastEnd{table.ptr+resources.stride};
                worlddiff::dissolve::GraphicsTables missing{original.root,original.pipeline,{&pastEnd,1}};
                const worlddiff::dissolve::MeshDrawBinding bound{reinterpret_cast<uintptr_t>(&pair),ib};
                const auto& target=*targets[slot];
                auto record=[&](const auto& selected,auto binding,const auto& tables) {
                    return pass.TryRecordMaterial(*cmd.Get(),selected,binding,tables,requiredTables,resources,{},*root.Get(),*p.Get());
                };
                require(!record(target,bound,missing),
                        "accepted descriptor outside the current heap");
                auto peer=bound;
                peer.model=reinterpret_cast<uintptr_t>(this);
                require(!record(target,peer,original),"changed another model using the same index buffer");
                require(!record(*targets[1-slot],bound,original),"changed the adjacent row");
                auto otherBuffer=bound;
                otherBuffer.indices.BufferLocation+=sizeof(UINT);
                require(!record(target,otherBuffer,original),"changed a different index view");
                require(record(target,bound,original),"prepare material draw");
            } else pass.Record(*cmd.Get(),*p.Get());
            if(amber)cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        };
        using worlddiff::dissolve::DrawRoute;
        using worlddiff::dissolve::RouteMesh;
        const DrawRoute routes[]={RouteMesh(drawTarget,progress==0,phase[0]),
                                  RouteMesh(drawSibling,siblingProgress==0,phase[1])};
        for(int slot:order) {
            if(universal) {
                auto spans=worlddiff::dissolve::NativeCoverage(geometry[slot].count,phase[slot],slot==0);
                for(unsigned i=0;i<spans.used;++i) {
                    auto r=spans.ranges[i];
                    cmd->DrawIndexedInstanced(r.count,1,geometry[slot].first+r.first,0,0);
                }
                continue;
            }
            if(routes[slot]==DrawRoute::clipped)draw(masked[shader],slot);
            if(routes[slot]==DrawRoute::original) {
                cmd->DrawIndexedInstanced(geometry[slot].count,1,geometry[slot].first,0,0);
            }
        }
        for(int slot:order) {
            if(universal || routes[slot]!=DrawRoute::clipped)continue;
            original.Bind(*cmd.Get(),*root.Get());
            auto readDepth=transition(depth.Get(),D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_DEPTH_READ);
            auto edgeDepth=readOnlyDsv->GetCPUDescriptorHandleForHeapStart();
            cmd->ResourceBarrier(1,&readDepth);
            auto maskRtv=coverageRtv->GetCPUDescriptorHandleForHeapStart();
            const float emptyMask[4]={1,0,0,1};
            cmd->ClearRenderTargetView(maskRtv,emptyMask,0,nullptr);
            cmd->OMSetRenderTargets(1,&maskRtv,FALSE,&edgeDepth);
            draw(coveragePass[shader],slot);
            auto readMask=transition(coverage.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
            cmd->ResourceBarrier(1,&readMask);
            D3D12_TEXTURE_COPY_LOCATION from{},to{};
            from.pResource=coverage.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            to.pResource=coverageBytes.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=maskFootprint;
            cmd->CopyTextureRegion(&to,0,0,0,&from,nullptr);
            auto sampleMask=transition(coverageBytes.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd->ResourceBarrier(1,&sampleMask);
            cmd->OMSetRenderTargets(1,&c,FALSE,&edgeDepth);
            cmd->SetGraphicsRootShaderResourceView(3,coverageBytes->GetGPUVirtualAddress());
            cmd->SetGraphicsRoot32BitConstant(4,maskFootprint.Footprint.RowPitch,0);
            float edgeWidth=.035f;
            cmd->SetGraphicsRoot32BitConstants(4,1,&edgeWidth,1);
            draw(edge,slot);
            std::swap(sampleMask.Transition.StateBefore,sampleMask.Transition.StateAfter);
            cmd->ResourceBarrier(1,&sampleMask);
            std::swap(readMask.Transition.StateBefore,readMask.Transition.StateAfter);
            cmd->ResourceBarrier(1,&readMask);
            std::swap(readDepth.Transition.StateBefore,readDepth.Transition.StateAfter);
            cmd->ResourceBarrier(1,&readDepth);
            cmd->OMSetRenderTargets(1,&c,FALSE,&z);
            original.Bind(*cmd.Get(),*originalRoot.Get());cmd->SetPipelineState(native[shader].Get());
        }
        auto cb=transition(color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
        auto db=transition(depth.Get(),D3D12_RESOURCE_STATE_DEPTH_WRITE,D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_RESOURCE_BARRIER barriers[]={cb,db};cmd->ResourceBarrier(2,barriers);
        for(UINT i=0;i<2;++i) {
            D3D12_TEXTURE_COPY_LOCATION to{},from{};to.pResource=readback.Get();
            to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=layout[i];
            from.pResource=i?depth.Get():color.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            cmd->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        }
        for(auto& b:barriers)std::swap(b.Transition.StateBefore,b.Transition.StateAfter);
        cmd->ResourceBarrier(2,barriers);check(cmd->Close(),"close list");ID3D12CommandList* lists[]={cmd.Get()};
        queue->ExecuteCommandLists(1,lists);check(queue->Signal(fence.Get(),++serial),"signal");
        check(fence->SetEventOnCompletion(serial,nullptr),"wait for readback");
        require(fence->GetCompletedValue()!=UINT64_MAX,"device removed during readback");
        uint8_t* mem=nullptr;check(readback->Map(0,nullptr,reinterpret_cast<void**>(&mem)),"map pixels");
        Pixels out;out.color.resize(side*side);out.depth.resize(side*side);
        for(UINT y=0;y<side;++y) for(UINT i=0;i<2;++i) {
            auto& plane=i?out.depth:out.color;auto& fp=layout[i];
            std::memcpy(&plane[y*side],mem+fp.Offset+UINT64(y)*fp.Footprint.RowPitch,side*4);
        }
        D3D12_RANGE noWrite{0,0};readback->Unmap(0,&noWrite);
        return out;
    }
};

void verify(Renderer& gpu,int material,bool siblingFirst) {
    auto control=gpu.frame(material,0,siblingFirst,false,true);
    auto target=gpu.frame(material,0,siblingFirst,true,false);
    auto hidden=gpu.frame(material,1,siblingFirst);
    require(hidden.color==control.color && hidden.depth==control.depth,"hidden endpoint leaked color/depth");
    const uint32_t bg=0xff000000;
    const auto originalArea=static_cast<size_t>(std::count_if(target.color.begin(),target.color.end(),
        [=](uint32_t color){return color!=bg;}));
    require(originalArea>25,"insufficient target area");
    size_t previous=originalArea;
    for(float t:{.2f,.5f,.8f}) {
        const auto out=gpu.frame(material,t,siblingFirst);
        const auto clipped=gpu.frame(material,t,siblingFirst,true,false);
        size_t retained=0,coloredEdge=0,siblingPixels=0;
        for(size_t i=0;i<out.color.size();++i) {
            if(clipped.color[i]!=bg) {
                require(target.color[i]!=bg,"clipping expanded target coverage");
                ++retained;
                if((clipped.color[i]&255)>145 && ((clipped.color[i]>>16)&255)>205)++coloredEdge;
            }
            // Independently rendered depth at this phase includes rounding
            // from clipping. Near-coplanar bodies can exchange the front pixel.
            auto z=std::min(clipped.depth[i],control.depth[i]);
            bool targetWins=clipped.depth[i]==z && out.color[i]==clipped.color[i];
            bool siblingWins=control.depth[i]==z && out.color[i]==control.color[i];
            require(out.depth[i]==z && (targetWins || siblingWins),"incorrect composite colour or depth");
            if(control.color[i]!=bg && control.depth[i]<clipped.depth[i])++siblingPixels;
        }
        require(siblingPixels>25,"insufficient visible control area");
        require(retained>0 && retained<previous,"coverage failed to shrink");
        require(coloredEdge>0 && coloredEdge<retained,"missing edge or entire surface recolored");
        previous=retained;
        std::cout << "material=" << material << " siblingFirst=" << siblingFirst << " phase=" << t
                  << " retained=" << retained << '/' << originalArea << " edge=" << coloredEdge
                  << " stableSibling=" << siblingPixels << '\n';
    }
}

void verifyHandoff(Renderer& gpu,const std::filesystem::path& pixels,bool shared) {
    using worlddiff::dissolve::Retarget;
    const auto outgoing=Retarget(0,0,0,true,false);
    const auto incoming=Retarget(1,0,1,false,true);
    const auto outgoingOnly=gpu.frame(0,0,false,true,false);
    const auto big=gpu.frame(0,0,false,false,true);
    require(outgoingOnly.color!=big.color,"preset endpoints must be distinguishable");
    std::ofstream preview;
    if(!pixels.empty()) {
        preview.open(pixels,std::ios::binary);
        require(preview.good(),"open RGBA readback");
    }
    unsigned overlap=0;
    for(unsigned i=0;i<=35;i++) {
        const unsigned ms=(i*1000+30)/60;
        const float aw=outgoing.At(ms),bw=incoming.At(ms);
        const float a=shared?worlddiff::dissolve::HandoffBoundary(aw,bw):aw,b=shared?1.f-a:bw;
        auto composed=gpu.frame(0,a,false,true,true,b,shared);
        if(!i)require(composed.color==outgoingOnly.color && composed.depth==outgoingOnly.depth,"small endpoint changed");
        if(i==35)require(composed.color==big.color && composed.depth==big.depth,"big endpoint changed");
        if(a>0 && a<1 && b>0 && b<1) {
            ++overlap;
            auto left=gpu.frame(0,a,false,true,false,0,shared);
            auto right=gpu.frame(0,a,false,false,true,b,shared);
            for(size_t k=0;k<composed.color.size();k++) {
                require(composed.depth[k]==std::min(left.depth[k],right.depth[k]),"wrong overlap depth");
                if(left.depth[k]!=right.depth[k])
                    require(composed.color[k]==(left.depth[k]<right.depth[k]?left.color[k]:right.color[k]),
                            "edge escaped its mesh during handoff");
            }
        }
        if(preview.is_open()) {
            preview.write(reinterpret_cast<const char*>(composed.color.data()),composed.color.size()*4);
            require(preview.good(),"write RGBA readback");
        }
    }
    require(overlap>0,"B handoff never overlapped");
    std::cout << "PASS B handoff, 36 frames, " << overlap << " frames with two active rows\n";
}

void verifyCapturedHandoff(Renderer& gpu,const std::filesystem::path& transcript) {
    using worlddiff::dissolve::DrawRoute;
    using worlddiff::dissolve::RouteMesh;
    std::ifstream in(transcript);
    require(in.is_open(),"open paired Present transcript");
    const auto smallOnly=gpu.frame(0,0,false,true,false);
    const auto bigOnly=gpu.frame(0,0,false,false,true);
    std::array<unsigned,2> retained{},delayed{},overlap{};
    unsigned count=0,rendered=0;
    float previous=-1;
    DWORD tick;
    while(in>>tick) {
        float smallHidden,bigHidden;
        bool smallOriginal,smallResolved,bigOriginal,bigResolved;
        require(bool(in>>smallHidden>>smallOriginal>>smallResolved>>bigHidden>>bigOriginal>>bigResolved),
                "invalid paired row");
        require(smallHidden>=0 && smallHidden<=1 && bigHidden>=0 && bigHidden<=1,"invalid capture phase");
        ++count;
        // This test compares the ordinary preset handoff with a prepared one.
        // Culls or external visibility overrides need their own control fixture.
        require(smallOriginal && bigOriginal && smallResolved!=bigResolved,"not a visible one-of-two preset");
        const unsigned dir=smallResolved?1:0;
        for(auto [hidden,resolved]:{std::pair{smallHidden,smallResolved},std::pair{bigHidden,bigResolved}}) {
            require(RouteMesh(true,resolved,std::nullopt)==(resolved?DrawRoute::original:DrawRoute::skip),
                    "backend unavailable changed the native result");
            if(!resolved && hidden>0 && hidden<1)++retained[dir];
            if(resolved && hidden==1)++delayed[dir];
        }
        float boundary=worlddiff::dissolve::HandoffBoundary(smallHidden,bigHidden);
        if(boundary>0 && boundary<1)++overlap[dir];
        if(boundary==previous)continue;
        previous=boundary;
        ++rendered;
        auto both=gpu.frame(0,boundary,false,true,true,1-boundary,true);
        if(boundary==0 || boundary==1) {
            const auto& expected=boundary==0?smallOnly:bigOnly;
            require(both.color==expected.color && both.depth==expected.depth,"captured endpoint mismatch");
        } else {
            auto left=gpu.frame(0,boundary,false,true,false,0,true);
            auto right=gpu.frame(0,boundary,false,false,true,1-boundary,true);
            for(size_t i=0;i<both.color.size();i++) {
                require(both.depth[i]==std::min(left.depth[i],right.depth[i]),"recorded-time depth mismatch");
                if(left.depth[i]!=right.depth[i])
                    require(both.color[i]==(left.depth[i]<right.depth[i]?left.color[i]:right.color[i]),
                            "recorded-time color mismatch");
            }
        }
    }
    require(in.eof() && retained[0] && retained[1] && overlap[0] && overlap[1],
            "missing two-direction transition evidence");
    require(RouteMesh(false,false,.5f)==DrawRoute::skip,"native cull revived a mesh");
    std::cout << "PASS paired ticks=" << count << " changed boundaries rendered=" << rendered
              << " outgoing filtered early=" << retained[0] << '/' << retained[1]
              << " incoming shown before start=" << delayed[0] << '/' << delayed[1]
              << " overlaps=" << overlap[0] << '/' << overlap[1] << '\n';
}

void verifyIndexedHandoff(const MeshPair& original, const std::filesystem::path& mod, bool warp) {
    worlddiff::dissolve::IndexedHandoff h;
    require(worlddiff::dissolve::LoadHandoff(mod,{0,11},h),"read original wrist pair");
    require(h.thresholds.size()*3==original.firstCount && original.firstCount==original.secondCount,
            "fixture and model draw ranges differ");
    const unsigned vertices=static_cast<unsigned>(original.vertices.size()/2);
    std::array<unsigned,2> sub{};
    for(unsigned s=0;s<2;++s)std::memcpy(&sub[s],h.headers[s].data()+16,4);
    // Compute a reference classification from the fixture in original draw
    // order, independently of the sorted index buffer and its slice offsets.
    auto field=[&](uint32_t v) {
        const auto& pos=original.vertices[v].local;
        const float x=pos[0],y=pos[1],z=pos[2];
        return std::clamp((y-original.low)*original.invHeight+.085f*std::sin(.37f*x+.20f*y+.31f*z)
                          +.025f*std::sin(-1.2f*x+.74f*y+.46f*z),0.f,1.f);
    };
    for(float boundary:{.2f,.5f,.8f}) {
        MeshPair sorted=original,reference=original;
        sorted.indices.clear();reference.indices.clear();
        unsigned cut=h.Cut(boundary),total=original.firstCount;
        for(unsigned s=0;s<2;++s) {
            unsigned begin=s?total:cut,end=s?total+cut:total;
            for(unsigned i=begin;i<end;++i)sorted.indices.push_back(h.indices[i]-sub[s]+s*vertices);
            for(unsigned i=0;i<total;i+=3) {
                float f=0.f;
                for(unsigned j=0;j<3;++j)f+=field(original.indices[i+j])/3.f;
                if((f<boundary)!=bool(s))continue;
                const auto at=original.indices.begin()+s*total+i;
                reference.indices.insert(reference.indices.end(),at,at+3);
            }
            require(sorted.indices.size()==reference.indices.size(),"partition sizes differ");
            if(!s)sorted.firstCount=reference.firstCount=static_cast<unsigned>(sorted.indices.size());
        }
        sorted.secondCount=reference.secondCount=cut;
        Renderer a(sorted,warp),b(reference,warp);
        for(int m:{0,1})for(bool reverse:{false,true}) {
            auto test=a.frame(m,0,reverse),ref=b.frame(m,0,reverse);
            require(test.color==ref.color && test.depth==ref.depth,"sorted paired geometry changed pixels or depth");
        }
    }
    std::cout<<"PASS sorted native triangles vs original-order reference: 3 boundaries, 2 materials, 2 draw orders\n";
}
void verifyUniversal(const MeshPair& source,bool warp) {
    Renderer renderer(source,warp);
    for(float phase:{.2f,.5f,.8f}) {
        MeshPair subset=source;subset.indices.clear();
        for(unsigned s=0;s<2;++s) {
            unsigned n=s?source.secondCount:source.firstCount;
            auto spans=worlddiff::dissolve::NativeCoverage(n,s?1.f-phase:phase,s==0);
            require(spans.indices>0 && spans.indices<n,"missing partial row");
            for(unsigned i=0;i<spans.used;++i){
                const auto r=spans.ranges[i];
                const auto it=source.indices.begin()+(s?source.firstCount:0)+r.first;
                subset.indices.insert(subset.indices.end(),it,it+r.count);
            }
            (s?subset.secondCount:subset.firstCount)=spans.indices;
        }
        Renderer reference(subset,warp);
        for(int material:{0,1})for(bool reverse:{false,true}) {
            auto got=renderer.frame(material,phase,reverse,true,true,1.f-phase,false,true);
            auto want=reference.frame(material,0.f,reverse);
            require(got.color==want.color && got.depth==want.depth,"native dissolve differs from selected original triangles");
        }
    }
    std::cout<<"PASS universal native triangle coverage, independent row sizes and original material/depth\n";
}

void verifySilverMotion(Renderer& gpu,const std::filesystem::path& file) {
    using worlddiff::dissolve::Transition;
    const Transition show{0,0,0,900,1.f,0.f}, hide{0,0,0,900,0.f,1.f};
    const float turning=hide.At(450);
    const Transition reverse{0,450,0,450,turning,0.f};
    auto image=[&](float hidden){return gpu.frame(0,hidden,false,true,false);};
    auto same=[](const Pixels& a,const Pixels& b){return a.color==b.color && a.depth==b.depth;};
    require(same(image(hide.At(450)),image(reverse.At(450))),"jump at reversal");
    require(same(image(show.At(900)),image(reverse.At(900))),"reverse endpoint");
    std::ofstream output;
    if(!file.empty()){output.open(file,std::ios::binary);require(output.good(),"open motion readback");}
    for(unsigned sequence=0;sequence<3;sequence++) {
        for(unsigned ms=0;ms<=900;ms+=30) {
            const auto phase=sequence==0?show.At(ms):sequence==1?hide.At(ms):ms<=450?hide.At(ms):reverse.At(ms);
            const auto pixels=image(phase);
            if(!file.empty())output.write(reinterpret_cast<const char*>(pixels.color.data()),
                                          pixels.color.size()*sizeof(uint32_t));
        }
    }
    require(file.empty()||output.good(),"write motion readback");
    std::cout<<"PASS single mesh show/hide/reversal at 450 ms, 93 DX12 frames\n";
}

int wmain(int argc,wchar_t** argv) {
    try {
        if(argc<3 || argc>5)throw std::runtime_error("usage: test_dissolve_dx12 <fixture> warp|hardware [handoff.rgba [shared] | --replay paired.tsv]");
        std::wstring backend=argv[2];require(backend==L"warp" || backend==L"hardware","unknown backend");
        auto data=readPair(argv[1]);
        if(argc==4 && std::wstring_view(argv[3])==L"--universal") {
            verifyUniversal(data,backend==L"warp");return 0;
        }
        if(argc==5 && std::wstring_view(argv[3])==L"--indexed") {
            verifyIndexedHandoff(data,argv[4],backend==L"warp");return 0;
        }
        const std::wstring_view mode=argc>=4?argv[3]:L"";
        using worlddiff::dissolve::Style;
        Renderer gpu(data,backend==L"warp",mode==L"--amber-motion"?Style::Amber:mode==L"--ink-motion"?Style::Ink:Style::Silver,mode==L"--silver-motion");
        if(mode==L"--amber-motion" || mode==L"--ink-motion" || mode==L"--silver-motion") {
            verifySilverMotion(gpu,argc==5?std::filesystem::path(argv[4]):std::filesystem::path{});
            return 0;
        }
        if(argc>=4){
            bool replay=argc==5 && std::wstring_view(argv[3])==L"--replay";
            bool shared=argc==5 && (replay || std::wstring_view(argv[4])==L"shared");
            require(argc==4 || shared,"unknown handoff mode");
            if(shared) {
                require(data.firstCount==data.secondCount,"shared boundary needs paired triangles");
                const auto sourceMin=*std::min_element(data.indices.begin(),data.indices.begin()+data.firstCount);
                const auto destMin=*std::min_element(data.indices.begin()+data.firstCount,data.indices.end());
                for(UINT i=0;i<data.firstCount;i++)
                    require(data.indices[i]-sourceMin==data.indices[data.firstCount+i]-destMin,
                            "shared boundary needs matching local index order");
            }
            if(replay)verifyCapturedHandoff(gpu,argv[4]);else verifyHandoff(gpu,argv[3],shared);
            return 0;
        }
        for(int material:{0,1})for(bool siblingFirst:{false,true})verify(gpu,material,siblingFirst);
        std::cout << "PASS offscreen same-material isolation, private R8 mask and D32 depth\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
