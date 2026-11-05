// Included by State.cpp. All callers use the State mutex (recursive for UI).
bool ValidMenuKey(int key){return key==0||(key>=VK_F1&&key<=VK_F24)||key==VK_HOME||key==VK_END||key==VK_INSERT||key==VK_PRIOR||key==VK_NEXT;}
bool IsMenuKey(int key,const Document& d){return d.menuKey?key==d.menuKey:key==VK_F9||key==VK_HOME;}
std::string MenuKeyName(const Document& d){return d.menuKey?PresetKeyName(d.menuKey,0):"F9 / Home";}
bool SetMenuKey(int key){
 auto& s=Get();if(!ValidMenuKey(key))return false;Document candidate;candidate.menuKey=key;
 for(auto& h:s.data.hotkeys)if(h.key&&IsMenuKey(h.key,candidate)){s.notice="面板键与预设方案「"+h.name+"」冲突，请先修改该方案的快捷键。";return false;}
 s.data.menuKey=key;s.captureHotkey=0;s.keysAllowed=false;Changed("面板快捷键已保存："+MenuKeyName(s.data));return true;
}
bool ValidPresetKey(int key,int modifiers){
 if(modifiers<0||modifiers>7||(key==VK_F9&&(modifiers&1))||(key==VK_F4&&(modifiers&4)))return false;
 return (key>='A'&&key<='Z')||(key>='0'&&key<='9')||(key>=VK_F1&&key<=VK_F12)||(key>=VK_NUMPAD0&&key<=VK_DIVIDE)||key==VK_SPACE||key==VK_TAB||(key>=VK_PRIOR&&key<=VK_DOWN)||key==VK_INSERT||key==VK_DELETE||(key>=VK_OEM_1&&key<=VK_OEM_3)||(key>=VK_OEM_4&&key<=VK_OEM_8);
}
std::string PresetKeyName(int key,int modifiers){
 if(!key)return "未设置";std::string result;if(modifiers&1)result+="Ctrl+";if(modifiers&2)result+="Shift+";if(modifiers&4)result+="Alt+";
 if((key>='A'&&key<='Z')||(key>='0'&&key<='9'))return result+char(key);
 if(key>=VK_F1&&key<=VK_F12)return result+"F"+std::to_string(key-VK_F1+1);
 if(key>=VK_NUMPAD0&&key<=VK_NUMPAD9)return result+"Num "+std::to_string(key-VK_NUMPAD0);
 wchar_t name[128]{};LONG scan=(LONG)(MapVirtualKeyW((UINT)key,MAPVK_VK_TO_VSC)<<16);if(key>=VK_PRIOR&&key<=VK_DELETE)scan|=1<<24;
 if(GetKeyNameTextW(scan,name,128))return result+Utf8(name);return result+"VK "+std::to_string(key);
}
bool ValidateHotkey(const PresetHotkey& h,const Document& d,bool requireTargets){
 if(!h.id||h.name.empty()||h.name.size()>160||h.scope.size()!=10||h.scope.rfind("pl",0)!=0||h.scope[5]!='_'||h.targets.size()>64||(!h.key&&h.modifiers)||h.key&&!ValidPresetKey(h.key,h.modifiers))return false;
 for(size_t i=2;i<h.scope.size();i++)if(i!=5&&(h.scope[i]<'0'||h.scope[i]>'9'))return false;
 for(unsigned char c:h.name)if(c<32)return false;
 if(requireTargets&&(h.targets.empty()||(h.key&&IsMenuKey(h.key,d))))return false;std::set<uint64_t> groups;
 for(auto& target:h.targets){if(!target.group||!groups.insert(target.group).second)return false;auto g=std::find_if(d.groups.begin(),d.groups.end(),[&](auto& row){return row.id==target.group&&row.scope==h.scope;});
  if(g==d.groups.end()||(!target.preset&&requireTargets))return false;if(target.preset&&std::none_of(g->presets.begin(),g->presets.end(),[&](auto& p){return p.id==target.preset;}))return false;
 }return true;
}
bool SetAutomationEnabled(bool hotkeys,bool enabled){
 auto& s=Get();if(enabled&&(hotkeys?s.data.transformsEnabled:s.data.hotkeysEnabled)){
  s.notice=hotkeys?"变身功能和预设快捷键只能启用其中一个。请先关闭变身功能，再开启预设快捷键。":"变身功能和预设快捷键只能启用其中一个。请先关闭预设快捷键，再开启变身功能。";return false;
 }
 (hotkeys?s.data.hotkeysEnabled:s.data.transformsEnabled)=enabled;s.captureHotkey=0;s.keysAllowed=false;ResetTransforms();Changed(enabled?"功能已启用":"功能已关闭");return true;
}
PresetHotkey* FindHotkey(uint64_t id){for(auto& h:Get().data.hotkeys)if(h.id==id)return &h;return nullptr;}
uint64_t CreateHotkey(const std::string& scope,const std::string& name){
 auto& s=Get();if(!CanEditScope(scope)||!GoodName(name)||s.data.hotkeys.size()>=512)return 0;
 for(auto& h:s.data.hotkeys)if(h.scope==scope&&h.name==name)return 0;
 PresetHotkey h;h.id=s.data.nextId;h.scope=scope;h.name=name;if(!ValidateHotkey(h,s.data))return 0;s.data.nextId++;s.data.hotkeys.push_back(h);Changed("快捷键方案已创建");return h.id;
}
bool SetHotkeyBinding(uint64_t id,int key,int modifiers){
 auto& s=Get();auto* h=FindHotkey(id);if(!h||!CanEditScope(h->scope)||(!key&&modifiers)||(key&&(!ValidPresetKey(key,modifiers)||IsMenuKey(key,s.data)))){s.notice="此按键不可用；当前面板键、Ctrl+F9、Esc、Enter 与系统组合键不能用于预设。";return false;}
 for(auto& other:s.data.hotkeys)if(other.id!=id&&other.scope==h->scope&&key&&other.key==key&&other.modifiers==modifiers){s.notice="当前装备已有方案使用此快捷键，请换一个按键。";return false;}
 h->key=key;h->modifiers=modifiers;Changed("快捷键已保存："+PresetKeyName(key,modifiers));return true;
}
bool SetHotkeyTarget(uint64_t id,size_t index,TransformTarget target){
 auto* h=FindHotkey(id);if(!h||!CanEditScope(h->scope)||index>h->targets.size()||index>=64)return false;auto copy=*h;
 if(index==copy.targets.size())copy.targets.push_back(target);else copy.targets[index]=target;
 if(!ValidateHotkey(copy,Get().data))return false;h->targets=std::move(copy.targets);Changed("预设组合已保存");return true;
}
bool DeleteHotkey(uint64_t id){auto& s=Get();auto* h=FindHotkey(id);if(!h||!CanEditScope(h->scope))return false;std::erase_if(s.data.hotkeys,[&](auto& row){return row.id==id;});if(s.captureHotkey==id)s.captureHotkey=0;Changed("快捷键方案已删除");return true;}
bool RenameHotkey(uint64_t id,const std::string& name){auto& s=Get();auto* h=FindHotkey(id);if(!h||!CanEditScope(h->scope)||!GoodName(name))return false;for(auto& row:s.data.hotkeys)if(row.id!=id&&row.scope==h->scope&&row.name==name)return false;h->name=name;Changed("快捷键方案已重命名");return true;}
static bool ApplyHotkeyBatch(const std::vector<uint64_t>& ids){
 auto& s=Get();if(!s.storageReady||!s.data.enabled||!s.data.hotkeysEnabled||s.data.transformsEnabled||s.outfitChanging)return false;
 // Validate every reference before changing any group, including mixed outfits.
 std::map<uint64_t,uint64_t> targets;
 for(auto id:ids){auto* h=FindHotkey(id);if(!h||!h->enabled||!CanEditScope(h->scope)||!ValidateHotkey(*h,s.data,true))return false;
  for(auto& t:h->targets)if(!targets.emplace(t.group,t.preset).second)return false;
 }
 if(targets.empty())return false;bool changed=false;
 for(auto& [gid,pid]:targets){auto* g=Find(gid);auto p=std::find_if(g->presets.begin(),g->presets.end(),[&](auto& row){return row.id==pid;});
  changed|=g->selected!=pid||g->working.hidden!=p->rules.hidden||g->working.shown!=p->rules.shown||(s.data.autoDefault&&g->defaultPreset!=pid);
  g->working=p->rules;g->selected=pid;if(s.data.autoDefault)g->defaultPreset=pid;
 }
 if(changed)Changed("已应用快捷键预设组合");else s.notice="当前已是该预设组合";return true;
}
bool ApplyHotkey(uint64_t id){return ApplyHotkeyBatch({id});}
bool ProcessPresetKeys(const std::array<bool,256>& keys,bool allowRuntime,bool allowCapture,bool(*textInputActive)()){
 auto& s=Get();auto before=s.previousKeys;s.previousKeys=keys;
 if(!s.keysPrimed){s.keysPrimed=true;s.keysAllowed=allowRuntime;return false;}
 bool newlyAllowed=allowRuntime&&!s.keysAllowed;s.keysAllowed=allowRuntime;
 int modifiers=(keys[VK_CONTROL]||keys[VK_LCONTROL]||keys[VK_RCONTROL]?1:0)|(keys[VK_SHIFT]||keys[VK_LSHIFT]||keys[VK_RSHIFT]?2:0)|(keys[VK_MENU]||keys[VK_LMENU]||keys[VK_RMENU]?4:0);
 if(keys[VK_LWIN]||keys[VK_RWIN])return false;
 if(s.captureHotkey){
  if(!allowCapture)return false;if(keys[VK_ESCAPE]&&!before[VK_ESCAPE]){s.captureHotkey=0;s.notice="已取消快捷键录制";return false;}
  for(int key=8;key<256;key++)if(keys[key]&&!before[key]&&key!=VK_CONTROL&&key!=VK_SHIFT&&key!=VK_MENU&&!(key>=VK_LSHIFT&&key<=VK_RMENU)){
   if(SetHotkeyBinding(s.captureHotkey,key,modifiers))s.captureHotkey=0;return false;
  }return false;
 }
 if(!allowRuntime||newlyAllowed||!s.data.hotkeysEnabled||s.data.transformsEnabled||!s.data.enabled||s.outfitChanging||(s.trackingPlayer&&(!s.outfit.ready||s.outfit.savePreview)))return false;
 std::vector<uint64_t> matches;
 for(auto& h:s.data.hotkeys)if(h.enabled&&h.key&&!IsMenuKey(h.key,s.data)&&keys[h.key]&&!before[h.key]&&h.modifiers==modifiers&&CanEditScope(h.scope))matches.push_back(h.id);
 if(matches.empty()||(textInputActive&&textInputActive()))return false;return ApplyHotkeyBatch(matches);
}
