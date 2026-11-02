#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <array>
namespace worlddiff::app {
enum class ConditionKind : int { WeaponState, LongSword, DualBlades, InsectGlaive, Enhanced, Health, WeaponType, ActionLmt, Count };
struct Condition {
 ConditionKind kind=ConditionKind::WeaponState;
 int state=0,weapon=-1,minimum=0,maximum=65535;
 float healthMin=0,healthMax=50;
 bool lowerOpen=false,latch=false;
};
struct TransformTarget {uint64_t group=0,preset=0;};
struct TransformRule {
 uint64_t id=0;std::string name,scope;bool enabled=true;int priority=1;
 Condition condition;std::vector<TransformTarget> targets;
};
struct PlayerSnapshot {
 uintptr_t entity=0;
 int weapon=-1,lmt=-1,spirit=-1,demon=-1,archdemon=-1,map=-1;
 float health=-1,maxHealth=-1,healthPercent=-1;
 int extracts=-1;std::array<float,3> extractTimers{{-1,-1,-1}};
};
struct TransformMode {
 bool enabled=false,parallel=false,legacy=false;int selected=0;
 std::array<bool,5> categories{{true,false,false,false,false}};
 std::array<int,5> priorities{{1,2,3,4,5}};
};
struct TransformSlot {std::string name;Condition condition;};
struct TransformRuntime {
 PlayerSnapshot player;
 std::map<uint64_t,uint64_t> activePresets;
 std::set<uint64_t> activeRules,latched;
 std::map<uint64_t,uint64_t> signatures;
 uintptr_t previousEntity=0;int previousMap=-1,previousWeapon=-1;
 bool wasZero=false;
};
struct Document;struct Group;struct Rules;
const char* ConditionName(ConditionKind);
const char* WeaponName(int);
std::string DescribeCondition(const Condition&);
bool ValidCondition(const Condition&);
bool Matches(const Condition&,const PlayerSnapshot&);
bool ValidateTransform(const TransformRule&,const Document&,std::string* reason=nullptr);
int TransformCategory(ConditionKind);
const char* TransformCategoryName(int);
std::vector<TransformSlot> TransformSlots(int);
bool SameCondition(const Condition&,const Condition&);
TransformMode& EnsureTransformMode(Document&,const std::string&);
void InferTransformModes(Document&);
int ExtractMask(const std::array<float,3>&);
PlayerSnapshot ReadPlayerSnapshot();
void EvaluateTransforms(const Document&,TransformRuntime&,const PlayerSnapshot&);
void UpdateTransforms();
void ResetTransforms();
const Rules& EffectiveRules(const Group&);
uint64_t ActivePreset(const Group&);
}
