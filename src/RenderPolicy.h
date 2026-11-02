#pragma once
#include "Protocol.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
namespace worlddiff {
// Published as one immutable frame. The render hook never reads the editable
// document or a different character's transient transform results.
struct ModelPolicy {
 uintptr_t context=0,resource=0,actor=0,record=0,token=0,container=0;
 uintptr_t meshes=0,names=0;uint32_t materialCount=0,part=0;
 std::string path;
 std::vector<uint16_t> materials;
 std::vector<uint8_t> hidden;
 bool local=false,preview=false;
};
struct RenderPolicies {
 DWORD tick=0;uintptr_t localEntity=0;std::vector<ModelPolicy> models;
 const ModelPolicy* Find(uintptr_t ctx)const{
  auto it=std::lower_bound(models.begin(),models.end(),ctx,[](auto& p,uintptr_t value){return p.context<value;});
  return it!=models.end()&&it->context==ctx?&*it:nullptr;
 }
};
inline std::atomic<std::shared_ptr<const RenderPolicies>> renderPolicies;
// Keep the diagnostics ABI compatible; instance identity is process-local.
inline std::atomic<uintptr_t> rowContexts[kMaxRows]{};
inline bool PolicyHides(const ModelPolicy& p,uintptr_t resource,uint32_t index,uint32_t material,uint32_t count){
 return p.resource==resource&&p.materials.size()==count&&index<p.hidden.size()&&index<p.materials.size()&&p.materials[index]==material&&p.hidden[index]!=0;
}
}
