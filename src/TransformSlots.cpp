#include "State.h"
#include <cmath>
namespace worlddiff::app {
int TransformCategory(ConditionKind kind){switch(kind){case ConditionKind::WeaponState:return 0;case ConditionKind::LongSword:return 1;case ConditionKind::DualBlades:return 2;case ConditionKind::InsectGlaive:return 3;case ConditionKind::Health:return 4;default:return -1;}}
const char* TransformCategoryName(int category){const char* names[]={"收刀与拔刀","太刀气刃等级","双刀状态","操虫棍三灯","猎人血量"};return category>=0&&category<5?names[category]:"其他条件";}
int ExtractMask(const std::array<float,3>& timers){int mask=0;for(int i=0;i<3;i++){if(!std::isfinite(timers[i])||timers[i]<-3600||timers[i]>3600)return -1;if(timers[i]>0)mask|=1<<i;}return mask;}
bool SameCondition(const Condition& a,const Condition& b){
 if(a.kind!=b.kind||a.weapon!=b.weapon||a.latch!=b.latch)return false;
 if(a.kind==ConditionKind::Health)return a.healthMin==b.healthMin&&a.healthMax==b.healthMax&&a.lowerOpen==b.lowerOpen;
 if(a.kind==ConditionKind::ActionLmt)return a.minimum==b.minimum&&a.maximum==b.maximum;
 return a.state==b.state;
}
std::vector<TransformSlot> TransformSlots(int category){
 std::vector<TransformSlot> result;
 auto add=[&](const char* name,ConditionKind kind,int state){Condition c;c.kind=kind;c.state=state;result.push_back({name,c});};
 if(category==0){add("收刀",ConditionKind::WeaponState,0);add("拔刀",ConditionKind::WeaponState,1);}
 else if(category==1){const char* names[]={"无刃","白刃","黄刃","红刃"};for(int i=0;i<4;i++)add(names[i],ConditionKind::LongSword,i);}
 else if(category==2){add("普通",ConditionKind::DualBlades,0);add("鬼人化",ConditionKind::DualBlades,1);add("鬼人强化",ConditionKind::DualBlades,2);}
 else if(category==3){const char* names[]={"无精华","红灯","白灯","红＋白","橙灯","红＋橙","白＋橙","三灯齐全"};for(int state:{7,0,1,2,4,3,5,6})add(names[state],ConditionKind::InsectGlaive,state);}
 else if(category==4){
  auto hp=[&](const char* name,float min,float max,bool open,bool latch=false){Condition c;c.kind=ConditionKind::Health;c.healthMin=min;c.healthMax=max;c.lowerOpen=open;c.latch=latch;result.push_back({name,c});};
  hp("血量归零",0,0,false);hp("0% < 血量 ≤ 25%",0,25,true);hp("25% < 血量 ≤ 50%",25,50,true);hp("50% < 血量 ≤ 75%",50,75,true);hp("75% < 血量 ≤ 100%",75,100,true);
  hp("血量 ≤ 25% · 触发后保持",0,25,true,true);hp("血量 ≤ 50% · 触发后保持",0,50,true,true);hp("血量 ≤ 75% · 触发后保持",0,75,true,true);
 }
 return result;
}
TransformMode& EnsureTransformMode(Document& d,const std::string& scope){
 auto [it,inserted]=d.transformModes.try_emplace(scope);auto& mode=it->second;if(!inserted)return mode;
 mode.categories.fill(false);int count=0;
 for(auto& rule:d.transforms)if(rule.scope==scope&&rule.enabled&&!rule.targets.empty()){
  int category=TransformCategory(rule.condition.kind);if(category>=0){if(!mode.categories[category]){mode.categories[category]=true;if(!count)mode.selected=category;count++;}mode.enabled=true;}
 }
 mode.parallel=count>1;if(!count)mode.categories[0]=true;
 return mode;
}
void InferTransformModes(Document& d){for(auto& g:d.groups)EnsureTransformMode(d,g.scope);}
}
