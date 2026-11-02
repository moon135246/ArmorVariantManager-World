#pragma once
#include "State.h"
#include <algorithm>
#include <iterator>
#include <string_view>

namespace worlddiff::app {
namespace armor_names {
struct Entry {std::string_view id,name,female,male,aliases;};
inline constexpr Entry entries[]={
#include "ArmorNamesData.inl"
};
inline const Entry* Find(std::string_view id){
 std::string key(id);for(auto& c:key)if(c>='A'&&c<='Z')c+=32;
 auto found=std::lower_bound(std::begin(entries),std::end(entries),key,[](const Entry& entry,const std::string& value){return entry.id<value;});
 return found!=std::end(entries)&&found->id==key?found:nullptr;
}
inline int Gender(const std::string& scope){
 auto& s=Get();bool female=false,male=false;int recent=0;DWORD bestAge=1800,now=GetTickCount();
 for(auto& model:s.models)if(model.equip==scope&&IsCurrentModel(model.relativePath)){
  auto path=Normalize(model.relativePath);int sex=path.find("f_equip\\")!=std::string::npos?1:path.find("m_equip\\")!=std::string::npos?2:0;
  female|=sex==1;male|=sex==2;auto seen=s.lastSeen.find(path);
  if(sex&&seen!=s.lastSeen.end()){DWORD age=now-seen->second;if(age<bestAge){bestAge=age;recent=sex;}}
 }
 return recent?recent:female&&!male?1:male&&!female?2:0;
}
}
// Presentation only. Configuration scopes, profile filenames and model paths
// always retain the original ID; display names never participate in matching.
inline std::string EquipmentLabel(const std::string& scope){
 auto* entry=armor_names::Find(scope);if(!entry)return scope;
 int sex=armor_names::Gender(scope);auto name=sex==1?entry->female:sex==2?entry->male:entry->name;
 return std::string(name)+"("+scope+")";
}
inline std::string_view EquipmentAliases(const std::string& scope){auto* entry=armor_names::Find(scope);return entry?entry->aliases:std::string_view{};}
}
