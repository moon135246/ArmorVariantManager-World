#include "../src/Multiplayer.h"
#include <fstream>
#include <iostream>
#include <functional>
#include <thread>
#include <cstring>
#include <stdexcept>
using namespace worlddiff;using namespace worlddiff::app;
namespace {
constexpr uintptr_t base=0x1000000000ull;
std::map<uintptr_t,uintptr_t> globals;std::function<void(uintptr_t)> during;
bool Memory(uintptr_t a,void* b,size_t n){if(during)during(a);if(auto it=globals.find(a);it!=globals.end()&&n==8){memcpy(b,&it->second,n);return true;}SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(void*)a,b,n,&got)&&got==n;}
template<class T>void Put(std::vector<uint8_t>& b,size_t offset,T value){memcpy(b.data()+offset,&value,sizeof(value));}
struct Avatar {
 std::vector<uint8_t> entity=std::vector<uint8_t>(0xD000),armor=std::vector<uint8_t>(0x380),resource=std::vector<uint8_t>(0x500),vital=std::vector<uint8_t>(0x100),action=std::vector<uint8_t>(0xEA00);
 std::string path="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";
 Avatar(){Put(entity,0,base+0x34A6D18);Put(entity,0xC928,(uintptr_t)armor.data());Put(armor,0,(uintptr_t)entity.data());Put(entity,0x2A0,(uintptr_t)resource.data());memcpy(resource.data()+0xC,path.c_str(),path.size()+1);Put(entity,0x7630,(uintptr_t)vital.data());Put(entity,0x468,(uintptr_t)action.data());HP(100);Put(vital,0x60,100.f);Motion(0);}
 uintptr_t Actor()const{return (uintptr_t)entity.data();}
 void HP(float value){Put(vital,0x64,value);}void Motion(int value){Put(action,0xE9C4,value);}
 OutfitSnapshot Outfit(){return InspectPlayerOutfit(Actor(),Memory);}
};
struct Roster {
 std::vector<uint8_t> manager=std::vector<uint8_t>(0x100),container=std::vector<uint8_t>(0x7500);
 std::array<Avatar,16> avatars;
 Roster(){globals.clear();during={};globals[base+0x50139A0]=(uintptr_t)manager.data();globals[base+0x500ECA0]=(uintptr_t)container.data();Put(manager,0x50,avatars[3].Actor());Put(container,0,base+0x33FE5B0);for(size_t i=0;i<16;i++){Put(container,0x40+i*0x740,base+0x33FE588);Put(container,0x48+i*0x740,uintptr_t{100}+i);Set(i,true);}}
 void Set(size_t slot,bool active){Put(container,0x58+slot*0x740,active?avatars[slot].Actor():uintptr_t{0});}
 std::vector<PlayerInstance> Read(){return InspectPlayers(base,avatars[3].Outfit(),Memory);}
};
std::shared_ptr<const Model> TestModel(const std::string& scope="pl105_0000",const std::string& part="body"){
 auto m=std::make_shared<Model>();m->equip=scope;m->part=part;m->relativePath="pl\\f_equip\\"+scope+"\\"+part+"\\mod\\f_"+part+scope.substr(2);
 m->materials={{"cloth",MaterialNameHash("cloth")},{"skin",MaterialNameHash("skin")}};
 for(uint16_t i=0;i<2;i++){Mesh mesh;mesh.index=i;mesh.materialId=i;mesh.vertexCount=10;mesh.indexCount=30;m->meshes.push_back(mesh);}return m;
}
Document HealthDocument(const Model& model,bool latch=false){
 Document d;Group g;g.id=1;g.main=true;g.scope=model.equip;g.name="main";Preset p;p.id=2;p.name="low";p.rules.hidden.insert({model.relativePath,0,true});g.presets.push_back(p);d.groups.push_back(g);
 TransformRule r;r.id=3;r.scope=model.equip;r.name="low hp";r.condition.kind=ConditionKind::Health;r.condition.healthMax=50;r.condition.latch=latch;r.targets.push_back({1,2});d.transforms.push_back(r);return d;
}
ActorFrame Frame(uintptr_t actor,uintptr_t ctx,bool local,std::shared_ptr<const Model> model){
 ActorFrame f;f.instance.local=local;auto& o=f.instance.outfit;o.ready=true;o.entity=actor;o.paths[1]=model->relativePath;o.contexts[1]=ctx;o.resources[1]=10000;f.instance.record=actor+10;f.instance.token=actor+20;f.instance.container=20000;f.models[1]={model,30000,40000};f.player.entity=actor;f.player.healthPercent=90;f.player.health=90;f.player.maxHealth=100;f.player.map=101;f.player.weapon=11;return f;
}
std::string Snapshot(){std::ostringstream text;WriteDocument(text,Get().data);return text.str();}
}
int main(){try{
 unsigned checks=0;auto check=[&](bool good,const char* reason){checks++;if(!good)throw std::runtime_error(reason);};
 Roster roster;auto all=roster.Read();check(all.size()==16,"all 16 occupied player slots are enumerated");check(std::count_if(all.begin(),all.end(),[](auto& p){return p.local;})==1&&all[3].local,"local identity uses explicit actor, not first slot");
 for(size_t i=0;i<16;i++)if(i!=3&&i!=5&&i!=6)roster.Set(i,false);
 auto quest=roster.Read();check(quest.size()==3&&quest[0].local&&quest[1].outfit.entity==roster.avatars[5].Actor(),"quest peers beyond first four slots are found");
 Put(roster.container,0x50+3*0x740,uintptr_t{0xffffffff00000001});check(roster.Read()[0].local,"changing lobby/quest seat fields never changes local identity");
 Put(roster.avatars[5].entity,0,base+0x34A6D20);check(roster.Read().size()==2,"wrong actor class is rejected");Put(roster.avatars[5].entity,0,base+0x34A6D18);
 Put(roster.avatars[5].armor,0,roster.avatars[6].Actor());check(roster.Read().size()==2,"foreign body ownership is rejected");Put(roster.avatars[5].armor,0,roster.avatars[5].Actor());
 roster.Set(3,false);check(roster.Read().empty(),"missing local roster entry never selects a remote fallback");roster.Set(3,true);
 Put(roster.manager,0x50,uintptr_t{0});check(roster.Read().empty(),"no local actor disables peer inspection");Put(roster.manager,0x50,roster.avatars[3].Actor());
 auto preview=roster.avatars[3].Outfit();preview.savePreview=true;check(InspectPlayers(base,preview,Memory).empty(),"save preview never enumerates gameplay peers");
 auto tokenAddress=(uintptr_t)roster.container.data()+0x48+5*0x740;bool changed=false;
 during=[&](uintptr_t a){if(a==(uintptr_t)roster.avatars[5].resource.data()+0xC&&!changed){Put(roster.container,0x48+5*0x740,uintptr_t{999});changed=true;}};
 check(roster.Read().size()==2,"slot identity change during inspection discards that actor");during={};
 during=[&](uintptr_t a){if(a==tokenAddress)globals[base+0x500ECA0]=0;};check(roster.Read().empty(),"container replaced during inspection discards entire roster");during={};globals[base+0x500ECA0]=(uintptr_t)roster.container.data();
 roster.avatars[5].HP(23);roster.avatars[6].HP(81);auto hp5=InspectPlayerSnapshot(base,roster.avatars[5].Actor(),Memory,true),hp6=InspectPlayerSnapshot(base,roster.avatars[6].Actor(),Memory,true);
 check(hp5.healthPercent==23&&hp6.healthPercent==81,"health is read from each actor, independently");
 for(int invalid:{0,65535}){roster.avatars[5].Motion(invalid);check(InspectPlayerSnapshot(base,roster.avatars[5].Actor(),Memory,true).lmt==-1,"remote absent/sentinel motion is unknown");}
 check(InspectPlayerSnapshot(base,roster.avatars[5].Actor(),Memory,false).lmt==65535,"local action semantics stay compatible");roster.avatars[5].Motion(49300);check(InspectPlayerSnapshot(base,roster.avatars[5].Actor(),Memory,true).lmt==49300,"real remote action remains usable");
 roster.avatars[5].HP(-2);check(InspectPlayerSnapshot(base,roster.avatars[5].Actor(),Memory,true).healthPercent<0,"invalid remote health is unknown");
 auto model=TestModel();auto local=Frame(100,1000,true,model),a=Frame(200,2000,false,model),b=Frame(300,3000,false,model);a.player.healthPercent=20;b.player.healthPercent=80;
 Document doc=HealthDocument(*model);PlayerPolicyController controller;TransformRuntime localRuntime;EvaluateTransforms(doc,localRuntime,local.player);uint64_t epoch=1;
 auto build=[&](std::vector<ActorFrame> frames){return controller.Build(frames,local.instance.outfit,doc,localRuntime,{model->equip},epoch,100);};
 auto mask=[&](const std::shared_ptr<const RenderPolicies>& p,uintptr_t ctx,size_t mesh=0){auto* m=p->Find(ctx);return m&&PolicyHides(*m,10000,(uint32_t)mesh,(uint32_t)mesh,2);};
 auto policy=build({a,local,b});check(controller.peers==2&&controller.matched==2,"both same-equipment peers match local base configuration");check(!mask(policy,1000)&&mask(policy,2000)&&!mask(policy,3000),"shared resource produces independent instance visibility from individual HP");
 check(policy->Find(1000)->resource==policy->Find(2000)->resource,"independent masks are proven on a shared resource");
 auto ambiguous=a;ambiguous.instance.outfit.contexts[1]=1000;check(build({ambiguous,local})->models.empty(),"ambiguous cross-actor context cannot apply a peer mask to local avatar");
 std::vector<ActorFrame> full{local};for(uintptr_t i=0;i<15;i++)full.push_back(Frame(10000+i,5000+i,false,model));check(build(full)->models.size()==16&&controller.peers==15,"full 16-player frame maintains distinct instance policies");
 doc.groups[0].working.hidden.insert({model->relativePath,1,true});policy=build({local,a,b});check(mask(policy,1000,1)&&mask(policy,3000,1)&&!mask(policy,2000,1),"manual base selection is shared, active transform overrides only its own actor");
 doc=HealthDocument(*model,true);a.player.healthPercent=20;policy=build({local,a,b});a.player.healthPercent=90;policy=build({local,a,b});check(mask(policy,2000)&&!mask(policy,3000),"latched transform is per actor");
 policy=build({local,b});check(controller.RuntimeCount()==1&&!policy->Find(2000),"departed actor state and policy are evicted");policy=build({local,a,b});check(!mask(policy,2000),"returning actor cannot inherit departed latch");
 a.player.healthPercent=20;build({local,a});a.player.healthPercent=90;a.instance.token++;check(!mask(build({local,a}),2000),"same actor address with new record identity resets latch");
 a.player.healthPercent=20;build({local,a});a.player.healthPercent=90;a.instance.outfit.contexts[1]=2001;check(!mask(build({local,a}),2001),"model instance rebuild resets latch");a.instance.outfit.contexts[1]=2000;
 a.player.healthPercent=20;build({local,a});a.player.healthPercent=90;a.player.map++;check(!mask(build({local,a}),2000),"map transition resets actor latch");
 a.player.healthPercent=20;build({local,a});a.player.healthPercent=90;epoch++;check(!mask(build({local,a}),2000),"explicit transform reset reaches all actors");
 a.player.healthPercent=20;build({local,a});a.player.healthPercent=0;check(!mask(build({local,a}),2000),"death clears latched peer state");a.player.healthPercent=90;
 doc.transformsEnabled=false;check(!mask(build({local,a}),2000),"global transform pause affects peers");doc.enabled=false;check(!mask(build({local,a}),2000),"global visibility pause affects peers");doc=HealthDocument(*model);
 auto other=TestModel("pl106_0000");b=Frame(300,3000,false,other);b.player.healthPercent=20;check(!build({local,b})->Find(3000),"unmatched remote equipment follows original visibility");
 auto profile=std::make_shared<PeerProfile>();profile->matched=true;profile->document=HealthDocument(*other);b.profiles[other->equip]=profile;policy=build({local,a,b});check(mask(policy,3000)&&!mask(policy,1000),"different equipment may reuse numeric group IDs without runtime collision");
 auto mismatch=std::make_shared<Model>(*model);mismatch->meshes[0].vertexCount++;b=Frame(300,3000,false,mismatch);check(!build({local,b})->Find(3000),"same equipment with incompatible geometry cannot inherit local configuration");
 b=Frame(300,3000,false,other);b.profiles[other->equip]=profile;auto arm=TestModel("pl107_0000","arm");auto armProfile=std::make_shared<PeerProfile>();armProfile->matched=true;armProfile->document=HealthDocument(*arm);b.instance.outfit.paths[2]=arm->relativePath;b.instance.outfit.resources[2]=10001;b.instance.outfit.contexts[2]=3001;b.models[2]={arm,30001,40001};b.profiles[arm->equip]=armProfile;b.player.healthPercent=20;policy=build({local,b});check(policy->Find(3000)&&policy->Find(3001)&&policy->Find(3001)->hidden[0],"mixed equipment evaluates separate scoped documents on one actor");
 auto invalidLocal=local.instance.outfit;invalidLocal.ready=false;check(controller.Build({local,b},invalidLocal,doc,localRuntime,{model->equip},epoch,100)->models.empty()&&controller.RuntimeCount()==0,"invalid local scene clears all peer runtime and rendering policies");
 local.instance.outfit.savePreview=true;policy=build({local,b});check(policy->Find(1000)&&!policy->Find(3000),"save selection controls only its explicit avatar");local.instance.outfit.savePreview=false;
 policy=build({local,a});auto* p=policy->Find(2000);check(p&&!PolicyHides(*p,999,0,0,2)&&!PolicyHides(*p,10000,9,0,2)&&!PolicyHides(*p,10000,0,1,2)&&!PolicyHides(*p,10000,0,0,3),"render mask rejects resource, index, material and mesh table mismatch");
 // Read-only profile loading must not import files or change local edits.
 auto temporary=std::filesystem::temp_directory_path()/(L"WorldDiff119-profile-test-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directories(temporary);
 auto& s=Get();s.directory=temporary;s.trackingPlayer=false;s.data=HealthDocument(*other);s.models={*other};s.storageReady=true;
 check(ExportProfile(other->equip),"create canonical profile fixture");auto before=Snapshot();auto scopesBefore=s.loadedScopes;auto fingerprintsBefore=s.loadedFingerprints;bool dirtyBefore=s.dirty;auto filesBefore=std::distance(std::filesystem::recursive_directory_iterator(temporary),std::filesystem::recursive_directory_iterator{});Document loaded;std::string revision;
 check(ReadPeerProfile(other->equip,{other.get()},loaded,revision)&&!revision.empty(),"peer reader accepts exact canonical model stamp");
 check(Snapshot()==before&&s.dirty==dirtyBefore&&s.loadedScopes==scopesBefore&&s.loadedFingerprints==fingerprintsBefore,"peer reading leaves local document and save bookkeeping untouched");
 check(std::distance(std::filesystem::recursive_directory_iterator(temporary),std::filesystem::recursive_directory_iterator{})==filesBefore,"peer reading creates no config or backup files");
 Model different=*other;different.meshes[0].indexCount++;check(!ReadPeerProfile(other->equip,{&different},loaded,revision),"peer reader rejects different geometry");
 check(!ReadPeerProfile("pl108_0000",{other.get()},loaded,revision),"missing peer preset does not create one");
 std::filesystem::create_directories(temporary/L"Templates");std::filesystem::rename(temporary/L"Presets"/PathUtf8(other->equip+".wdp"),temporary/L"Templates"/L"search.wdp");check(!ReadPeerProfile(other->equip,{other.get()},loaded,revision),"templates are never auto-imported for peers");
 check(!ReadPeerProfile("../escape",{other.get()},loaded,revision),"peer scope cannot escape canonical folder");
 // Immutable publication remains valid while frames are being replaced.
 auto one=build({local,a}),two=build({local});std::atomic<bool> bad=false;std::thread reader([&]{for(int i=0;i<10000;i++){auto view=renderPolicies.load();if(view&&view->models.size()!=1&&view->models.size()!=2)bad=true;}});
 for(int i=0;i<10000;i++)renderPolicies.store(i%2?one:two);reader.join();check(!bad.load(),"render thread observes complete immutable frames");ResetPlayerPolicies();check(!renderPolicies.load(),"reset clears published policy");
 std::cout<<"PASS "<<checks<<" multiplayer checks: roster, per-instance rendering, independent transforms, lifecycle, mixed equipment and read-only profiles\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
