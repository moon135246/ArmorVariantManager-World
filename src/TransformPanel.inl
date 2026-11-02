// Included inside Panel.cpp's UI namespace; all game-state access remains in
// the worker, and this editor only changes saved rules after explicit Save.
TransformRule transformDraft;
char transformName[192]{};
std::string transformError;
uint64_t confirmTransform=0;
void WeaponChoice(const char* label,int& value,bool any){
 ImGui::SetNextItemWidth(-1);
 if(ImGui::BeginCombo(label,value<0?"不限武器":WeaponName(value))){
  if(any&&ImGui::Selectable("不限武器",value==-1))value=-1;
  for(int i=0;i<14;i++)if(ImGui::Selectable(WeaponName(i),value==i))value=i;
  ImGui::EndCombo();
 }
}
void EditCondition(Condition& c){
 ImGui::TextUnformatted("触发条件");ImGui::SetNextItemWidth(-1);
 if(ImGui::BeginCombo("##条件类型",ConditionName(c.kind))){
  for(int i=0;i<(int)ConditionKind::Count;i++)if(i!=(int)ConditionKind::InsectGlaive&&ImGui::Selectable(ConditionName((ConditionKind)i),c.kind==(ConditionKind)i)){
   c={};c.kind=(ConditionKind)i;if(c.kind==ConditionKind::Enhanced)c.state=1;if(c.kind==ConditionKind::WeaponType)c.weapon=0;
  }ImGui::EndCombo();
 }
 ImGui::SetNextItemWidth(-1);
 if(c.kind==ConditionKind::WeaponState){const char* n[]={"收刀","拔刀"};ImGui::Combo("##收拔状态",&c.state,n,2);}
 else if(c.kind==ConditionKind::LongSword){const char* n[]={"无刃","白刃","黄刃","红刃","白刃及以上","黄刃及以上"};ImGui::Combo("##练气等级",&c.state,n,6);}
 else if(c.kind==ConditionKind::DualBlades){const char* n[]={"普通（未鬼人化、未鬼人强化）","鬼人化","鬼人强化（可与鬼人化重叠）"};ImGui::Combo("##双刀状态",&c.state,n,3);}
 else if(c.kind==ConditionKind::InsectGlaive){const char* n[]={"无灯 / 档位 0","一灯 / 档位 1","二灯 / 档位 2","三灯 / 档位 3"};ImGui::Combo("##灯级",&c.state,n,4);ImGui::TextWrapped("沿用参考实现的灯级读数；不同灯色组合请在游戏内验证。");}
 else if(c.kind==ConditionKind::Enhanced){const char* n[]={"未强化","强化中"};ImGui::Combo("##强化状态",&c.state,n,2);ImGui::TextWrapped("当前支持太刀红刃与双刀鬼人强化。");}
 else if(c.kind==ConditionKind::Health){
  const char* ranges[]={"自定义范围","血量归零","0% < 血量 ≤ 25%","25% < 血量 ≤ 50%","50% < 血量 ≤ 75%","75% < 血量 ≤ 100%","血量 ≤ 25%（触发后保持）","血量 ≤ 50%（触发后保持）","血量 ≤ 75%（触发后保持）"};int range=0;
  if(ImGui::Combo("快速选择",&range,ranges,9)&&range){c.healthMin=0;c.healthMax=0;c.lowerOpen=false;c.latch=range>=6;if(range>=2&&range<=5){c.healthMin=(float)((range-2)*25);c.healthMax=c.healthMin+25;c.lowerOpen=true;}else if(range>=6){c.healthMax=(float)((range-5)*25);c.lowerOpen=true;}}
  ImGui::SetNextItemWidth(S(140));ImGui::InputFloat("下限 (%)",&c.healthMin,1,5,"%.1f");ImGui::SetNextItemWidth(S(140));ImGui::InputFloat("上限 (%)",&c.healthMax,1,5,"%.1f");
  ImGui::Checkbox("不包含下限",&c.lowerOpen);ImGui::Checkbox("触发后保持",&c.latch);
  if(c.latch)ImGui::TextWrapped("正血量时触发后保持；血量归零、切图、换角色、换武器、关闭变身或重载时复位。");
 }
 else if(c.kind==ConditionKind::WeaponType){WeaponChoice("##武器种类",c.weapon,false);}
 else if(c.kind==ConditionKind::ActionLmt){ImGui::SetNextItemWidth(S(150));ImGui::InputInt("动作下限",&c.minimum);ImGui::SetNextItemWidth(S(150));ImGui::InputInt("动作上限",&c.maximum);}
 if(c.kind==ConditionKind::Health||c.kind==ConditionKind::WeaponState||c.kind==ConditionKind::ActionLmt){ImGui::TextUnformatted("限定武器");WeaponChoice("##限定武器",c.weapon,true);}
}
void AddTransformTarget(){
 auto& d=Get().data;
 auto available=[&](const Group& g){return g.scope==transformDraft.scope&&!g.presets.empty()&&std::none_of(transformDraft.targets.begin(),transformDraft.targets.end(),[&](auto& t){return t.group==g.id;});};
 if(auto* g=Find(groupId);g&&available(*g)){transformDraft.targets.push_back({g->id,g->selected?g->selected:g->presets.front().id});return;}
 for(auto& g:d.groups)if(available(g)){transformDraft.targets.push_back({g.id,g.presets.front().id});return;}
}
void StartTransformEditor(const TransformRule* rule=nullptr){
 transformDraft=rule?*rule:TransformRule{};transformError.clear();
 if(!rule){transformDraft.scope=outfit;transformDraft.name="新变身规则";AddTransformTarget();}
 strcpy_s(transformName,transformDraft.name.c_str());ImGui::OpenPopup("编辑变身规则");
}
void TransformEditor(){
 auto& s=Get();auto display=ImGui::GetIO().DisplaySize;
 ImGui::SetNextWindowSize(ImVec2(std::min(S(660),display.x-S(50)),std::min(S(700),display.y-S(60))),ImGuiCond_Appearing);
 if(!ImGui::BeginPopupModal("编辑变身规则",nullptr,ImGuiWindowFlags_NoCollapse))return;
 ImGui::BeginChild("变身编辑内容",ImVec2(0,-S(65)));
 ImGui::TextUnformatted("规则名称");ImGui::SetNextItemWidth(-1);TextField("##变身名称","规则名称",transformName,sizeof(transformName));
 ImGui::SetNextItemWidth(S(130));ImGui::InputInt("优先级（1 最高，99 最低）",&transformDraft.priority);ImGui::Checkbox("启用此规则",&transformDraft.enabled);
 ImGui::Separator();EditCondition(transformDraft.condition);ImGui::Separator();ImGui::TextUnformatted("命中条件时切换到");
 int remove=-1;
 if(ImGui::BeginTable("变身目标",3,ImGuiTableFlags_SizingStretchProp)){
  ImGui::TableSetupColumn("分组",ImGuiTableColumnFlags_WidthStretch,1);ImGui::TableSetupColumn("预设",ImGuiTableColumnFlags_WidthStretch,1.3f);ImGui::TableSetupColumn("删除",ImGuiTableColumnFlags_WidthFixed,S(34));ImGui::TableHeadersRow();
  for(size_t i=0;i<transformDraft.targets.size();i++){
   auto& t=transformDraft.targets[i];ImGui::PushID((int)i);ImGui::TableNextRow();ImGui::TableNextColumn();auto* g=Find(t.group);ImGui::SetNextItemWidth(-1);
   if(ImGui::BeginCombo("##目标分组",g?g->name.c_str():"选择分组")){for(auto& item:s.data.groups)if(item.scope==transformDraft.scope&&!item.presets.empty()&&ImGui::Selectable(item.name.c_str(),item.id==t.group)){t.group=item.id;t.preset=item.presets.front().id;}ImGui::EndCombo();}
   g=Find(t.group);const char* preset="选择预设";if(g)for(auto& p:g->presets)if(p.id==t.preset)preset=p.name.c_str();
   ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##目标预设",preset)){if(g)for(auto& p:g->presets)if(ImGui::Selectable(p.name.c_str(),p.id==t.preset))t.preset=p.id;ImGui::EndCombo();}
   ImGui::TableNextColumn();if(ImGui::SmallButton("x"))remove=(int)i;ImGui::PopID();
  }ImGui::EndTable();
 }
 if(remove>=0)transformDraft.targets.erase(transformDraft.targets.begin()+remove);
 ImGui::BeginDisabled(transformDraft.targets.size()>=64);if(Button("添加目标分组"))AddTransformTarget();ImGui::EndDisabled();
 if(transformDraft.targets.empty())ImGui::TextWrapped("请先在左侧保存分组预设，再添加变身目标。");
 ImGui::TextWrapped("未命中条件时恢复基础预设。同一分组按优先级选择一条规则；同优先级按列表顺序。全局分组仍叠加隐藏。");
 if(!transformError.empty())ImGui::TextColored(danger,"%s",transformError.c_str());ImGui::EndChild();ImGui::Separator();
 if(Button("保存规则")){
  transformDraft.name=transformName;transformError.clear();
  if(ValidateTransform(transformDraft,s.data,&transformError)){
   if(!transformDraft.id){transformDraft.id=s.data.nextId++;s.data.transforms.push_back(transformDraft);}else{auto it=std::find_if(s.data.transforms.begin(),s.data.transforms.end(),[&](auto& r){return r.id==transformDraft.id;});if(it!=s.data.transforms.end())*it=transformDraft;else transformError="规则已被重新载入，请重新打开编辑器";}
   if(transformError.empty()){ResetTransforms();Changed("变身规则已保存，随预设文件一起分享");ImGui::CloseCurrentPopup();}
  }
 }ImGui::SameLine();if(Button("取消##变身编辑"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
}
void TransformPanel(){
 auto& s=Get();if(ImGui::Checkbox("启用变身",&s.data.transformsEnabled)){ResetTransforms();Changed(s.data.transformsEnabled?"变身已启用":"变身已暂停，恢复基础预设");}
 ImGui::SameLine();if(Button("复位保持状态")){ResetTransforms();s.notice="保持状态已复位；条件仍满足时会再次触发";}
 auto& p=s.transform.player;
 if(p.entity){
  if(p.weapon>=0&&p.lmt>=0)ImGui::TextColored(accent,"当前：%s · %s",WeaponName(p.weapon),p.lmt<0x8000?"收刀":"拔刀");else ImGui::TextColored(muted,"正在读取猎人状态…");
  if(p.healthPercent>=0)ImGui::TextColored(muted,"血量：%.0f / %.0f（%.1f%%）",p.health,p.maxHealth,p.healthPercent);
  if(p.weapon==3&&p.spirit>=0)ImGui::TextColored(muted,"太刀练气：%d",p.spirit);
  if(p.weapon==2)ImGui::TextColored(muted,"鬼人化：%s · 鬼人强化：%s",p.demon==1?"开启":p.demon==0?"关闭":"未知",p.archdemon==1?"开启":p.archdemon==0?"关闭":"未知");
  if(p.weapon==10&&p.spirit>=0)ImGui::TextColored(muted,"操虫棍灯级：%d",p.spirit);
 }else ImGui::TextWrapped("等待本地猎人进入场景；读取不到状态时使用基础预设。");
 ImGui::TextWrapped("条件触发时临时切换分组预设，退出后恢复基础状态。数字越小优先级越高；默认预设不会被变身改写。");
 if(Button("新建变身规则"))StartTransformEditor();ImGui::Separator();bool any=false;const TransformRule* editRequested=nullptr;
 for(auto& r:s.data.transforms){if(r.scope!=outfit)continue;any=true;ImGui::PushID((int)r.id);
  if(ImGui::Checkbox("##启用规则",&r.enabled)){ResetTransforms();Changed();}ImGui::SameLine();ImGui::TextUnformatted(r.name.c_str());if(s.transform.activeRules.count(r.id)){ImGui::SameLine();ImGui::TextColored(accent,"[生效]");}
  ImGui::TextColored(muted,"优先级 %d · %s",r.priority,DescribeCondition(r.condition).c_str());
  for(auto& t:r.targets){auto* g=Find(t.group);if(!g)continue;for(auto& preset:g->presets)if(preset.id==t.preset)ImGui::TextWrapped("%s → %s",g->name.c_str(),preset.name.c_str());}
  if(Button("编辑"))editRequested=&r;ImGui::SameLine();if(Button("删除")){confirmTransform=r.id;}
  ImGui::Separator();ImGui::PopID();
 }
 if(editRequested)StartTransformEditor(editRequested);
 if(!any)ImGui::TextColored(muted,"尚无变身规则\n先保存所需预设，再选择条件和目标分组。");
 if(confirmTransform){ImGui::OpenPopup("删除变身规则？");}
 if(ImGui::BeginPopupModal("删除变身规则？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::TextUnformatted("删除此变身规则？已有分组与预设会保留。");if(Button("删除规则")){std::erase_if(s.data.transforms,[](auto& r){return r.id==confirmTransform;});ResetTransforms();Changed("变身规则已删除");confirmTransform=0;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##删除变身")){confirmTransform=0;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
 }
 TransformEditor();
}
