#pragma once
#include "Protocol.h"
#include "ModelCatalog.h"
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
namespace worlddiff::app {
struct Target {
 std::string model; uint32_t value=0; bool mesh=false;
 bool operator<(const Target& b)const{return std::tie(model,mesh,value)<std::tie(b.model,b.mesh,b.value);}
 bool operator==(const Target& b)const{return model==b.model&&value==b.value&&mesh==b.mesh;}
};
struct Rules {std::set<Target> hidden,shown;};
struct Preset {uint64_t id=0;std::string name;Rules rules;};
struct Group {
 uint64_t id=0;std::string name,scope;bool main=false,global=false;
 std::set<Target> members;Rules working;std::vector<Preset> presets;
 uint64_t selected=0,defaultPreset=0;
};
struct Document {
 bool enabled=true,autoDefault=true,autoOpen=false;int menuKey=VK_F9;float scale=1.0f;
 uint64_t nextId=1;std::vector<Group> groups;
};
struct State {
 std::recursive_mutex mutex;Document data;std::vector<Model> models;
 std::map<std::string,DWORD> lastSeen;std::filesystem::path directory,gameRoot,configPath;
 SharedState* live=nullptr;bool dirty=false,loaded=false;DWORD dirtyAt=0;std::string notice;
};
State& Get();
void Initialize(SharedState*,const std::filesystem::path& pluginDir,const std::filesystem::path& gameRoot);
void Tick();
void Changed(const std::string& message={});
bool Save();bool Load();
std::string Normalize(std::string);
std::string Equip(const std::string& path);
Group* Find(uint64_t);
Group& Main(const std::string& scope);
Group* Owner(const std::string& model,uint32_t material);
bool InGroup(const Group&,const std::string& model,uint32_t material);
bool RuleHidden(const Rules&,const std::string& model,uint32_t mesh,uint32_t material);
bool Hidden(const std::string& model,uint32_t mesh,uint32_t material);
bool GlobalHidden(const std::string& model,uint32_t mesh,uint32_t material);
void SetVisible(Group&,const Target&,bool visible);
bool CreateGroup(const std::string& name,const std::string& scope,bool global,const std::set<Target>& members,uint64_t& id);
bool DeleteGroup(uint64_t);
bool RenameGroup(uint64_t,const std::string&);
bool SavePreset(Group&,const std::string&,bool overwrite=false);
bool ApplyPreset(Group&,uint64_t);
bool DeletePreset(Group&,uint64_t);
bool SetDefault(Group&,uint64_t);
void Restore(Group&);void RestoreAll();
void Import102(const std::filesystem::path&);
}
