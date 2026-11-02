#include "State.h"
#include <cmath>
#include <algorithm>
namespace worlddiff::app {
int TransformCategory(ConditionKind kind){switch(kind){case ConditionKind::WeaponState:return 0;case ConditionKind::LongSword:return 1;case ConditionKind::DualBlades:return 2;case ConditionKind::InsectGlaive:return 3;case ConditionKind::Health:return 4;case ConditionKind::BowCharge:return 5;case ConditionKind::HammerCharge:return 6;case ConditionKind::ChargeBlade:return 7;case ConditionKind::SwitchAxe:return 8;default:return -1;}}
const char* TransformCategoryName(int category){const char* names[]={"收刀与拔刀","太刀气刃等级","双刀状态","操虫棍灯数","猎人血量","弓蓄力等级","大锤蓄力等级","盾斧形态","斩斧形态"};return category>=0&&category<kTransformCategories?names[category]:"";}
int ExtractMask(const std::array<float,3>& timers){int mask=0;for(int i=0;i<3;i++){if(!std::isfinite(timers[i])||timers[i]<-3600||timers[i]>3600)return -1;if(timers[i]>0)mask|=1<<i;}return mask;}
int ExtractCount(int mask){if(mask<0||mask>7)return -1;return (mask&1)+((mask>>1)&1)+((mask>>2)&1);}
int TimerState(float seconds){return !std::isfinite(seconds)||seconds<-3600||seconds>3600?-1:seconds>0?1:0;}
const char* BowChargeName(int level){const char* names[]={"未蓄力","一蓄","二蓄","三蓄"};return level>=1&&level<=4?names[level-1]:"读取中";}
int ChargeBladeState(const PlayerSnapshot& p){
 if(p.weapon!=9||p.chargeBladeMode<0||p.chargeBladeMode>1||p.chargeBladeBuffs<0||p.chargeBladeBuffs>7)return -1;
 if(p.chargeBladeBuffs==7)return 6;
 if(p.chargeBladeMode==1)return (p.chargeBladeBuffs&4)?5:1;
 int sword=p.chargeBladeBuffs&3;return sword==3?4:sword==1?2:sword==2?3:0;
}
int SwitchAxeState(const PlayerSnapshot& p){
 if(p.weapon!=8)return -1;
 if(p.switchAxeMode==0&&p.switchAxePowered>=0&&p.switchAxePowered<=1)return p.switchAxePowered?2:0;
 if(p.switchAxeMode==1&&p.switchAxeAmped>=0&&p.switchAxeAmped<=1)return p.switchAxeAmped?3:1;
 return -1;
}
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
 else if(category==3){const char* names[]={"无精华","一灯","二灯","三灯齐全"};for(int state=0;state<4;state++)add(names[state],ConditionKind::InsectGlaive,state);}
 else if(category==5){for(int level=1;level<=4;level++)add(BowChargeName(level),ConditionKind::BowCharge,level);}
 else if(category==6){const char* names[]={"未蓄力","一段蓄力","二段蓄力","三段蓄力"};for(int level=0;level<=3;level++)add(names[level],ConditionKind::HammerCharge,level);}
 else if(category==7){const char* names[]={"剑形态（基础）","斧形态（基础）","剑形态 · 红盾","剑形态 · 红剑","剑形态 · 红盾＋红剑","斧形态 · 红斧","三红齐全（盾＋剑＋斧）"};for(int state=0;state<7;state++)add(names[state],ConditionKind::ChargeBlade,state);}
 else if(category==8){const char* names[]={"斧形态（基础）","剑形态（基础）","斧形态 · 强化斧","剑形态 · 高出力"};for(int state=0;state<4;state++)add(names[state],ConditionKind::SwitchAxe,state);}
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
void UpgradeTransformDocument(Document& d,unsigned version){
 for(auto& r:d.transforms)if(version<109&&r.condition.kind==ConditionKind::InsectGlaive)r.condition.state=ExtractCount(r.condition.state);
 std::erase_if(d.transforms,[&](auto& r){auto& c=r.condition;return TransformCategory(c.kind)<0||(version<109&&c.kind==ConditionKind::Health&&c.latch&&c.healthMin==0&&(c.healthMax==25||c.healthMax==50||c.healthMax==75));});
 if(version<109){
  std::vector<TransformRule> merged;
  for(auto r:d.transforms){
   auto it=std::find_if(merged.begin(),merged.end(),[&](auto& x){return r.condition.kind==ConditionKind::InsectGlaive&&x.scope==r.scope&&SameCondition(x.condition,r.condition);});
   if(it==merged.end()){merged.push_back(std::move(r));continue;}
   // A count cannot retain different presets for different colors. Preserve
   // the earlier binding for a group and merge the remaining target groups.
   for(auto& target:r.targets)if(std::none_of(it->targets.begin(),it->targets.end(),[&](auto& t){return t.group==target.group;}))it->targets.push_back(target);
   it->enabled=it->enabled||r.enabled;it->priority=std::min(it->priority,r.priority);
  }d.transforms=std::move(merged);
 }
}
}
