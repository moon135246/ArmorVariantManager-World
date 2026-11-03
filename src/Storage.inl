// Included by Profiles.cpp: settings, canonical equipment files and migration.
bool IsCurrentModel(const std::string& path){auto& s=Get();return !s.trackingPlayer||(s.outfit.ready&&std::find(s.outfit.paths.begin(),s.outfit.paths.end(),Normalize(path))!=s.outfit.paths.end());}
bool CanEditScope(const std::string& scope){auto& s=Get();if(!s.storageReady||s.blockedScopes.count(scope)||s.outfitChanging)return false;if(!s.trackingPlayer)return true;return s.outfit.ready&&std::any_of(s.outfit.paths.begin(),s.outfit.paths.end(),[&](auto& p){return !p.empty()&&Equip(p)==scope;});}
bool SaveSettings(){
 auto& s=Get();if(!s.storageReady)return false;auto& d=s.data;if(d.transformsEnabled&&d.hotkeysEnabled){s.notice="变身与预设快捷键不能同时开启，未保存设置";return false;}std::ostringstream text;
 text<<"WorldDiffSettings 123\nSET "<<d.enabled<<' '<<d.autoDefault<<' '<<d.autoOpen<<' '<<d.menuKey<<' '<<d.scale<<' '<<d.hideDragHint<<' '<<d.transformsEnabled<<' '<<d.autoFind<<' '<<d.showStartupNotice<<' '<<d.hotkeysEnabled<<' '<<static_cast<int>(d.theme)<<'\n';
 std::string before;if(!ReadText(s.configPath,before)||(!before.empty()&&before==text.str()))return before==text.str();
 if(!AtomicText(s.configPath,text.str())){s.notice="设置保存失败，尚未保存的编辑仍在内存中";return false;}return true;
}
bool LoadSettings(){
 auto& s=Get();std::ifstream in(s.configPath,std::ios::binary);std::string magic,tag;unsigned version=0;Document d;
 if(!(in>>magic>>version)||magic!="WorldDiffSettings"||(version!=117&&version!=121&&version!=122&&version!=123)||!(in>>tag)||tag!="SET"||!(in>>d.enabled>>d.autoDefault>>d.autoOpen>>d.menuKey>>d.scale>>d.hideDragHint>>d.transformsEnabled>>d.autoFind)||(version>=121&&!(in>>d.showStartupNotice))||(version>=122&&!(in>>d.hotkeysEnabled))||(d.hotkeysEnabled&&d.transformsEnabled)||d.menuKey<0x70||d.menuKey>0x87||d.scale<.65f||d.scale>1.7f){s.notice="Settings.cfg 无效，原文件保留";return false;}
 int theme=0;
 if(version>=123&&(!(in>>theme)||theme<0||theme>1)){s.notice="设置中的界面风格无效，原文件保留";return false;}
 in>>std::ws;if(!in.eof()){s.notice="设置文件含有未知内容，已停止读取";return false;}
 auto& to=s.data;to.theme=static_cast<UiTheme>(theme);to.enabled=d.enabled;to.autoDefault=d.autoDefault;to.autoOpen=d.autoOpen;to.menuKey=d.menuKey;to.scale=d.scale;to.hideDragHint=d.hideDragHint;to.transformsEnabled=d.transformsEnabled;to.autoFind=d.autoFind;to.showStartupNotice=d.showStartupNotice;to.hotkeysEnabled=d.hotkeysEnabled;s.captureHotkey=0;s.keysAllowed=false;return true;
}
namespace {
bool CompleteMapping(const std::string& scope,const File& file){
 auto models=CurrentModels(scope,true);if(models.empty())return false;
 for(auto* model:models)for(auto& mesh:model->meshes){if(mesh.materialId>=model->materials.size()||!mesh.vertexCount||!mesh.indexCount)return false;auto hash=model->materials[mesh.materialId].hash;
  if(std::none_of(file.meshes.begin(),file.meshes.end(),[&](auto& b){return ProfilePart(b.model)==model->part&&b.material==hash&&b.vertices==mesh.vertexCount&&b.indices==mesh.indexCount;}))return false;
 }return true;
}
bool MatchesEquipment(const std::string& scope,const File& file){
 if(file.source!=scope)return false;
 for(auto* model:CurrentModels(scope,true)){
  auto path=Normalize(model->relativePath);auto stamp=file.stamps.find(path);
  if(stamp!=file.stamps.end()){if(stamp->second!=ModelStamp(*model))return false;continue;}
  // Legacy WDPs have no stamp: require an exact table match, never only a name.
  std::vector<MeshBinding> bindings;for(auto& b:file.meshes)if(b.model==path)bindings.push_back(b);if(bindings.size()!=model->meshes.size())return false;
  for(auto& mesh:model->meshes){if(mesh.materialId>=model->materials.size())return false;auto b=std::find_if(bindings.begin(),bindings.end(),[&](auto& row){return row.index==mesh.index;});if(b==bindings.end()||b->material!=model->materials[mesh.materialId].hash||b->vertices!=mesh.vertexCount||b->indices!=mesh.indexCount||b->lod!=mesh.lod)return false;}
 }
 return !CurrentModels(scope,true).empty();
}
void StampCurrentViews(Document& document){auto& s=Get();for(auto& [target,view]:document.meshDisplay){auto model=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return Normalize(m.relativePath)==target.model;});if(model!=s.models.end()&&IsCurrentModel(target.model))view.signature=model->signature;}}
}
bool LoadEquipmentProfile(const std::string& scope){
 auto& s=Get();if(!EquipmentId(scope))return false;auto path=Folder()/PathUtf8(scope+".wdp");std::error_code ec;
 if(!std::filesystem::exists(path,ec)&&!ec){EraseScope(scope);Main(scope);s.loadedScopes.insert(scope);s.blockedScopes.erase(scope);s.loadedFingerprints.erase(scope);s.profileSnapshots.erase(scope);s.profileStatus[scope]="当前装备尚无预设，可手动查找或自动查找模板";return true;}
 File file;bool readable=!ec&&ReadFile(path,file);
 if(readable&&file.source!=scope&&CompleteMapping(scope,file))return ApplyFile(scope,file,scope+".wdp");
 if(!readable||!MatchesEquipment(scope,file)){
  EraseScope(scope);Main(scope);s.loadedScopes.insert(scope);s.blockedScopes.insert(scope);s.profileSnapshots.erase(scope);if(readable)s.loadedFingerprints[scope]=file.fingerprint;else s.loadedFingerprints.erase(scope);s.profileStatus[scope]="预设无效或与当前模型不匹配，请查找模板重新匹配；原文件保留";return false;
 }
 StampCurrentViews(file.document);InstallDocument(scope,std::move(file.document));s.loadedFingerprints[scope]=file.fingerprint;s.profileSnapshots[scope]=Fingerprint(ProfileText(scope));s.profileStatus[scope]="已载入装备预设："+scope+".wdp";return true;
}
bool ReloadEquipmentProfile(const std::string& scope){auto& s=Get();if(!EquipmentId(scope))return false;if(HasConfiguration(scope)&&!Backup(scope,ProfileText(scope),"reload")){s.notice="当前编辑备份失败，未重新载入";return false;}bool okay=LoadEquipmentProfile(scope);TrimBackups(scope);return okay;}
std::vector<std::string> ProfileBackups(const std::string& scope){
 if(!EquipmentId(scope))return {};std::vector<std::string> names;std::error_code ec;auto folder=BackupFolder(scope);if(!std::filesystem::exists(folder,ec))return names;
 for(std::filesystem::directory_iterator it(folder,ec),end;it!=end&&!ec;it.increment(ec)){auto name=Utf8(it->path().filename().wstring());if(it->is_regular_file(ec)&&SafeFile(name))names.push_back(name);}std::sort(names.rbegin(),names.rend());return names;
}
bool RestoreProfileBackup(const std::string& scope,const std::string& name){
 auto& s=Get();if(!CanEditScope(scope)||!SafeFile(name))return false;File file;if(!ReadFile(BackupFolder(scope)/PathUtf8(name),file)||!MatchesEquipment(scope,file)){s.notice="这份备份不匹配当前模型，未恢复";return false;}
 if(!SaveEquipmentProfiles())return false;std::string before;if(!ReadText(Folder()/PathUtf8(scope+".wdp"),before)||!Backup(scope,before,"restore"))return false;
 auto original=s.data;auto snapshots=s.profileSnapshots;StampCurrentViews(file.document);InstallDocument(scope,std::move(file.document));s.profileSnapshots.erase(scope);if(!before.empty())s.loadedFingerprints[scope]=Fingerprint(before);
 if(!ExportProfile(scope)){s.data=std::move(original);s.profileSnapshots=std::move(snapshots);ResetTransforms();return false;}TrimBackups(scope);Changed("已恢复当前装备的预设备份");return true;
}
bool MigrateStorage(){
 auto& s=Get();InitializeProfiles();std::error_code ec;std::filesystem::path legacy;Document document;
 for(auto name:{L"WorldDiff115.cfg",L"WorldDiff110.cfg",L"WorldDiff109.cfg",L"WorldDiff108.cfg",L"WorldDiff107.cfg",L"WorldDiff104.cfg",L"WorldDiff103.cfg"}){
  auto path=s.directory/name;if(!std::filesystem::exists(path,ec)){if(ec)return false;continue;}std::ifstream in(path,std::ios::binary);if(!ReadDocument(in,document)){s.notice="旧配置无法读取，未开始迁移";return false;}legacy=path;break;
 }
 auto archive=s.directory/L"Archive"/(L"Before117-"+std::to_wstring(GetTickCount64()));std::vector<std::filesystem::path> oldConfigs,oldTemplates;
 for(std::filesystem::directory_iterator it(s.directory,ec),end;it!=end&&!ec;it.increment(ec)){auto name=Utf8(it->path().filename().wstring());if(it->is_regular_file(ec)&&name.rfind("WorldDiff",0)==0&&it->path().extension()==L".cfg")oldConfigs.push_back(it->path());}if(ec)return false;
 for(std::filesystem::directory_iterator it(Folder(),ec),end;it!=end&&!ec;it.increment(ec))if(it->is_regular_file(ec)&&it->path().extension()==L".wdp"&&!EquipmentId(Utf8(it->path().stem().wstring())))oldTemplates.push_back(it->path());if(ec)return false;
 bool existing=!oldConfigs.empty()||!oldTemplates.empty()||!std::filesystem::is_empty(Folder(),ec);if(ec)return false;
 if(existing){
  std::filesystem::create_directories(archive,ec);if(ec)return false;
  for(auto& path:oldConfigs){std::filesystem::copy_file(path,archive/path.filename(),std::filesystem::copy_options::none,ec);if(ec)return false;}
  std::filesystem::copy(Folder(),archive/L"Presets",std::filesystem::copy_options::recursive,ec);if(ec)return false;
 }
 // Templates are independent copies. Never bind an equipment back to its source.
 for(auto& path:oldTemplates){std::string content,before;if(!ReadText(path,content))return false;auto to=Templates()/path.filename();if(!ReadText(to,before))return false;if(!before.empty()&&before!=content)to=Templates()/(path.stem().wstring()+L"-legacy-"+PathUtf8(Fingerprint(content)).wstring()+L".wdp");if(!AtomicText(to,content))return false;}
 auto original=s.data;s.data=std::move(document);s.migrating=true;bool okay=true;std::set<std::string> scopes;for(auto& g:s.data.groups)if(HasConfiguration(g.scope))scopes.insert(g.scope);
 for(auto& scope:scopes)if(!ExportProfile(scope)){okay=false;break;}
 if(okay)okay=SaveSettings();s.migrating=false;
 if(!okay){s.data=std::move(original);s.notice="迁移未完成，旧配置与归档仍保留，稍后可重试";return false;}
 // Commit marker is Settings.cfg. Only now remove byte-verified archived originals.
 for(auto& path:oldConfigs){std::string before,copied;if(ReadText(path,before)&&ReadText(archive/path.filename(),copied)&&before==copied)std::filesystem::remove(path,ec);}
 for(auto& path:oldTemplates){std::string before,copied;if(ReadText(path,before)&&ReadText(archive/L"Presets"/path.filename(),copied)&&before==copied)std::filesystem::remove(path,ec);}
 auto backups=s.directory/L"Backups";if(existing&&std::filesystem::exists(backups,ec)){
  // Old CFG snapshots are historical data, separate from the new per-file WDP history.
  std::vector<std::filesystem::path> oldBackups;for(std::filesystem::directory_iterator it(backups,ec),end;it!=end&&!ec;it.increment(ec))if(it->is_regular_file(ec)&&it->path().extension()==L".cfg")oldBackups.push_back(it->path());
  if(!oldBackups.empty()){std::filesystem::create_directories(archive/L"Backups",ec);for(auto& path:oldBackups)std::filesystem::rename(path,archive/L"Backups"/path.filename(),ec);}
 }
 // No historical equipment documents stay active after migration.
 s.data.groups.clear();s.data.hotkeys.clear();s.captureHotkey=0;s.keysAllowed=false;s.data.transforms.clear();s.data.transformModes.clear();s.data.meshDisplay.clear();s.data.bindings.clear();s.profileSnapshots.clear();s.loadedFingerprints.clear();s.loadedScopes.clear();s.backedUpScopes.clear();s.dirty=false;
 s.notice=existing?"已迁移：装备预设独立保存，原配置与旧预设已归档":"当前装备尚无预设";return true;
}
void FollowPlayerOutfit(const OutfitSnapshot& snapshot){
 auto& s=Get();for(auto& ctx:localModelContexts)ctx.store(0,std::memory_order_relaxed);
 if(!snapshot.ready){s.outfit.ready=false;s.pendingOutfit={};s.outfitSince=0;s.outfitChanging=true;return;}
 bool same=s.outfit.ready&&s.outfit.savePreview==snapshot.savePreview&&s.outfit.entity==snapshot.entity&&s.outfit.paths==snapshot.paths&&s.outfit.resources==snapshot.resources&&s.outfit.contexts==snapshot.contexts;
 if(same){s.outfitChanging=false;for(size_t i=0;i<5;i++)localModelContexts[i].store(snapshot.contexts[i],std::memory_order_relaxed);return;}
 s.outfitChanging=true;s.captureHotkey=0;s.keysAllowed=false;
 if(s.pendingOutfit.savePreview!=snapshot.savePreview||s.pendingOutfit.entity!=snapshot.entity||s.pendingOutfit.paths!=snapshot.paths||s.pendingOutfit.resources!=snapshot.resources||s.pendingOutfit.contexts!=snapshot.contexts){s.pendingOutfit=snapshot;s.outfitSince=GetTickCount();return;}
 if((DWORD)(GetTickCount()-s.outfitSince)<300)return;
 // Flush the old model before replacing its metadata or allowing new edits.
 bool saved=SaveEquipmentProfiles();if(!saved){s.outfit.ready=false;return;}std::vector<Model> models;
 for(size_t i=0;i<5;i++)if(!snapshot.paths[i].empty()){
  auto resource=s.modelResources.find(snapshot.paths[i]);if(resource!=s.modelResources.end()&&resource->second==snapshot.resources[i])continue;
  Model model;if(!ReadRuntimeModel(snapshot.resources[i],snapshot.paths[i],model)){s.outfit.ready=false;return;}LoadMaterialOrder(model,s.gameRoot);models.push_back(std::move(model));
 }
 std::set<std::string> previous=s.loadedScopes,current;for(auto& p:snapshot.paths)if(!p.empty())current.insert(Equip(p));
 for(auto& model:models){auto at=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return m.relativePath==model.relativePath;});if(at==s.models.end())s.models.push_back(std::move(model));else *at=std::move(model);}
 for(size_t i=0;i<5;i++)if(!snapshot.paths[i].empty())s.modelResources[snapshot.paths[i]]=snapshot.resources[i];s.outfit=snapshot;s.outfitChanging=false;
 if(saved)for(auto& scope:previous)if(!current.count(scope)){EraseScope(scope);s.loadedScopes.erase(scope);s.profileSnapshots.erase(scope);s.loadedFingerprints.erase(scope);s.blockedScopes.erase(scope);s.profileStatus.erase(scope);}
 for(auto& scope:current){if(!saved&&s.loadedScopes.count(scope))continue;LoadEquipmentProfile(scope);}ResetTransforms();
 for(size_t i=0;i<5;i++)localModelContexts[i].store(snapshot.contexts[i],std::memory_order_relaxed);
}
