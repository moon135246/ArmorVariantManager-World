#include "../src/State.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace worlddiff;
using namespace worlddiff::app;
namespace fs=std::filesystem;

static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static Model Part(const std::string& part,const std::vector<std::string>& materials){
 Model m;m.equip="pl105_0000";m.part=part;m.relativePath="pl\\f_equip\\pl105_0000\\"+part+"\\mod\\asset";
 for(auto& name:materials){
  auto hash=MaterialNameHash(name);Mesh mesh;mesh.index=(uint32_t)m.meshes.size();mesh.materialId=(uint16_t)mesh.index;
  mesh.vertexCount=uint16_t(10+hash%5000);mesh.indexCount=mesh.vertexCount*3;mesh.lod=65535;
  m.materials.push_back({name,hash});m.meshes.push_back(mesh);
 }
 m.signature=MeshOrderSignature(m);return m;
}
int main(int argc,char** argv){try{
 auto& s=Get();auto root=fs::temp_directory_path()/("WorldDiffVariant_"+std::to_string(GetTickCount64()));
 s.directory=root;s.configPath=root/L"Settings.cfg";InitializeProfiles();
 auto body=Part("body",{"shared","moved","exclusive","a","b","c","d","e"}),legs=Part("leg",{"boots"});
 auto alternateBody=Part("body",{"shared","a","b","c","d","e"}),alternateLegs=Part("leg",{"moved","boots"});
 // Keep the template family overlap above the automatic-selection reference.
 for(int i=0;i<20;i++){
  auto extra=Part("body",{"anchor"+std::to_string(i)});
  for(auto* m:{&body,&alternateBody}){
   auto mesh=extra.meshes.front();mesh.index=(uint32_t)m->meshes.size();mesh.materialId=(uint16_t)m->materials.size();
   m->materials.push_back(extra.materials.front());m->meshes.push_back(mesh);
  }
 }
 alternateLegs.meshes[0].vertexCount+=7;alternateLegs.meshes[0].indexCount+=21;
 const auto scope=body.equip;s.models={body,legs};
 auto& main=Main(scope);SetVisible(main,{body.relativePath,0,true},true);SetVisible(main,{body.relativePath,1,true},false);SavePreset(main,"default");
 uint64_t id=0;Check(CreateGroup("optional",scope,false,{{body.relativePath,body.materials[2].hash,false}},id),"create optional group");
 auto* group=Find(id);SetVisible(*group,{body.relativePath,2,true},false);SavePreset(*group,"hide");
 auto hotkey=CreateHotkey(scope,"combo");SetHotkeyTarget(hotkey,0,{id,group->presets[0].id});SetMeshNote(body,2,"optional note");
 Check(Save()&&ExportProfile(scope,"shared"),"initial save");
 s.models={alternateBody,alternateLegs};Check(AutoFindProfile(scope),"recognize moved mesh with other source mesh absent");
 Check(Main(scope).presets.front().rules.hidden.count({legs.relativePath,0,true}),"moved rule follows body to leg");
 Check(!s.data.inactiveMeshes.empty(),"missing rules parked");
 Check(!Hidden(body.relativePath,2,alternateBody.materials[2].hash),"old index cannot hide its replacement");
 Main(scope).presets.front().name="edited";Check(Save()&&ExportProfile(scope,"both"),"save and export alternate");
 Check(LoadEquipmentProfile(scope)&&!s.data.inactiveMeshes.empty(),"inactive bindings reload from disk");
 body.meshes[2].vertexCount+=3;body.meshes[2].indexCount+=9;
 s.models={body,legs};Check(LoadEquipmentProfile(scope),"automatic reverse load after absent mesh geometry edit");
 Check(s.data.inactiveMeshes.empty()&&Main(scope).presets.front().name=="edited","reverse load recovers bindings and keeps edits");
 Check(Hidden(body.relativePath,2,body.materials[2].hash)&&MeshView(s.models[0],2)&&MeshView(s.models[0],2)->note=="optional note","optional visibility and note restored");
 Check(s.data.hotkeys.size()==1&&ValidateHotkey(s.data.hotkeys.front(),s.data,true),"scheme retained");
 Check(s.data.hotkeys.front().targets.size()==1&&Find(s.data.hotkeys.front().targets[0].group)->name=="optional","scheme still targets restored group");
 Check(ApplyProfile(scope,"both.wdp")&&s.data.inactiveMeshes.empty()&&Hidden(body.relativePath,2,body.materials[2].hash),"exported alternate template keeps inactive bindings");
 fs::remove(s.directory/L"Templates/both.wdp");
 s.models={alternateBody,alternateLegs,Part("wst",{"moved"})};
 Check(AutoFindProfile(scope)&&!s.data.inactiveMeshes.empty(),"family recognition can retain unresolved mesh rules");
 Check(ApplyProfile(scope,"shared.wdp")&&!s.data.inactiveMeshes.empty(),"manual partial import parks ambiguous targets");
 Check(!Main(scope).presets.front().rules.hidden.count({legs.relativePath,0,true}),"no arbitrary cross-part target chosen");

 {
  auto previousDirectory=s.directory;auto previousModels=s.models;
  s.directory=root/L"duplicates";s.configPath=s.directory/L"Settings.cfg";s.data={};s.profileSnapshots.clear();s.loadedFingerprints.clear();InitializeProfiles();
  auto source=Part("body",{"fabric","fabric","anchor"});
  source.meshes[1].vertexCount+=5;source.meshes[1].indexCount+=15;
  s.models={source};auto& g=Main(scope);
  SetVisible(g,{source.relativePath,0,true},false);SavePreset(g,"one mesh");
  Check(ExportProfile(scope,"mesh-only"),"single-mesh rule template");
  uint64_t materialGroup=0;Check(CreateGroup("whole material",scope,true,{{source.relativePath,source.materials[0].hash,false}},materialGroup),"duplicate material group");
  SetVisible(*Find(materialGroup),{source.relativePath,source.materials[0].hash,false},false);SavePreset(*Find(materialGroup),"all fabric");
  Check(ExportProfile(scope,"duplicate"),"duplicate source export");
  auto reordered=source;std::swap(reordered.meshes[0].vertexCount,reordered.meshes[1].vertexCount);std::swap(reordered.meshes[0].indexCount,reordered.meshes[1].indexCount);
  s.models={reordered};Check(ApplyProfile(scope,"duplicate.wdp"),"duplicate import");
  Check(Main(scope).presets[0].rules.hidden.count({source.relativePath,1,true}),"same-material meshes follow unique geometry");
  reordered.meshes[0].vertexCount=300;reordered.meshes[1].vertexCount=300;
  reordered.meshes[0].indexCount=900;reordered.meshes[1].indexCount=900;
  auto extra=reordered.meshes[0];extra.index=3;reordered.meshes.push_back(extra);
  s.models={reordered};Check(ApplyProfile(scope,"duplicate.wdp"),"ambiguous mesh plus whole-material import");
  Check(!s.data.inactiveMeshes.empty(),"specific ambiguous mesh kept dormant");
  auto wholeGroup=std::find_if(s.data.groups.begin(),s.data.groups.end(),[](auto& x){return x.name=="whole material";});
  Check(wholeGroup!=s.data.groups.end()&&wholeGroup->presets[0].rules.hidden.count({source.relativePath,source.materials[0].hash,false}),"whole material includes new same-material meshes");
  auto summaries=FindProfiles(scope);auto summary=std::find_if(summaries.begin(),summaries.end(),[](auto& f){return f.file=="duplicate.wdp";});Check(summary!=summaries.end()&&summary->materialCoverage==1&&summary->resolved<summary->controlled,"material similarity never implies every control resolved");
  Check(ApplyProfile(scope,"mesh-only.wdp")&&!s.data.inactiveMeshes.empty(),"manual import keeps even an entirely unresolved rule set");
  Check(Main(scope).presets[0].rules.hidden.size()==1,"all-pending preset retains its rule");
  s.directory=previousDirectory;s.models=previousModels;
 }
 if(argc==3){
  s.directory=root/L"Mei";s.configPath=s.directory/L"Settings.cfg";s.data={};s.profileSnapshots.clear();s.loadedFingerprints.clear();InitializeProfiles();
  s.models=LoadCatalog(PathUtf8(argv[1]));fs::copy_file(PathUtf8(argv[2]),s.directory/L"Templates/Mei_梅.wdp");
  auto files=FindProfiles(scope);Check(files.size()==1&&files[0].strong&&files[0].coverage==1,"Mei live-model fixture fully matched");
  Check(files[0].hits==58&&files[0].total==59&&files[0].currentMaterials==58&&files[0].controlled==93&&files[0].resolved==93&&files[0].missingMaterials==1&&files[0].addedMaterials==0,"Mei name and control metrics include unused missing iris correctly");
  std::cout<<"Mei materials "<<files[0].materialCoverage*100<<"%, controls "<<files[0].resolved<<"/"<<files[0].controlled<<"\n";
  Check(AutoFindProfile(scope)&&s.data.groups.size()==8&&s.data.inactiveMeshes.empty(),"Mei version 115 imports all eight groups");
  size_t presets=0;for(auto& g:s.data.groups)presets+=g.presets.size();Check(presets==18,"all 18 Mei presets");
  auto head=std::find_if(s.models.begin(),s.models.end(),[](auto& m){return m.part=="helm";});
  Check(head!=s.models.end()&&Main(scope).presets.front().rules.shown.count({Normalize(head->relativePath),0,true}),"Mei facial rule reached current head mesh");
  // Reconstruct the no-expression mesh tables from the original 115 template
  // to exercise a return to the old indices without replacing any game asset.
  std::ifstream input(PathUtf8(argv[2]));std::string line;
  std::map<uint32_t,std::string> names;std::map<std::string,Model> oldParts;
  while(std::getline(input,line)&&line!="DOCUMENT"){
   std::istringstream row(line);std::string tag,path,name;row>>tag;
   if(tag=="MAT"){uint32_t hash;row>>std::quoted(path)>>hash>>std::quoted(name);names[hash]=name;}
   if(tag=="MESH"){
    uint32_t hash,ordinal;Mesh mesh;row>>std::quoted(path)>>mesh.index>>hash>>ordinal>>mesh.vertexCount>>mesh.indexCount>>mesh.lod;
    Model& m=oldParts[path];m.equip=scope;m.relativePath=path;m.part=ProfilePart(path);mesh.materialId=(uint16_t)m.materials.size();
    m.materials.push_back({names.at(hash),hash});m.meshes.push_back(mesh);
   }
  }
  Document original;Check(ReadDocument(input,original),"read unmodified Mei source document");
  auto expression=s.models;s.models.clear();for(auto& [path,m]:oldParts){m.signature=MeshOrderSignature(m);s.models.push_back(m);}
  Check(LoadEquipmentProfile(scope),"automatic reverse to reconstructed no-expression model tables");
  for(auto& old:original.groups){
   auto g=std::find_if(s.data.groups.begin(),s.data.groups.end(),[&](auto& group){return group.name==old.name;});
   Check(g!=s.data.groups.end()&&g->members==old.members&&g->presets.size()==old.presets.size(),"Mei group membership round trip");
   for(size_t i=0;i<old.presets.size();i++)Check(g->presets[i].rules.hidden==old.presets[i].rules.hidden&&g->presets[i].rules.shown==old.presets[i].rules.shown,"every stored Mei preset rule round trips to original mesh");
  }
  s.models=expression;Check(LoadEquipmentProfile(scope),"Mei switches forward again from canonical file");
 }
 std::cout<<"PASS variant forward, reverse, dormant export/reload, ambiguity and supplied model fixture\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
