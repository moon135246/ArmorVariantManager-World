#include "../src/State.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace worlddiff;using namespace worlddiff::app;
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool okay,const char* reason){checks++;if(!okay)throw std::runtime_error(reason);};auto& s=Get();s.directory=std::filesystem::temp_directory_path()/(L"WorldDiff104_"+std::to_wstring(GetTickCount64()));s.configPath=s.directory/L"测试配置.cfg";std::filesystem::create_directories(s.directory);
 s.models=LoadCatalog(argc>1?PathUtf8(argv[1]):std::filesystem::path(L"F:\\SteamLibrary\\steamapps\\common\\Monster Hunter World"));for(auto& m:s.models){m.relativePath=Normalize(m.relativePath);s.lastSeen[m.relativePath]=GetTickCount();}
 std::set<std::string> sourceNames,targetNames;std::string sourceBody,targetBody;for(auto& m:s.models)if(m.equip=="pl105_0000"||m.equip=="pl106_0000"){for(auto& mesh:m.meshes)if(mesh.materialId<m.materials.size())(m.equip=="pl105_0000"?sourceNames:targetNames).insert(m.materials[mesh.materialId].name);if(m.part=="body")(m.equip=="pl105_0000"?sourceBody:targetBody)=m.relativePath;}
 check(!sourceNames.empty()&&sourceNames==targetNames,"actual pl105 and pl106 material signatures match");
 std::ifstream legacy(s.directory.parent_path()/L"unused");
 auto oldFile=argc>2?PathUtf8(argv[2]):PathUtf8("../差分管理器_v1.0.4(世界内置版)/验证记录/升级前备份/WorldDiff103.cfg");std::ifstream old(oldFile,std::ios::binary);Document migrated;check(ReadDocument(old,migrated),"read immutable 1.0.3 Unicode migration fixture");s.data=migrated;
 auto originalCount=s.data.groups.size();check(originalCount>=7,"legacy grouped preset collection retained");
 TransformRule transform;transform.id=s.data.nextId++;transform.name="跨装备变身测试";transform.scope="pl105_0000";transform.condition.kind=ConditionKind::Health;transform.condition.healthMax=30;transform.condition.latch=true;for(auto& g:s.data.groups)if(g.scope==transform.scope&&!g.presets.empty()&&transform.targets.size()<2)transform.targets.push_back({g.id,g.presets.front().id});check(transform.targets.size()==2,"fixture has two preset targets");s.data.transforms.push_back(transform);auto& mode=EnsureTransformMode(s.data,"pl105_0000");mode.enabled=true;mode.parallel=true;mode.categories[4]=true;mode.priorities[4]=2;
 check(ExportProfile("pl105_0000","黎明の巫女"),"export portable Unicode filename");auto files=FindProfiles("pl106_0000");check(files.size()==1&&files[0].hits==sourceNames.size(),"grouped preset matches all five target parts");check(ApplyProfile("pl106_0000","黎明の巫女.wdp"),"manually import historical MOD template on different equipment");
 check(s.data.transforms.size()==2,"portable profile clones transform rule");check(s.data.transformModes.count("pl106_0000")&&s.data.transformModes.at("pl106_0000").parallel&&s.data.transformModes.at("pl106_0000").priorities[4]==2,"portable mode and category priority remapped to target equipment");auto& cloned=s.data.transforms.back();check(cloned.id!=transform.id&&cloned.scope=="pl106_0000"&&cloned.condition.latch&&cloned.targets.size()==2,"rule identity scope and condition remapped");for(auto& t:cloned.targets){auto* g=Find(t.group);check(g&&g->scope=="pl106_0000","transform target group remapped to target armor");check(g&&std::any_of(g->presets.begin(),g->presets.end(),[&](auto& p){return p.id==t.preset;}),"transform preset ID remapped");}
 size_t groups=0,targets=0;for(auto& g:s.data.groups)if(g.scope=="pl106_0000"){groups++;for(auto& t:g.members){targets++;check(Equip(t.model)=="pl106_0000","membership remapped away from pl105");}for(auto& p:g.presets)for(auto* set:{&p.rules.hidden,&p.rules.shown})for(auto& t:*set)check(Equip(t.model)=="pl106_0000","material and mesh rules remapped away from pl105");}
 check(groups>=7&&targets>=20,"all ordinary groups and presets cloned");
 auto compare=[&](const Model& a,const Model& b){for(auto& mesh:a.meshes){if(mesh.materialId>=a.materials.size())continue;uint32_t hash=a.materials[mesh.materialId].hash;auto other=std::find_if(b.meshes.begin(),b.meshes.end(),[&](auto& x){return x.materialId<b.materials.size()&&b.materials[x.materialId].hash==hash;});if(other!=b.meshes.end())check(Hidden(a.relativePath,mesh.index,hash)==Hidden(b.relativePath,other->index,hash),"default visibility preserved on target armor");}};
 for(auto& a:s.models)if(a.equip=="pl105_0000")for(auto& b:s.models)if(b.equip=="pl106_0000"&&a.part==b.part)compare(a,b);
 Group* edit=nullptr;for(auto& g:s.data.groups)if(g.scope=="pl106_0000"&&!g.main){edit=&g;break;}check(edit!=nullptr,"find remapped group");edit->name="跨装备编辑中文测试";Changed();SaveProfiles();std::ifstream shared(s.directory/L"Presets"/L"pl106_0000.wdp",std::ios::binary);std::string saved((std::istreambuf_iterator<char>(shared)),{});check(saved.find("跨装备编辑中文测试")!=std::string::npos,"target edit persists in its independent equipment file");
 shared.close();
 // A global preference must not rewrite the immutable source template.
 auto sharedPath=s.directory/L"Templates"/L"黎明の巫女.wdp";
 auto readShared=[&](){std::ifstream input(sharedPath,std::ios::binary);return std::string((std::istreambuf_iterator<char>(input)),{});};
 auto fingerprint=[](const std::string& text){uint64_t hash=1469598103934665603ull;for(unsigned char ch:text)hash=(hash^ch)*1099511628211ull;return hash;};
 const auto beforeToggle=readShared();const auto beforeFingerprint=fingerprint(beforeToggle);const auto beforeWriteTime=std::filesystem::last_write_time(sharedPath);
 check(s.data.bindings["pl105_0000"]!=s.data.bindings["pl106_0000"],"template import gives equipment independent bindings");
 s.data.transformsEnabled=false;SaveProfiles();check(readShared()==beforeToggle&&fingerprint(readShared())==beforeFingerprint,"global transform pause cannot overwrite shared WDP content");check(std::filesystem::last_write_time(sharedPath)==beforeWriteTime,"global transform pause does not rewrite shared WDP");
 s.data.transformsEnabled=true;SaveProfiles();check(readShared()==beforeToggle&&fingerprint(readShared())==beforeFingerprint,"global transform resume cannot overwrite shared WDP content");check(std::filesystem::last_write_time(sharedPath)==beforeWriteTime,"global transform resume does not rewrite shared WDP");
 check(ExportProfile("pl106_0000","pl106_0000"),"explicitly export equipment-named template");files=FindProfiles("pl106_0000");check(std::any_of(files.begin(),files.end(),[](auto& f){return f.file=="pl106_0000.wdp"&&f.filenameMatch;}),"explicit equipment-named template can be found separately");
 auto before=s.data.groups.size();check(!ApplyProfile("pl106_0000","../bad.wdp")&&s.data.groups.size()==before,"reject path traversal without changing active groups");
 check(Save(),"save independent equipment documents");check(LoadSettings()&&std::filesystem::exists(s.directory/L"Presets/pl106_0000.wdp"),"small settings and canonical equipment file persist separately");
 // Simulated new export: swap vertex-independent mesh indices while retaining materials.
 for(auto& m:s.models)if(m.equip=="pl106_0000")for(auto& mesh:m.meshes)mesh.index=100+mesh.index;
 check(ApplyProfile("pl106_0000","黎明の巫女.wdp"),"rebind reordered submesh indices by material and geometry");bool reordered=true;for(auto& g:s.data.groups)if(g.scope=="pl106_0000")for(auto& p:g.presets)for(auto* set:{&p.rules.hidden,&p.rules.shown})for(auto& t:*set)if(t.mesh&&t.value<100)reordered=false;check(reordered,"no old mesh indices leak into remapped rules");
 // Newly added families must also travel with a MOD-named profile; armor IDs
 // and preset IDs are rebound independently of weapon condition values.
 auto targetGroup=std::find_if(s.data.groups.begin(),s.data.groups.end(),[](auto& g){return g.scope=="pl106_0000"&&!g.presets.empty();});check(targetGroup!=s.data.groups.end(),"find portable-condition target group");
 for(auto kind:{ConditionKind::InsectGlaive,ConditionKind::BowCharge,ConditionKind::HammerCharge,ConditionKind::ChargeBlade,ConditionKind::SwitchAxe}){
  TransformRule added;added.id=s.data.nextId++;added.name=ConditionName(kind);added.scope="pl106_0000";added.condition.kind=kind;added.condition.state=kind==ConditionKind::InsectGlaive?2:kind==ConditionKind::BowCharge?4:kind==ConditionKind::ChargeBlade?6:3;added.targets={{targetGroup->id,targetGroup->presets[0].id}};s.data.transforms.push_back(added);
 }
 auto& portableMode=EnsureTransformMode(s.data,"pl106_0000");portableMode.parallel=true;portableMode.categories.fill(true);portableMode.priorities[8]=17;
 check(ExportProfile("pl106_0000","九类状态测试")&&ApplyProfile("pl105_0000","九类状态测试.wdp"),"new count/charge/form families export and rebind across armor");
 for(auto kind:{ConditionKind::InsectGlaive,ConditionKind::BowCharge,ConditionKind::HammerCharge,ConditionKind::ChargeBlade,ConditionKind::SwitchAxe}){
  auto rule=std::find_if(s.data.transforms.begin(),s.data.transforms.end(),[&](auto& r){return r.scope=="pl105_0000"&&r.condition.kind==kind;});int expected=kind==ConditionKind::InsectGlaive?2:kind==ConditionKind::BowCharge?4:kind==ConditionKind::ChargeBlade?6:3;check(rule!=s.data.transforms.end()&&rule->condition.state==expected&&rule->targets.size()==1&&Find(rule->targets[0].group)->scope=="pl105_0000"&&ValidateTransform(*rule,s.data),"enhanced weapon state and binding survive profile remapping");
 }
 check(s.data.transformModes.at("pl105_0000").categories[8]&&s.data.transformModes.at("pl105_0000").priorities[8]==17,"ninth parallel category survives portable profile remapping");
 auto removed=std::find_if(s.data.transforms.begin(),s.data.transforms.end(),[](auto& r){return r.scope=="pl105_0000"&&r.condition.kind==ConditionKind::Health;});check(removed!=s.data.transforms.end(),"find imported health node for deletion");auto deletedMax=removed->condition.healthMax;check(DeleteHealthNode(removed->id,"pl105_0000"),"delete imported health node");auto emptyNode=AddHealthNode("pl105_0000");check(emptyNode!=0,"create independent empty health node");
 check(ExportProfile("pl105_0000","血量节点删除测试")&&ApplyProfile("pl106_0000","血量节点删除测试.wdp"),"save node deletion and rebind updated WDP across armor");
 std::vector<TransformRule> healthNodes;for(auto& r:s.data.transforms)if(r.scope=="pl106_0000"&&r.condition.kind==ConditionKind::Health)healthNodes.push_back(r);check(healthNodes.size()==1&&healthNodes[0].targets.empty()&&healthNodes[0].condition.healthMax!=deletedMax,"deleted node does not reappear and empty node survives portable reload");
 std::cout<<"PASS "<<checks<<" checks. actual source/target materials="<<sourceNames.size()<<" groups="<<groups<<"\n";std::cout<<"Artifacts "<<s.directory.string()<<"\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
