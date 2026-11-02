#pragma once
#include "State.h"
#include "RenderPolicy.h"
namespace worlddiff::app {
struct PeerProfile {Document document;std::string revision;bool matched=false;};
struct ActorModel {
 std::shared_ptr<const Model> model;uintptr_t meshes=0,names=0;
};
struct ActorFrame {
 PlayerInstance instance;PlayerSnapshot player;
 std::array<ActorModel,5> models;
 std::map<std::string,std::shared_ptr<const PeerProfile>> profiles;
};
// Pure actor/frame controller, also exercised with independent synthetic actors.
class PlayerPolicyController {
 struct ScopeRuntime {TransformRuntime transform;std::string revision;};
 struct ActorRuntime {std::array<uintptr_t,5> contexts{},resources{};std::array<uint64_t,5> stamps{};std::map<std::string,ScopeRuntime> scopes;};
 using Key=std::tuple<uintptr_t,uintptr_t,uintptr_t,uintptr_t>;
 std::map<Key,ActorRuntime> actors;uint64_t epoch=0;
public:
 size_t peers=0,matched=0;
 size_t RuntimeCount()const{return actors.size();}
 std::shared_ptr<const RenderPolicies> Build(const std::vector<ActorFrame>&,const OutfitSnapshot& local,const Document&,const TransformRuntime&,const std::set<std::string>& allowedScopes,uint64_t generation,DWORD now);
};
void UpdatePlayerPolicies();
void ResetPlayerPolicies();
}
