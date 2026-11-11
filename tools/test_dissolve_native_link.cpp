// Offline PSO linkage check for a captured triangle pass; no game process is opened.
// Layout text: DSVFormat RTCount RTFormats... ElementCount, then one IA element per line.
#include <windows.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/Dx12DissolveDraw.h"
#ifdef WORLDDIFF_TEST_SILVER
#include "../src/SilverFlow.h"
#include "../src/SilverBindings.h"
#include <cstdarg>
namespace worlddiff {
void RuntimeLog(const char* format,...) {
    va_list args;va_start(args,format);vfprintf(stderr,format,args);va_end(args);fputc('\n',stderr);
}
}
#endif

using Microsoft::WRL::ComPtr;
using Blob = ComPtr<ID3DBlob>;
void check(HRESULT hr,const char* stage) {
    if(FAILED(hr)) {
        std::cerr << stage << ": HRESULT 0x" << std::hex << hr << std::dec << '\n';
        throw std::runtime_error(stage);
    }
}
Blob loadBlob(const wchar_t* path) {
    Blob b;
    check(D3DReadFileToBlob(path,&b),"read captured shader or root signature");
    return b;
}
D3D12_SHADER_BYTECODE bytes(const Blob& b) { return {b->GetBufferPointer(),b->GetBufferSize()}; }
Blob buildEffect(const Blob& vs,const char* entry,const char* profile) {
    ComPtr<ID3D12ShaderReflection> reflection;
    check(D3DReflect(vs->GetBufferPointer(),vs->GetBufferSize(),IID_PPV_ARGS(&reflection)),"reflect VS");
    D3D12_SHADER_DESC info{};
    check(reflection->GetDesc(&info),"read VS signature");
    std::ostringstream source;
    source << "#define DISSOLVE_POSITIONS_REGISTER register(t0,space31)\n"
              "#define DISSOLVE_SETTINGS_REGISTER register(b0,space31)\n"
              "#define DISSOLVE_MASK_REGISTER register(t1,space31)\n"
              "#define DISSOLVE_MASK_LAYOUT_REGISTER register(b1,space31)\n#define MATERIAL_VERTEX_FIELDS(F)";
    for(UINT i=0;i<info.OutputParameters;i++) {
        D3D12_SIGNATURE_PARAMETER_DESC p{};
        check(reflection->GetOutputParameterDesc(i,&p),"read VS output");
        if(p.SystemValueType!=D3D_NAME_UNDEFINED && p.SystemValueType!=D3D_NAME_POSITION)
            throw std::runtime_error("unhandled VS system value");
        unsigned width=p.Mask==1?1:p.Mask==3?2:p.Mask==7?3:p.Mask==15?4:0;
        if(!width || p.Register!=i || p.Stream)throw std::runtime_error("unhandled packed or streamed VS output");
        const char* type=nullptr;
        switch(p.ComponentType) {
            case D3D_REGISTER_COMPONENT_UINT32:type="uint";break;
            case D3D_REGISTER_COMPONENT_SINT32:type="int";break;
            case D3D_REGISTER_COMPONENT_FLOAT32:type="float";break;
            default:throw std::runtime_error("unsupported component type");
        }
        source << " F(" << type;
        if(width>1)source << width;
        source << ", " << (p.SystemValueType==D3D_NAME_POSITION?"clipPosition":"field"+std::to_string(i))
               << ", " << p.SemanticName << p.SemanticIndex << ')';
    }
    std::ifstream mask("src/shaders/DissolveCoverage.hlsl");
    if(!mask)throw std::runtime_error("run from repository root");
    source << '\n' << mask.rdbuf();
    std::string text=source.str();
    Blob gs,errors;
    auto hr=D3DCompile(text.data(),text.size(),"capture geometry adapter",nullptr,nullptr,
                       entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&gs,&errors);
    if(errors)std::cerr << static_cast<const char*>(errors->GetBufferPointer());
    check(hr,entry);
    return gs;
}
Blob extendSignature(const Blob& captured) {
    ComPtr<ID3D12VersionedRootSignatureDeserializer> reader;
    check(D3D12CreateVersionedRootSignatureDeserializer(captured->GetBufferPointer(),captured->GetBufferSize(),
        IID_PPV_ARGS(&reader)),"deserialize root signature");
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* original=nullptr;
    check(reader->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1,&original),"read 1.1 root signature");
    auto desc=*original;
    auto& d=desc.Desc_1_1;
    std::vector<worlddiff::dissolve::TableRequirement> requirements;
    if(!worlddiff::dissolve::DescribeTables(d,requirements))throw std::runtime_error("unsupported native root layout");
    std::cout<<"native descriptor tables:";
    for(auto& t:requirements)std::cout<<' '<<(t.heap==D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER?"sampler=":"resources=")<<t.size;
    std::cout<<'\n';
    std::vector<D3D12_ROOT_PARAMETER1> params;
    if(d.NumParameters)params.assign(d.pParameters,d.pParameters+d.NumParameters);
    D3D12_ROOT_PARAMETER1 constants{};
    constants.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants.ShaderVisibility=D3D12_SHADER_VISIBILITY_GEOMETRY;
    constants.Constants={0,31,5};
    params.push_back(constants);
    D3D12_ROOT_PARAMETER1 positions{};
    positions.ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
    positions.ShaderVisibility=D3D12_SHADER_VISIBILITY_GEOMETRY;
    positions.Descriptor={0,31,D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE};
    params.push_back(positions);
    D3D12_ROOT_PARAMETER1 mask=positions;
    mask.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    mask.Descriptor.ShaderRegister=1;
    params.push_back(mask);
    D3D12_ROOT_PARAMETER1 layout=constants;
    layout.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;layout.Constants={1,31,2};
    params.push_back(layout);
    d.NumParameters=static_cast<UINT>(params.size());d.pParameters=params.data();
    d.Flags=static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(d.Flags & ~(D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                                                                D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS));
    Blob blob,errors;
    auto hr=D3D12SerializeVersionedRootSignature(&desc,&blob,&errors);
    if(errors)std::cerr << static_cast<const char*>(errors->GetBufferPointer());
    check(hr,"serialize extended signature");
    return blob;
}
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=6)throw std::runtime_error("usage: test_dissolve_native_link vs.cso ps.cso root.cso layout.txt hardware|warp");
        auto vs=loadBlob(argv[1]),ps=loadBlob(argv[2]),root=loadBlob(argv[3]);
        std::ifstream layout{std::filesystem::path(argv[4])};
        auto number=[&](){unsigned n=0;if(!(layout>>n))throw std::runtime_error("missing layout field");return n;};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
        d.VS=bytes(vs);d.PS=bytes(ps);d.DSVFormat=static_cast<DXGI_FORMAT>(number());
        d.NumRenderTargets=number();
        if(d.NumRenderTargets>8)throw std::runtime_error("invalid RT count");
        for(UINT i=0;i<d.NumRenderTargets;i++)d.RTVFormats[i]=static_cast<DXGI_FORMAT>(number());
        auto count=number();
        if(count>32)throw std::runtime_error("invalid IA layout");
        std::vector<std::string> names(count);
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements(count);
        for(UINT i=0;i<count;i++) {
            layout>>names[i];auto& e=elements[i];e.SemanticName=names[i].c_str();
            e.SemanticIndex=number();e.Format=static_cast<DXGI_FORMAT>(number());e.InputSlot=number();
            e.AlignedByteOffset=number();e.InputSlotClass=static_cast<D3D12_INPUT_CLASSIFICATION>(number());e.InstanceDataStepRate=number();
        }
        d.InputLayout={elements.data(),count};d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        d.RasterizerState.DepthClipEnable=TRUE;d.SampleDesc.Count=1;d.SampleMask=UINT_MAX;
        auto& blend=d.BlendState.RenderTarget[0];
        blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlend=blend.DestBlendAlpha=D3D12_BLEND_ZERO;
        blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;
        d.DepthStencilState.DepthEnable=TRUE;d.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;
        d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;
        std::wstring mode=argv[5];ComPtr<IDXGIAdapter> adapter;
        if(mode==L"warp") {
            ComPtr<IDXGIFactory4> factory;
            check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI");check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"WARP");
        } else if(mode!=L"hardware") throw std::runtime_error("unknown adapter");
        ComPtr<ID3D12Device> device;check(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"DX12");
#ifdef WORLDDIFF_TEST_SILVER
        using namespace worlddiff::silver;
        originalRoot=[](ID3D12Device* gpu,UINT node,const void* blob,SIZE_T size,REFIID iid,void** out) {
            return gpu->CreateRootSignature(node,blob,size,iid,out);
        };
        originalPipeline=[](ID3D12Device* gpu,const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,REFIID iid,void** out) {
            return gpu->CreateGraphicsPipelineState(desc,iid,out);
        };
        ComPtr<ID3D12RootSignature> gameRoot;ComPtr<ID3D12PipelineState> gamePSO;
        // The game can create its signature before the material plugin starts.
        check(device->CreateRootSignature(0,root->GetBufferPointer(),root->GetBufferSize(),IID_PPV_ARGS(&gameRoot)),"pre-existing game root");
        d.pRootSignature=gameRoot.Get();
        check(CreatePipeline(device.Get(),&d,IID_PPV_ARGS(&gamePSO)),"game material");
        // The captured armor color/depth shaders supply UV_Pack; the three-target pass uses generic coverage.
        const unsigned expectedStage=d.NumRenderTargets==6?2u:d.NumRenderTargets==0?1u:4u;
        for(auto style:{worlddiff::dissolve::Style::Silver,worlddiff::dissolve::Style::Amber,worlddiff::dissolve::Style::Ink})
            if(Stage(gamePSO.Get(),style,true)!=expectedStage)throw std::runtime_error("material UV pipeline readiness");
        if(Stage(gamePSO.Get())!=(d.NumRenderTargets==6?2u:d.NumRenderTargets==0?1u:4u))
            throw std::runtime_error("unexpected silver flow pipeline classification");
        if(Stage(gamePSO.Get(),worlddiff::dissolve::Style::Amber)!=Stage(gamePSO.Get()))
            throw std::runtime_error("amber pipeline did not link");
        if(Stage(gamePSO.Get(),worlddiff::dissolve::Style::Ink)!=Stage(gamePSO.Get(),worlddiff::dissolve::Style::Amber))
            throw std::runtime_error("ink did not select the tessellated pipeline");
        if(Stage(gamePSO.Get())!=4) {
            ComPtr<ID3D12CommandAllocator> alloc;ComPtr<ID3D12GraphicsCommandList> list;
            check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)),"allocator");
            check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list)),"list");
            ComPtr<ID3D12DescriptorHeap> views,samplers;
            D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,128,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
            check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&views)),"views");
            hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;hd.NumDescriptors=32;
            check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&samplers)),"samplers");
            ID3D12DescriptorHeap* heaps[]={views.Get(),samplers.Get()};
            worlddiff::dissolve::SceneDraw hit{};hit.commands=list.Get();hit.root=gameRoot.Get();hit.pipeline=gamePSO.Get();
            auto setTables=[&]{for(UINT slot=0;slot<4;++slot)OnTable(list.Get(),slot,heaps[slot%2]->GetGPUDescriptorHandleForHeapStart());};
            Invalidate(list.Get());OnHeaps(list.Get(),2,heaps);OnRoot(list.Get(),gameRoot.Get());setTables();
            if(!CanBind(hit))throw std::runtime_error("complete live tables rejected");
            AfterIndirect(list.Get(),false);
            if(CanBind(hit))throw std::runtime_error("indirect tables reused");
            setTables();
            if(!CanBind(hit))throw std::runtime_error("rebound tables rejected");
            OnTable(list.Get(),0,{views->GetGPUDescriptorHandleForHeapStart().ptr+1});
            if(CanBind(hit))throw std::runtime_error("misaligned material table accepted");
            Invalidate(list.Get());
            if(CanBind(hit))throw std::runtime_error("reset retained material tables");
            check(list->Close(),"close list");
        }
        std::cout<<"PASS game silver flow pipeline, targets="<<d.NumRenderTargets<<'\n';
#endif
        auto create=[&](const Blob& signature) {
            ComPtr<ID3D12RootSignature> rs;ComPtr<ID3D12PipelineState> pipeline;
            check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&rs)),"create root");
            d.pRootSignature=rs.Get();check(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&pipeline)),"link pass");
        };
        create(root);
        auto gs=buildEffect(vs,"CoverageGS","gs_5_1");
        auto extended=extendSignature(root);
        d.GS=bytes(gs);create(extended);
        auto originalTargets=d.NumRenderTargets;
        if(originalTargets) {
            d.NumRenderTargets=1;
            for(auto& fmt:d.RTVFormats)fmt=DXGI_FORMAT_UNKNOWN;
            d.RTVFormats[0]=DXGI_FORMAT_R8_UNORM;
            blend.BlendEnable=TRUE;blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ZERO;
            d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_EQUAL;
            d.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
            create(extended);
            auto edge=buildEffect(vs,"SilverEdgePS","ps_5_1");
            d.PS=bytes(edge);d.RTVFormats[0]=DXGI_FORMAT_B8G8R8A8_UNORM;
            blend.BlendEnable=FALSE;blend.SrcBlend=blend.SrcBlendAlpha=D3D12_BLEND_ONE;
            create(extended);
        }
        std::cout << "PASS native and clipped PSO, render targets=" << originalTargets
                  << ", private mask and silver edge PSOs=" << (originalTargets?"yes":"depth only") << '\n';
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
