#include "State.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <optional>
namespace worlddiff::app {
std::string Utf8(const std::wstring& text){if(text.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),nullptr,0,nullptr,nullptr);std::string r(n,' ');WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),r.data(),n,nullptr,nullptr);return r;}
std::filesystem::path PathUtf8(const std::string& text){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);if(n<=0)return {};std::wstring r(n,L' ');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),r.data(),n);return r;}
std::string ProfilePart(const std::string& p){auto start=p.find("_equip\\");if(start==std::string::npos)return {};start=p.find('\\',start+7);if(start==std::string::npos)return {};auto end=p.find('\\',++start);return p.substr(start,end-start);}
namespace {
struct MeshBinding {std::string model;uint32_t index=0,material=0,ordinal=0,vertices=0,indices=0,lod=0;};
struct File {std::string source;Document document;std::set<uint32_t> materials;std::vector<MeshBinding> meshes;std::string fingerprint;};
bool HasConfiguration(const std::string& scope){for(auto& g:Get().data.groups)if(g.scope==scope&&(!g.main||!g.presets.empty()||!g.working.hidden.empty()||!g.working.shown.empty()))return true;return false;}
std::string Lower(std::string x){for(auto& c:x)if(c>='A'&&c<='Z')c+=32;return x;}
std::string Fingerprint(const std::string& x){uint64_t h=1469598103934665603ull;for(unsigned char c:x)h=(h^c)*1099511628211ull;return std::to_string(h);}
bool SafeFile(const std::string& file){if(file.empty()||file.size()>220||file.find_first_of("<>:\"/\\|?*\r\n")!=std::string::npos||file=="."||file=="..")return false;auto p=PathUtf8(file);auto extension=Lower(Utf8(p.extension().wstring()));return !p.empty()&&(extension==".wdp"||extension==".cfg");}
std::filesystem::path Folder(){return Get().directory/L"Presets";}
void AddModel(File& file,const Model& model){
 std::map<uint32_t,uint32_t> ordinals;
 auto ordered=model.meshes;std::sort(ordered.begin(),ordered.end(),[](auto& a,auto& b){return a.index<b.index;});
 for(auto& m:ordered){if(m.materialId>=model.materials.size())continue;auto hash=model.materials[m.materialId].hash;file.materials.insert(hash);file.meshes.push_back({Normalize(model.relativePath),m.index,hash,ordinals[hash]++,m.vertexCount,m.indexCount,m.lod});}
}
bool ReadFile(const std::filesystem::path& path,File& file){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>8*1024*1024||ec)return false;std::ifstream input(path,std::ios::binary);if(!input)return false;std::string raw((std::istreambuf_iterator<char>(input)),{});file={};file.fingerprint=Fingerprint(raw);std::istringstream in(raw);std::string first;unsigned version=0;in>>first>>version;
 if(first=="WorldDiffProfile"&&(version==104||version==107||version==108||version==109)){std::string line;std::getline(in,line);size_t count=0;bool doc=false;while(std::getline(in,line)){if(++count>100000)return false;std::istringstream row(line);std::string tag;row>>tag;if(tag=="SOURCE")row>>std::quoted(file.source);else if(tag=="MAT"){std::string part,name;uint32_t hash=0;row>>std::quoted(part)>>hash>>std::quoted(name);if(name.size()>127||MaterialNameHash(name)!=hash)return false;file.materials.insert(hash);}else if(tag=="MESH"){MeshBinding m;row>>std::quoted(m.model)>>m.index>>m.material>>m.ordinal>>m.vertices>>m.indices>>m.lod;m.model=Normalize(m.model);if(m.model.size()>255||m.index>=65536)return false;file.meshes.push_back(m);}else if(tag=="DOCUMENT"){doc=ReadDocument(in,file.document);break;}else if(!tag.empty())return false;if(row.fail())return false;}if(!doc)return false;}
 else{std::istringstream legacy(raw);if(!ReadDocument(legacy,file.document))return false;if(file.document.groups.empty())return false;file.source=file.document.groups.front().scope;}
 if(file.source.empty()||file.source.size()>128)return false;
 for(auto& g:file.document.groups){if(g.scope!=file.source)return false;auto collect=[&](const Rules& r){for(auto* set:{&r.hidden,&r.shown})for(auto& t:*set)if(!t.mesh)file.materials.insert(t.value);};for(auto& t:g.members)file.materials.insert(t.value);collect(g.working);for(auto& p:g.presets)collect(p.rules);}
 if(file.meshes.empty())for(auto& m:Get().models)if(m.equip==file.source)AddModel(file,m);
 return true;
}
std::vector<const Model*> CurrentModels(const std::string& scope,bool liveOnly){
 auto& s=Get();std::vector<const Model*> out;DWORD now=GetTickCount();for(auto& m:s.models)if(m.equip==scope){auto it=s.lastSeen.find(m.relativePath);if(!liveOnly||(it!=s.lastSeen.end()&&(DWORD)(now-it->second)<1800))out.push_back(&m);}return out;
}
std::set<uint32_t> CurrentMaterials(const std::string& scope){std::set<uint32_t> hashes;for(auto* m:CurrentModels(scope,true))for(auto& mesh:m->meshes)if(mesh.materialId<m->materials.size())hashes.insert(m->materials[mesh.materialId].hash);return hashes;}
std::vector<Target> Remap(const Target& t,const File& file,const std::string& scope,size_t& skipped){
 auto models=CurrentModels(scope,false);std::string part=ProfilePart(t.model);uint32_t hash=t.value;const MeshBinding* bind=nullptr;
 if(t.mesh){for(auto& b:file.meshes)if(b.model==t.model&&b.index==t.value){bind=&b;break;}if(!bind){skipped++;return {};}hash=bind->material;}
 std::vector<const Model*> matches,samePart;
 for(auto* model:models){bool has=false;for(auto& mesh:model->meshes)if(mesh.materialId<model->materials.size()&&model->materials[mesh.materialId].hash==hash){has=true;break;}if(has){matches.push_back(model);if(model->part==part)samePart.push_back(model);}}
 if(!samePart.empty())matches=samePart;else if(matches.size()!=1){skipped++;return {};}
 std::vector<Target> result;
 for(auto* model:matches){if(!t.mesh){result.push_back({Normalize(model->relativePath),hash,false});continue;}
  std::vector<const Mesh*> meshes;for(auto& mesh:model->meshes)if(mesh.materialId<model->materials.size()&&model->materials[mesh.materialId].hash==hash)meshes.push_back(&mesh);std::sort(meshes.begin(),meshes.end(),[](auto* a,auto* b){return a->index<b->index;});
  auto geometry=[&](const Mesh* m){return (!bind->vertices||!m->vertexCount||bind->vertices==m->vertexCount)&&(!bind->indices||!m->indexCount||bind->indices==m->indexCount);};
  std::vector<const Mesh*> exact;for(auto* m:meshes)if(geometry(m))exact.push_back(m);
  const Mesh* chosen=nullptr;if(exact.size()==1)chosen=exact[0];else if(bind->ordinal<meshes.size()&&geometry(meshes[bind->ordinal]))chosen=meshes[bind->ordinal];
  if(chosen)result.push_back({Normalize(model->relativePath),chosen->index,true});else skipped++;
 }if(result.empty()&&matches.empty())skipped++;return result;
}
bool AtomicText(const std::filesystem::path& file,const std::string& data){std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);if(ec)return false;auto temp=file;temp+=L".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(data.data(),(std::streamsize)data.size());out.flush();if(!out)return false;out.close();return MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;}
}
static std::string ProfileText(const std::string& scope){
 auto& s=Get();Document part;if(auto mode=s.data.transformModes.find(scope);mode!=s.data.transformModes.end())part.transformModes[scope]=mode->second;for(auto& r:s.data.transforms)if(r.scope==scope){part.transforms.push_back(r);part.nextId=std::max(part.nextId,r.id+1);}for(auto& g:s.data.groups)if(g.scope==scope){part.groups.push_back(g);part.nextId=std::max(part.nextId,g.id+1);for(auto& p:g.presets)part.nextId=std::max(part.nextId,p.id+1);}if(part.groups.empty())return {};
 std::ostringstream text;text<<"WorldDiffProfile 109\nSOURCE "<<std::quoted(scope)<<'\n';std::set<std::pair<std::string,uint32_t>> signatures;
 File metadata;for(auto& m:s.models)if(m.equip==scope){AddModel(metadata,m);for(auto& mesh:m.meshes)if(mesh.materialId<m.materials.size()){auto& mat=m.materials[mesh.materialId];if(signatures.insert({m.part,mat.hash}).second)text<<"MAT "<<std::quoted(m.part)<<' '<<mat.hash<<' '<<std::quoted(mat.name)<<'\n';}}
 for(auto& b:metadata.meshes)text<<"MESH "<<std::quoted(b.model)<<' '<<b.index<<' '<<b.material<<' '<<b.ordinal<<' '<<b.vertices<<' '<<b.indices<<' '<<b.lod<<'\n';text<<"DOCUMENT\n";WriteDocument(text,part);
 return text.str();
}
bool ExportProfile(const std::string& scope,const std::string& requested){
 auto& s=Get();std::string name=requested;if(name.empty()){auto it=s.data.bindings.find(scope);name=it!=s.data.bindings.end()?it->second:scope+".wdp";}else if(PathUtf8(name).extension().empty())name+=".wdp";
 if(!SafeFile(name)){s.notice="预设文件名无效，请不要使用路径分隔符或 Windows 保留字符";return false;}
 std::string content=ProfileText(scope);if(content.empty())return false;auto fingerprint=Fingerprint(content);
 if(requested.empty()&&s.profileSnapshots[scope]==fingerprint)return true;
 auto path=Folder()/PathUtf8(name);std::ifstream previous(path,std::ios::binary);std::string before((std::istreambuf_iterator<char>(previous)),{});
 previous.close();
 if(content!=before&&!AtomicText(path,content)){s.notice="预设文件保存失败："+name;return false;}s.data.bindings[scope]=name;s.loadedFingerprints[scope]=fingerprint;s.profileSnapshots[scope]=fingerprint;return true;
}
std::vector<ProfileCandidate> FindProfiles(const std::string& scope){
 std::vector<ProfileCandidate> candidates;std::error_code ec;auto materials=CurrentMaterials(scope);auto folder=Folder();if(!std::filesystem::exists(folder,ec))return candidates;
 size_t count=0;for(std::filesystem::directory_iterator it(folder,ec),end;it!=end&&!ec;it.increment(ec)){if(++count>1024)break;if(!it->is_regular_file(ec))continue;auto name=Utf8(it->path().filename().wstring());if(!SafeFile(name))continue;File f;if(!ReadFile(it->path(),f))continue;ProfileCandidate c;c.file=name;c.name=Utf8(it->path().stem().wstring());c.total=f.materials.size();auto lower=Lower(c.name);c.filenameMatch=lower==Lower(scope)||lower=="f_body"+Lower(scope.substr(2));
  for(auto hash:f.materials)if(materials.count(hash))c.hits++;
  std::set<std::string> parts;for(auto* m:CurrentModels(scope,true))for(auto& mesh:m->meshes)if(mesh.materialId<m->materials.size()&&f.materials.count(m->materials[mesh.materialId].hash))parts.insert(m->part);c.parts=parts.size();candidates.push_back(c);
 }
 std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){if(a.hits!=b.hits)return a.hits>b.hits;if(a.filenameMatch!=b.filenameMatch)return a.filenameMatch;if(a.parts!=b.parts)return a.parts>b.parts;double ac=a.total?(double)a.hits/a.total:0,bc=b.total?(double)b.hits/b.total:0;if(ac!=bc)return ac>bc;return a.file<b.file;});return candidates;
}
bool ApplyProfile(const std::string& scope,const std::string& name){
 auto& s=Get();if(!SafeFile(name))return false;File file;if(!ReadFile(Folder()/PathUtf8(name),file)){s.notice="无法读取预设文件："+name;return false;}
 size_t skipped=0,mapped=0;std::vector<Group> groups;uint64_t next=s.data.nextId;std::map<uint64_t,uint64_t> groupIds;std::map<std::pair<uint64_t,uint64_t>,uint64_t> allPresets;
 for(const auto& original:file.document.groups){Group g=original;g.id=next++;g.scope=scope;g.members.clear();g.presets.clear();std::map<uint64_t,uint64_t> presetIds;
  auto mapSet=[&](const std::set<Target>& from){std::set<Target> to;for(auto& target:from)for(auto& t:Remap(target,file,scope,skipped)){to.insert(t);mapped++;}return to;};
  auto mapRules=[&](const Rules& from){Rules to;to.hidden=mapSet(from.hidden);to.shown=mapSet(from.shown);return to;};
  g.members=mapSet(original.members);g.working=mapRules(original.working);for(const auto& old:original.presets){Preset p=old;p.id=next++;p.rules=mapRules(old.rules);presetIds[old.id]=p.id;allPresets[{original.id,old.id}]=p.id;g.presets.push_back(std::move(p));}g.selected=presetIds.count(original.selected)?presetIds[original.selected]:0;g.defaultPreset=presetIds.count(original.defaultPreset)?presetIds[original.defaultPreset]:0;
  if(g.main||!g.members.empty()){groupIds[original.id]=g.id;groups.push_back(std::move(g));}
 }
 bool needsMapping=false;for(auto& g:file.document.groups){if(!g.members.empty()||!g.working.hidden.empty()||!g.working.shown.empty())needsMapping=true;for(auto& p:g.presets)if(!p.rules.hidden.empty()||!p.rules.shown.empty())needsMapping=true;}
 if(groups.empty()||(needsMapping&&!mapped)){s.notice="未能将预设绑定到当前装备，当前设置保持不变";return false;}
 std::set<Target> owners;for(auto& g:groups)if(!g.main&&!g.global)for(auto& t:g.members)if(!owners.insert(t).second){s.notice="预设含有重复的普通分组归属，已取消载入";return false;}
 if(HasConfiguration(scope)){std::ostringstream old;WriteDocument(old,s.data);auto backup=s.directory/L"Backups"/(PathUtf8(scope+"-"+std::to_string(GetTickCount64())+".cfg"));if(!AtomicText(backup,old.str())){s.notice="备份失败，已取消替换当前分组";return false;}}
 std::vector<TransformRule> transforms;
 for(auto& original:file.document.transforms){TransformRule r=original;r.id=next++;r.scope=scope;r.targets.clear();for(auto& t:original.targets){auto g=groupIds.find(t.group);auto p=allPresets.find({t.group,t.preset});if(g!=groupIds.end()&&(!t.preset||p!=allPresets.end()))r.targets.push_back({g->second,t.preset?p->second:0});else skipped++;}if(original.targets.empty()||!r.targets.empty())transforms.push_back(std::move(r));}
 s.data.transformModes.erase(scope);if(auto mode=file.document.transformModes.find(file.source);mode!=file.document.transformModes.end())s.data.transformModes[scope]=mode->second;std::erase_if(s.data.transforms,[&](auto& r){return r.scope==scope;});s.data.transforms.insert(s.data.transforms.end(),transforms.begin(),transforms.end());ResetTransforms();
 auto& all=s.data.groups;all.erase(std::remove_if(all.begin(),all.end(),[&](auto& g){return g.scope==scope;}),all.end());all.insert(all.end(),groups.begin(),groups.end());s.data.nextId=next;s.data.bindings[scope]=name;s.loadedFingerprints[scope]=file.fingerprint;Main(scope);s.profileSnapshots[scope]=Fingerprint(ProfileText(scope));
 Changed("已载入 "+name+" → "+scope+"，分组和默认预设已重绑定"+(skipped?"；有 "+std::to_string(skipped)+" 条无法匹配的规则已跳过":""));return true;
}
bool AutoFindProfile(const std::string& scope,bool automatic){
 auto& s=Get();auto candidates=FindProfiles(scope);auto materials=CurrentMaterials(scope);for(auto& c:candidates){if((c.hits&&c.total&&(double)c.hits/c.total>=.03)||(materials.empty()&&c.filenameMatch)){if(ApplyProfile(scope,c.file)){s.notice="已匹配 "+c.file+"：材质 "+std::to_string(c.hits)+"/"+std::to_string(c.total)+"，覆盖 "+std::to_string(c.parts)+" 部位";return true;}return false;}}
 if(!automatic)s.notice="未找到匹配预设：扫描 "+std::to_string(candidates.size())+" 份，当前材质 "+std::to_string(materials.size())+" 个";return false;
}
void InitializeProfiles(){auto& s=Get();std::error_code ec;std::filesystem::create_directories(Folder(),ec);std::set<std::string> scopes;for(auto& g:s.data.groups)scopes.insert(g.scope);for(auto& scope:scopes){if(HasConfiguration(scope)&&!s.data.bindings.count(scope))ExportProfile(scope);else if(s.data.bindings.count(scope))s.profileSnapshots[scope]=Fingerprint(ProfileText(scope));}if(s.migrating)Changed("旧版分组、预设和默认项已迁移到 1.0.9，原配置保留");}
void SaveProfiles(){auto& s=Get();std::set<std::string> scopes;for(auto& g:s.data.groups)scopes.insert(g.scope);for(auto& scope:scopes)if(HasConfiguration(scope))ExportProfile(scope);}
void TickProfiles(){
 auto& s=Get();DWORD now=GetTickCount();if(!s.data.autoFind)return;std::set<std::string> active;for(auto& [path,tick]:s.lastSeen)if((DWORD)(now-tick)<1500)active.insert(Equip(path));
 for(auto& scope:active){auto& last=s.profileAttempts[scope];if(last&&(DWORD)(now-last)<1500)continue;last=now;
  auto binding=s.data.bindings.find(scope);if(binding==s.data.bindings.end()){if(!HasConfiguration(scope))AutoFindProfile(scope,true);continue;}
  if(s.dirty)continue;File file;if(!ReadFile(Folder()/PathUtf8(binding->second),file)){AutoFindProfile(scope,true);continue;}if(s.loadedFingerprints[scope]!=file.fingerprint)ApplyProfile(scope,binding->second);
 }
}
}
