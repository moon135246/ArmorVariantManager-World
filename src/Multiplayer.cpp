#include "Multiplayer.h"
#include <algorithm>
#include <sstream>
namespace worlddiff::app {
namespace {
Document ScopeDocument(const Document& d,const std::string& scope){
 Document out;out.enabled=d.enabled;out.transformsEnabled=d.transformsEnabled;out.hotkeysEnabled=d.hotkeysEnabled;
 for(auto& g:d.groups)if(g.scope==scope)out.groups.push_back(g);
 for(auto& r:d.transforms)if(r.scope==scope)out.transforms.push_back(r);
 if(auto it=d.transformModes.find(scope);it!=d.transformModes.end())out.transformModes.insert(*it);
 return out;
}
std::string Revision(const Document& d){std::ostringstream text;WriteDocument(text,d,true);return text.str();}
const Rules& ActorRules(const Group& g,const TransformRuntime& rt){
 auto id=rt.activePresets.find(g.id);if(id!=rt.activePresets.end())for(auto& p:g.presets)if(p.id==id->second)return p.rules;return g.working;
}
bool ActorHidden(const Document& d,const TransformRuntime& rt,const std::string& path,uint32_t index,uint32_t material){
 const Group* owner=nullptr;const Group* main=nullptr;
 for(auto& g:d.groups){if(g.scope!=Equip(path))continue;if(g.main)main=&g;
  bool member=std::any_of(g.members.begin(),g.members.end(),[&](auto& t){return t.model==path&&t.value==material&&!t.mesh;});
  if(g.global&&member&&RuleHidden(ActorRules(g,rt),path,index,material))return true;
  if(!g.global&&!g.main&&member)owner=&g;
 }
 return owner?RuleHidden(ActorRules(*owner,rt),path,index,material):main&&RuleHidden(ActorRules(*main,rt),path,index,material);
}
using ResourceKey=std::pair<uintptr_t,std::string>;
struct CachedModel {ActorModel value;DWORD checked=0;};
struct CachedProfile {std::shared_ptr<const PeerProfile> value;DWORD checked=0;};
PlayerPolicyController controller;
std::map<ResourceKey,CachedModel> models;
std::map<std::string,CachedProfile> profiles;
bool ReadPointer(uintptr_t address,uintptr_t& value){SIZE_T n=0;return ReadProcessMemory(GetCurrentProcess(),(void*)address,&value,8,&n)&&n==8;}
ActorModel RuntimeModel(uintptr_t resource,const std::string& path,DWORD now){
 auto& cache=models[{resource,path}];uintptr_t meshes=0,names=0;
 if(!ReadPointer(resource+0xC8,meshes)||!ReadPointer(resource+0x400,names)){cache={};return {};}
 if(!cache.value.model||cache.value.meshes!=meshes||cache.value.names!=names||(DWORD)(now-cache.checked)>=1000){
  Model model;cache.checked=now;if(!ReadRuntimeModel(resource,path,model)){cache.value={};return {};}
  cache.value={std::make_shared<const Model>(std::move(model)),meshes,names};
 }
 return cache.value;
}
}
std::shared_ptr<const RenderPolicies> PlayerPolicyController::Build(const std::vector<ActorFrame>& frames,const OutfitSnapshot& local,const Document& document,const TransformRuntime& localTransform,const std::set<std::string>& allowedScopes,uint64_t generation,DWORD now){
 auto result=std::make_shared<RenderPolicies>();result->tick=now;result->localEntity=local.entity;peers=matched=0;
 if(epoch!=generation){actors.clear();epoch=generation;}
 const ActorFrame* localFrame=nullptr;for(auto& frame:frames)if(frame.instance.local&&frame.instance.outfit.entity==local.entity){localFrame=&frame;break;}
 if(!local.ready||!localFrame){actors.clear();return result;}
 std::set<Key> alive;std::set<uintptr_t> contexts;std::map<uintptr_t,size_t> owners;
 for(auto& frame:frames)if(frame.instance.outfit.ready)for(auto ctx:frame.instance.outfit.contexts)if(ctx)owners[ctx]++;
 for(auto& frame:frames){
  auto& instance=frame.instance;auto& outfit=instance.outfit;bool isLocal=instance.local&&outfit.entity==local.entity;
  if(!outfit.ready||(!isLocal&&(local.savePreview||instance.local||!instance.record||frame.player.entity!=outfit.entity)))continue;
  if(!isLocal)peers++;
  Key key{outfit.entity,instance.record,instance.token,instance.container};ActorRuntime* actor=nullptr;
  if(!isLocal){
   std::array<uint64_t,5> stamps{};for(size_t i=0;i<5;i++)if(frame.models[i].model)stamps[i]=StableModelStamp(*frame.models[i].model);
   alive.insert(key);actor=&actors[key];if(actor->contexts!=outfit.contexts||actor->resources!=outfit.resources||actor->stamps!=stamps){*actor={};actor->contexts=outfit.contexts;actor->resources=outfit.resources;actor->stamps=stamps;}
  }
  std::map<std::string,Document> documents;std::map<std::string,const TransformRuntime*> runtimes;std::set<std::string> scopes;
  for(auto& model:frame.models)if(model.model)scopes.insert(model.model->equip);
  for(auto& scope:scopes){
   if(isLocal){if(allowedScopes.count(scope)){documents[scope]=ScopeDocument(document,scope);runtimes[scope]=&localTransform;}continue;}
   bool shared=allowedScopes.count(scope)!=0;bool localScope=std::any_of(local.paths.begin(),local.paths.end(),[&](auto& p){return !p.empty()&&Equip(p)==scope;});
   if(localScope){
    for(auto& model:frame.models)if(model.model&&model.model->equip==scope){
     auto same=std::find_if(localFrame->models.begin(),localFrame->models.end(),[&](auto& m){return m.model&&m.model->relativePath==model.model->relativePath&&StableModelStamp(*m.model)==StableModelStamp(*model.model);});
     if(same==localFrame->models.end())shared=false;
    }
    if(shared)documents[scope]=ScopeDocument(document,scope);
   }else if(auto it=frame.profiles.find(scope);it!=frame.profiles.end()&&it->second&&it->second->matched)documents[scope]=it->second->document;
   auto doc=documents.find(scope);if(doc==documents.end()){actor->scopes.erase(scope);continue;}
   doc->second.enabled=document.enabled;doc->second.transformsEnabled=document.transformsEnabled;doc->second.hotkeysEnabled=document.hotkeysEnabled;
   auto revision=Revision(doc->second);auto& runtime=actor->scopes[scope];if(runtime.revision!=revision){runtime.transform={};runtime.revision=std::move(revision);}
   EvaluateTransforms(doc->second,runtime.transform,frame.player);runtimes[scope]=&runtime.transform;
  }
  if(actor)std::erase_if(actor->scopes,[&](auto& value){return !documents.count(value.first);});
  bool any=false;
  for(size_t part=0;part<5;part++){
   auto& source=frame.models[part];if(!source.model||!outfit.contexts[part]||owners[outfit.contexts[part]]!=1||!contexts.insert(outfit.contexts[part]).second)continue;
   auto& model=*source.model;auto scope=model.equip;auto doc=documents.find(scope);if(doc==documents.end())continue;
   ModelPolicy policy;policy.context=outfit.contexts[part];policy.resource=outfit.resources[part];policy.actor=outfit.entity;policy.record=instance.record;policy.token=instance.token;policy.container=instance.container;policy.local=isLocal;policy.preview=outfit.savePreview;policy.meshes=source.meshes;policy.names=source.names;policy.materialCount=(uint32_t)model.materials.size();policy.part=(uint32_t)part;policy.path=model.relativePath;
   policy.materials.resize(model.meshes.size(),UINT16_MAX);policy.hidden.resize(model.meshes.size());
   for(auto& mesh:model.meshes)if(mesh.index<policy.materials.size()&&mesh.materialId<model.materials.size()){
    policy.materials[mesh.index]=mesh.materialId;policy.hidden[mesh.index]=doc->second.enabled&&ActorHidden(doc->second,*runtimes.at(scope),model.relativePath,mesh.index,model.materials[mesh.materialId].hash)?1:0;
   }
   result->models.push_back(std::move(policy));any=true;
  }
  if(!isLocal&&any)matched++;
 }
 std::erase_if(actors,[&](auto& item){return !alive.count(item.first);});
 std::sort(result->models.begin(),result->models.end(),[](auto& a,auto& b){return a.context<b.context;});return result;
}
void ResetPlayerPolicies(){controller={};models.clear();profiles.clear();renderPolicies.store({},std::memory_order_release);}
void UpdatePlayerPolicies(){
 auto& s=Get();if(!s.trackingPlayer)return;DWORD now=GetTickCount();std::vector<ActorFrame> frames;std::set<ResourceKey> usedModels;std::set<std::string> usedProfiles;
 auto outfit=s.outfit;if(s.outfitChanging||!s.storageReady)outfit.ready=false;
 if(outfit.ready){
  auto instances=ReadPlayers(outfit);
  // Keep the explicitly selected save avatar / gameplay actor available even
  // while the live roster is being reconstructed. Never substitute a peer.
  if(instances.empty())instances.push_back({outfit,0,0,0,true});
  for(auto& instance:instances){
   if(instance.local&&(instance.outfit.contexts!=outfit.contexts||instance.outfit.resources!=outfit.resources))continue;
   ActorFrame frame;frame.instance=instance;frame.player=instance.local?s.transform.player:ReadActorSnapshot(instance.outfit.entity,true);
   std::map<std::string,std::vector<const Model*>> byScope;
   for(size_t part=0;part<5;part++)if(!instance.outfit.paths[part].empty()){
    auto resource=instance.outfit.resources[part];auto& path=instance.outfit.paths[part];usedModels.insert({resource,path});frame.models[part]=RuntimeModel(resource,path,now);
    if(frame.models[part].model)byScope[Equip(path)].push_back(frame.models[part].model.get());
   }
   if(!instance.local)for(auto& [scope,worn]:byScope){
    std::string key=scope;for(auto* model:worn)key+='|'+model->relativePath+':'+std::to_string(StableModelStamp(*model));usedProfiles.insert(key);auto& cache=profiles[key];
    if(!cache.value||(DWORD)(now-cache.checked)>=1500){auto profile=std::make_shared<PeerProfile>();cache.checked=now;profile->matched=ReadPeerProfile(scope,worn,profile->document,profile->revision);cache.value=std::move(profile);}
    frame.profiles[scope]=cache.value;
   }
   frames.push_back(std::move(frame));
  }
 }
 std::set<std::string> allowed;for(auto& scope:s.loadedScopes)if(!s.blockedScopes.count(scope))allowed.insert(scope);
 auto snapshot=controller.Build(frames,outfit,s.data,s.transform,allowed,s.transformEpoch,now);
 s.peerCount=controller.peers;s.matchedPeers=controller.matched;
 renderPolicies.store(std::move(snapshot),std::memory_order_release);
 std::erase_if(models,[&](auto& value){return !usedModels.count(value.first);});std::erase_if(profiles,[&](auto& value){return !usedProfiles.count(value.first);});
}
}
