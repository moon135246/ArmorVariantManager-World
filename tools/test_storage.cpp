#include "../src/State.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <memory>
#include <cstring>
#include <stdexcept>
using namespace worlddiff;using namespace worlddiff::app;namespace fs=std::filesystem;
static std::string Read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
static void Write(const fs::path& p,const std::string& text){fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);out<<text;if(!out)throw std::runtime_error("fixture write failed");}
static std::string Dump(){std::ostringstream out;WriteDocument(out,Get().data);return out.str();}
static void Reset(const fs::path& p){auto& s=Get();std::destroy_at(&s);std::construct_at(&s);s.directory=p;s.configPath=p/L"Settings.cfg";fs::create_directories(p);}
static Model MakeModel(const std::string& scope,const std::string& part="body"){
 Model m;m.equip=scope;m.part=part;m.relativePath="pl\\f_equip\\"+scope+"\\"+part+"\\mod\\f_"+part+scope.substr(2);m.display="fixture";
 for(int i=0;i<3;i++){auto name="StorageMaterial"+std::to_string(i);m.materials.push_back({name,MaterialNameHash(name),i});Mesh mesh;mesh.index=i;mesh.materialId=(uint16_t)i;mesh.vertexCount=(uint16_t)(100+i);mesh.indexCount=300+i*3;mesh.lod=65535;mesh.blockType=77;memcpy(mesh.raw.data()+2,&mesh.vertexCount,2);memcpy(mesh.raw.data()+6,&mesh.materialId,2);memcpy(mesh.raw.data()+8,&mesh.lod,2);memcpy(mesh.raw.data()+0x18,&mesh.blockType,4);memcpy(mesh.raw.data()+0x20,&mesh.indexCount,4);m.meshes.push_back(mesh);}m.signature=MeshOrderSignature(m);return m;
}
static uint64_t Configure(const Model& m){auto& s=Get();Main(m.equip);uint64_t id=0;if(!CreateGroup("衣服",m.equip,false,{{m.relativePath,m.materials[0].hash,false}},id))throw std::runtime_error("fixture group");SetVisible(*Find(id),{m.relativePath,0,true},false);SavePreset(*Find(id),"隐藏");SetMeshNote(m,1,"中文备注");MoveMeshBefore(m,2,0);auto preset=Find(id)->selected;TransformRule rule;rule.id=s.data.nextId++;rule.name="血量节点";rule.scope=m.equip;rule.condition.kind=ConditionKind::Health;rule.condition.healthMax=25;rule.targets={{id,preset}};s.data.transforms.push_back(rule);EnsureTransformMode(s.data,m.equip).enabled=true;return id;}
struct LiveFixture {
 std::vector<unsigned char> entity=std::vector<unsigned char>(0xD000),armor=std::vector<unsigned char>(0x380);
 struct Part{std::vector<unsigned char> ctx=std::vector<unsigned char>(0x800),resource=std::vector<unsigned char>(0x500),names,meshes;};
 std::array<Part,5> parts;OutfitSnapshot snapshot;
 static void Ptr(std::vector<unsigned char>& b,size_t off,uintptr_t value){memcpy(b.data()+off,&value,8);}
 explicit LiveFixture(const std::array<Model,5>& models){
  snapshot.entity=(uintptr_t)entity.data();snapshot.ready=true;Ptr(entity,0xC928,(uintptr_t)armor.data());constexpr size_t offsets[]={0x300,0,0x80,0x100,0x180};
  for(size_t i=0;i<5;i++){auto& p=parts[i];auto& m=models[i];p.names.resize(m.materials.size()*128);p.meshes.resize(m.meshes.size()*80);for(size_t n=0;n<m.materials.size();n++)memcpy(p.names.data()+n*128,m.materials[n].name.c_str(),m.materials[n].name.size()+1);for(size_t n=0;n<m.meshes.size();n++)memcpy(p.meshes.data()+n*80,m.meshes[n].raw.data(),80);
   auto ctx=i==1?snapshot.entity:(uintptr_t)p.ctx.data();auto res=(uintptr_t)p.resource.data();Ptr(armor,offsets[i],ctx);if(i==1)Ptr(entity,0x2A0,res);else Ptr(p.ctx,0x2A0,res);memcpy(p.resource.data()+0xC,m.relativePath.c_str(),m.relativePath.size()+1);Ptr(p.resource,0xC8,(uintptr_t)p.meshes.data());Ptr(p.resource,0x400,(uintptr_t)p.names.data());uint32_t count=(uint32_t)m.meshes.size(),mats=(uint32_t)m.materials.size();memcpy(p.resource.data()+0xD0,&count,4);memcpy(p.resource.data()+0xE4,&mats,4);snapshot.contexts[i]=ctx;snapshot.resources[i]=res;snapshot.paths[i]=m.relativePath;
  }
 }
};
static bool Memory(uintptr_t p,void* out,size_t n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)p,out,n,&got)&&got==n;}
static void Follow(const OutfitSnapshot& snapshot){FollowPlayerOutfit(snapshot);Get().outfitSince=GetTickCount()-400;FollowPlayerOutfit(snapshot);}
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);};auto root=fs::temp_directory_path()/(L"WorldDiff117Storage_"+std::to_wstring(GetTickCount64()));Reset(root/L"normal");auto& s=Get();auto a=MakeModel("pl105_0000"),b=MakeModel("pl106_0000");s.models={a,b};auto gid=Configure(a);auto preset=Find(gid)->selected;
 check(Save(),"first canonical save succeeds");auto canonical=s.directory/L"Presets/pl105_0000.wdp";auto settings=Read(s.configPath);auto content=Read(canonical);
 check(settings.find("WorldDiffSettings 123")==0&&settings.find("GROUP")==std::string::npos&&settings.find("MESH")==std::string::npos&&settings.find("BINDING")==std::string::npos,"settings contain no equipment data");
 check(s.data.theme==UiTheme::ClassicWhite,"new configuration defaults to classic white");
 s.data.theme=UiTheme::Basic;check(SaveSettings(),"explicit basic setting saved");s.data.theme=UiTheme::ClassicWhite;
 check(LoadSettings()&&s.data.theme==UiTheme::Basic,"existing basic preference is respected");
 s.data.theme=UiTheme::ClassicWhite;check(SaveSettings()&&Read(canonical)==content,"theme saved independently of equipment");
 s.data.theme=UiTheme::Basic;check(LoadSettings()&&s.data.theme==UiTheme::ClassicWhite,"classic white survives reload");
 Write(s.configPath,"WorldDiffSettings 123\nSET 1 1 0 120 1 0 1 1 1 0 4\n");
 check(!LoadSettings()&&s.data.theme==UiTheme::ClassicWhite,"unsupported theme cannot partially replace settings");
 Write(s.configPath,"WorldDiffSettings 122\nSET 1 1 0 120 1 0 1 1 1 0\n");
 check(LoadSettings()&&s.data.theme==UiTheme::ClassicWhite,"pre-theme settings default to classic white");Write(s.configPath,settings);
 check(content.find("WorldDiffProfile 122")==0&&content.find("WorldDiffEquipment 122")!=std::string::npos&&content.find("\nSET ")==std::string::npos&&content.find("AUTOFIND")==std::string::npos,"WDP contains only equipment definitions and identity");
 check(content.find("STAMP ")!=std::string::npos&&content.find("MESHVIEW")!=std::string::npos&&content.find("TRANSFORM ")!=std::string::npos,"identity notes order and transformations stored together");
 check(ExportProfile(a.equip,"共享模板"),"explicit export creates template");auto tpl=s.directory/L"Templates/共享模板.wdp";auto source=Read(tpl);auto sourceTime=fs::last_write_time(tpl);
 check(s.data.bindings[a.equip]=="pl105_0000.wdp","export never rebinds equipment to template");
 check(ApplyProfile(b.equip,"共享模板.wdp"),"template remaps to second equipment");check(s.data.bindings[b.equip]=="pl106_0000.wdp"&&fs::exists(s.directory/L"Presets/pl106_0000.wdp"),"import commits an independent canonical file");
 auto bg=std::find_if(s.data.groups.begin(),s.data.groups.end(),[&](auto& g){return g.scope==b.equip&&!g.main;});bg->name="浴场独立修改";Changed();check(Save(),"target edit saves independently");
 check(Read(tpl)==source&&fs::last_write_time(tpl)==sourceTime,"automatic editing cannot rewrite source template");check(Read(canonical)==content,"editing target cannot rewrite source equipment");
 check(s.data.transforms.size()==2&&s.data.meshDisplay.size()==6,"transforms and presentation remap to independent equipment");
 auto idBefore=gid;s.data.groups.clear();s.data.transforms.clear();s.data.transformModes.clear();s.data.meshDisplay.clear();check(Load(),"settings plus equipment reload succeeds");check(Find(idBefore)&&Find(idBefore)->selected==preset,"stable IDs and default selection survive direct load");check(MeshView(s.models[0],1)&&MeshView(s.models[0],1)->note=="中文备注","notes survive canonical reload");
 auto stable=Read(canonical);auto stableTime=fs::last_write_time(canonical);s.data.scale=1.1f;Changed();check(Save()&&Read(canonical)==stable&&fs::last_write_time(canonical)==stableTime,"global settings change does not rewrite any WDP");
 auto files=FindProfiles(b.equip);check(files.size()==1&&files[0].strong,"matching searches templates, excluding equipment copies");
 check(ExportProfile(a.equip,"重复模板"),"create ambiguous candidate");auto before=Dump();check(!AutoFindProfile(b.equip)&&Dump()==before,"automatic matching refuses multiple complete candidates");fs::remove(s.directory/L"Templates/重复模板.wdp");check(AutoFindProfile(b.equip),"unique complete geometry match can auto import");
 check(!ApplyProfile(b.equip,"../bad.wdp")&&!ExportProfile(a.equip,"CON"),"reject traversal and Windows reserved names");
 for(int i=0;i<9;i++){Find(gid)->name="模板版本"+std::to_string(i);check(ExportProfile(a.equip,"共享模板"),"explicit template update succeeds");check(ApplyProfile(b.equip,"共享模板.wdp"),"repeated import succeeds");}
 check(ProfileBackups(b.equip).size()==5,"equipment backup history capped at five distinct revisions");auto names=ProfileBackups(b.equip);check(RestoreProfileBackup(b.equip,names.back()),"restore real equipment WDP backup");check(ProfileBackups(b.equip).size()<=5,"restore also respects history limit");
 auto conflict=Read(canonical);Write(canonical,conflict+"\n");Find(gid)->name="未保存编辑";Changed();check(!Save()&&Read(canonical)==conflict+"\n","external write conflict cannot be overwritten");Write(canonical,conflict);check(LoadEquipmentProfile(a.equip),"explicit reload resolves external conflict");
 auto mismatch=Read(canonical);s.models[0].meshes[0].vertexCount++;check(!LoadEquipmentProfile(a.equip)&&s.blockedScopes.count(a.equip),"same filename with changed mesh structure is blocked");check(Save()&&Read(canonical)==mismatch,"blocked old preset remains byte intact");s.models[0].meshes[0].vertexCount--;check(LoadEquipmentProfile(a.equip),"matching model can load again");
 s.models[0].meshes[0].blockType^=0x12345678;s.models[0].meshes[0].raw[72]^=77;s.models[0].signature=MeshOrderSignature(s.models[0]);check(LoadEquipmentProfile(a.equip)&&MeshView(s.models[0],1),"runtime layout handles do not cause false model mismatch or lose notes");
 s.models[0].meshes[0].index=100;s.models[0].signature++;check(!LoadEquipmentProfile(a.equip),"reexported indices require explicit remapping");check(RebindEquipmentProfile(a.equip)&&Hidden(a.relativePath,100,a.materials[0].hash),"existing local file can explicitly remap after reexport without a template");
 auto badBefore=Dump();Write(s.configPath,"WorldDiffSettings 117\nSET broken\n");check(!Load()&&Dump()==badBefore,"corrupt settings cannot partially replace equipment state");
 Reset(root/L"migration");s.models={a,b};gid=Configure(a);s.data.scale=1.2f;s.data.hideDragHint=true;check(ExportProfile(a.equip,"旧模板"),"prepare portable legacy fixture");fs::create_directories(s.directory/L"Presets");fs::rename(s.directory/L"Templates/旧模板.wdp",s.directory/L"Presets/旧模板.wdp");s.data.bindings[a.equip]="旧模板.wdp";std::ostringstream legacy;WriteDocument(legacy,s.data);auto old=legacy.str();old.replace(0,std::string("WorldDiffInternal 122").size(),"WorldDiffInternal 115");Write(s.directory/L"WorldDiff115.cfg",old);Write(s.directory/L"Backups/old.cfg",old);auto oldTemplate=Read(s.directory/L"Presets/旧模板.wdp");
 check(MigrateStorage(),"legacy configuration migrates with final settings commit");check(s.data.groups.empty()&&s.data.transforms.empty()&&s.data.meshDisplay.empty(),"historical equipment not retained as active documents");
 check(fs::exists(s.directory/L"Presets/pl105_0000.wdp")&&Read(s.directory/L"Templates/旧模板.wdp")==oldTemplate,"canonical and independent unchanged template produced");
 check(!fs::exists(s.directory/L"WorldDiff115.cfg")&&!fs::exists(s.directory/L"Presets/旧模板.wdp")&&!fs::exists(s.directory/L"Backups/old.cfg"),"legacy clutter archived only after migration succeeds");
 auto archive=*fs::directory_iterator(s.directory/L"Archive");check(Read(archive.path()/L"WorldDiff115.cfg")==old&&Read(archive.path()/L"Backups/old.cfg")==old,"original configuration and historical backup preserved byte for byte");
 check(LoadSettings()&&s.data.scale==1.2f&&s.data.hideDragHint,"personal global settings preserved");check(LoadEquipmentProfile(a.equip)&&Find(gid)&&s.data.transforms.size()==1&&s.data.meshDisplay.size()==3,"groups transformations notes and order survive migration");
 Reset(root/L"changedmigration");s.models={a};Configure(a);check(ExportProfile(a.equip,"旧模型"),"prepare older model metadata");fs::create_directories(s.directory/L"Presets");fs::rename(s.directory/L"Templates/旧模型.wdp",s.directory/L"Presets/旧模型.wdp");s.data.bindings[a.equip]="旧模型.wdp";std::ostringstream changedLegacy;WriteDocument(changedLegacy,s.data);Write(s.directory/L"WorldDiff115.cfg",changedLegacy.str());s.models[0].meshes[0].vertexCount++;
 check(MigrateStorage()&&!LoadEquipmentProfile(a.equip)&&s.blockedScopes.count(a.equip),"migration cannot stamp an unrelated replacement model as compatible");
 Reset(root/L"badmigration");Write(s.directory/L"WorldDiff115.cfg","corrupt");check(!MigrateStorage()&&!fs::exists(s.configPath)&&Read(s.directory/L"WorldDiff115.cfg")=="corrupt","invalid migration cannot create settings or erase source");
 Reset(root/L"live");std::array<Model,5> ma={MakeModel(a.equip,"helm"),MakeModel(a.equip,"body"),MakeModel(a.equip,"arm"),MakeModel(a.equip,"wst"),MakeModel(a.equip,"leg")};LiveFixture live(ma);auto inspected=InspectPlayerOutfit(live.snapshot.entity,Memory);
 check(inspected.ready&&inspected.paths==live.snapshot.paths&&inspected.contexts==live.snapshot.contexts,"reader identifies all five local-player model contexts");check(!InspectPlayerOutfit(0,Memory).ready&&!InspectPlayerOutfit(0x10,Memory).ready,"reader rejects absent or invalid player");
 Model runtime;check(ReadRuntimeModel(live.snapshot.resources[2],ma[2].relativePath,runtime)&&runtime.meshes.size()==3&&runtime.meshes[1].vertexCount==101&&runtime.signature==ma[2].signature,"runtime reader obtains complete tables independent of visibility queries");
 check(!ReadRuntimeModel(live.snapshot.resources[2],b.relativePath,runtime),"runtime resource path must match the captured player slot");
 s.trackingPlayer=true;Follow(inspected);check(s.outfit.ready&&s.loadedScopes==std::set<std::string>{a.equip}&&s.data.groups.size()==1,"first appearance creates only transient main list");check(!fs::exists(s.directory/L"Presets/pl105_0000.wdp"),"viewing an unconfigured appearance creates no WDP");check(IsLocalModelContext(inspected.contexts[2])&&!IsLocalModelContext(0x123456),"visibility context gate excludes other actors");
 gid=Configure(s.models[1]);check(Save(),"current outfit definitions save");check(!CreateGroup("历史装备",b.equip,true,{{b.relativePath,b.materials[0].hash,false}},preset),"inactive equipment cannot be edited");
 std::array<Model,5> mb={MakeModel(b.equip,"helm"),MakeModel(b.equip,"body"),MakeModel(b.equip,"arm"),MakeModel(b.equip,"wst"),MakeModel(b.equip,"leg")};LiveFixture bath(mb);auto bathSnapshot=InspectPlayerOutfit(bath.snapshot.entity,Memory);
 auto liveFile=s.directory/L"Presets/pl105_0000.wdp";auto beforeFailure=Read(liveFile);Find(gid)->name="换装前尚未保存的编辑";Changed();fs::create_directory(s.directory/L"Presets/pl105_0000.wdp.tmp");Follow(bathSnapshot);
 check(!s.outfit.ready&&Find(gid)&&Read(liveFile)==beforeFailure,"failed old-outfit save pauses the switch and preserves edits plus disk content");check(!IsLocalModelContext(inspected.contexts[2]),"failed save cannot apply old rules to a new model");fs::remove(s.directory/L"Presets/pl105_0000.wdp.tmp");Follow(bathSnapshot);check(Read(liveFile).find("换装前尚未保存的编辑")!=std::string::npos,"retry flushes old edits before switching equipment");
 check(s.loadedScopes==std::set<std::string>{b.equip}&&s.data.groups.size()==1&&!Find(gid),"switch unloads old equipment documents and follows current actor");check(!IsCurrentModel(a.relativePath)&&IsCurrentModel(b.relativePath),"historical installed models cannot appear in current UI");
 check(!fs::exists(s.directory/L"Presets/pl106_0000.wdp"),"switching to no preset does not create a file");
 Follow(inspected);check(s.loadedScopes==std::set<std::string>{a.equip}&&Find(gid)&&s.data.transforms.size()==1,"switch back reloads prior equipment from its WDP");
 auto beforeSceneSwitch=Read(liveFile);auto preview=inspected;preview.savePreview=true;Follow(preview);check(s.outfit.ready&&s.outfit.savePreview&&Find(gid),"save preview activates the same equipment profile without changing its identity");
 FollowPlayerOutfit({});check(!CanEditScope(a.equip)&&!IsLocalModelContext(preview.contexts[2]),"leaving save selection immediately clears preview editing and visibility");
 Follow(inspected);check(s.outfit.ready&&!s.outfit.savePreview&&Read(liveFile)==beforeSceneSwitch,"gameplay retakes even identical cached contexts without rewriting the WDP");
 auto mixed=inspected;mixed.paths[0]=bathSnapshot.paths[0];mixed.resources[0]=bathSnapshot.resources[0];mixed.contexts[0]=bathSnapshot.contexts[0];Follow(mixed);check(s.loadedScopes.size()==2&&IsCurrentModel(mixed.paths[0])&&!IsCurrentModel(inspected.paths[0]),"mixed appearance includes only actually worn parts and scopes");
 FollowPlayerOutfit({});check(!CanEditScope(a.equip)&&!IsLocalModelContext(inspected.contexts[2]),"missing player disables editing and local visibility overrides");
 if(argc>1){Reset(root/L"actual");auto input=PathUtf8(argv[1]);fs::copy(input,s.directory,fs::copy_options::recursive|fs::copy_options::overwrite_existing);s.models=LoadCatalog(argc>2?PathUtf8(argv[2]):fs::path{});auto actual=Read(s.directory/L"WorldDiff115.cfg");Document oldDocument;std::istringstream original(actual);check(ReadDocument(original,oldDocument),"real legacy document fixture is valid");check(!actual.empty()&&MigrateStorage(),"real copied game configuration migrates offline");check(fs::exists(s.directory/L"Presets/pl105_0000.wdp")&&fs::exists(s.configPath),"real migration produces equipment file and small settings");check(LoadEquipmentProfile("pl105_0000"),"real migrated WDP validates against installed model tables");
  size_t groups=0,presets=0,views=0,transforms=0;for(auto& g:oldDocument.groups)if(g.scope=="pl105_0000"){groups++;presets+=g.presets.size();}for(auto& [t,v]:oldDocument.meshDisplay)if(Equip(t.model)=="pl105_0000")views++;for(auto& r:oldDocument.transforms)if(r.scope=="pl105_0000")transforms++;size_t actualPresets=0;for(auto& g:s.data.groups)actualPresets+=g.presets.size();check(s.data.groups.size()==groups&&actualPresets==presets&&s.data.meshDisplay.size()==views&&s.data.transforms.size()==transforms,"real groups presets mesh notes order and transform counts all preserved");}
 std::cout<<"PASS "<<checks<<" storage and current-player appearance checks\nArtifacts "<<root.string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
