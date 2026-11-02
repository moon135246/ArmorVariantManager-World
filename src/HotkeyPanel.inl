std::string automationConflict;bool showAutomationConflict=false;
void AutomationSwitch(const char* label,bool hotkeys){
 auto& s=Get();bool enabled=hotkeys?s.data.hotkeysEnabled:s.data.transformsEnabled;
 if(ImGui::Checkbox(label,&enabled)){if(!SetAutomationEnabled(hotkeys,enabled)){automationConflict=s.notice;showAutomationConflict=true;}else Save();}
}
void AutomationConflictPopup(){
 if(showAutomationConflict){ImGui::OpenPopup("功能互斥提示");showAutomationConflict=false;}
 ImGui::SetNextWindowSizeConstraints(ImVec2(S(300),0),ImVec2(std::max(S(300),ImGui::GetIO().DisplaySize.x-S(60)),FLT_MAX));
 if(ImGui::BeginPopupModal("功能互斥提示",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+S(420));ImGui::TextUnformatted(automationConflict.c_str());ImGui::PopTextWrapPos();
  if(Button("知道了"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
 }
}
uint64_t hotkeyId=0,hotkeyNameId=0;char hotkeyName[192]{};std::string hotkeyOriginalName;
void HotkeyPanel(){
 auto& s=Get();AutomationSwitch("启用预设快捷键",true);
 ImGui::TextWrapped("按一次应用整套组合，未列入的分组保持当前选择。此功能与变身总开关二选一。");
 ImGui::BeginChild("快捷键方案编辑区",ImVec2(0,0));
 auto* current=FindHotkey(hotkeyId);if(!current||current->scope!=outfit){hotkeyId=0;for(auto& h:s.data.hotkeys)if(h.scope==outfit){hotkeyId=h.id;break;}current=FindHotkey(hotkeyId);}
 ImGui::SetNextItemWidth(-1);std::string preview=current?current->name+" · "+PresetKeyName(current->key,current->modifiers):"尚无方案";
 if(ImGui::BeginCombo("##快捷键方案",preview.c_str())){for(auto& h:s.data.hotkeys)if(h.scope==outfit){auto label=h.name+" · "+PresetKeyName(h.key,h.modifiers)+(h.enabled?"":" [已停用]");if(ImGui::Selectable(label.c_str(),h.id==hotkeyId)){hotkeyId=h.id;s.captureHotkey=0;}}ImGui::EndCombo();}
 if(Button("新建方案")){s.captureHotkey=0;for(int i=1;i<=512;i++){auto name="快捷键方案 "+std::to_string(i);if(std::none_of(s.data.hotkeys.begin(),s.data.hotkeys.end(),[&](auto& h){return h.scope==outfit&&h.name==name;})){hotkeyId=CreateHotkey(outfit,name);break;}}}
 current=FindHotkey(hotkeyId);if(!current){ImGui::TextWrapped("先在左侧保存需要的分组预设，再新建快捷键方案，将它们组合起来。");ImGui::EndChild();return;}
 ImGui::SameLine();if(Button("删除方案"))ImGui::OpenPopup("删除快捷键方案？");
 if(ImGui::BeginPopupModal("删除快捷键方案？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("仅删除此快捷键方案，已有分组和预设会保留。");if(Button("删除##快捷键")){DeleteHotkey(hotkeyId);hotkeyId=0;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##快捷键"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 current=FindHotkey(hotkeyId);if(!current){ImGui::EndChild();return;}
 if(hotkeyNameId!=hotkeyId||hotkeyOriginalName!=current->name){hotkeyNameId=hotkeyId;hotkeyOriginalName=current->name;strcpy_s(hotkeyName,current->name.c_str());}
 float saveWidth=ImGui::CalcTextSize("保存名称").x+ImGui::GetStyle().FramePadding.x*2;ImGui::SetNextItemWidth(std::max(S(90),ImGui::GetContentRegionAvail().x-saveWidth-ImGui::GetStyle().ItemSpacing.x));TextField("##快捷键名称","方案名称",hotkeyName,sizeof(hotkeyName));if(ImGui::IsItemClicked())s.captureHotkey=0;ImGui::SameLine();if(Button("保存名称"))if(!RenameHotkey(hotkeyId,hotkeyName))s.notice="方案名称为空或重复，未保存";
 if(ImGui::Checkbox("启用此方案",&current->enabled))Changed();ImGui::SameLine();ImGui::BeginDisabled(!s.data.hotkeysEnabled||s.data.transformsEnabled||!s.data.enabled||!current->enabled||!ValidateHotkey(*current,s.data,true));if(Button("立即应用"))ApplyHotkey(hotkeyId);ImGui::EndDisabled();
 ImGui::TextWrapped("快捷键：%s",PresetKeyName(current->key,current->modifiers).c_str());
 if(s.captureHotkey==hotkeyId){ImGui::TextColored(accent,"请按快捷键，可带 Ctrl / Shift / Alt；Esc 取消。");if(Button("取消录制"))s.captureHotkey=0;}
 else{if(Button("录制快捷键")){NativeInputClose();s.captureHotkey=hotkeyId;}ImGui::SameLine();if(Button("清除按键")){s.captureHotkey=0;SetHotkeyBinding(hotkeyId,0,0);}}
 ImGui::Separator();if(Button("添加目标分组")){for(auto& g:s.data.groups)if(g.scope==outfit&&!g.presets.empty()&&std::none_of(current->targets.begin(),current->targets.end(),[&](auto& t){return t.group==g.id;})){SetHotkeyTarget(hotkeyId,current->targets.size(),{g.id,0});break;}}
 auto targets=current->targets;
 if(std::none_of(s.data.groups.begin(),s.data.groups.end(),[&](auto& g){return g.scope==outfit&&!g.presets.empty();}))ImGui::TextWrapped("当前装备尚无已保存预设，请先在左侧保存预设。");
 if(targets.empty())ImGui::TextWrapped("尚未添加目标。普通分组、全局分组和主列表都可以参与。");
 for(size_t i=0;i<targets.size();i++){
  auto target=targets[i];ImGui::PushID((int)i);if(ImGui::BeginTable("快捷键目标",3,ImGuiTableFlags_SizingStretchProp)){
   ImGui::TableSetupColumn("分组",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("预设",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("移除",ImGuiTableColumnFlags_WidthFixed,S(48));ImGui::TableNextColumn();
   auto* g=Find(target.group);std::string groupLabel=g?(g->global?"[全局] ":"")+g->name:"选择分组";ImGui::SetNextItemWidth(-1);
   if(ImGui::BeginCombo("##组合分组",groupLabel.c_str())){for(auto& group:s.data.groups)if(group.scope==outfit&&!group.presets.empty()){
    bool duplicate=std::any_of(targets.begin(),targets.end(),[&](auto& row){return row.group==group.id&&row.group!=target.group;});ImGui::BeginDisabled(duplicate);auto label=(group.global?"[全局] ":"")+group.name;if(ImGui::Selectable(label.c_str(),group.id==target.group)){target={group.id,0};SetHotkeyTarget(hotkeyId,i,target);}ImGui::EndDisabled();}ImGui::EndCombo();}
   ImGui::TableNextColumn();g=Find(target.group);const char* preset="选择预设";if(g)for(auto& p:g->presets)if(p.id==target.preset)preset=p.name.c_str();ImGui::SetNextItemWidth(-1);ImGui::BeginDisabled(!g);
   if(ImGui::BeginCombo("##组合预设",preset)){if(g)for(auto& p:g->presets)if(ImGui::Selectable(p.name.c_str(),p.id==target.preset))SetHotkeyTarget(hotkeyId,i,{target.group,p.id});ImGui::EndCombo();}ImGui::EndDisabled();ImGui::TableNextColumn();
   if(ImGui::SmallButton("移除")){current->targets.erase(current->targets.begin()+i);Changed("目标已移除");ImGui::EndTable();ImGui::PopID();break;}ImGui::EndTable();
  }ImGui::PopID();
 }
 ImGui::EndChild();
}
