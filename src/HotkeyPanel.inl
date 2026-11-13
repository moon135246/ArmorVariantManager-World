std::string automationConflict;bool showAutomationConflict=false;
void AutomationSwitch(const char* label,bool hotkeys){
 auto& s=Get();bool enabled=hotkeys?s.data.hotkeysEnabled:s.data.transformsEnabled;
 if(SidebarCheckbox(label,&enabled)){if(!SetAutomationEnabled(hotkeys,enabled)){automationConflict=s.notice;showAutomationConflict=true;}else Save();}
}
void AutomationConflictPopup(){
 if(showAutomationConflict){ImGui::OpenPopup("功能互斥提示");showAutomationConflict=false;}
 ImGui::SetNextWindowSizeConstraints(ImVec2(S(300),0),ImVec2(std::max(S(300),ImGui::GetIO().DisplaySize.x-S(60)),FLT_MAX));
 if(PanelModal("功能互斥提示",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+S(420));ImGui::TextUnformatted(automationConflict.c_str());ImGui::PopTextWrapPos();
  if(Button("知道了"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
 }
}
char hotkeyName[192]{};
uint64_t recentHotkey=0;
double highlightUntil=0;
bool revealHotkey=false;

void OpenHotkeyDraft(const PresetHotkey* source){
 auto& s=Get();
 s.hotkeyDraft=source?*source:PresetHotkey{};
 s.hotkeyDraft.scope=outfit;
 strcpy_s(hotkeyName,s.hotkeyDraft.name.c_str());
 s.captureHotkey=false;
 NativeInputClose();
}

std::string HotkeyEffects(const PresetHotkey& scheme){
 if(scheme.targets.empty())return "尚未添加目标分组";
 constexpr size_t visible=2;
 std::string result;
 for(size_t i=0;i<std::min(visible,scheme.targets.size());++i){
  const auto& target=scheme.targets[i];
  const auto* group=Find(target.group);
  if(i)result+="　　";
  if(!group){result+="分组已失效";continue;}
  result+=group->name+" → ";
  const auto preset=std::find_if(group->presets.begin(),group->presets.end(),[&](const auto& p){return p.id==target.preset;});
  result+=preset!=group->presets.end()?preset->name:(target.preset?"预设已失效":"未选预设");
 }
 if(scheme.targets.size()>visible)result+="　另有 "+std::to_string(scheme.targets.size()-visible)+" 组";
 return result;
}

std::string HotkeyLabel(const PresetHotkey& scheme){
 if(!scheme.key)return "未绑键";
 const char* arrow="";
 switch(scheme.key){
 case VK_UP:arrow="↑ ";break;
 case VK_DOWN:arrow="↓ ";break;
 case VK_LEFT:arrow="← ";break;
 case VK_RIGHT:arrow="→ ";break;
 }
 return arrow+PresetKeyName(scheme.key,scheme.modifiers);
}

void HotkeyChip(const std::string& label){
 if(paper::active)InlineChip(label.c_str());
 else ImGui::TextUnformatted(label.c_str());
}

// Table work rects also respect the reserved actions column at narrow widths.
void HotkeyInline(float width,bool right=false){
 const float edge=ImGui::GetCurrentWindow()->WorkRect.Max.x;
 if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+width<=edge){
  ImGui::SameLine();
  if(right)ImGui::SetCursorScreenPos({edge-width,ImGui::GetCursorScreenPos().y});
 }
}
float HotkeyButtonWidth(const char* label){
 return ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x+S(4);
}

template<class Action>
void HotkeyHeading(const char* id,const std::string& title,float actionWidth,Action action){
 ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(S(8),S(5)));
 if(ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_PadOuterX)){
  ImGui::TableSetupColumn("标题",ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("操作",ImGuiTableColumnFlags_WidthFixed,actionWidth);
  ImGui::TableNextRow();ImGui::TableNextColumn();
  ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,paper::active?paper::colors->actionArea:ImGui::GetColorU32(ImGuiCol_FrameBg));
  ImGui::AlignTextToFramePadding();
  ImGui::PushFont(PaperBold(),ImGui::GetFontSize()*1.12f);
  ImGui::TextWrapped("%s",title.c_str());
  ImGui::PopFont();
  ImGui::TableNextColumn();action();
  ImGui::EndTable();
 }
 ImGui::PopStyleVar();
}

void HotkeyList(){
 auto& s=Get();
 auto count=std::count_if(s.data.hotkeys.begin(),s.data.hotkeys.end(),[&](const auto& h){return h.scope==outfit;});
 HotkeyHeading("列表标题","方案列表  "+std::to_string(count),HotkeyButtonWidth("新建方案"),[]{
  if(Button("新建方案",0,ButtonTone::Primary))OpenHotkeyDraft(nullptr);
 });
 ImGui::Separator();
 bool empty=true;
 ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(ImGui::GetStyle().CellPadding.x,S(7)));
 if(ImGui::BeginTable("方案列表行",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH)){
  ImGui::TableSetupColumn("方案",ImGuiTableColumnFlags_WidthStretch);
  float actions=ImGui::CalcTextSize("应用编辑").x+ImGui::GetStyle().FramePadding.x*4+ImGui::GetStyle().ItemSpacing.x+S(12);
  ImGui::TableSetupColumn("操作",ImGuiTableColumnFlags_WidthFixed,actions);
  for(const auto& scheme:s.data.hotkeys){
   if(scheme.scope!=outfit)continue;
   empty=false;
   ImGui::PushID(std::to_string(scheme.id).c_str());
   ImGui::TableNextRow();ImGui::TableNextColumn();
   if(scheme.id==recentHotkey){
    if(ImGui::GetTime()<highlightUntil)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImGuiCol_Header,.6f));
   }
   ImGui::PushFont(PaperBold(),ImGui::GetFontSize());
   ImGui::TextWrapped("%s",scheme.name.c_str());ImGui::PopFont();
   if(scheme.id==recentHotkey&&revealHotkey){ImGui::SetScrollHereY(.5f);revealHotkey=false;}
   auto key=HotkeyLabel(scheme);
   HotkeyInline(ImGui::CalcTextSize(key.c_str()).x+S(16));HotkeyChip(key);
   bool conflict=scheme.key&&IsMenuKey(scheme.key,s.data);
   bool complete=ValidateHotkey(scheme,s.data,true);
   std::string status;
   if(!scheme.enabled)status="已停用";
   if(conflict||!complete){
    if(!status.empty())status+=" / ";
    status+=conflict?"与面板键冲突":"待配置";
   }
   if(!status.empty()){HotkeyInline(ImGui::CalcTextSize(status.c_str()).x+S(16));HotkeyChip(status);}
   auto effects=HotkeyEffects(scheme);
   ImGui::PushStyleColor(ImGuiCol_Text,muted);
   ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(effects.c_str());ImGui::PopTextWrapPos();
   ImGui::PopStyleColor();
   ImGui::TableNextColumn();
   ImGui::SetCursorPosY(ImGui::GetCursorPosY()+S(4));
   ImGui::BeginDisabled(!s.data.hotkeysEnabled||s.data.transformsEnabled||!s.data.enabled||!scheme.enabled||!complete);
   if(Button("应用"))ApplyHotkey(scheme.id);
   ImGui::EndDisabled();ImGui::SameLine();
   if(Button("编辑"))OpenHotkeyDraft(&scheme);
   ImGui::PopID();
  }
  ImGui::EndTable();
 }
 ImGui::PopStyleVar();
 if(empty)ImGui::TextWrapped("当前装备还没有快捷方案。");
}

void HotkeyDraftTargets(PresetHotkey& draft){
 auto& groups=Get().data.groups;

 auto included=[&](uint64_t id){return std::any_of(draft.targets.begin(),draft.targets.end(),[&](const auto& t){return t.group==id;});};
 const auto& main=Main(draft.scope);
 const float h=ImGui::GetFrameHeight();
 const float bodyWidth=HotkeyButtonWidth("添加目标分组");
 ImGui::AlignTextToFramePadding();SectionTitle("目标分组");HotkeyInline(bodyWidth+h,true);
 ImGui::BeginDisabled(draft.targets.size()>=64);
 ImGui::BeginDisabled(included(main.id));
 if(paper::active)ImGui::PushStyleColor(ImGuiCol_Button,paper::colors->button);
 if(ImGui::Button("添加目标分组",{bodyWidth,h}))draft.targets.push_back({main.id,main.selected});
 if(paper::active)ImGui::PopStyleColor();
 ImGui::EndDisabled();
 Tip("直接添加主列表；右侧箭头可选择其他分组。");
 ImGui::SameLine(0,0);
 ImGui::SetNextWindowSizeConstraints({bodyWidth+h,0},{FLT_MAX,ImGui::GetTextLineHeightWithSpacing()*12});
 if(SidebarCombo("##选择要添加的分组",nullptr,ImGuiComboFlags_NoPreview|ImGuiComboFlags_PopupAlignLeft)){
  bool available=false;
  for(const auto& g:groups){
   if(g.scope!=draft.scope||(!g.main&&g.presets.empty())||included(g.id))continue;
   available=true;
   ImGui::PushID(std::to_string(g.id).c_str());
   if(ImGui::Selectable((g.global?"[全局] "+g.name:g.name).c_str()))draft.targets.push_back({g.id,g.selected});
   ImGui::PopID();
  }
  if(!available)ImGui::TextDisabled("没有可添加的分组");
  ImGui::EndCombo();
 }
 ImGui::EndDisabled();
 if(draft.targets.empty())return;
 if(!ImGui::BeginTable("快捷键目标",3,ImGuiTableFlags_SizingStretchSame))return;
 ImGui::TableSetupColumn("分组",ImGuiTableColumnFlags_WidthStretch);
 ImGui::TableSetupColumn("预设",ImGuiTableColumnFlags_WidthStretch);
 ImGui::TableSetupColumn("操作",ImGuiTableColumnFlags_WidthFixed,ImGui::CalcTextSize("移除").x+ImGui::GetStyle().FramePadding.x*2);
 ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextColored(muted,"分组");
 ImGui::TableNextColumn();ImGui::TextColored(muted,"应用的预设");
 size_t remove=draft.targets.size();
 for(size_t i=0;i<draft.targets.size();++i){
  auto& target=draft.targets[i];auto* group=Find(target.group);
  ImGui::PushID((int)i);ImGui::TableNextRow();ImGui::TableNextColumn();
  auto label=group?(group->global?"[全局] "+group->name:group->name):"选择分组";
  ImGui::SetNextItemWidth(-1);
  if(SidebarCombo("##组合分组",label.c_str())){
   for(const auto& g:groups){
    if(g.scope!=draft.scope||g.presets.empty()||(g.id!=target.group&&included(g.id)))continue;
    if(ImGui::Selectable((g.global?"[全局] "+g.name:g.name).c_str(),g.id==target.group)&&g.id!=target.group)target={g.id,g.selected};
   }
   ImGui::EndCombo();
  }
  group=Find(target.group);
  const char* preset="选择预设";
  if(group)for(const auto& p:group->presets)if(p.id==target.preset)preset=p.name.c_str();
  ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
  if(SidebarCombo("##组合预设",preset)){
   if(group)for(const auto& p:group->presets)if(ImGui::Selectable(p.name.c_str(),p.id==target.preset))target.preset=p.id;
   ImGui::EndCombo();
  }
  ImGui::TableNextColumn();if(Button("移除"))remove=i;
  ImGui::PopID();
 }
 ImGui::EndTable();
 if(remove<draft.targets.size())draft.targets.erase(draft.targets.begin()+remove);
}

void HotkeyEditor(){
 auto& s=Get();auto& draft=s.hotkeyDraft;
 auto* source=FindHotkey(draft.id);
 bool isNewScheme=!draft.id;
 ImGui::PushID(std::to_string(draft.id).c_str());
 HotkeyHeading("编辑标题",source?"编辑方案 / "+source->name:"新建方案",
  ImGui::GetFrameHeight()+ImGui::CalcTextSize("启用此方案").x+S(12),[&]{SidebarCheckbox("启用此方案",&draft.enabled);});
 const float footer=S(30)+ImGui::GetStyle().ItemSpacing.y*3+ImGui::GetStyle().SeparatorSize;
 ImGui::BeginChild("方案字段",{0,-footer});
 if(ImGui::BeginTable("名称与按键",2,ImGuiTableFlags_SizingStretchProp)){
  ImGui::TableSetupColumn("字段",ImGuiTableColumnFlags_WidthFixed,ImGui::CalcTextSize("方案名称").x+S(8));
  ImGui::TableSetupColumn("内容",ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextColored(muted,"方案名称");
  ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);
  TextField("##快捷键名称","输入方案名称",hotkeyName,sizeof(hotkeyName));
  if(ImGui::IsItemClicked())s.captureHotkey=false;
  ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::AlignTextToFramePadding();ImGui::TextColored(muted,"快捷键");
  ImGui::TableNextColumn();
  const float rowHeight=ImGui::GetFrameHeight();
  auto key=HotkeyLabel(draft);
  if(paper::active)InlineChip(key.c_str(),rowHeight);
  else{ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(key.c_str());}
  if(s.captureHotkey){
   ImGui::TextWrapped("请按快捷键，可带 Ctrl / Shift / Alt。");
   if(Button("取消录制"))s.captureHotkey=false;
  }else{
   HotkeyInline(HotkeyButtonWidth("录制快捷键"));
   if(PanelButton("录制快捷键",{0,rowHeight})){NativeInputClose();s.captureHotkey=true;}
   if(draft.key){
    HotkeyInline(HotkeyButtonWidth("清除按键"));
    if(PanelButton("清除按键",{0,rowHeight})){draft.key=0;draft.modifiers=0;}
   }
  }
  ImGui::EndTable();
 }
 ImGui::Separator();HotkeyDraftTargets(draft);ImGui::EndChild();ImGui::Separator();
 if(Button(isNewScheme?"创建方案":"保存修改",0,ButtonTone::Primary)){
  draft.name=hotkeyName;
  if(auto id=CommitHotkeyDraft()){
   recentHotkey=id;highlightUntil=ImGui::GetTime()+2.0;revealHotkey=true;NativeInputClose();
  }
 }
 ImGui::SameLine();
 if(Button("取消")){s.hotkeyDraft={};s.captureHotkey=false;NativeInputClose();}
 if(!isNewScheme){
  HotkeyInline(HotkeyButtonWidth("删除方案"),true);
  if(Button("删除方案",0,ButtonTone::Delete))ImGui::OpenPopup("删除快捷键方案？");
  if(PanelModal("删除快捷键方案？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
   ImGui::TextWrapped("删除「%s」？分组和预设会保留。",source?source->name.c_str():"");
   if(Button("删除##快捷键",0,ButtonTone::Delete)){DeleteHotkey(draft.id);NativeInputClose();ImGui::CloseCurrentPopup();}
   ImGui::SameLine();if(Button("取消##快捷键"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
  }
 }
 ImGui::PopID();
}

void HotkeyPanel(){
 PaperKicker("//  SHORTCUTS",14);
 auto& s=Get();
 AutomationSwitch("启用预设快捷键",true);
 HotkeyInline(ImGui::CalcTextSize("与变身总开关互斥").x,true);ImGui::TextColored(muted,"与变身总开关互斥");
 ImGui::TextWrapped("按键应用方案中的分组，其余分组保持当前选择。");
 if((!s.hotkeyDraft.scope.empty()&&s.hotkeyDraft.scope!=outfit)||(s.hotkeyDraft.id&&!FindHotkey(s.hotkeyDraft.id))){s.hotkeyDraft={};s.captureHotkey=false;}
 bool editing=!s.hotkeyDraft.scope.empty();
 ImGui::BeginChild(editing?"快捷键方案编辑区":"快捷键方案列表区",ImVec2(0,0));
 if(editing)HotkeyEditor();else HotkeyList();
 ImGui::EndChild();
}
