#include "Transform.h"
#include "State.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace worlddiff::app {
const char* ConditionName(ConditionKind kind){
 const char* names[]={"收刀与拔刀","太刀练气","双刀状态","操虫棍灯数","强化状态","猎人血量","武器种类","动作 LMT","弓蓄力等级","大锤蓄力等级","盾斧形态","斩斧形态"};
 int n=(int)kind;return n>=0&&n<(int)ConditionKind::Count?names[n]:"未知条件";
}
const char* WeaponName(int weapon){
 // MHW weaponType order, which differs from the menu's display order.
 const char* names[]={"大剑","片手剑","双刀","太刀","大锤","狩猎笛","长枪","铳枪","斩斧","盾斧","操虫棍","弓","重弩","轻弩"};
 return weapon>=0&&weapon<14?names[weapon]:"不限武器";
}
bool ValidCondition(const Condition& c){
 if((int)c.kind<0||c.kind>=ConditionKind::Count||c.weapon < -1||c.weapon>13)return false;
 if(c.latch&&c.kind!=ConditionKind::Health)return false;
 switch(c.kind){
 case ConditionKind::WeaponState:case ConditionKind::Enhanced:return c.state>=0&&c.state<=1;
 case ConditionKind::ChargeBlade:return c.state>=0&&c.state<=6;
 case ConditionKind::SwitchAxe:return c.state>=0&&c.state<=3;
 case ConditionKind::LongSword:return c.state>=0&&c.state<=5;
 case ConditionKind::DualBlades:return c.state>=0&&c.state<=2;
 case ConditionKind::InsectGlaive:case ConditionKind::HammerCharge:return c.state>=0&&c.state<=3;
 case ConditionKind::BowCharge:return c.state>=1&&c.state<=4;
 case ConditionKind::Health:return std::isfinite(c.healthMin)&&std::isfinite(c.healthMax)&&c.healthMin>=0&&c.healthMax<=100&&c.healthMin<=c.healthMax&&(!c.lowerOpen||c.healthMin<c.healthMax);
 case ConditionKind::WeaponType:return c.weapon>=0;
 case ConditionKind::ActionLmt:return c.minimum>=0&&c.maximum>=c.minimum&&c.maximum<=0x7fffffff;
 default:return false;
 }
}
bool Matches(const Condition& c,const PlayerSnapshot& p){
 if(!p.entity||!ValidCondition(c)||(c.weapon>=0&&p.weapon!=c.weapon))return false;
 switch(c.kind){
 // Confirmed by the live hub idle (LMT 1) and reference combat motion 49265.
 // The reference README inverts its prose for ID30/31; use motion semantics.
 case ConditionKind::WeaponState:return p.lmt>=0&&(c.state==0?p.lmt<0x8000:p.lmt>=0x8000);
 case ConditionKind::LongSword:return p.weapon==3&&p.spirit>=0&&p.spirit<=3&&(c.state<4?p.spirit==c.state:p.spirit>=c.state-3);
 case ConditionKind::DualBlades:
  if(p.weapon!=2)return false;
  if(c.state==1)return p.demon==1;
  if(c.state==2)return p.demon==0&&p.archdemon==1;
  return p.demon==0&&p.archdemon==0;
 case ConditionKind::InsectGlaive:return p.weapon==10&&ExtractCount(p.extracts)==c.state;
 case ConditionKind::BowCharge:return p.weapon==11&&p.bowCharge==c.state;
 case ConditionKind::HammerCharge:return p.weapon==4&&p.hammerCharge==c.state;
 // Base form bindings remain a per-group fallback for 1.1.0 presets. A
 // configured enhanced state is sorted before the base at equal priority.
 case ConditionKind::ChargeBlade:return p.weapon==9&&(c.state<2?p.chargeBladeMode==c.state:ChargeBladeState(p)==c.state);
 case ConditionKind::SwitchAxe:return p.weapon==8&&(c.state<2?p.switchAxeMode==c.state:SwitchAxeState(p)==c.state);
 case ConditionKind::Enhanced:{
  if(p.weapon==3&&p.spirit>=0&&p.spirit<=3)return (p.spirit==3)==(c.state==1);
  if(p.weapon==2&&p.archdemon>=0&&p.archdemon<=1)return (p.archdemon==1)==(c.state==1);
  return false;
 }
 case ConditionKind::Health:return std::isfinite(p.healthPercent)&&p.healthPercent>=0&&p.healthPercent<=100&&(c.lowerOpen?p.healthPercent>c.healthMin:p.healthPercent>=c.healthMin)&&p.healthPercent<=c.healthMax&&(!c.latch||p.healthPercent>0);
 case ConditionKind::WeaponType:return p.weapon==c.weapon;
 case ConditionKind::ActionLmt:return p.lmt>=c.minimum&&p.lmt<=c.maximum;
 default:return false;
 }
}
std::string DescribeCondition(const Condition& c){
 std::ostringstream s;s<<ConditionName(c.kind)<<" · ";
 switch(c.kind){
 case ConditionKind::WeaponState:s<<(c.state?"拔刀":"收刀");break;
 case ConditionKind::LongSword:{const char* n[]={"无刃","白刃","黄刃","红刃","白刃及以上","黄刃及以上"};s<<n[std::clamp(c.state,0,5)];break;}
 case ConditionKind::DualBlades:{const char* n[]={"普通（无鬼人化/强化）","鬼人化","鬼人强化"};s<<n[std::clamp(c.state,0,2)];break;}
 case ConditionKind::InsectGlaive:{const char* names[]={"无精华","一灯","二灯","三灯齐全"};s<<names[std::clamp(c.state,0,3)];break;}
 case ConditionKind::BowCharge:s<<BowChargeName(c.state);break;
 case ConditionKind::HammerCharge:s<<c.state<<" 段";break;
 case ConditionKind::ChargeBlade:case ConditionKind::SwitchAxe:{auto slots=TransformSlots(TransformCategory(c.kind));s<<slots[std::clamp(c.state,0,(int)slots.size()-1)].name;break;}
 case ConditionKind::Enhanced:s<<(c.state?"强化中":"未强化");break;
 case ConditionKind::Health:s<<(c.lowerOpen?"(":"[")<<c.healthMin<<"%, "<<c.healthMax<<"%]"<<(c.latch?" · 触发后保持":"");break;
 case ConditionKind::WeaponType:s<<WeaponName(c.weapon);break;
 case ConditionKind::ActionLmt:s<<c.minimum<<" ~ "<<c.maximum;break;
 default:break;
 }
 if(c.weapon>=0&&c.kind!=ConditionKind::WeaponType)s<<" · "<<WeaponName(c.weapon);
 return s.str();
}
bool ValidateTransform(const TransformRule& r,const Document& d,std::string* reason){
 auto fail=[&](const char* message){if(reason)*reason=message;return false;};
 if(r.name.empty()||r.name.size()>160||r.name.find_first_of("\r\n\t")!=std::string::npos)return fail("请填写有效的规则名称");
 if(r.scope.empty()||r.scope.size()>128||r.priority<1||r.priority>99||!ValidCondition(r.condition))return fail("请检查条件范围和优先级");
 if(r.targets.size()>64)return fail("最多选择 64 个目标分组");
 std::set<uint64_t> seen;
 for(auto& target:r.targets){
  if(!seen.insert(target.group).second)return fail("同一规则内，每个分组只能选择一个预设");
  const Group* g=nullptr;for(auto& x:d.groups)if(x.id==target.group&&x.scope==r.scope)g=&x;
  if(!g)return fail("目标分组已不存在");
  if(target.preset&&std::none_of(g->presets.begin(),g->presets.end(),[&](auto& p){return p.id==target.preset;}))return fail("请选择有效的目标预设");
 }
 return true;
}
namespace {
uint64_t Signature(const TransformRule& r){
 std::ostringstream s;auto& c=r.condition;
 s<<r.scope<<' '<<r.enabled<<' '<<(int)c.kind<<' '<<c.state<<' '<<c.weapon<<' '<<c.minimum<<' '<<c.maximum<<' '<<c.healthMin<<' '<<c.healthMax<<' '<<c.lowerOpen<<' '<<c.latch;
 for(auto& t:r.targets)s<<' '<<t.group<<' '<<t.preset;
 uint64_t h=1469598103934665603ull;for(unsigned char ch:s.str())h=(h^ch)*1099511628211ull;return h;
}
}
void EvaluateTransforms(const Document& d,TransformRuntime& rt,const PlayerSnapshot& p){
 rt.player=p;rt.activePresets.clear();rt.activeRules.clear();
 bool reset=!p.entity||p.entity!=rt.previousEntity||(p.map>=0&&rt.previousMap>=0&&p.map!=rt.previousMap)||(p.weapon>=0&&rt.previousWeapon>=0&&p.weapon!=rt.previousWeapon)||(p.healthPercent==0&&!rt.wasZero);
 if(reset)rt.latched.clear();rt.previousEntity=p.entity;if(p.map>=0)rt.previousMap=p.map;if(p.weapon>=0)rt.previousWeapon=p.weapon;
 if(p.healthPercent>=0)rt.wasZero=p.healthPercent==0;
 if(!d.enabled||!d.transformsEnabled||!p.entity){rt.latched.clear();rt.signatures.clear();return;}
 std::vector<const TransformRule*> ordered;std::set<uint64_t> ids;
 for(auto& r:d.transforms){
  if(TransformCategory(r.condition.kind)<0)continue;
  ids.insert(r.id);uint64_t sig=Signature(r);auto it=rt.signatures.find(r.id);if(it==rt.signatures.end()||it->second!=sig)rt.latched.erase(r.id);rt.signatures[r.id]=sig;
  auto mode=d.transformModes.find(r.scope);bool allowed=true;
  if(mode!=d.transformModes.end()){int category=TransformCategory(r.condition.kind);auto& setting=mode->second;allowed=setting.enabled&&(setting.parallel?setting.categories[category]:setting.selected==category);}
  if(!allowed||!r.enabled||!ValidateTransform(r,d)){rt.latched.erase(r.id);continue;}ordered.push_back(&r);
 }
 std::erase_if(rt.latched,[&](auto id){return !ids.count(id);});std::erase_if(rt.signatures,[&](auto& item){return !ids.count(item.first);});
 auto priority=[&](const TransformRule& r){auto it=d.transformModes.find(r.scope);int category=TransformCategory(r.condition.kind);int value=it!=d.transformModes.end()&&category>=0?it->second.priorities[category]*100+r.priority:r.priority;bool base=(r.condition.kind==ConditionKind::ChargeBlade||r.condition.kind==ConditionKind::SwitchAxe)&&r.condition.state<2;return value*2+(base?1:0);};
 std::stable_sort(ordered.begin(),ordered.end(),[&](auto* a,auto* b){
  int ap=priority(*a),bp=priority(*b);if(ap!=bp)return ap<bp;
  if(a->condition.kind==ConditionKind::Health&&b->condition.kind==ConditionKind::Health){if(a->condition.healthMax!=b->condition.healthMax)return a->condition.healthMax<b->condition.healthMax;if(a->condition.healthMin!=b->condition.healthMin)return a->condition.healthMin>b->condition.healthMin;}
  if(a->condition.kind==ConditionKind::LongSword&&b->condition.kind==ConditionKind::LongSword){bool exactA=a->condition.state<4,exactB=b->condition.state<4;if(exactA!=exactB)return exactA;}
  return false;
 });
 for(auto* rule:ordered){
  bool matched=Matches(rule->condition,p);
  if(rule->condition.latch){if(matched)rt.latched.insert(rule->id);matched=(matched||rt.latched.count(rule->id))&&p.healthPercent>0&&(rule->condition.weapon<0||rule->condition.weapon==p.weapon);}
  if(!matched)continue;bool won=false;
  for(auto& target:rule->targets)if(target.preset&&rt.activePresets.emplace(target.group,target.preset).second)won=true;
  if(won)rt.activeRules.insert(rule->id);
 }
}
void UpdateTransforms(){auto& s=Get();EvaluateTransforms(s.data,s.transform,ReadPlayerSnapshot());}
void ResetTransforms(){Get().transform={};}
uint64_t AddHealthNode(const std::string& scope){
 auto& s=Get();if(scope.empty()||scope.size()>128||s.data.transforms.size()>=2048)return 0;
 TransformRule r;r.id=s.data.nextId++;r.name="自定义血量";r.scope=scope;r.condition.kind=ConditionKind::Health;auto id=r.id;s.data.transforms.push_back(std::move(r));ResetTransforms();Changed("血量节点已添加");return id;
}
bool DeleteHealthNode(uint64_t id,const std::string& scope){
 auto& s=Get();auto it=std::find_if(s.data.transforms.begin(),s.data.transforms.end(),[&](auto& r){return r.id==id&&r.scope==scope&&r.condition.kind==ConditionKind::Health;});
 if(it==s.data.transforms.end())return false;s.data.transforms.erase(it);ResetTransforms();Changed("血量节点已删除");return true;
}
uint64_t ActivePreset(const Group& g){auto& s=Get();if(!s.data.enabled||!s.data.transformsEnabled)return 0;auto at=s.transform.activePresets.find(g.id);return at==s.transform.activePresets.end()?0:at->second;}
const Rules& EffectiveRules(const Group& g){uint64_t id=ActivePreset(g);if(id)for(auto& p:g.presets)if(p.id==id)return p.rules;return g.working;}
}
