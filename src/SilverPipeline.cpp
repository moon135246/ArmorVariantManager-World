#include "SilverFlow.h"
#include "SilverBindings.h"
#include "Overlay.h"
#include <d3dcompiler.h>
#include <d3d12shader.h>
#include <wrl/client.h>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace worlddiff::silver {
using Microsoft::WRL::ComPtr;
RootCreate originalRoot{};
PipelineCreate originalPipeline{};
namespace {
const GUID flowRoot={0x078b64e6,0x657f,0x457b,{0x95,0xdc,0x2c,0x2a,0x82,0x6d,0xd3,0x93}};
const GUID readTables={0x0c437e22,0x69c6,0x4eef,{0x8d,0x31,0xe8,0xa0,0x9d,0xc1,0x27,0xd2}};
const GUID flowSurface={0xb85522ee,0xf166,0x4102,{0xa2,0xcf,0xc7,0x75,0x88,0xb8,0x58,0xd6}};
const GUID flowEdge={0xad952273,0x6dd1,0x4497,{0xbb,0x42,0xd4,0xf2,0xb6,0xb2,0x86,0x42}};
const GUID amberSurface={0xee633228,0x0c9e,0x4c62,{0xaf,0x01,0xd9,0x0d,0xbb,0x5c,0xaa,0x21}};
const GUID amberEdge={0x293861a8,0xa093,0x4ea9,{0xb8,0x70,0xb2,0x77,0xdd,0x0a,0x45,0xfb}};
const GUID flowReady={0x0b2b648f,0x74af,0x4951,{0x80,0xe7,0x91,0x93,0x94,0x24,0xdc,0xef}};
// UV variants consume the original vertex shader's primary texture coordinates.
const GUID uvSilverSurface={0x01b89e70,0x1fe3,0x4a82,{0xba,0x73,0x6f,0x57,0x6a,0x7f,0xe3,0xa0}};
const GUID uvSilverEdge={0x01b89e71,0x1fe3,0x4a82,{0xba,0x73,0x6f,0x57,0x6a,0x7f,0xe3,0xa0}};
const GUID uvAmberSurface={0x01b89e72,0x1fe3,0x4a82,{0xba,0x73,0x6f,0x57,0x6a,0x7f,0xe3,0xa0}};
const GUID uvAmberEdge={0x01b89e73,0x1fe3,0x4a82,{0xba,0x73,0x6f,0x57,0x6a,0x7f,0xe3,0xa0}};
const GUID nativeSilver={0x82fa6c33,0x147b,0x4663,{0xba,0xcf,0xbb,0x26,0x51,0xc5,0x9b,0xd1}};
const GUID nativeViolet={0xb182579f,0x19c8,0x4998,{0x83,0x17,0xd3,0x71,0x32,0x2d,0xb0,0x96}};
using Blob=ComPtr<ID3DBlob>;
using Requirements=std::array<dissolve::TableRequirement,4>;
struct Layout {
    ComPtr<ID3D12RootSignature> native,sibling;
    Requirements required{};
};
const Layout* KnownLayout(ID3D12RootSignature* root) {
    const Layout* layout{};UINT size=sizeof(layout);
    if(FAILED(root->GetPrivateData(flowRoot,&size,&layout)))return nullptr;
    return layout && layout->native.Get()==root?layout:nullptr;
}
Blob Extend(const void* data,SIZE_T bytes,Requirements& required) {
    ComPtr<ID3D12VersionedRootSignatureDeserializer> reader;
    if(FAILED(D3D12CreateVersionedRootSignatureDeserializer(data,bytes,IID_PPV_ARGS(&reader))))return {};
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* input{};
    if(FAILED(reader->GetRootSignatureDescAtVersion(D3D_ROOT_SIGNATURE_VERSION_1_1,&input)))return {};
    auto copy=*input;
    auto& desc=copy.Desc_1_1;
    std::vector<dissolve::TableRequirement> tables;
    if(desc.NumParameters!=4 || !(desc.Flags&D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT) ||
       !dissolve::DescribeTables(desc,tables))return {};
    for(UINT i=0;i<4;++i) {
        const auto& p=desc.pParameters[i];
        if(p.ShaderVisibility!=(i<2?D3D12_SHADER_VISIBILITY_PIXEL:D3D12_SHADER_VISIBILITY_VERTEX))return {};
        if(tables[i].heap!=(i%2?D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))return {};
    }
    std::copy(tables.begin(),tables.end(),required.begin());
    std::vector<D3D12_ROOT_PARAMETER1> params(desc.pParameters,desc.pParameters+4);
    D3D12_ROOT_PARAMETER1 settings{};
    settings.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    settings.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    settings.Constants={0,31,5};params.push_back(settings);
    D3D12_ROOT_PARAMETER1 positions{};
    positions.ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
    positions.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    positions.Descriptor={0,31,D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE};
    params.push_back(positions);
    desc.Flags=static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(desc.Flags & ~(D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS));
    desc.pParameters=params.data();desc.NumParameters=static_cast<UINT>(params.size());
    Blob blob;
    if(FAILED(D3D12SerializeVersionedRootSignature(&copy,&blob,nullptr)))return {};
    return blob;
}
void AttachLayout(ID3D12Device* device,UINT node,ID3D12RootSignature* native,const void* data,SIZE_T bytes) {
    if(KnownLayout(native))return;
    Requirements required{};
    auto blob=Extend(data,bytes,required);
    if(!blob)return;
    ComPtr<ID3D12RootSignature> sibling;
    if(FAILED(originalRoot(device,node,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&sibling))))return;
    static std::mutex mutex;
    static std::vector<std::unique_ptr<Layout>> layouts;
    std::lock_guard lock(mutex);
    if(KnownLayout(native)||layouts.size()>=64)return;
    auto entry=std::make_unique<Layout>();
    entry->native=native;entry->sibling=std::move(sibling);entry->required=required;
    const Layout* stable=entry.get();layouts.push_back(std::move(entry));
    native->SetPrivateData(flowRoot,sizeof(stable),&stable);
}
// The renderer may already own its material signatures when the plugin loads.
// Recreate the two observed table layouts via D3D12. Only identical signature
// objects receive the attachment. Unrelated layouts never pass this lookup.
void PrepareExistingLayouts(ID3D12Device* device) {
    static std::mutex mutex;
    static std::vector<ComPtr<ID3D12Device>> prepared;
    std::lock_guard lock(mutex);
    if(std::any_of(prepared.begin(),prepared.end(),[&](auto& d){return d.Get()==device;}))return;
    for(bool color:{false,true}) {
        D3D12_DESCRIPTOR_RANGE1 ranges[]={
            {D3D12_DESCRIPTOR_RANGE_TYPE_CBV,color?14u:5u,0,0,static_cast<D3D12_DESCRIPTOR_RANGE_FLAGS>(5),D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
            {D3D12_DESCRIPTOR_RANGE_TYPE_SRV,color?32u:3u,0,0,D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
            {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,9,0,0,D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
            {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,16,0,0,D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE,D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND}
        };
        D3D12_ROOT_PARAMETER1 params[4]{};
        for(UINT i=0;i<4;++i) {
            params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[i].ShaderVisibility=i<2?D3D12_SHADER_VISIBILITY_PIXEL:D3D12_SHADER_VISIBILITY_VERTEX;
            params[i].DescriptorTable={i%2?1u:color?3u:2u,ranges+(i%2?3:0)};
        }
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc{};
        desc.Version=D3D_ROOT_SIGNATURE_VERSION_1_1;
        desc.Desc_1_1={4,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        Blob blob;ComPtr<ID3D12RootSignature> native;
        if(SUCCEEDED(D3D12SerializeVersionedRootSignature(&desc,&blob,nullptr)) &&
           SUCCEEDED(originalRoot(device,0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&native))))
            AttachLayout(device,0,native.Get(),blob->GetBufferPointer(),blob->GetBufferSize());
    }
    prepared.emplace_back(device);
}
unsigned UsedTables(const D3D12_SHADER_BYTECODE& shader,unsigned first) {
    if(!shader.BytecodeLength)return 0;
    ComPtr<ID3D12ShaderReflection> reflection;D3D12_SHADER_DESC desc{};
    if(FAILED(D3DReflect(shader.pShaderBytecode,shader.BytecodeLength,IID_PPV_ARGS(&reflection))) ||
       FAILED(reflection->GetDesc(&desc)))return 3u<<first;
    unsigned mask=0;
    for(UINT i=0;i<desc.BoundResources;++i) {
        D3D12_SHADER_INPUT_BIND_DESC resource{};
        if(FAILED(reflection->GetResourceBindingDesc(i,&resource)))return 3u<<first;
        mask|=1u<<(first+(resource.Type==D3D_SIT_SAMPLER?1:0));
    }
    return mask;
}
std::string EffectSource(WORD resource) {
    HMODULE self{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&EffectSource),&self))return {};
    HRSRC id=FindResourceW(self,MAKEINTRESOURCEW(resource),MAKEINTRESOURCEW(10));
    if(!id)return {};
    const auto loaded=LoadResource(self,id);
    const auto* data=static_cast<const char*>(LockResource(loaded));
    return data?std::string(data,SizeofResource(self,id)):std::string{};
}
struct Programs { Blob gs,edge,hull,domain,crystalColor,uvGs,uvHull; };
Programs ProgramsFor(const D3D12_SHADER_BYTECODE& vs) {
    ComPtr<ID3D12ShaderReflection> reflect;
    D3D12_SHADER_DESC description{};
    if(FAILED(D3DReflect(vs.pShaderBytecode,vs.BytecodeLength,IID_PPV_ARGS(&reflect))) ||
       FAILED(reflect->GetDesc(&description)))return {};
    std::ostringstream fields,interpolation;
    std::string uvField;
    fields<<"#define MATERIAL_VERTEX_FIELDS(F)";
    interpolation<<"\n#define MATERIAL_INTERPOLATE(o,p,w)";
    for(UINT i=0;i<description.OutputParameters;++i) {
        D3D12_SIGNATURE_PARAMETER_DESC p{};
        if(FAILED(reflect->GetOutputParameterDesc(i,&p)) || p.Stream || p.Register!=i ||
           (p.SystemValueType!=D3D_NAME_UNDEFINED && p.SystemValueType!=D3D_NAME_POSITION))return {};
        int width=p.Mask==1?1:p.Mask==3?2:p.Mask==7?3:p.Mask==15?4:0;
        const char* type=p.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32?"float":
                         p.ComponentType==D3D_REGISTER_COMPONENT_SINT32?"int":
                         p.ComponentType==D3D_REGISTER_COMPONENT_UINT32?"uint":nullptr;
        if(!width||!type)return {};
        fields<<" F("<<type;if(width>1)fields<<width;
        const auto name=p.SystemValueType==D3D_NAME_POSITION?"clipPosition":"field"+std::to_string(i);
        fields<<","<<name<<","<<p.SemanticName<<p.SemanticIndex<<")";
        if(!_stricmp(p.SemanticName,"UV_Pack") && p.SemanticIndex==0 &&
           p.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32 && width>=2)uvField=name;
        interpolation<<" o."<<name<<"=p[0]."<<name;
        if(p.ComponentType==D3D_REGISTER_COMPONENT_FLOAT32)
            interpolation<<"*w.x+p[1]."<<name<<"*w.y+p[2]."<<name<<"*w.z";
        interpolation<<";";
    }
    static std::mutex mutex;
    static std::unordered_map<std::string,Programs> compiled;
    std::lock_guard lock(mutex);
    const auto key=fields.str();
    if(const auto it=compiled.find(key);it!=compiled.end())return it->second;
    static const std::string silverSource=EffectSource(311),amberSource=EffectSource(312);
    const auto prefix=key+interpolation.str()+"\n#define SILVER_GAME_FIELD 1\n#define DISSOLVE_POSITIONS_REGISTER register(t0,space31)\n"
                         "#define DISSOLVE_SETTINGS_REGISTER register(b0,space31)\n"
                         "#define DISSOLVE_MASK_REGISTER register(t1,space31)\n"
                         "#define DISSOLVE_MASK_LAYOUT_REGISTER register(b1,space31)\n";
    Programs shaders;
    auto compile=[&](const std::string& source,const char* name,const char* profile,Blob& output) {
        Blob error;
        if(FAILED(D3DCompile(source.data(),source.size(),name,nullptr,nullptr,name,profile,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&output,&error)))
            RuntimeLog("Silver flow shader %s: %s",name,error?static_cast<const char*>(error->GetBufferPointer()):"compile failed");
    };
    auto source=prefix+silverSource;
    compile(source,"CoverageGS","gs_5_1",shaders.gs);
    compile(source,"SilverAlbedoPS","ps_5_1",shaders.edge);
    source=prefix+amberSource;
    compile(source,"AmberHS","hs_5_1",shaders.hull);
    compile(source,"AmberDS","ds_5_1",shaders.domain);
    compile(source,"AmberAlbedoPS","ps_5_1",shaders.crystalColor);
    if(!uvField.empty()) {
        // Mirror tiled islands into [0,1], keeping the two ends of a full UV tile distinct.
        const auto uvSource=prefix+"\n#define MATERIAL_UV(v) (1.0-abs(2.0*frac(0.5*(v)."+uvField+".xy)-1.0))\n";
        compile(uvSource+silverSource,"CoverageGS","gs_5_1",shaders.uvGs);
        compile(uvSource+amberSource,"AmberHS","hs_5_1",shaders.uvHull);
    }
    compiled.emplace(key,shaders);
    return shaders;
}
bool ArmorPass(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    if(!d.pRootSignature || !d.VS.BytecodeLength || d.GS.BytecodeLength || d.HS.BytecodeLength ||
       d.DS.BytecodeLength || d.StreamOutput.NumEntries || d.SampleDesc.Count!=1 ||
       !d.DepthStencilState.DepthEnable || d.PrimitiveTopologyType!=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE ||
       (d.NumRenderTargets && (d.NumRenderTargets!=6 || d.RTVFormats[0]!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)))return false;
    bool skin=false,pos=false;
    for(UINT i=0;i<d.InputLayout.NumElements;i++) {
        const auto& e=d.InputLayout.pInputElementDescs[i];
        if(!_stricmp(e.SemanticName,"JOINT"))skin=true;
        if(!_stricmp(e.SemanticName,"POSITION") && e.Format==DXGI_FORMAT_R32G32B32_FLOAT)pos=true;
    }
    return skin && pos;
}
Blob Solid(const char* source) {
    Blob shader;
    D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main","ps_5_0",
               D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,nullptr);
    return shader;
}
void EdgeBlend(D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_EQUAL;
    d.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    d.DepthStencilState.StencilWriteMask=0;
    d.BlendState.IndependentBlendEnable=TRUE;
    d.BlendState.AlphaToCoverageEnable=FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC neutral{};
    neutral.LogicOp=D3D12_LOGIC_OP_NOOP;
    neutral.SrcBlend=neutral.SrcBlendAlpha=D3D12_BLEND_ONE;
    neutral.DestBlend=neutral.DestBlendAlpha=D3D12_BLEND_ZERO;
    neutral.BlendOp=neutral.BlendOpAlpha=D3D12_BLEND_OP_ADD;
    for(auto& target:d.BlendState.RenderTarget)target=neutral;
    auto& b=d.BlendState.RenderTarget[0];
    b.RenderTargetWriteMask=7;b.BlendEnable=TRUE;
    b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
}
void PrepareBands(ID3D12Device* device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& source,ID3D12PipelineState* owner) {
    if(source.NumRenderTargets!=6)return;
    static const Blob core=Solid("float4 main():SV_Target{return float4(.95,.91,1,.94);}");
    static const Blob glow=Solid("float4 main():SV_Target{return float4(.62,.39,.86,.57);}");
    if(!core||!glow)return;
    auto d=source;d.CachedPSO={};EdgeBlend(d);
    auto make=[&](const Blob& shader,const GUID& id) {
        d.PS={shader->GetBufferPointer(),shader->GetBufferSize()};
        ComPtr<ID3D12PipelineState> edge;
        if(FAILED(originalPipeline(device,&d,IID_PPV_ARGS(&edge))))return false;
        return SUCCEEDED(owner->SetPrivateDataInterface(id,edge.Get()));
    };
    if(make(glow,nativeViolet)&&make(core,nativeSilver)) {
        static std::atomic_uint count{};
        if(count++<6)RuntimeLog("Silver flow: native-root color bands ready");
    }
}
}
HRESULT STDMETHODCALLTYPE CreateRoot(ID3D12Device* device,UINT node,const void* data,SIZE_T bytes,REFIID iid,void** result) {
    auto hr=originalRoot(device,node,data,bytes,iid,result);
    if(SUCCEEDED(hr)&&result&&*result&&iid==__uuidof(ID3D12RootSignature)) {
        try {AttachLayout(device,node,static_cast<ID3D12RootSignature*>(*result),data,bytes);} catch(...) {}
    }
    return hr;
}
void Register(ID3D12Device* device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC* src,ID3D12PipelineState* owner) {
    try {
        static std::atomic_uint creation{};
        if(creation++==0)RuntimeLog("Silver flow: game is creating graphics pipelines");
        if(!ArmorPass(*src))return;
        PrepareBands(device,*src,owner);
        if(!originalRoot)return;
        PrepareExistingLayouts(device);
        const auto* layout=KnownLayout(src->pRootSignature);
        if(!layout)return;
        auto shaders=ProgramsFor(src->VS);
        if(!shaders.gs||!shaders.edge)return;
        auto make=[&](dissolve::Style style,bool uv) {
            const bool amber=style==dissolve::Style::Amber;
            const auto& gs=uv?shaders.uvGs:shaders.gs;
            const auto& hull=uv?shaders.uvHull:shaders.hull;
            const GUID& surfaceKey=amber?(uv?uvAmberSurface:amberSurface):(uv?uvSilverSurface:flowSurface);
            const GUID& edgeKey=amber?(uv?uvAmberEdge:amberEdge):(uv?uvSilverEdge:flowEdge);
            auto d=*src;d.pRootSignature=layout->sibling.Get();d.CachedPSO={};
            if(amber) {
                if(!hull||!shaders.domain||!shaders.crystalColor)return false;
                d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
                d.HS={hull->GetBufferPointer(),hull->GetBufferSize()};
                d.DS={shaders.domain->GetBufferPointer(),shaders.domain->GetBufferSize()};
            } else {
                if(!gs)return false;
                d.GS={gs->GetBufferPointer(),gs->GetBufferSize()};
            }
            ComPtr<ID3D12PipelineState> surface,edge;
            if(FAILED(originalPipeline(device,&d,IID_PPV_ARGS(&surface))))return false;
            if(d.NumRenderTargets) {
                auto& ps=amber?shaders.crystalColor:shaders.edge;
                d.PS={ps->GetBufferPointer(),ps->GetBufferSize()};EdgeBlend(d);
                if(FAILED(originalPipeline(device,&d,IID_PPV_ARGS(&edge))))return false;
                if(FAILED(owner->SetPrivateDataInterface(edgeKey,edge.Get())))return false;
            }
            return SUCCEEDED(owner->SetPrivateDataInterface(surfaceKey,surface.Get()));
        };
        const UINT kind=src->NumRenderTargets?2u:1u;
        const UINT silverKind=make(dissolve::Style::Silver,false)?kind:4u;
        const UINT amberKind=make(dissolve::Style::Amber,false)?kind:4u;
        const unsigned used=UsedTables(src->VS,2)|UsedTables(src->PS,0);
        if(FAILED(owner->SetPrivateData(readTables,sizeof(used),&used)))return;
        const UINT uvSilverKind=make(dissolve::Style::Silver,true)?kind:4u;
        const UINT uvAmberKind=make(dissolve::Style::Amber,true)?kind:4u;
        const UINT styles=silverKind|(amberKind<<4)|(uvSilverKind<<8)|(uvAmberKind<<12);
        owner->SetPrivateData(flowReady,sizeof(styles),&styles);
        static std::atomic_uint variants{};
        if(variants++<12)RuntimeLog("Dissolve styles: colors=%u silver=%u amber=%u",src->NumRenderTargets,silverKind,amberKind);
    } catch(...) {}
    return;
}
HRESULT STDMETHODCALLTYPE CreatePipeline(ID3D12Device* device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC* src,REFIID iid,void** result) {
    auto hr=originalPipeline(device,src,iid,result);
    if(SUCCEEDED(hr)&&src&&result&&*result&&iid==__uuidof(ID3D12PipelineState))
        Register(device,src,static_cast<ID3D12PipelineState*>(*result));
    return hr;
}
LibraryLoad originalLibrary{};
HRESULT STDMETHODCALLTYPE LoadGraphics(ID3D12PipelineLibrary* library,LPCWSTR name,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC* src,REFIID iid,void** result) {
    auto hr=originalLibrary(library,name,src,iid,result);
    if(SUCCEEDED(hr)&&src&&result&&*result&&iid==__uuidof(ID3D12PipelineState)) {
        ComPtr<ID3D12Device> device;
        if(SUCCEEDED(library->GetDevice(IID_PPV_ARGS(&device))))Register(device.Get(),src,static_cast<ID3D12PipelineState*>(*result));
    }
    return hr;
}
bool RecordBands(const dissolve::SceneDraw& h,UINT begin,UINT inner,UINT end) {
    if(h.colors!=6||end<=begin)return false;
    ComPtr<ID3D12PipelineState> glow,core;UINT size=sizeof(ID3D12PipelineState*);
    if(FAILED(h.pipeline->GetPrivateData(nativeSilver,&size,core.GetAddressOf())) || !core ||
       FAILED(h.pipeline->GetPrivateData(nativeViolet,&size,glow.GetAddressOf())) || !glow)return false;
    auto band=[&](ID3D12PipelineState* p,UINT first,UINT stop) {
        if(first==stop)return;
        h.commands->SetPipelineState(p);
        h.commands->DrawIndexedInstanced(stop-first,1,first,h.draw.BaseVertexLocation,0);
    };
    band(glow.Get(),begin,inner);band(core.Get(),inner,end);
    h.commands->SetPipelineState(h.pipeline);
    return true;
}
unsigned Stage(ID3D12PipelineState* pso,dissolve::Style style,bool uv) {
    // Keep the source object alive in this small recording-thread cache; its
    // address cannot be recycled while its classification is cached.
    struct Entry { ComPtr<ID3D12PipelineState> state; UINT stage{}; };
    thread_local std::array<Entry,8> recent;
    thread_local unsigned cursor{};
    const auto shift=(style==dissolve::Style::Silver?0:4)+(uv?8:0);
    for(const auto& entry:recent)if(entry.state.Get()==pso)return (entry.stage>>shift)&15;
    UINT stage{},size=sizeof(stage);
    if(FAILED(pso->GetPrivateData(flowReady,&size,&stage)))stage=0x4444;
    recent[cursor++%recent.size()]={pso,stage};
    return (stage>>shift)&15;
}
static bool Capture(const dissolve::SceneDraw& hit,MaterialBindings& saved,const Layout*& layout) {
    if(!Snapshot(hit.commands,saved)||saved.root!=hit.root) {
        static std::atomic_uint missed{};
        if(missed++<3)RuntimeLog("Silver pixel: incomplete command-list root, colors=%u",hit.colors);
        return false;
    }
    layout=KnownLayout(hit.root);if(!layout)return false;
    const auto& required=layout->required;
    UINT used{},size=sizeof(used);
    if(FAILED(hit.pipeline->GetPrivateData(readTables,&size,&used)))return false;
    for(UINT i=0;i<4;++i) {
        const auto table=saved.tables[i];
        if(!table.ptr && !(used&(1u<<i)))continue;
        if(!saved.bounds[required[i].heap].Contains(table,required[i].size)) {
            static std::atomic_uint missed{};
            if(missed++<6)RuntimeLog("Silver pixel: material table %u unavailable, colors=%u used=%u",i,hit.colors,used);
            return false;
        }
    }
    return true;
}
bool CanBind(const dissolve::SceneDraw& hit) {
    MaterialBindings saved;const Layout* layout{};
    return Capture(hit,saved,layout);
}
bool RecordMaterial(const dissolve::SceneDraw& hit, D3D12_GPU_VIRTUAL_ADDRESS coords,
                    const std::array<float,5>& constants,dissolve::Style style) {
    ComPtr<ID3D12PipelineState> surface,edge;
    UINT size=sizeof(ID3D12PipelineState*);
    const bool patches=style!=dissolve::Style::Silver;
    const bool uv=coords==0;
    const GUID& surfaceKey=patches?(uv?uvAmberSurface:amberSurface):(uv?uvSilverSurface:flowSurface);
    const GUID& edgeKey=patches?(uv?uvAmberEdge:amberEdge):(uv?uvSilverEdge:flowEdge);
    if(FAILED(hit.pipeline->GetPrivateData(surfaceKey,&size,surface.GetAddressOf())) || !surface)return false;
    if(hit.colors && (FAILED(hit.pipeline->GetPrivateData(edgeKey,&size,edge.GetAddressOf())) || !edge))return false;
    auto* cmd=hit.commands;
    MaterialBindings saved;const Layout* layout{};
    if(!Capture(hit,saved,layout))return false;
    auto bind=[&](ID3D12RootSignature* root) {
        cmd->SetGraphicsRootSignature(root);
        for(UINT i=0;i<4;++i)if(saved.tables[i].ptr)cmd->SetGraphicsRootDescriptorTable(i,saved.tables[i]);
    };
    bind(layout->sibling.Get());
    cmd->SetGraphicsRoot32BitConstants(4,5,constants.data(),0);
    if(!uv)cmd->SetGraphicsRootShaderResourceView(5,coords);
    auto draw=[&](ID3D12PipelineState* pso) {
        cmd->SetPipelineState(pso);
        cmd->DrawIndexedInstanced(hit.draw.IndexCountPerInstance,1,hit.draw.StartIndexLocation,hit.draw.BaseVertexLocation,0);
    };
    if(patches)cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
    draw(surface.Get());
    if(edge)draw(edge.Get());
    if(patches)cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    bind(hit.root);
    cmd->SetPipelineState(hit.pipeline);
    return true;
}
}
