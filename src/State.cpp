#include "State.h"
#include "Multiplayer.h"
#include <algorithm>
#include <climits>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <cstdio>
namespace worlddiff::app {
State& Get(){static State* state=new State;return *state;}
std::string Normalize(std::string p){for(auto& c:p){if(c=='/')c='\\';else if(c>='A'&&c<='Z')c+=32;}if(p.size()>5&&p.substr(p.size()-5)==".mod3")p.resize(p.size()-5);return p;}
std::string Equip(const std::string& p){auto x=p.find("_equip\\");if(x==std::string::npos)return {};x+=7;auto e=p.find('\\',x);return p.substr(x,e-x);}
static bool GoodName(const std::string& name){if(name.empty()||name.size()>160)return false;for(unsigned char c:name)if(c<32)return false;return true;}
Group* Find(uint64_t id){for(auto& g:Get().data.groups)if(g.id==id)return &g;return nullptr;}
Group& Main(const std::string& scope){auto& s=Get();for(auto& g:s.data.groups)if(g.main&&g.scope==scope)return g;Group g;g.id=s.data.nextId++;g.name="主列表";g.scope=scope;g.main=true;s.data.groups.push_back(g);return s.data.groups.back();}
Group* Owner(const std::string& model,uint32_t material){Target t{model,material,false};for(auto& g:Get().data.groups)if(!g.main&&!g.global&&g.members.count(t))return &g;return nullptr;}
bool InGroup(const Group& g,const std::string& model,uint32_t material){if(Equip(model)!=g.scope)return false;if(g.main)return Owner(model,material)==nullptr;return g.members.count({model,material,false})!=0;}
bool RuleHidden(const Rules& r,const std::string& p,uint32_t mesh,uint32_t material){if(r.shown.count({p,mesh,true}))return false;return r.hidden.count({p,mesh,true})||r.hidden.count({p,material,false});}
bool GlobalHidden(const std::string& p,uint32_t mesh,uint32_t material){for(auto& g:Get().data.groups)if(g.global&&InGroup(g,p,material)&&RuleHidden(EffectiveRules(g),p,mesh,material))return true;return false;}
bool Hidden(const std::string& p,uint32_t mesh,uint32_t material){if(GlobalHidden(p,mesh,material))return true;if(auto* owner=Owner(p,material))return RuleHidden(EffectiveRules(*owner),p,mesh,material);for(auto& g:Get().data.groups)if(g.main&&g.scope==Equip(p))return RuleHidden(EffectiveRules(g),p,mesh,material);return false;}
void Changed(const std::string& text){auto& s=Get();s.dirty=true;s.dirtyAt=GetTickCount();if(!text.empty())s.notice=text;}
static void ClearMaterialMeshOverrides(Rules& r,const Target& material){auto& s=Get();for(auto& m:s.models)if(Normalize(m.relativePath)==material.model)for(auto& mesh:m.meshes)if(mesh.materialId<m.materials.size()&&m.materials[mesh.materialId].hash==material.value){r.hidden.erase({material.model,mesh.index,true});r.shown.erase({material.model,mesh.index,true});}}
void SetVisible(Group& g,const Target& t,bool visible){
 if(!CanEditScope(g.scope))return;
 auto& r=g.working;if(t.mesh){if(visible){r.hidden.erase(t);r.shown.insert(t);}else{r.shown.erase(t);r.hidden.insert(t);}}
 else{ClearMaterialMeshOverrides(r,t);if(visible)r.hidden.erase(t);else r.hidden.insert(t);}
 Changed();
}
const MeshDisplay* MeshView(const Model& model,uint32_t index){
 auto it=Get().data.meshDisplay.find({Normalize(model.relativePath),index,true});if(it==Get().data.meshDisplay.end())return nullptr;
 const Mesh* mesh=nullptr;if(index<model.meshes.size()&&model.meshes[index].index==index)mesh=&model.meshes[index];else{auto found=std::find_if(model.meshes.begin(),model.meshes.end(),[&](auto& m){return m.index==index;});if(found!=model.meshes.end())mesh=&*found;}
 if(!mesh||mesh->materialId>=model.materials.size()||it->second.material!=model.materials[mesh->materialId].hash)return nullptr;
 if(it->second.signature&&it->second.signature!=model.signature)return nullptr;return &it->second;
}
std::vector<const Mesh*> OrderedMeshes(const Model& model,bool manual){
 std::vector<const Mesh*> result;for(auto& m:model.meshes)if(m.materialId<model.materials.size())result.push_back(&m);
 std::sort(result.begin(),result.end(),[&](auto* a,auto* b){
  auto* av=manual?MeshView(model,a->index):nullptr;auto* bv=manual?MeshView(model,b->index):nullptr;
  int ar=av&&av->order>=0?av->order:INT_MAX,br=bv&&bv->order>=0?bv->order:INT_MAX;
  return std::make_tuple(ar,MaterialListOrder(model.materials[a->materialId],a->materialId),a->index)<std::make_tuple(br,MaterialListOrder(model.materials[b->materialId],b->materialId),b->index);
 });return result;
}
static MeshDisplay CurrentMeshView(const Model& model,const Mesh& mesh){
 MeshDisplay value;if(auto* old=MeshView(model,mesh.index))value=*old;value.signature=model.signature;value.material=model.materials[mesh.materialId].hash;return value;
}
bool MoveMeshBefore(const Model& model,uint32_t source,uint32_t before){
 if(!CanEditScope(model.equip))return false;
 if(source==before)return false;auto ordered=OrderedMeshes(model);auto from=std::find_if(ordered.begin(),ordered.end(),[&](auto* m){return m->index==source;});
 auto to=std::find_if(ordered.begin(),ordered.end(),[&](auto* m){return m->index==before;});if(from==ordered.end()||(before!=UINT32_MAX&&to==ordered.end()))return false;
 auto* moved=*from;ordered.erase(from);to=std::find_if(ordered.begin(),ordered.end(),[&](auto* m){return m->index==before;});ordered.insert(to,moved);
 int rank=0;auto path=Normalize(model.relativePath);for(auto* m:ordered){auto value=CurrentMeshView(model,*m);value.order=rank++;Get().data.meshDisplay[{path,m->index,true}]=std::move(value);}Changed("网格顺序已保存");return true;
}
bool SetMeshNote(const Model& model,uint32_t index,const std::string& note){
 if(!CanEditScope(model.equip))return false;
 if(!note.empty()&&!GoodName(note))return false;auto mesh=std::find_if(model.meshes.begin(),model.meshes.end(),[&](auto& m){return m.index==index;});
 if(mesh==model.meshes.end()||mesh->materialId>=model.materials.size())return false;auto value=CurrentMeshView(model,*mesh);value.note=note;Target key{Normalize(model.relativePath),index,true};
 if(value.order<0&&value.note.empty())Get().data.meshDisplay.erase(key);else Get().data.meshDisplay[key]=std::move(value);Changed(note.empty()?"网格备注已清除":"网格备注已保存");return true;
}
void ResetMeshOrder(const Model& model){
 if(!CanEditScope(model.equip))return;
 auto path=Normalize(model.relativePath);for(auto it=Get().data.meshDisplay.begin();it!=Get().data.meshDisplay.end();){if(it->first.model==path){it->second.order=-1;if(it->second.note.empty()){it=Get().data.meshDisplay.erase(it);continue;}}++it;}Changed("已恢复材质顺序，网格备注保留");
}
bool CreateGroup(const std::string& name,const std::string& scope,bool global,const std::set<Target>& members,uint64_t& id){
 if(!CanEditScope(scope))return false;
 auto& s=Get();if(!GoodName(name)||scope.empty()||members.empty())return false;
 for(auto& g:s.data.groups)if(g.scope==scope&&g.name==name)return false;
 for(auto& t:members){if(t.mesh||Equip(t.model)!=scope)return false;if(!global&&Owner(t.model,t.value))return false;}
 Group g;g.id=s.data.nextId++;g.name=name;g.scope=scope;g.global=global;g.members=members;id=g.id;
 if(!global)for(auto& old:s.data.groups)if(old.main&&old.scope==scope){
  for(auto& t:members){old.working.hidden.erase(t);ClearMaterialMeshOverrides(old.working,t);for(auto& p:old.presets){p.rules.hidden.erase(t);ClearMaterialMeshOverrides(p.rules,t);}}
 }
 s.data.groups.push_back(std::move(g));Changed("分组已创建");return true;
}
static void RemoveTransformTargets(uint64_t group,uint64_t preset=0){auto& s=Get();for(auto& r:s.data.transforms)std::erase_if(r.targets,[&](auto& t){return t.group==group&&(!preset||t.preset==preset);});for(auto& h:s.data.hotkeys){auto size=h.targets.size();std::erase_if(h.targets,[&](auto& t){return t.group==group&&(!preset||t.preset==preset);});if(size!=h.targets.size())h.enabled=false;}std::erase_if(s.data.transforms,[](auto& r){return r.targets.empty()&&r.condition.kind!=ConditionKind::Health;});ResetTransforms();}
bool DeleteGroup(uint64_t id){auto& s=Get();auto it=std::find_if(s.data.groups.begin(),s.data.groups.end(),[&](auto& g){return g.id==id;});if(it==s.data.groups.end()||it->main||!CanEditScope(it->scope))return false;s.data.groups.erase(it);RemoveTransformTargets(id);Changed("分组已删除，材质控制权已归还主列表");return true;}
bool RenameGroup(uint64_t id,const std::string& name){auto* g=Find(id);if(!g||g->main||!GoodName(name)||!CanEditScope(g->scope))return false;for(auto& x:Get().data.groups)if(x.id!=id&&x.scope==g->scope&&x.name==name)return false;g->name=name;Changed("分组已重命名");return true;}
template<class T,class Predicate>static bool InsertById(std::vector<T>& all,uint64_t source,uint64_t before,Predicate allowed){
 std::vector<size_t> slots;std::vector<uint64_t> keys;std::map<uint64_t,size_t> positions;
 for(size_t i=0;i<all.size();i++)if(allowed(all[i])){slots.push_back(i);keys.push_back(all[i].id);positions[all[i].id]=i;}
 if(source==before||!positions.count(source)||(before!=UINT64_MAX&&!positions.count(before)))return false;
 auto original=keys;keys.erase(std::find(keys.begin(),keys.end(),source));keys.insert(std::find(keys.begin(),keys.end(),before),source);if(keys==original)return false;
 std::vector<T> reordered;reordered.reserve(keys.size());for(auto key:keys)reordered.push_back(std::move(all[positions.at(key)]));
 for(size_t i=0;i<slots.size();i++)all[slots[i]]=std::move(reordered[i]);return true;
}
bool MoveGroupBefore(uint64_t source,uint64_t before){
 auto* group=Find(source);if(!group||group->main||!CanEditScope(group->scope))return false;auto scope=group->scope;
 if(!InsertById(Get().data.groups,source,before,[&](auto& g){return g.scope==scope&&!g.main;}))return false;Changed("分组顺序已保存");return true;
}
bool MovePresetBefore(Group& group,uint64_t source,uint64_t before){
 if(!CanEditScope(group.scope))return false; if(!InsertById(group.presets,source,before,[](auto&){return true;}))return false;Changed("预设顺序已保存");return true;
}
bool SavePreset(Group& g,const std::string& name,bool overwrite){
 if(!GoodName(name)||!CanEditScope(g.scope))return false;auto& s=Get();for(auto& p:g.presets)if(p.name==name){if(!overwrite)return false;p.rules=g.working;g.selected=p.id;Changed("预设已覆盖");return true;}
 Preset p;p.id=s.data.nextId++;p.name=name;p.rules=g.working;g.selected=p.id;g.presets.push_back(std::move(p));if(s.data.autoDefault)g.defaultPreset=g.selected;Changed("预设已保存");return true;
}
bool ApplyPreset(Group& g,uint64_t id){if(!CanEditScope(g.scope))return false;for(auto& p:g.presets)if(p.id==id){g.working=p.rules;g.selected=id;if(Get().data.autoDefault)g.defaultPreset=id;Changed("已应用："+p.name);return true;}return false;}
bool DeletePreset(Group& g,uint64_t id){if(!CanEditScope(g.scope))return false;auto it=std::find_if(g.presets.begin(),g.presets.end(),[&](auto& p){return p.id==id;});if(it==g.presets.end())return false;g.presets.erase(it);if(g.selected==id)g.selected=0;if(g.defaultPreset==id)g.defaultPreset=0;RemoveTransformTargets(g.id,id);Changed("预设已删除，相关变身目标已清理");return true;}
bool SetDefault(Group& g,uint64_t id){if(!CanEditScope(g.scope))return false;if(id)for(auto& p:g.presets)if(p.id==id){g.defaultPreset=id;Changed("启动默认预设已保存");return true;}if(!id){g.defaultPreset=0;Changed();return true;}return false;}
void Restore(Group& g){if(!CanEditScope(g.scope))return;g.working={};g.selected=0;Changed("当前分组已恢复游戏显隐");}
void RestoreAll(){auto& s=Get();for(auto& g:s.data.groups){g.working={};g.selected=0;}s.data.enabled=false;ResetTransforms();Changed("已暂停全部控制，游戏显隐已恢复；预设仍然保留");}
static void WriteRules(std::ostream& f,const Rules& r,uint64_t gid,uint64_t pid){for(auto& t:r.hidden)f<<"RULE "<<gid<<' '<<pid<<" 0 "<<t.mesh<<' '<<std::quoted(t.model)<<' '<<t.value<<'\n';for(auto& t:r.shown)f<<"RULE "<<gid<<' '<<pid<<" 1 "<<t.mesh<<' '<<std::quoted(t.model)<<' '<<t.value<<'\n';}
#include "Hotkeys.inl"
void WriteDocument(std::ostream& f,const Document& d,bool equipment){
 if(equipment)f<<"WorldDiffEquipment 122\nNEXTID "<<d.nextId<<'\n';
 else{f<<"WorldDiffInternal 122\nSET "<<d.enabled<<' '<<d.autoDefault<<' '<<d.autoOpen<<' '<<d.menuKey<<' '<<d.scale<<' '<<d.nextId<<' '<<d.hideDragHint<<'\n';
 f<<"AUTOFIND "<<d.autoFind<<'\n';for(const auto& [scope,file]:d.bindings)f<<"BINDING "<<std::quoted(scope)<<' '<<std::quoted(file)<<'\n';}
 for(const auto& g:d.groups){f<<"GROUP "<<g.id<<' '<<std::quoted(g.name)<<' '<<std::quoted(g.scope)<<' '<<g.main<<' '<<g.global<<' '<<g.selected<<' '<<g.defaultPreset<<'\n';for(const auto& t:g.members)f<<"MEMBER "<<g.id<<' '<<std::quoted(t.model)<<' '<<t.value<<'\n';WriteRules(f,g.working,g.id,0);for(const auto& p:g.presets){f<<"PRESET "<<g.id<<' '<<p.id<<' '<<std::quoted(p.name)<<'\n';WriteRules(f,p.rules,g.id,p.id);}}
 for(auto& [t,v]:d.meshDisplay)f<<"MESHVIEW "<<std::quoted(t.model)<<' '<<t.value<<' '<<v.signature<<' '<<v.material<<' '<<v.order<<' '<<std::quoted(v.note)<<'\n';
 if(!equipment)f<<"TRANSFORMS "<<d.transformsEnabled<<'\n';
 if(!equipment&&d.hotkeysEnabled)f<<"HOTKEYS 1\n";
 for(auto& h:d.hotkeys){f<<"HOTKEY "<<h.id<<' '<<std::quoted(h.name)<<' '<<std::quoted(h.scope)<<' '<<h.enabled<<' '<<h.key<<' '<<h.modifiers<<'\n';for(auto& t:h.targets)f<<"HKTARGET "<<h.id<<' '<<t.group<<' '<<t.preset<<'\n';}
 for(auto& [scope,m]:d.transformModes){f<<"TRMODE "<<std::quoted(scope)<<' '<<m.enabled<<' '<<m.parallel<<' '<<m.selected;for(int i=0;i<kTransformCategories;i++)f<<' '<<m.categories[i]<<' '<<m.priorities[i];f<<'\n';}
 for(auto& r:d.transforms){auto& c=r.condition;f<<"TRANSFORM "<<r.id<<' '<<std::quoted(r.name)<<' '<<std::quoted(r.scope)<<' '<<r.enabled<<' '<<r.priority<<' '<<(int)c.kind<<' '<<c.state<<' '<<c.weapon<<' '<<c.minimum<<' '<<c.maximum<<' '<<c.healthMin<<' '<<c.healthMax<<' '<<c.lowerOpen<<' '<<c.latch<<'\n';for(auto& t:r.targets)f<<"TRTARGET "<<r.id<<' '<<t.group<<' '<<t.preset<<'\n';}
}
bool Save(){
 auto& s=Get();if(!s.storageReady)return false;
 if(!SaveEquipmentProfiles()||!SaveSettings())return false;s.dirty=false;return true;
}
bool ReadDocument(std::istream& f,Document& result){
 std::string magic;unsigned version=0;f>>magic>>version;bool equipment=magic=="WorldDiffEquipment";
 if((magic!="WorldDiffInternal"&&!equipment)||(version!=103&&version!=104&&version!=107&&version!=108&&version!=109&&version!=110&&version!=115&&version!=117&&version!=122)||(equipment&&version!=117&&version!=122))return false;
 Document d;std::string line;std::getline(f,line);bool valid=true;size_t records=0;
 while(std::getline(f,line)&&valid){if(++records>200000){valid=false;break;}std::istringstream in(line);std::string tag;in>>tag;if(tag.empty())continue;
  if(equipment&&(tag=="SET"||tag=="AUTOFIND"||tag=="TRANSFORMS"||tag=="HOTKEYS"||tag=="BINDING")){valid=false;break;}
  if(tag=="NEXTID"&&equipment)in>>d.nextId;
  else if(tag=="SET"){in>>d.enabled>>d.autoDefault>>d.autoOpen>>d.menuKey>>d.scale>>d.nextId;if(in&&!in.eof()){in>>std::ws;if(!in.eof())in>>d.hideDragHint;}}
  else if(tag=="AUTOFIND"){in>>d.autoFind;}
  else if(tag=="TRANSFORMS"){in>>d.transformsEnabled;}
  else if(tag=="HOTKEYS"&&version>=122){in>>d.hotkeysEnabled;}
  else if(tag=="HOTKEY"&&version>=122){PresetHotkey h;in>>h.id>>std::quoted(h.name)>>std::quoted(h.scope)>>h.enabled>>h.key>>h.modifiers;if(d.hotkeys.size()>=512)valid=false;for(auto& old:d.hotkeys)if(old.id==h.id||(old.scope==h.scope&&(old.name==h.name||(h.key&&old.key==h.key&&old.modifiers==h.modifiers))))valid=false;d.hotkeys.push_back(std::move(h));}
  else if(tag=="HKTARGET"&&version>=122){uint64_t id=0;TransformTarget t;in>>id>>t.group>>t.preset;auto h=std::find_if(d.hotkeys.begin(),d.hotkeys.end(),[&](auto& row){return row.id==id;});if(h==d.hotkeys.end()||h->targets.size()>=64)valid=false;else h->targets.push_back(t);}
  else if(tag=="TRMODE"){std::string scope;TransformMode m;in>>std::quoted(scope)>>m.enabled>>m.parallel>>m.selected;if(version<109){bool removedLegacy=false;in>>removedLegacy;}int count=version<109?5:kTransformCategories;for(int i=0;i<count;i++){in>>m.categories[i]>>m.priorities[i];if(m.priorities[i]<1||m.priorities[i]>99)valid=false;}if(scope.empty()||scope.size()>128||m.selected<0||m.selected>=count||d.transformModes.count(scope))valid=false;d.transformModes[scope]=m;}
  else if(tag=="TRANSFORM"){
   TransformRule r;int kind=-1;auto& c=r.condition;in>>r.id>>std::quoted(r.name)>>std::quoted(r.scope)>>r.enabled>>r.priority>>kind>>c.state>>c.weapon>>c.minimum>>c.maximum>>c.healthMin>>c.healthMax>>c.lowerOpen>>c.latch;c.kind=(ConditionKind)kind;
   Condition checked=c;if(version<109&&c.kind==ConditionKind::InsectGlaive&&c.state>=0&&c.state<=7)checked.state=ExtractCount(c.state);
   if(!r.id||d.transforms.size()>=2048||!ValidCondition(checked))valid=false;
   for(auto& old:d.transforms)if(old.id==r.id)valid=false;d.transforms.push_back(std::move(r));
  }
  else if(tag=="TRTARGET"){
   uint64_t id=0;TransformTarget t;in>>id>>t.group>>t.preset;auto it=std::find_if(d.transforms.begin(),d.transforms.end(),[&](auto& r){return r.id==id;});
   if(it==d.transforms.end()||it->targets.size()>=64)valid=false;else it->targets.push_back(t);
  }
  else if(tag=="MESHVIEW"){
   Target t;MeshDisplay v;t.mesh=true;in>>std::quoted(t.model)>>t.value>>v.signature>>v.material>>v.order>>std::quoted(v.note);t.model=Normalize(t.model);
   if(version<115||t.model.empty()||t.model.size()>255||Equip(t.model).empty()||t.value>=65536||v.order < -1||v.order>=65536||(!v.note.empty()&&!GoodName(v.note))||d.meshDisplay.size()>=65536||d.meshDisplay.count(t))valid=false;
   else d.meshDisplay[t]=std::move(v);
  }
  else if(tag=="BINDING"){std::string scope,file;in>>std::quoted(scope)>>std::quoted(file);if(scope.empty()||scope.size()>128||file.size()>240)valid=false;d.bindings[scope]=file;}
  else if(tag=="GROUP"){Group g;in>>g.id>>std::quoted(g.name)>>std::quoted(g.scope)>>g.main>>g.global>>g.selected>>g.defaultPreset;if(!GoodName(g.name)||g.scope.empty()||g.scope.size()>128||!g.id||d.groups.size()>=2048)valid=false;for(auto& x:d.groups)if(x.id==g.id||(x.main&&g.main&&x.scope==g.scope))valid=false;d.groups.push_back(std::move(g));}
  else if(tag=="MEMBER"||tag=="PRESET"||tag=="RULE"){
   uint64_t gid=0;in>>gid;Group* g=nullptr;for(auto& x:d.groups)if(x.id==gid)g=&x;if(!g){valid=false;break;}
   if(tag=="MEMBER"){Target t;in>>std::quoted(t.model)>>t.value;t.model=Normalize(t.model);if(t.model.size()>255||Equip(t.model)!=g->scope)valid=false;g->members.insert(t);}
   else if(tag=="PRESET"){Preset p;in>>p.id>>std::quoted(p.name);if(!p.id||!GoodName(p.name)||g->presets.size()>2048)valid=false;for(auto& x:g->presets)if(x.id==p.id||x.name==p.name)valid=false;g->presets.push_back(std::move(p));}
   else{uint64_t pid=0;bool shown=false;Target t;in>>pid>>shown>>t.mesh>>std::quoted(t.model)>>t.value;t.model=Normalize(t.model);Rules* r=pid==0?&g->working:nullptr;for(auto& p:g->presets)if(p.id==pid)r=&p.rules;if(!r||t.model.size()>255||Equip(t.model)!=g->scope){valid=false;break;}(shown?r->shown:r->hidden).insert(t);}
  }else valid=false;
  if(in.fail())valid=false;
 }
 if(!valid||d.menuKey<0x70||d.menuKey>0x87||d.scale<0.65f||d.scale>1.7f)return false;
 uint64_t highest=0;for(auto& g:d.groups){highest=std::max(highest,g.id);for(auto& p:g.presets)highest=std::max(highest,p.id);if(g.defaultPreset)for(auto& p:g.presets)if(p.id==g.defaultPreset){g.working=p.rules;g.selected=p.id;}}
 for(auto& r:d.transforms)highest=std::max(highest,r.id);
 for(auto& h:d.hotkeys){if(!ValidateHotkey(h,d))return false;for(auto& g:d.groups){if(g.id==h.id)return false;for(auto& p:g.presets)if(p.id==h.id)return false;}for(auto& r:d.transforms)if(r.id==h.id)return false;highest=std::max(highest,h.id);}
 if(!equipment&&d.hotkeysEnabled&&d.transformsEnabled)return false;
 UpgradeTransformDocument(d,version);
 for(auto& r:d.transforms)if(!ValidateTransform(r,d))return false;
 if(highest>1000000000||d.nextId>1000000001)return false;d.nextId=std::max(d.nextId,highest+1);if(version<108)InferTransformModes(d);result=std::move(d);return true;
}
bool Load(){auto& s=Get();if(!LoadSettings())return false;auto scopes=s.loadedScopes;if(scopes.empty())for(auto& g:s.data.groups)scopes.insert(g.scope);bool okay=true;for(auto& scope:scopes)if(!LoadEquipmentProfile(scope))okay=false;ResetTransforms();return okay;}
void Import102(const std::filesystem::path& file){
 auto& s=Get();std::ifstream f(file,std::ios::binary);if(!f){s.notice="未找到旧配置，请将 WorldDiff102.ini 放入本插件配置目录";return;}
 std::string line,section;std::map<std::string,Rules> sections;
 while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]==';')continue;if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);continue;}if(section!="Current"&&section.rfind("Preset:",0)!=0)continue;auto pos=line.find_last_of('|');if(pos==std::string::npos)continue;try{char type='H';size_t begin=0;if(line.size()>2&&line[1]=='|'){type=line[0];begin=2;}Target t{Normalize(line.substr(begin,pos-begin)),(uint32_t)std::stoull(line.substr(pos+1)),type!='M'};if(Equip(t.model).empty())continue;(type=='S'?sections[section].shown:sections[section].hidden).insert(t);}catch(...){}}
 for(auto& [name,r]:sections){std::set<std::string> scopes;for(auto& t:r.hidden)scopes.insert(Equip(t.model));for(auto& t:r.shown)scopes.insert(Equip(t.model));for(auto& scope:scopes){auto& g=Main(scope);Rules part;for(auto& t:r.hidden)if(Equip(t.model)==scope)part.hidden.insert(t);for(auto& t:r.shown)if(Equip(t.model)==scope)part.shown.insert(t);if(name=="Current")g.working=part;else{Preset p;p.id=s.data.nextId++;p.name=name.substr(7);p.rules=part;g.presets.push_back(p);}}}Changed("旧版规则与预设已导入主列表");
}
void Initialize(SharedState* live,const std::filesystem::path& dir,const std::filesystem::path& root){
 auto& s=Get();std::lock_guard lock(s.mutex);ResetPlayerPolicies();s.live=live;s.directory=dir/L"WorldDiff";s.gameRoot=root;s.configPath=s.directory/L"Settings.cfg";s.trackingPlayer=true;
 std::vector<std::string> errors;s.models=LoadCatalog(root,&errors);for(auto& m:s.models)m.relativePath=Normalize(m.relativePath);
 s.storageReady=std::filesystem::exists(s.configPath)?LoadSettings():MigrateStorage();if(!s.storageReady)s.notice="配置迁移或读取失败，已暂停编辑；原文件保留。";
 s.loaded=true;
}
void Tick(){
 auto& s=Get();std::lock_guard lock(s.mutex);if(!s.live)return;DWORD now=GetTickCount();if(s.trackingPlayer)FollowPlayerOutfit(ReadPlayerOutfit());TickProfiles();UpdateTransforms();UpdatePlayerPolicies();auto policies=renderPolicies.load(std::memory_order_acquire);auto n=std::min<LONG>(std::max<LONG>((LONG)s.live->rowCount,0),(LONG)kMaxRows);
 for(LONG i=0;i<n;i++){auto& r=s.live->rows[i];if(r.ready!=1)continue;std::string path=Normalize(r.modelPath);if(Equip(path).empty())continue;
  if(s.trackingPlayer){auto context=rowContexts[i].load(std::memory_order_relaxed);auto* policy=policies?policies->Find(context):nullptr;bool current=policy&&policy->local;
   if(current&&(DWORD)(now-(DWORD)r.lastTick)<1500)s.lastSeen[path]=(DWORD)r.lastTick;
   InterlockedExchange(&r.hide,policy&&PolicyHides(*policy,(uintptr_t)r.resource,r.meshIndex,r.materialId,r.meshCount)?1:0);continue;
  }
  auto at=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return m.relativePath==path;});if(at==s.models.end()){Model m;m.relativePath=path;m.equip=Equip(path);auto p=path.find('\\',path.find(m.equip)+m.equip.size()+1);auto start=path.find(m.equip)+m.equip.size()+1;m.part=path.substr(start,p-start);m.display=path.substr(path.find_last_of('\\')+1);LoadMaterialOrder(m,s.gameRoot);s.models.push_back(std::move(m));at=s.models.end()-1;}
  auto& model=*at;if(r.materialId>=65536)continue;if(model.materials.size()<=r.materialId)model.materials.resize(r.materialId+1);auto& mat=model.materials[r.materialId];if(mat.name!=r.materialName||mat.hash!=r.materialHash){mat.name=r.materialName;mat.hash=r.materialHash;auto order=std::find(model.materialOrder.begin(),model.materialOrder.end(),mat.hash);mat.mrlIndex=order==model.materialOrder.end()?-1:(int)(order-model.materialOrder.begin());}
  auto mesh=std::find_if(model.meshes.begin(),model.meshes.end(),[&](auto& m){return m.index==r.meshIndex;});if(mesh==model.meshes.end()){Mesh m;m.index=r.meshIndex;model.meshes.push_back(m);mesh=model.meshes.end()-1;}mesh->materialId=(uint16_t)r.materialId;mesh->groupId=(uint16_t)r.groupId;mesh->lod=(uint16_t)r.lod;
  if((DWORD)(now-(DWORD)r.lastTick)<1500)s.lastSeen[path]=(DWORD)r.lastTick;
  InterlockedExchange(&r.hide,s.storageReady&&IsCurrentModel(path)&&!s.blockedScopes.count(model.equip)&&Hidden(path,r.meshIndex,r.materialHash)?1:0);
 }
 InterlockedExchange(&s.live->enabled,s.data.enabled?1:0);InterlockedExchange(&s.live->heartbeat,(LONG)now);
 if(s.trackingPlayer)sprintf_s(s.live->message,"WorldDiff 1.2.7 local=%llx preview=%d ready=%d peers=%zu matched=%zu modelInstances=%zu",(unsigned long long)s.outfit.entity,s.outfit.savePreview?1:0,s.outfit.ready&&!s.outfitChanging?1:0,s.peerCount,s.matchedPeers,policies?policies->models.size():0);
 if(s.panelSizeDirty&&(DWORD)(now-s.panelSizeChangedAt)>500){
  s.panelSizeDirty=!SaveSettings();s.panelSizeChangedAt=now;
 }
 if(s.dirty&&(DWORD)(now-s.dirtyAt)>450)Save();
}
}
