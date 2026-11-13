#include "State.h"
#include "ArmorNames.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <optional>
#include <cctype>
namespace worlddiff::app {
std::string Utf8(const std::wstring& text){if(text.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),nullptr,0,nullptr,nullptr);std::string r(n,' ');WideCharToMultiByte(CP_UTF8,0,text.data(),(int)text.size(),r.data(),n,nullptr,nullptr);return r;}
std::filesystem::path PathUtf8(const std::string& text){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),nullptr,0);if(n<=0)return {};std::wstring r(n,L' ');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),(int)text.size(),r.data(),n);return r;}
std::string ProfilePart(const std::string& p){auto start=p.find("_equip\\");if(start==std::string::npos)return {};start=p.find('\\',start+7);if(start==std::string::npos)return {};auto end=p.find('\\',++start);return p.substr(start,end-start);}
namespace {
struct File {std::map<uint32_t,std::string> materialNames;std::string source;Document document;std::set<uint32_t> materials;std::vector<MeshBinding> meshes;std::map<std::string,uint64_t> stamps;std::string fingerprint;bool catalogFallback=false;};
bool InactiveModel(const std::string& path){return path.starts_with("?variant/")||path.starts_with("?variant\\");}
bool HasConfiguration(const std::string& scope){if(Get().data.equipmentHeights.contains(scope))return true;for(auto& [t,v]:Get().data.meshDisplay)if(Equip(t.model)==scope)return true;for(auto& r:Get().data.transforms)if(r.scope==scope)return true;for(auto& h:Get().data.hotkeys)if(h.scope==scope)return true;if(auto it=Get().data.transformModes.find(scope);it!=Get().data.transformModes.end()&&it->second.enabled)return true;for(auto& g:Get().data.groups)if(g.scope==scope&&(!g.main||!g.presets.empty()||!g.working.hidden.empty()||!g.working.shown.empty()))return true;return false;}
std::string Lower(std::string x){for(auto& c:x)if(c>='A'&&c<='Z')c+=32;return x;}
std::string Fingerprint(const std::string& x){uint64_t h=1469598103934665603ull;for(unsigned char c:x)h=(h^c)*1099511628211ull;return std::to_string(h);}
bool SafeFile(const std::string& file){if(file.empty()||file.size()>220||file.find_first_of("<>:\"/\\|?*\r\n")!=std::string::npos||file.back()=='.'||file.back()==' ')return false;auto p=PathUtf8(file);auto stem=Lower(Utf8(p.stem().wstring()));if(stem=="con"||stem=="prn"||stem=="aux"||stem=="nul"||(stem.size()==4&&(stem.substr(0,3)=="com"||stem.substr(0,3)=="lpt")&&stem[3]>='1'&&stem[3]<='9'))return false;return !p.empty()&&Lower(Utf8(p.extension().wstring()))==".wdp";}
bool EquipmentId(const std::string& scope){if(scope.size()!=10||scope.substr(0,2)!="pl"||scope[5]!='_')return false;for(size_t i=2;i<scope.size();i++)if(i!=5&&(scope[i]<'0'||scope[i]>'9'))return false;return true;}
std::filesystem::path Folder(){return Get().directory/L"Presets";}
std::filesystem::path Templates(){return Get().directory/L"Templates";}
uint64_t ModelStamp(const Model& model){
 // +0x18 becomes a runtime vertex-layout handle; +0x2A and +0x48 are mutable.
 // Only fields verified equal between disk MOD3 and live tables identify an asset.
 std::ostringstream text;for(auto& m:model.materials)text<<std::quoted(m.name)<<' '<<m.hash<<'\n';for(auto& m:model.meshes)text<<m.index<<' '<<m.materialId<<' '<<m.groupId<<' '<<m.vertexCount<<' '<<m.indexCount<<' '<<m.lod<<'\n';return std::stoull(Fingerprint(text.str()));
}
void AddModel(File& file,const Model& model){
 std::map<uint32_t,uint32_t> ordinals;
 auto ordered=model.meshes;std::sort(ordered.begin(),ordered.end(),[](auto& a,auto& b){return a.index<b.index;});
 for(auto& m:ordered){if(m.materialId>=model.materials.size())continue;auto hash=model.materials[m.materialId].hash;file.materialNames[hash]=model.materials[m.materialId].name;file.materials.insert(hash);file.meshes.push_back({Normalize(model.relativePath),m.index,hash,ordinals[hash]++,m.vertexCount,m.indexCount,m.lod});}
}
bool ReadFile(const std::filesystem::path& path,File& file,bool allowCatalogFallback=true){
 std::error_code ec;if(std::filesystem::file_size(path,ec)>8*1024*1024||ec)return false;std::ifstream input(path,std::ios::binary);if(!input)return false;std::string raw((std::istreambuf_iterator<char>(input)),{});file={};file.fingerprint=Fingerprint(raw);std::istringstream in(raw);std::string first;unsigned version=0;in>>first>>version;
 if(first=="WorldDiffProfile"&&(version==104||version==107||version==108||version==109||version==110||version==115||version==117||version==122||version==128||version==140)){std::string line;std::getline(in,line);size_t count=0;bool doc=false;std::optional<EquipmentHeight> storedHeight;while(std::getline(in,line)){if(++count>100000)return false;std::istringstream row(line);std::string tag;row>>tag;if(tag=="SOURCE")row>>std::quoted(file.source);else if(tag=="HEIGHT"&&version==140){
  EquipmentHeight height;
  if(file.source.empty()||!(row>>height.enabled>>height.tenthsCm)||height.tenthsCm< -100||height.tenthsCm>500||storedHeight.has_value())return false;
  std::string extra;if(row>>extra)return false;storedHeight=height;
  continue;
 }else if(tag=="STAMP"&&version>=117){std::string stampPath;uint64_t stamp=0;row>>std::quoted(stampPath)>>stamp;stampPath=Normalize(stampPath);if(stampPath.size()>255||!stamp||file.stamps.count(stampPath))return false;file.stamps[stampPath]=stamp;}else if(tag=="MAT"){std::string part,name;uint32_t hash=0;row>>std::quoted(part)>>hash>>std::quoted(name);if(name.size()>127||MaterialNameHash(name)!=hash)return false;if(file.materialNames.count(hash)&&file.materialNames.at(hash)!=name)return false;file.materialNames[hash]=name;file.materials.insert(hash);}else if(tag=="MESH"){MeshBinding m;row>>std::quoted(m.model)>>m.index>>m.material>>m.ordinal>>m.vertices>>m.indices>>m.lod;m.model=Normalize(m.model);if(m.model.size()>255||m.index>=65536)return false;file.meshes.push_back(m);}else if(tag=="DOCUMENT"){doc=ReadDocument(in,file.document);break;}else if(!tag.empty())return false;if(row.fail())return false;}if(!doc||(version==140&&!storedHeight))return false;if(storedHeight)file.document.equipmentHeights[file.source]=*storedHeight;}
 else{std::istringstream legacy(raw);if(!ReadDocument(legacy,file.document))return false;if(file.document.groups.empty())return false;file.source=file.document.groups.front().scope;}
 if(!EquipmentId(file.source))return false;
 if(std::count_if(file.document.groups.begin(),file.document.groups.end(),[](auto& g){return g.main;})!=1)return false;
 for(auto& g:file.document.groups){if(g.scope!=file.source)return false;auto collect=[&](const Rules& r){for(auto* set:{&r.hidden,&r.shown})for(auto& t:*set)if(!t.mesh)file.materials.insert(t.value);};for(auto& t:g.members)file.materials.insert(t.value);collect(g.working);for(auto& p:g.presets)collect(p.rules);}
 std::set<Target> owners;for(auto& g:file.document.groups)if(!g.main&&!g.global)for(auto& t:g.members)if(!owners.insert(t).second)return false;
 for(auto& r:file.document.transforms)if(r.scope!=file.source)return false;for(auto& h:file.document.hotkeys)if(h.scope!=file.source)return false;for(auto& [scope,mode]:file.document.transformModes)if(scope!=file.source)return false;
 for(auto& [target,view]:file.document.meshDisplay)if(Equip(target.model)!=file.source)return false;
 for(auto& [model,stamp]:file.stamps)if(Equip(model)!=file.source)return false;
 for(auto& mesh:file.meshes){if(Equip(mesh.model)!=file.source)return false;file.materials.insert(mesh.material);if(InactiveModel(mesh.model))file.document.inactiveMeshes.push_back(mesh);}
 if(allowCatalogFallback&&file.meshes.empty()){file.catalogFallback=true;for(auto& m:Get().models)if(m.equip==file.source)AddModel(file,m);}
 for(auto& [hash,name]:file.materialNames)file.document.materialNames[{file.source,hash}]=name;
 return true;
}
std::vector<const Model*> CurrentModels(const std::string& scope,bool liveOnly){
 auto& s=Get();std::vector<const Model*> out;for(auto& m:s.models)if(m.equip==scope&&(!liveOnly||IsCurrentModel(m.relativePath)))out.push_back(&m);return out;
}
std::set<uint32_t> CurrentMaterials(const std::string& scope){std::set<uint32_t> hashes;for(auto* m:CurrentModels(scope,true))for(auto& mesh:m->meshes)if(mesh.materialId<m->materials.size())hashes.insert(m->materials[mesh.materialId].hash);return hashes;}
#include "VariantMapping.inl"
bool AtomicText(const std::filesystem::path& file,const std::string& data,bool replace=true){std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);if(ec)return false;auto temp=file;temp+=L".tmp";std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(data.data(),(std::streamsize)data.size());out.flush();if(!out)return false;out.close();return MoveFileExW(temp.c_str(),file.c_str(),(replace?MOVEFILE_REPLACE_EXISTING:0)|MOVEFILE_WRITE_THROUGH)!=FALSE;}
bool ReadText(const std::filesystem::path& path,std::string& text){std::error_code ec;if(!std::filesystem::exists(path,ec))return !ec;auto size=std::filesystem::file_size(path,ec);if(ec||size>16*1024*1024)return false;std::ifstream in(path,std::ios::binary);if(!in)return false;text.assign((std::istreambuf_iterator<char>(in)),{});return !in.bad();}
std::filesystem::path BackupFolder(const std::string& key){return Get().directory/L"Backups"/PathUtf8(key);}
bool Backup(const std::string& key,const std::string& text,const char* reason){
 if(text.empty())return true;auto folder=BackupFolder(key);std::error_code ec;std::filesystem::create_directories(folder,ec);if(ec)return false;
 auto stamp=Fingerprint(text);for(std::filesystem::directory_iterator it(folder,ec),end;it!=end&&!ec;it.increment(ec)){if(!it->is_regular_file(ec)||it->path().extension()!=L".wdp")continue;std::string old;if(!ReadText(it->path(),old))return false;if(old==text)return true;}if(ec)return false;
 SYSTEMTIME time{};GetSystemTime(&time);char prefix[64];sprintf_s(prefix,"%04u%02u%02u-%02u%02u%02u%03uZ-",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
 return AtomicText(folder/PathUtf8(std::string(prefix)+reason+"-"+stamp+".wdp"),text);
}
void TrimBackups(const std::string& key){
 std::error_code ec;auto folder=BackupFolder(key);std::vector<std::filesystem::directory_entry> files;if(!std::filesystem::exists(folder,ec))return;
 for(std::filesystem::directory_iterator it(folder,ec),end;it!=end&&!ec;it.increment(ec))if(it->is_regular_file(ec)&&it->path().extension()==L".wdp")files.push_back(*it);if(ec)return;
 std::sort(files.begin(),files.end(),[](auto& a,auto& b){std::error_code ae,be;auto at=a.last_write_time(ae),bt=b.last_write_time(be);return at!=bt?at>bt:a.path()>b.path();});
 for(size_t i=5;i<files.size();i++)std::filesystem::remove(files[i].path(),ec);
}
void EraseScope(const std::string& scope){auto& s=Get();s.data.equipmentHeights.erase(scope);std::erase_if(s.data.inactiveMeshes,[&](auto& m){return Equip(m.model)==scope;});std::erase_if(s.data.materialNames,[&](auto& entry){return entry.first.first==scope;});std::erase_if(s.data.groups,[&](auto& g){return g.scope==scope;});std::erase_if(s.data.transforms,[&](auto& r){return r.scope==scope;});std::erase_if(s.data.hotkeys,[&](auto& h){return h.scope==scope;});if(s.hotkeyDraft.scope==scope)s.hotkeyDraft={};s.captureHotkey=false;s.keysAllowed=false;std::erase_if(s.data.meshDisplay,[&](auto& item){return Equip(item.first.model)==scope;});s.data.transformModes.erase(scope);s.data.bindings.erase(scope);}
void InstallDocument(const std::string& scope,Document doc){
 auto& s=Get();EraseScope(scope);s.data.equipmentHeights.insert(doc.equipmentHeights.begin(),doc.equipmentHeights.end());s.data.materialNames.insert(doc.materialNames.begin(),doc.materialNames.end());s.data.inactiveMeshes.insert(s.data.inactiveMeshes.end(),doc.inactiveMeshes.begin(),doc.inactiveMeshes.end());std::set<uint64_t> used;for(auto& g:s.data.groups){used.insert(g.id);for(auto& p:g.presets)used.insert(p.id);}for(auto& r:s.data.transforms)used.insert(r.id);for(auto& h:s.data.hotkeys)used.insert(h.id);
 s.data.nextId=std::max(s.data.nextId,doc.nextId);auto unique=[&](uint64_t old){if(!used.insert(old).second){while(used.count(s.data.nextId))s.data.nextId++;old=s.data.nextId++;used.insert(old);}return old;};
 std::map<uint64_t,uint64_t> groups;std::map<std::pair<uint64_t,uint64_t>,uint64_t> presets;
 for(auto& g:doc.groups){auto old=g.id;g.id=unique(old);groups[old]=g.id;for(auto& p:g.presets){auto previous=p.id;p.id=unique(previous);presets[{old,previous}]=p.id;if(g.selected==previous)g.selected=p.id;if(g.defaultPreset==previous)g.defaultPreset=p.id;}s.data.groups.push_back(std::move(g));}
 for(auto& r:doc.transforms){r.id=unique(r.id);for(auto& t:r.targets){auto old=t.group;t.group=groups.at(old);if(t.preset)t.preset=presets.at({old,t.preset});}s.data.transforms.push_back(std::move(r));}
 for(auto& h:doc.hotkeys){h.id=unique(h.id);for(auto& t:h.targets){auto old=t.group;t.group=groups.at(old);if(t.preset)t.preset=presets.at({old,t.preset});}s.data.hotkeys.push_back(std::move(h));}
 for(auto& [t,v]:doc.meshDisplay)s.data.meshDisplay[t]=v;for(auto& [name,mode]:doc.transformModes)s.data.transformModes[name]=mode;
 Main(scope);s.data.bindings[scope]=scope+".wdp";s.loadedScopes.insert(scope);s.blockedScopes.erase(scope);ResetTransforms();
}
}
static std::string ProfileText(const std::string& scope,bool freshIdentity=false){
 auto& s=Get();Document part;for(auto& h:s.data.hotkeys)if(h.scope==scope){part.hotkeys.push_back(h);part.nextId=std::max(part.nextId,h.id+1);}if(auto mode=s.data.transformModes.find(scope);mode!=s.data.transformModes.end())part.transformModes[scope]=mode->second;for(auto& r:s.data.transforms)if(r.scope==scope){part.transforms.push_back(r);part.nextId=std::max(part.nextId,r.id+1);}for(auto& g:s.data.groups)if(g.scope==scope){part.groups.push_back(g);part.nextId=std::max(part.nextId,g.id+1);for(auto& p:g.presets)part.nextId=std::max(part.nextId,p.id+1);}if(part.groups.empty())return {};
 for(auto& [target,view]:s.data.meshDisplay)if(Equip(target.model)==scope)part.meshDisplay[target]=view;
 std::set<Target> referenced;
 for(auto& g:part.groups){
  referenced.insert(g.members.begin(),g.members.end());
  auto collect=[&](const Rules& r){referenced.insert(r.hidden.begin(),r.hidden.end());referenced.insert(r.shown.begin(),r.shown.end());};
  collect(g.working);for(auto& preset:g.presets)collect(preset.rules);
 }
 for(auto& [target,view]:part.meshDisplay)referenced.insert(target);
 const auto height=GetEquipmentHeight(scope);
 const bool hasHeight=height!=EquipmentHeight{};
 const bool inactive=std::any_of(referenced.begin(),referenced.end(),[](auto& t){return InactiveModel(t.model);});
 std::ostringstream text;text<<"WorldDiffProfile "<<(hasHeight?140:inactive?128:122)<<"\nSOURCE "<<std::quoted(scope)<<'\n';
 if(hasHeight)text<<"HEIGHT "<<height.enabled<<' '<<height.tenthsCm<<'\n';
 std::set<std::pair<std::string,uint32_t>> signatures;
 File metadata;if(!freshIdentity)ReadFile(Folder()/PathUtf8(scope+".wdp"),metadata);if(metadata.source!=scope)metadata={};
 if(!freshIdentity&&metadata.meshes.empty()){auto binding=s.data.bindings.find(scope);if(binding!=s.data.bindings.end())ReadFile(Folder()/PathUtf8(binding->second),metadata);if(metadata.source!=scope)metadata={};}
 for(auto& m:s.models)if(m.equip==scope&&(!s.trackingPlayer||IsCurrentModel(m.relativePath))){auto path=Normalize(m.relativePath);
  if(s.migrating){std::vector<MeshBinding> prior;for(auto& mesh:metadata.meshes)if(mesh.model==path)prior.push_back(mesh);bool verified=prior.size()==m.meshes.size()&&!prior.empty();
   for(auto& mesh:m.meshes){auto old=std::find_if(prior.begin(),prior.end(),[&](auto& row){return row.index==mesh.index;});if(old==prior.end()||mesh.materialId>=m.materials.size()||old->material!=m.materials[mesh.materialId].hash||old->vertices!=mesh.vertexCount||old->indices!=mesh.indexCount||old->lod!=mesh.lod)verified=false;}
   if(!verified){metadata.stamps.erase(path);continue;}
  }
  std::erase_if(metadata.meshes,[&](auto& mesh){return mesh.model==path;});AddModel(metadata,m);metadata.stamps[path]=ModelStamp(m);
 }
 for(auto& [path,stamp]:metadata.stamps)text<<"STAMP "<<std::quoted(path)<<' '<<stamp<<'\n';
 std::erase_if(metadata.meshes,[](auto& b){return InactiveModel(b.model);});
 for(auto& b:s.data.inactiveMeshes)if(referenced.count({b.model,b.index,true})||referenced.count({b.model,b.material,false}))metadata.meshes.push_back(b);
 for(auto& [key,name]:s.data.materialNames)if(key.first==scope)metadata.materialNames.emplace(key.second,name);
 for(auto& b:metadata.meshes)signatures.insert({ProfilePart(b.model),b.material});
 for(auto& t:referenced)if(!t.mesh)signatures.insert({ProfilePart(t.model),t.value});
 for(auto& [partName,hash]:signatures){
  auto name=metadata.materialNames.find(hash);
  if(name!=metadata.materialNames.end())text<<"MAT "<<std::quoted(partName)<<' '<<hash<<' '<<std::quoted(name->second)<<'\n';
 }
 for(auto& b:metadata.meshes)text<<"MESH "<<std::quoted(b.model)<<' '<<b.index<<' '<<b.material<<' '<<b.ordinal<<' '<<b.vertices<<' '<<b.indices<<' '<<b.lod<<'\n';text<<"DOCUMENT\n";WriteDocument(text,part,true);
 return text.str();
}
std::vector<std::string> ListExportTemplates(){
 std::vector<std::string> names;
 std::error_code ec;
 for(std::filesystem::directory_iterator it(Templates(),ec),end;!ec&&it!=end;it.increment(ec)){
  if(!it->is_regular_file(ec))continue;
  auto name=Utf8(it->path().filename().wstring());
  if(SafeFile(name))names.push_back(name);
 }
 std::sort(names.begin(),names.end());
 return names;
}
// Supplying existingFile requests create-only export and returns the conflicting filename.
static bool WriteProfile(const std::string& scope,const std::string& requested,std::string* existingFile,bool freshIdentity){
 if(existingFile){existingFile->clear();if(requested.empty())return false;}
 auto& s=Get();if(!s.storageReady||!EquipmentId(scope)||(!requested.empty()&&!s.migrating&&!CanEditScope(scope)))return false;std::string name=requested;if(name.empty())name=scope+".wdp";else if(PathUtf8(name).extension().empty())name+=".wdp";
 if(!SafeFile(name)){s.notice="预设文件名无效，请不要使用路径分隔符或 Windows 保留字符";return false;}
 std::string content=ProfileText(scope,freshIdentity);if(content.empty())return false;auto fingerprint=Fingerprint(content);
 if(requested.empty()&&s.profileSnapshots[scope]==fingerprint)return true;
 auto path=(requested.empty()?Folder():Templates())/PathUtf8(name);
 auto reportCollision=[&]{
  if(!existingFile)return false;
  WIN32_FIND_DATAW match{};
  HANDLE search=FindFirstFileW(path.c_str(),&match);
  if(search==INVALID_HANDLE_VALUE)return false;
  FindClose(search);
  *existingFile=Utf8(match.cFileName);
  return true;
 };
 if(reportCollision())return false;
 std::string before;if(!ReadText(path,before)){s.notice="预设文件读取失败，已停止保存："+name;return false;}
 if(requested.empty()&&!s.migrating&&before.empty()&&s.loadedFingerprints.count(scope)){s.notice="装备预设已被外部删除，请导出预设保留编辑，或重新读取当前装备文件";return false;}
 if(requested.empty()&&!s.migrating&&!before.empty()&&(!s.loadedFingerprints.count(scope)||s.loadedFingerprints[scope]!=Fingerprint(before))){s.notice="装备预设已被外部修改，请重新载入后再编辑："+name;return false;}
 auto key=requested.empty()?scope:"Template-"+Utf8(path.stem().wstring());bool backup=!existingFile&&!before.empty()&&content!=before&&(!requested.empty()||!s.backedUpScopes.count(scope));
 if(backup&&!Backup(key,before,requested.empty()?"edit":"export")){s.notice="备份失败，未替换预设文件";return false;}
 if((existingFile||content!=before)&&!AtomicText(path,content,!existingFile)){
  if(reportCollision())return false;
  s.notice="预设文件保存失败："+name;return false;}
 if(backup){TrimBackups(key);if(requested.empty())s.backedUpScopes.insert(scope);}
 if(requested.empty()){s.data.bindings[scope]=name;s.loadedScopes.insert(scope);s.loadedFingerprints[scope]=fingerprint;s.profileSnapshots[scope]=fingerprint;s.profileStatus[scope]="已载入装备预设："+name;}
 return true;
}
bool ExportProfile(const std::string& scope,const std::string& requested,std::string* existingFile){
 return WriteProfile(scope,requested,existingFile,false);
}
std::vector<ProfileCandidate> FindProfiles(const std::string& scope){
 std::vector<ProfileCandidate> candidates;std::error_code ec;auto folder=Templates();if(!std::filesystem::exists(folder,ec))return candidates;
 size_t count=0;for(std::filesystem::directory_iterator it(folder,ec),end;it!=end&&!ec;it.increment(ec)){if(++count>1024)break;if(!it->is_regular_file(ec))continue;auto name=Utf8(it->path().filename().wstring());if(!SafeFile(name))continue;File f;if(!ReadFile(it->path(),f))continue;ProfileCandidate c;c.file=name;c.name=Utf8(it->path().stem().wstring());c.total=f.materials.size();auto lower=Lower(c.name);c.filenameMatch=lower==Lower(scope)||lower=="f_body"+Lower(scope.substr(2));
  VariantMapping plan(f,CurrentModels(scope,true));
  c.hits=plan.sharedNames;c.total=plan.oldNames;
  c.currentMaterials=plan.newNames;c.missingMaterials=plan.missingNames.size();
  c.addedMaterials=plan.newNames-plan.sharedNames;c.unknownMaterials=plan.unknownNames;
  c.missingNames=plan.missingNames;c.addedNames=plan.addedNames;
  c.materialCoverage=plan.similarity();c.controlled=plan.controlled;c.resolved=plan.resolved;
  c.parts=plan.parts.size();c.coverage=c.controlled?double(c.resolved)/c.controlled:0;
  c.strong=plan.strong();candidates.push_back(c);
 }
 std::sort(candidates.begin(),candidates.end(),[](auto& a,auto& b){if(a.strong!=b.strong)return a.strong;if(a.materialCoverage!=b.materialCoverage)return a.materialCoverage>b.materialCoverage;if(a.coverage!=b.coverage)return a.coverage>b.coverage;if(a.hits!=b.hits)return a.hits>b.hits;return a.file<b.file;});return candidates;
}
static bool ApplyFile(const std::string& scope,const File& file,const std::string& name){
 auto& s=Get();if(!s.storageReady||!EquipmentId(scope)||!SafeFile(name)||(s.trackingPlayer&&(!s.outfit.ready||s.outfitChanging||!std::any_of(s.outfit.paths.begin(),s.outfit.paths.end(),[&](auto& p){return !p.empty()&&Equip(p)==scope;}))))return false;
 bool activeMatch=false;for(auto* model:CurrentModels(scope,true))for(auto& mesh:model->meshes)if(mesh.materialId<model->materials.size()&&file.materials.count(model->materials[mesh.materialId].hash))activeMatch=true;
 if(!activeMatch){s.notice="模板与当前实际穿着的部位没有匹配，未导入";return false;}
 VariantMapping plan(file,CurrentModels(scope,true));
 PendingTargets pending(file,scope);
 size_t skipped=0,mapped=0;std::vector<Group> groups;uint64_t next=s.data.nextId;std::map<uint64_t,uint64_t> groupIds;std::map<std::pair<uint64_t,uint64_t>,uint64_t> allPresets;
 for(const auto& original:file.document.groups){Group g=original;g.id=next++;g.scope=scope;g.members.clear();g.presets.clear();std::map<uint64_t,uint64_t> presetIds;
  auto mapSet=[&](const std::set<Target>& from){
   std::set<Target> to;
   for(auto& target:from){
    if(auto dest=plan.resolve(target)){to.insert(*dest);mapped++;}
    else{to.insert(pending.park(target));skipped++;}
   }
   return to;
  };
  auto mapRules=[&](const Rules& from){Rules to;to.hidden=mapSet(from.hidden);to.shown=mapSet(from.shown);return to;};
  g.members=mapSet(original.members);g.working=mapRules(original.working);for(const auto& old:original.presets){Preset p=old;p.id=next++;p.rules=mapRules(old.rules);presetIds[old.id]=p.id;allPresets[{original.id,old.id}]=p.id;g.presets.push_back(std::move(p));}g.selected=presetIds.count(original.selected)?presetIds[original.selected]:0;g.defaultPreset=presetIds.count(original.defaultPreset)?presetIds[original.defaultPreset]:0;
  if(g.main||!g.members.empty()){groupIds[original.id]=g.id;groups.push_back(std::move(g));}
 }
 std::map<Target,MeshDisplay> meshDisplay;
 for(auto& [old,view]:file.document.meshDisplay){
  auto dest=plan.resolve(old);
  if(!dest){meshDisplay[pending.park(old)]=view;skipped++;continue;}
  const auto& target=*dest;
  auto model=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return Normalize(m.relativePath)==target.model;});if(model==s.models.end())continue;
  auto mesh=std::find_if(model->meshes.begin(),model->meshes.end(),[&](auto& m){return m.index==target.value;});if(mesh==model->meshes.end()||mesh->materialId>=model->materials.size())continue;
  MeshDisplay value=view;value.signature=model->signature;value.material=model->materials[mesh->materialId].hash;meshDisplay[target]=std::move(value);mapped++;
 }
 bool needsMapping=!file.document.meshDisplay.empty();for(auto& g:file.document.groups){if(!g.members.empty()||!g.working.hidden.empty()||!g.working.shown.empty())needsMapping=true;for(auto& p:g.presets)if(!p.rules.hidden.empty()||!p.rules.shown.empty())needsMapping=true;}
 if(groups.empty()||(needsMapping&&!mapped&&!plan.sameFamily())){s.notice="未能将预设绑定到当前装备，当前设置保持不变";return false;}
 std::set<Target> owners;for(auto& g:groups)if(!g.main&&!g.global)for(auto& t:g.members)if(!owners.insert(t).second){s.notice="预设含有重复的普通分组归属，已取消载入";return false;}
 std::vector<TransformRule> transforms;
 for(auto& original:file.document.transforms){TransformRule r=original;r.id=next++;r.scope=scope;r.targets.clear();for(auto& t:original.targets){auto g=groupIds.find(t.group);auto p=allPresets.find({t.group,t.preset});if(g!=groupIds.end()&&(!t.preset||p!=allPresets.end()))r.targets.push_back({g->second,t.preset?p->second:0});else skipped++;}if(original.targets.empty()||!r.targets.empty())transforms.push_back(std::move(r));}
 std::vector<PresetHotkey> hotkeys;
 for(auto& old:file.document.hotkeys){auto h=old;h.id=next++;h.scope=scope;h.targets.clear();for(auto& target:old.targets){auto g=groupIds.find(target.group);auto p=allPresets.find({target.group,target.preset});if(g!=groupIds.end()&&(!target.preset||p!=allPresets.end()))h.targets.push_back({g->second,target.preset?p->second:0});else{h.enabled=false;skipped++;}}hotkeys.push_back(std::move(h));}
 if(!s.blockedScopes.count(scope)&&HasConfiguration(scope)){auto working=ProfileText(scope);if((!s.profileSnapshots.count(scope)||s.profileSnapshots[scope]!=Fingerprint(working))&&!Backup(scope,working,"draft")){s.notice="当前编辑备份失败，已取消导入";return false;}}
 auto original=s.data;auto snapshots=s.profileSnapshots,loaded=s.loadedFingerprints;auto blocked=s.blockedScopes;auto oldStatus=s.profileStatus;
 std::string before;if(!ReadText(Folder()/PathUtf8(scope+".wdp"),before)||(!before.empty()&&!Backup(scope,before,"import"))){s.notice="备份失败，已取消导入模板";return false;}
 std::erase_if(s.data.inactiveMeshes,[&](auto& m){return Equip(m.model)==scope;});
 std::erase_if(s.data.materialNames,[&](auto& entry){return entry.first.first==scope;});
 s.data.equipmentHeights.erase(scope);
 if(auto height=file.document.equipmentHeights.find(file.source);height!=file.document.equipmentHeights.end())s.data.equipmentHeights[scope]=height->second;
 for(auto& [hash,materialName]:file.materialNames)s.data.materialNames[{scope,hash}]=materialName;
 s.data.inactiveMeshes.insert(s.data.inactiveMeshes.end(),pending.bindings.begin(),pending.bindings.end());
 std::erase_if(s.data.hotkeys,[&](auto& h){return h.scope==scope;});s.data.hotkeys.insert(s.data.hotkeys.end(),hotkeys.begin(),hotkeys.end());if(s.hotkeyDraft.scope==scope)s.hotkeyDraft={};s.captureHotkey=false;s.keysAllowed=false;
 s.data.transformModes.erase(scope);if(auto mode=file.document.transformModes.find(file.source);mode!=file.document.transformModes.end())s.data.transformModes[scope]=mode->second;std::erase_if(s.data.transforms,[&](auto& r){return r.scope==scope;});s.data.transforms.insert(s.data.transforms.end(),transforms.begin(),transforms.end());ResetTransforms();
 std::erase_if(s.data.meshDisplay,[&](auto& entry){return Equip(entry.first.model)==scope;});s.data.meshDisplay.insert(meshDisplay.begin(),meshDisplay.end());
 auto& all=s.data.groups;all.erase(std::remove_if(all.begin(),all.end(),[&](auto& g){return g.scope==scope;}),all.end());all.insert(all.end(),groups.begin(),groups.end());s.data.nextId=next;s.data.bindings[scope]=scope+".wdp";Main(scope);s.blockedScopes.erase(scope);s.profileSnapshots.erase(scope);if(before.empty())s.loadedFingerprints.erase(scope);else s.loadedFingerprints[scope]=Fingerprint(before);
 if(!WriteProfile(scope,{},nullptr,true)){s.data=std::move(original);s.profileSnapshots=std::move(snapshots);s.loadedFingerprints=std::move(loaded);s.blockedScopes=std::move(blocked);s.profileStatus=std::move(oldStatus);ResetTransforms();return false;}
 TrimBackups(scope);s.backedUpScopes.insert(scope);
 Changed("已从模板 "+name+" 导入 → "+EquipmentLabel(scope)+"，保存为 "+scope+".wdp"+(skipped?"；有 "+std::to_string(skipped)+" 条记录待匹配，已保留":""));return true;
}
bool ApplyProfile(const std::string& scope,const std::string& name){File file;if(!SafeFile(name)||!ReadFile(Templates()/PathUtf8(name),file)){Get().notice="无法读取模板文件："+name;return false;}return ApplyFile(scope,file,name);}
bool RebindEquipmentProfile(const std::string& scope){File file;if(!EquipmentId(scope)||!ReadFile(Folder()/PathUtf8(scope+".wdp"),file))return false;return ApplyFile(scope,file,scope+".wdp");}
bool AutoFindProfile(const std::string& scope,bool automatic){
 auto& s=Get();auto candidates=FindProfiles(scope);std::vector<ProfileCandidate> strong;for(auto& c:candidates)if(c.strong)strong.push_back(c);
 if(strong.size()==1)return ApplyProfile(scope,strong[0].file);
 if(!automatic)s.notice=strong.size()>1?"多个模板都匹配当前模型，请使用手动查找选择":"没有可自动应用的模板，请使用手动查找";return false;
}
void InitializeProfiles(){std::error_code ec;std::filesystem::create_directories(Folder(),ec);std::filesystem::create_directories(Templates(),ec);}
bool SaveEquipmentProfiles(){auto& s=Get();if(!s.storageReady)return false;std::set<std::string> scopes;for(auto& g:s.data.groups)scopes.insert(g.scope);for(auto& scope:scopes)if(!s.blockedScopes.count(scope)&&(HasConfiguration(scope)||s.profileSnapshots.count(scope)))if(!ExportProfile(scope))return false;return true;}
void SaveProfiles(){SaveEquipmentProfiles();}
void TickProfiles(){
 auto& s=Get();DWORD now=GetTickCount();if(!s.data.autoFind||!s.storageReady)return;std::set<std::string> active;for(auto& p:s.outfit.paths)if(!p.empty())active.insert(Equip(p));
 for(auto& scope:active){auto& last=s.profileAttempts[scope];if(last&&(DWORD)(now-last)<1500)continue;last=now;
  if(!s.blockedScopes.count(scope)&&HasConfiguration(scope)&&(!s.profileSnapshots.count(scope)||s.profileSnapshots[scope]!=Fingerprint(ProfileText(scope))))continue;
  File file;auto path=Folder()/PathUtf8(scope+".wdp");std::error_code ec;if(!std::filesystem::exists(path,ec)&&!ec){if(s.data.bindings.count(scope))LoadEquipmentProfile(scope);continue;}if(!ReadFile(path,file))continue;
  if(!s.loadedFingerprints.count(scope)||s.loadedFingerprints[scope]!=file.fingerprint)LoadEquipmentProfile(scope);
 }
}
#include "Storage.inl"
uint64_t StableModelStamp(const Model& model){return ModelStamp(model);}
bool ReadPeerProfile(const std::string& scope,const std::vector<const Model*>& models,Document& document,std::string& revision){
 // Canonical files only: no template search, import, backup, migration or save.
 if(!EquipmentId(scope)||models.empty())return false;File file;
 if(!ReadFile(Folder()/PathUtf8(scope+".wdp"),file,false)||file.source!=scope)return false;
 for(auto* model:models){
  if(!model||model->equip!=scope||model->meshes.empty())return false;auto path=Normalize(model->relativePath);auto stamp=file.stamps.find(path);
  if(stamp!=file.stamps.end()){if(stamp->second!=ModelStamp(*model))return false;continue;}
  std::vector<const MeshBinding*> bindings;for(auto& b:file.meshes)if(b.model==path)bindings.push_back(&b);if(bindings.size()!=model->meshes.size())return false;
  std::set<uint32_t> seen;
  for(auto* b:bindings)if(!seen.insert(b->index).second)return false;
  for(auto& mesh:model->meshes){if(mesh.materialId>=model->materials.size())return false;auto b=std::find_if(bindings.begin(),bindings.end(),[&](auto* row){return row->index==mesh.index;});if(b==bindings.end()||(*b)->material!=model->materials[mesh.materialId].hash||(*b)->vertices!=mesh.vertexCount||(*b)->indices!=mesh.indexCount||(*b)->lod!=mesh.lod)return false;}
 }
 document=std::move(file.document);revision=file.fingerprint;return true;
}
}
