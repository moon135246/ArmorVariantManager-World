#include "State.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
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
bool GlobalHidden(const std::string& p,uint32_t mesh,uint32_t material){for(auto& g:Get().data.groups)if(g.global&&InGroup(g,p,material)&&RuleHidden(g.working,p,mesh,material))return true;return false;}
bool Hidden(const std::string& p,uint32_t mesh,uint32_t material){if(GlobalHidden(p,mesh,material))return true;if(auto* owner=Owner(p,material))return RuleHidden(owner->working,p,mesh,material);for(auto& g:Get().data.groups)if(g.main&&g.scope==Equip(p))return RuleHidden(g.working,p,mesh,material);return false;}
void Changed(const std::string& text){auto& s=Get();s.dirty=true;s.dirtyAt=GetTickCount();if(!text.empty())s.notice=text;}
static void ClearMaterialMeshOverrides(Rules& r,const Target& material){auto& s=Get();for(auto& m:s.models)if(Normalize(m.relativePath)==material.model)for(auto& mesh:m.meshes)if(mesh.materialId<m.materials.size()&&m.materials[mesh.materialId].hash==material.value){r.hidden.erase({material.model,mesh.index,true});r.shown.erase({material.model,mesh.index,true});}}
void SetVisible(Group& g,const Target& t,bool visible){
 auto& r=g.working;if(t.mesh){if(visible){r.hidden.erase(t);r.shown.insert(t);}else{r.shown.erase(t);r.hidden.insert(t);}}
 else{ClearMaterialMeshOverrides(r,t);if(visible)r.hidden.erase(t);else r.hidden.insert(t);}
 Changed();
}
bool CreateGroup(const std::string& name,const std::string& scope,bool global,const std::set<Target>& members,uint64_t& id){
 auto& s=Get();if(!GoodName(name)||scope.empty()||members.empty())return false;
 for(auto& g:s.data.groups)if(g.scope==scope&&g.name==name)return false;
 for(auto& t:members){if(t.mesh||Equip(t.model)!=scope)return false;if(!global&&Owner(t.model,t.value))return false;}
 Group g;g.id=s.data.nextId++;g.name=name;g.scope=scope;g.global=global;g.members=members;id=g.id;
 if(!global)for(auto& old:s.data.groups)if(old.main&&old.scope==scope){
  for(auto& t:members){old.working.hidden.erase(t);ClearMaterialMeshOverrides(old.working,t);for(auto& p:old.presets){p.rules.hidden.erase(t);ClearMaterialMeshOverrides(p.rules,t);}}
 }
 s.data.groups.push_back(std::move(g));Changed("分组已创建");return true;
}
bool DeleteGroup(uint64_t id){auto& s=Get();auto it=std::find_if(s.data.groups.begin(),s.data.groups.end(),[&](auto& g){return g.id==id;});if(it==s.data.groups.end()||it->main)return false;s.data.groups.erase(it);Changed("分组已删除，材质控制权已归还主列表");return true;}
bool RenameGroup(uint64_t id,const std::string& name){auto* g=Find(id);if(!g||g->main||!GoodName(name))return false;for(auto& x:Get().data.groups)if(x.id!=id&&x.scope==g->scope&&x.name==name)return false;g->name=name;Changed("分组已重命名");return true;}
bool SavePreset(Group& g,const std::string& name,bool overwrite){
 if(!GoodName(name))return false;auto& s=Get();for(auto& p:g.presets)if(p.name==name){if(!overwrite)return false;p.rules=g.working;g.selected=p.id;Changed("预设已覆盖");return true;}
 Preset p;p.id=s.data.nextId++;p.name=name;p.rules=g.working;g.selected=p.id;g.presets.push_back(std::move(p));if(s.data.autoDefault)g.defaultPreset=g.selected;Changed("预设已保存");return true;
}
bool ApplyPreset(Group& g,uint64_t id){for(auto& p:g.presets)if(p.id==id){g.working=p.rules;g.selected=id;if(Get().data.autoDefault)g.defaultPreset=id;Changed("已应用："+p.name);return true;}return false;}
bool DeletePreset(Group& g,uint64_t id){auto it=std::find_if(g.presets.begin(),g.presets.end(),[&](auto& p){return p.id==id;});if(it==g.presets.end())return false;g.presets.erase(it);if(g.selected==id)g.selected=0;if(g.defaultPreset==id)g.defaultPreset=0;Changed("预设已删除");return true;}
bool SetDefault(Group& g,uint64_t id){if(id)for(auto& p:g.presets)if(p.id==id){g.defaultPreset=id;Changed("启动默认预设已保存");return true;}if(!id){g.defaultPreset=0;Changed();return true;}return false;}
void Restore(Group& g){g.working={};g.selected=0;Changed("当前分组已恢复游戏显隐");}
void RestoreAll(){auto& s=Get();for(auto& g:s.data.groups){g.working={};g.selected=0;}s.data.enabled=false;Changed("已暂停全部控制，游戏显隐已恢复；预设仍然保留");}
static void WriteRules(std::ostream& f,const Rules& r,uint64_t gid,uint64_t pid){for(auto& t:r.hidden)f<<"RULE "<<gid<<' '<<pid<<" 0 "<<t.mesh<<' '<<std::quoted(t.model)<<' '<<t.value<<'\n';for(auto& t:r.shown)f<<"RULE "<<gid<<' '<<pid<<" 1 "<<t.mesh<<' '<<std::quoted(t.model)<<' '<<t.value<<'\n';}
void WriteDocument(std::ostream& f,const Document& d){
 f<<"WorldDiffInternal 104\nSET "<<d.enabled<<' '<<d.autoDefault<<' '<<d.autoOpen<<' '<<d.menuKey<<' '<<d.scale<<' '<<d.nextId<<'\n';
 f<<"AUTOFIND "<<d.autoFind<<'\n';
 for(const auto& [scope,file]:d.bindings)f<<"BINDING "<<std::quoted(scope)<<' '<<std::quoted(file)<<'\n';
 for(const auto& g:d.groups){f<<"GROUP "<<g.id<<' '<<std::quoted(g.name)<<' '<<std::quoted(g.scope)<<' '<<g.main<<' '<<g.global<<' '<<g.selected<<' '<<g.defaultPreset<<'\n';for(const auto& t:g.members)f<<"MEMBER "<<g.id<<' '<<std::quoted(t.model)<<' '<<t.value<<'\n';WriteRules(f,g.working,g.id,0);for(const auto& p:g.presets){f<<"PRESET "<<g.id<<' '<<p.id<<' '<<std::quoted(p.name)<<'\n';WriteRules(f,p.rules,g.id,p.id);}}
}
bool Save(){
 auto& s=Get();std::error_code ec;std::filesystem::create_directories(s.directory,ec);auto temp=s.configPath;temp+=L".tmp";
 SaveProfiles();
 std::ofstream f(temp,std::ios::binary|std::ios::trunc);if(!f){s.notice="保存失败：配置目录不可写";return false;}
 WriteDocument(f,s.data);
 f.flush();if(!f){s.notice="保存失败：写入不完整";return false;}f.close();
 if(!MoveFileExW(temp.c_str(),s.configPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){s.notice="保存失败：无法替换配置文件";return false;}s.dirty=false;return true;
}
bool ReadDocument(std::istream& f,Document& result){
 std::string magic;unsigned version=0;f>>magic>>version;if(magic!="WorldDiffInternal"||(version!=103&&version!=104))return false;
 Document d;std::string line;std::getline(f,line);bool valid=true;size_t records=0;
 while(std::getline(f,line)&&valid){if(++records>200000){valid=false;break;}std::istringstream in(line);std::string tag;in>>tag;if(tag.empty())continue;
  if(tag=="SET"){in>>d.enabled>>d.autoDefault>>d.autoOpen>>d.menuKey>>d.scale>>d.nextId;}
  else if(tag=="AUTOFIND"){in>>d.autoFind;}
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
 if(highest>1000000000||d.nextId>1000000001)return false;d.nextId=std::max(d.nextId,highest+1);result=std::move(d);return true;
}
bool Load(){auto& s=Get();std::ifstream f(s.configPath,std::ios::binary);if(!f)return false;Document d;if(!ReadDocument(f,d)){s.notice="配置损坏，未应用；原文件保留";return false;}s.data=std::move(d);s.notice="配置与默认预设已自动加载";return true;}
void Import102(const std::filesystem::path& file){
 auto& s=Get();std::ifstream f(file,std::ios::binary);if(!f){s.notice="未找到旧配置，请将 WorldDiff102.ini 放入本插件配置目录";return;}
 std::string line,section;std::map<std::string,Rules> sections;
 while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]==';')continue;if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);continue;}if(section!="Current"&&section.rfind("Preset:",0)!=0)continue;auto pos=line.find_last_of('|');if(pos==std::string::npos)continue;try{char type='H';size_t begin=0;if(line.size()>2&&line[1]=='|'){type=line[0];begin=2;}Target t{Normalize(line.substr(begin,pos-begin)),(uint32_t)std::stoull(line.substr(pos+1)),type!='M'};if(Equip(t.model).empty())continue;(type=='S'?sections[section].shown:sections[section].hidden).insert(t);}catch(...){}}
 for(auto& [name,r]:sections){std::set<std::string> scopes;for(auto& t:r.hidden)scopes.insert(Equip(t.model));for(auto& t:r.shown)scopes.insert(Equip(t.model));for(auto& scope:scopes){auto& g=Main(scope);Rules part;for(auto& t:r.hidden)if(Equip(t.model)==scope)part.hidden.insert(t);for(auto& t:r.shown)if(Equip(t.model)==scope)part.shown.insert(t);if(name=="Current")g.working=part;else{Preset p;p.id=s.data.nextId++;p.name=name.substr(7);p.rules=part;g.presets.push_back(p);}}}Changed("旧版规则与预设已导入主列表");
}
void Initialize(SharedState* live,const std::filesystem::path& dir,const std::filesystem::path& root){auto& s=Get();std::lock_guard lock(s.mutex);s.live=live;s.directory=dir/L"WorldDiff";s.gameRoot=root;s.configPath=s.directory/L"WorldDiff104.cfg";std::vector<std::string> errors;s.models=LoadCatalog(root,&errors);for(auto& m:s.models)m.relativePath=Normalize(m.relativePath);if(!Load()){std::ifstream legacy(s.directory/L"WorldDiff103.cfg",std::ios::binary);Document old;if(legacy&&ReadDocument(legacy,old)){s.data=std::move(old);s.migrating=true;Changed("1.0.3 分组与预设已迁移，旧配置保留");}}InitializeProfiles();s.loaded=true;}
void Tick(){
 auto& s=Get();std::lock_guard lock(s.mutex);if(!s.live)return;DWORD now=GetTickCount();auto n=std::min<LONG>(std::max<LONG>((LONG)s.live->rowCount,0),(LONG)kMaxRows);
 for(LONG i=0;i<n;i++){auto& r=s.live->rows[i];if(r.ready!=1)continue;std::string path=Normalize(r.modelPath);if(Equip(path).empty())continue;auto at=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return m.relativePath==path;});if(at==s.models.end()){Model m;m.relativePath=path;m.equip=Equip(path);auto p=path.find('\\',path.find(m.equip)+m.equip.size()+1);auto start=path.find(m.equip)+m.equip.size()+1;m.part=path.substr(start,p-start);m.display=path.substr(path.find_last_of('\\')+1);s.models.push_back(m);at=s.models.end()-1;}
  auto& model=*at;if(r.materialId>=65536)continue;if(model.materials.size()<=r.materialId)model.materials.resize(r.materialId+1);auto& mat=model.materials[r.materialId];if(mat.name!=r.materialName||mat.hash!=r.materialHash){mat.name=r.materialName;mat.hash=r.materialHash;mat.mrlIndex=-1;}
  auto mesh=std::find_if(model.meshes.begin(),model.meshes.end(),[&](auto& m){return m.index==r.meshIndex;});if(mesh==model.meshes.end()){Mesh m;m.index=r.meshIndex;model.meshes.push_back(m);mesh=model.meshes.end()-1;}mesh->materialId=(uint16_t)r.materialId;mesh->groupId=(uint16_t)r.groupId;mesh->lod=(uint16_t)r.lod;
  if((DWORD)(now-(DWORD)r.lastTick)<1500)s.lastSeen[path]=(DWORD)r.lastTick;
  Main(model.equip);InterlockedExchange(&r.hide,Hidden(path,r.meshIndex,r.materialHash)?1:0);
 }
 InterlockedExchange(&s.live->enabled,s.data.enabled?1:0);InterlockedExchange(&s.live->heartbeat,(LONG)now);
 TickProfiles();
 if(s.dirty&&(DWORD)(now-s.dirtyAt)>450)Save();
}
}
