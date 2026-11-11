#pragma once
#include "Protocol.h"
#include "DissolveTransition.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
namespace worlddiff {
struct MeshObservation {
 std::atomic<bool> seen{};
#ifdef WORLDDIFF_SILVER_FLOW
 std::atomic<unsigned> silverStages{};
 std::atomic<unsigned> amberStages{};
 std::atomic<unsigned> uvStages{}; // Silver in the low nibble, amber/ink in the high nibble.
 std::atomic<unsigned> silverBindings{};
 std::atomic<DWORD> silverColorTransition{};
#endif
 std::atomic<DWORD> controllerTick{}, admittedTick{};
};
// Published as one immutable frame. The render hook never reads the editable
// document or a different character's transient transform results.
struct ModelPolicy {
 uintptr_t context=0,resource=0,actor=0,record=0,token=0,container=0;
 uintptr_t meshes=0,names=0;uint32_t materialCount=0,part=0;
 std::string path;
 std::vector<uint16_t> materials;
 std::vector<uint16_t> groups;
 std::vector<uint8_t> hidden;
 uint64_t geometrySignature=0;
 std::shared_ptr<MeshObservation[]> observations;
 std::vector<dissolve::Transition> transitions;
 const dissolve::Transition* TransitionFor(uint32_t mesh) const {
  const auto it=std::lower_bound(transitions.begin(),transitions.end(),mesh,
      [](const auto& item,uint32_t index){return item.mesh<index;});
  return it!=transitions.end() && it->mesh==mesh ? &*it : nullptr;
 }
 bool local=false,preview=false;
};
struct RenderPolicies {
 bool enabled=true,worldDiffPriority=true;
 bool dissolveEnabled=false;
 unsigned dissolveMs=900;
 dissolve::Style dissolveStyle=dissolve::DefaultStyle;
 DWORD tick=0;uintptr_t localEntity=0;std::vector<ModelPolicy> models;
 const ModelPolicy* Find(uintptr_t ctx)const{
  auto it=std::lower_bound(models.begin(),models.end(),ctx,[](auto& p,uintptr_t value){return p.context<value;});
  return it!=models.end()&&it->context==ctx?&*it:nullptr;
 }
};
inline bool SameDissolveInstance(const ModelPolicy& a,const ModelPolicy& b) {
 return a.local && b.local && !a.preview && !b.preview && a.context==b.context &&
        a.actor==b.actor && a.token==b.token && a.record==b.record && a.container==b.container &&
        a.resource==b.resource && a.meshes==b.meshes && a.names==b.names && a.part==b.part &&
        a.path==b.path && a.geometrySignature==b.geometrySignature;
}
inline std::atomic<std::shared_ptr<const RenderPolicies>> renderPolicies;
// Keep the diagnostics ABI compatible; instance identity is process-local.
inline std::atomic<uintptr_t> rowContexts[kMaxRows]{};
inline bool PolicyMatches(const ModelPolicy& p,uintptr_t resource,uint32_t index,uint32_t material,uint32_t count){
 return p.resource==resource&&p.materials.size()==count&&p.hidden.size()==count&&index<p.materials.size()&&p.materials[index]==material;
}
inline bool PolicyHides(const ModelPolicy& p,uintptr_t resource,uint32_t index,uint32_t material,uint32_t count){
 return PolicyMatches(p,resource,index,material,count)&&p.hidden[index]!=0;
}
inline bool ResolveVisibility(bool original,bool hidden,bool controllerHandled,bool worldDiffPriority){
 if(controllerHandled)return worldDiffPriority?!hidden:original;
 return original&&!hidden;
}
}
