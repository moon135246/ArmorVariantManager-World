#pragma once
#include "Protocol.h"
#include "ModelCatalog.h"
#include "Transform.h"
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <iosfwd>
namespace worlddiff::app {
struct Target {
 std::string model; uint32_t value=0; bool mesh=false;
 bool operator<(const Target& b)const{return std::tie(model,mesh,value)<std::tie(b.model,b.mesh,b.value);}
 bool operator==(const Target& b)const{return model==b.model&&value==b.value&&mesh==b.mesh;}
};
struct Rules {std::set<Target> hidden,shown;};
struct MeshDisplay {
 uint64_t signature=0;uint32_t material=0;int order=-1;std::string note;
};
struct Preset {uint64_t id=0;std::string name;Rules rules;};
struct Group {
 uint64_t id=0;std::string name,scope;bool main=false,global=false;
 std::set<Target> members;Rules working;std::vector<Preset> presets;
 uint64_t selected=0,defaultPreset=0;
};
struct Document {
 bool enabled=true,autoDefault=true,autoOpen=false,autoFind=true,hideDragHint=false,transformsEnabled=true;int menuKey=VK_F9;float scale=1.0f;
 uint64_t nextId=1;std::vector<Group> groups;
 std::map<std::string,std::string> bindings;
 std::vector<TransformRule> transforms;
 std::map<std::string,TransformMode> transformModes;
 std::map<Target,MeshDisplay> meshDisplay;
};
struct OutfitSnapshot {
 uintptr_t entity=0;
 std::array<uintptr_t,5> contexts{},resources{};
 std::array<std::string,5> paths{}; // helm, body, arm, waist, legs
 bool ready=false;
};
using MemoryReader=bool(*)(uintptr_t,void*,size_t);
OutfitSnapshot InspectPlayerOutfit(uintptr_t entity,MemoryReader);
OutfitSnapshot ReadPlayerOutfit();
bool ReadRuntimeModel(uintptr_t resource,const std::string& path,Model&);
struct State {
 std::recursive_mutex mutex;Document data;std::vector<Model> models;
 std::map<std::string,DWORD> lastSeen;std::filesystem::path directory,gameRoot,configPath;
 SharedState* live=nullptr;bool dirty=false,loaded=false;DWORD dirtyAt=0;std::string notice;
 std::map<std::string,DWORD> profileAttempts;
 std::map<std::string,std::string> loadedFingerprints;
 std::map<std::string,std::string> profileSnapshots;
 bool migrating=false;
 bool trackingPlayer=false,storageReady=true,outfitChanging=false;
 OutfitSnapshot outfit,pendingOutfit;DWORD outfitSince=0;
 std::set<std::string> loadedScopes,blockedScopes,backedUpScopes;
 std::map<std::string,std::string> profileStatus;
 std::map<std::string,uintptr_t> modelResources;
 TransformRuntime transform;
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
const MeshDisplay* MeshView(const Model&,uint32_t index);
std::vector<const Mesh*> OrderedMeshes(const Model&,bool manual=true);
bool MoveMeshBefore(const Model&,uint32_t source,uint32_t before);
bool SetMeshNote(const Model&,uint32_t index,const std::string& note);
void ResetMeshOrder(const Model&);
bool CreateGroup(const std::string& name,const std::string& scope,bool global,const std::set<Target>& members,uint64_t& id);
bool DeleteGroup(uint64_t);
bool RenameGroup(uint64_t,const std::string&);
bool MoveGroupBefore(uint64_t source,uint64_t before=UINT64_MAX);
bool MovePresetBefore(Group&,uint64_t source,uint64_t before=UINT64_MAX);
bool SavePreset(Group&,const std::string&,bool overwrite=false);
bool ApplyPreset(Group&,uint64_t);
bool DeletePreset(Group&,uint64_t);
bool SetDefault(Group&,uint64_t);
void Restore(Group&);void RestoreAll();
void Import102(const std::filesystem::path&);
struct ProfileCandidate {
 std::string file,name;size_t hits=0,total=0,parts=0;bool filenameMatch=false,backup=false;
 bool strong=false;double coverage=0;
};
std::vector<ProfileCandidate> FindProfiles(const std::string& scope);
bool AutoFindProfile(const std::string& scope,bool automatic=false);
bool ApplyProfile(const std::string& scope,const std::string& file);
bool ExportProfile(const std::string& scope,const std::string& name={});
void TickProfiles();
void SaveProfiles();
void InitializeProfiles();
std::string ProfilePart(const std::string& model);
std::string Utf8(const std::wstring&);
std::filesystem::path PathUtf8(const std::string&);
void WriteDocument(std::ostream&,const Document&,bool equipment=false);
bool ReadDocument(std::istream&,Document&);
bool SaveSettings();bool LoadSettings();bool MigrateStorage();
bool SaveEquipmentProfiles();
bool LoadEquipmentProfile(const std::string& scope);
bool RebindEquipmentProfile(const std::string& scope);
bool ReloadEquipmentProfile(const std::string& scope);
bool RestoreProfileBackup(const std::string& scope,const std::string& name);
std::vector<std::string> ProfileBackups(const std::string& scope);
void FollowPlayerOutfit(const OutfitSnapshot& snapshot);
bool IsCurrentModel(const std::string& path);
bool CanEditScope(const std::string& scope);
}
