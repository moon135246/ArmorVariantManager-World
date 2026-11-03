// State-first configuration: every built-in state exists in the UI before the
// user assigns any preset. Only actual assignments are materialized as rules.
TransformRule* TransformById(uint64_t id){for(auto& r:Get().data.transforms)if(r.id==id)return &r;return nullptr;}
void TransformChanged(const char* message="变身设置已自动保存"){ResetTransforms();Changed(message);}
uint64_t FindTransformSlot(const TransformSlot& slot){for(auto& r:Get().data.transforms)if(r.scope==outfit&&SameCondition(r.condition,slot.condition))return r.id;return 0;}
uint64_t CreateTransformSlot(const TransformSlot& slot){
 auto& d=Get().data;TransformRule r;r.id=d.nextId++;r.scope=outfit;r.name=slot.name;r.condition=slot.condition;d.transforms.push_back(r);return r.id;
}
uint64_t StoreTransformTarget(uint64_t id,const TransformSlot& slot,size_t index,const TransformTarget& target){
 if(!id)id=CreateTransformSlot(slot);auto* r=TransformById(id);if(!r)return id;
 if(index<r->targets.size())r->targets[index]=target;else if(r->targets.size()<64)r->targets.push_back(target);
 TransformChanged();return id;
}
void DrawTransformTarget(uint64_t& id,const TransformSlot& slot,size_t index,TransformTarget target,bool placeholder){
 auto& d=Get().data;ImGui::PushID((int)index);
 if(ImGui::BeginTable("目标",3,ImGuiTableFlags_SizingStretchProp)){
  ImGui::TableSetupColumn("分组",ImGuiTableColumnFlags_WidthStretch,1);ImGui::TableSetupColumn("预设",ImGuiTableColumnFlags_WidthStretch,1.3f);ImGui::TableSetupColumn("移除",ImGuiTableColumnFlags_WidthFixed,S(25));ImGui::TableNextRow();ImGui::TableNextColumn();
  auto* group=Find(target.group);ImGui::SetNextItemWidth(-1);
  if(SidebarCombo("##选择分组",group?group->name.c_str():"选择分组")){
   for(auto& g:d.groups)if(g.scope==outfit&&!g.presets.empty()){
    auto* rule=TransformById(id);bool duplicate=false;if(rule)for(size_t j=0;j<rule->targets.size();j++)if(j!=index&&rule->targets[j].group==g.id)duplicate=true;
    ImGui::BeginDisabled(duplicate);auto label=(g.global?"[全局] ":"")+g.name;
    if(ImGui::Selectable(label.c_str(),target.group==g.id)){target={g.id,0};id=StoreTransformTarget(id,slot,index,target);}ImGui::EndDisabled();
   }ImGui::EndCombo();
  }
  ImGui::TableNextColumn();group=Find(target.group);const char* preset="不切换";if(group)for(auto& p:group->presets)if(p.id==target.preset)preset=p.name.c_str();ImGui::SetNextItemWidth(-1);ImGui::BeginDisabled(!group);
  if(SidebarCombo("##选择预设",preset)){
   if(ImGui::Selectable("不切换",!target.preset)){target.preset=0;if(!placeholder)id=StoreTransformTarget(id,slot,index,target);}
   if(group)for(auto& p:group->presets)if(ImGui::Selectable(p.name.c_str(),p.id==target.preset)){target.preset=p.id;id=StoreTransformTarget(id,slot,index,target);}ImGui::EndCombo();
  }ImGui::EndDisabled();ImGui::TableNextColumn();
  if(!placeholder&&ImGui::SmallButton("x")){if(auto* rule=TransformById(id);rule&&index<rule->targets.size()){rule->targets.erase(rule->targets.begin()+index);TransformChanged();}}
  ImGui::EndTable();
 }ImGui::PopID();
}
void DrawTransformBindings(uint64_t& id,const TransformSlot& slot){
 auto* r=TransformById(id);bool enabled=r?r->enabled:true;
 if(r&&!r->enabled){if(PanelCheckbox("启用此状态",&enabled)){r->enabled=enabled;TransformChanged();}}
 std::vector<TransformTarget> targets=r?r->targets:std::vector<TransformTarget>{};
 if(targets.empty()){
  auto* current=Find(groupId);if(!current||current->scope!=outfit||current->presets.empty()){current=nullptr;for(auto& g:Get().data.groups)if(g.scope==outfit&&!g.presets.empty()){current=&g;break;}}
  DrawTransformTarget(id,slot,0,{current?current->id:0,0},true);
 }else{
  for(size_t i=0;i<targets.size();i++)DrawTransformTarget(id,slot,i,targets[i],false);
  if(targets.size()<64&&Button("添加目标分组")){
   for(auto& g:Get().data.groups)if(g.scope==outfit&&!g.presets.empty()&&std::none_of(targets.begin(),targets.end(),[&](auto& t){return t.group==g.id;})){StoreTransformTarget(id,slot,targets.size(),{g.id,0});break;}
  }
 }
}
void DrawTransformState(const TransformSlot& slot,uint64_t id,int key,bool editableHealth=false){
 ImGui::PushID(key);auto& s=Get();bool current=Matches(slot.condition,s.transform.player);if(slot.condition.kind==ConditionKind::ChargeBlade)current=ChargeBladeState(s.transform.player)==slot.condition.state;else if(slot.condition.kind==ConditionKind::SwitchAxe)current=SwitchAxeState(s.transform.player)==slot.condition.state;auto* rule=TransformById(id);bool applied=rule&&s.transform.activeRules.count(id);auto label=slot.name+(applied?"  [生效]":current?"  [当前]":"");
 if(current&&!paper::active)ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(.1f,.36f,.43f,1));
 bool expanded=false,removed=false;
 if(editableHealth&&rule){
  if(ImGui::BeginTable("血量节点标题",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX)){
   ImGui::TableSetupColumn("节点",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("操作",ImGuiTableColumnFlags_WidthFixed,S(64));ImGui::TableNextColumn();expanded=PanelHeader(label.c_str(),ImGuiTreeNodeFlags_DefaultOpen,current);ImGui::TableNextColumn();
   if(PanelButton("删除##血量节点",ImVec2(-1,ImGui::GetFrameHeight()),ButtonTone::Delete))removed=DeleteHealthNode(id,outfit);ImGui::EndTable();
  }
 }else expanded=PanelHeader(label.c_str(),ImGuiTreeNodeFlags_DefaultOpen,current);
 if(current&&!paper::active)ImGui::PopStyleColor();if(removed){ImGui::PopID();return;}
 if(expanded){
  if(editableHealth&&rule){
   auto c=rule->condition;bool change=false;ImGui::SetNextItemWidth(S(115));change|=ImGui::InputFloat("下限 %",&c.healthMin,1,5,"%.0f");ImGui::SetNextItemWidth(S(115));change|=ImGui::InputFloat("上限 %",&c.healthMax,1,5,"%.0f");change|=PanelCheckbox("不包含下限",&c.lowerOpen);change|=PanelCheckbox("触发后保持",&c.latch);
   if(change&&ValidCondition(c)){rule->condition=c;TransformChanged();}
  }
  DrawTransformBindings(id,slot);ImGui::Spacing();
 }ImGui::PopID();
}
std::string CurrentTransformState(int category){
 auto& p=Get().transform.player;if(!p.entity)return "等待猎人进入场景";
 if(category==0)return p.lmt<0?"读取中":p.lmt<0x8000?"收刀":"拔刀";
 if(category==1){if(p.weapon!=3)return "当前未装备太刀";const char* n[]={"无刃","白刃","黄刃","红刃"};return p.spirit>=0&&p.spirit<=3?n[p.spirit]:"读取中";}
 if(category==2){if(p.weapon!=2)return "当前未装备双刀";if(p.demon==1)return "鬼人化";if(p.demon==0&&p.archdemon==1)return "鬼人强化";if(p.demon==0&&p.archdemon==0)return "普通";return "读取中";}
 if(category==3){if(p.weapon!=10)return "当前未装备操虫棍";const char* n[]={"无精华","一灯","二灯","三灯齐全"};int count=ExtractCount(p.extracts);return count>=0?n[count]:"读取中";}
 if(category==5){if(p.weapon!=11)return "当前未装备弓";return BowChargeName(p.bowCharge);}
 if(category==6){if(p.weapon!=4)return "当前未装备大锤";return p.hammerCharge>=0&&p.hammerCharge<=3?(p.hammerCharge?std::to_string(p.hammerCharge)+" 段蓄力":"未蓄力"):"读取中";}
 if(category==7){if(p.weapon!=9)return "当前未装备盾斧";int state=ChargeBladeState(p);return state>=0?TransformSlots(7)[state].name:"读取中";}
 if(category==8){if(p.weapon!=8)return "当前未装备斩斧";int state=SwitchAxeState(p);return state>=0?TransformSlots(8)[state].name:"读取中";}
 if(p.healthPercent<0)return "读取中";char value[96];sprintf_s(value,"%.0f / %.0f（%.1f%%）",p.health,p.maxHealth,p.healthPercent);return value;
}
void DrawTransformCategory(int category){
 auto slots=TransformSlots(category);auto title=std::string(TransformCategoryName(category))+" · "+CurrentTransformState(category);ImGui::TextWrapped("%s",title.c_str());
 if(category==2)ImGui::TextColored(muted,"鬼人化时使用鬼人化项；退出后按强化状态切换。");
 if(category==3)ImGui::TextColored(muted,"按已点亮的精华数量匹配，颜色不影响一灯与二灯状态。");
 if(category==4){
  if(Button("添加血量节点"))AddHealthNode(outfit);
  std::vector<uint64_t> nodes;for(auto& r:Get().data.transforms)if(r.scope==outfit&&r.condition.kind==ConditionKind::Health)nodes.push_back(r.id);
  for(auto id:nodes)if(auto* r=TransformById(id)){TransformSlot slot{DescribeCondition(r->condition),r->condition};DrawTransformState(slot,id,(int)id+100,true);}
  return;
 }
 if(category==7||category==8)ImGui::TextWrapped("强化项优先于同优先级的基础形态；未绑定的分组继续使用基础形态。");
 if(category==7)ImGui::TextWrapped("盾、剑、斧强化同时存在时，两种形态都使用三红项。");
 std::set<uint64_t> shown;int key=0;
 for(auto& slot:slots){auto id=FindTransformSlot(slot);if(id)shown.insert(id);DrawTransformState(slot,id,key++);}
 std::vector<uint64_t> extra;for(auto& r:Get().data.transforms)if(r.scope==outfit&&TransformCategory(r.condition.kind)==category&&!shown.count(r.id))extra.push_back(r.id);
 if(!extra.empty()){ImGui::Separator();ImGui::TextColored(muted,"其他已保存的状态");}
 for(auto id:extra)if(auto* rule=TransformById(id)){TransformSlot slot{DescribeCondition(rule->condition),rule->condition};DrawTransformState(slot,id,(int)id+100,category==4);}
}
void TransformPanel(){
 auto& s=Get();auto& mode=EnsureTransformMode(s.data,outfit);AutomationSwitch("启用变身功能",false);SidebarInline(S(34)+ImGui::CalcTextSize("当前装备启用变身").x,22);bool enabled=mode.enabled;
 if(PanelCheckbox("当前装备启用变身",&enabled)){mode.enabled=enabled;TransformChanged(enabled?"当前装备的变身已启用":"当前装备的变身已暂停");}
 if(Button("复位保持状态")){ResetTransforms();s.notice="保持状态已复位；条件仍满足时会再次触发";}SidebarInline(S(26)+ImGui::CalcTextSize("选择模式").x,12);
 if(SidebarRadio("选择模式",!mode.parallel)){mode.parallel=false;TransformChanged();}SidebarInline(S(26)+ImGui::CalcTextSize("并行模式").x,22);if(SidebarRadio("并行模式",mode.parallel)){mode.parallel=true;TransformChanged();}
 if(!mode.parallel){
  ImGui::SetNextItemWidth(-1);if(SidebarCombo("##变身条件类型",TransformCategoryName(mode.selected))){for(int i=0;i<kTransformCategories;i++)if(ImGui::Selectable(TransformCategoryName(i),mode.selected==i)){mode.selected=i;TransformChanged();}ImGui::EndCombo();}
 }else{
  ImGui::TextColored(muted,"勾选同时使用的条件；优先级数字越小越优先。");
  ImGui::BeginChild("并行条件选择",ImVec2(0,std::clamp(ImGui::GetContentRegionAvail().y-S(170),S(55),S(175))));
  if(ImGui::BeginTable("并行条件",2,ImGuiTableFlags_SizingStretchProp)){
   ImGui::TableSetupColumn("条件",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("优先级",ImGuiTableColumnFlags_WidthFixed,S(105));
   for(int i=0;i<kTransformCategories;i++){ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();if(PanelCheckbox(TransformCategoryName(i),&mode.categories[i]))TransformChanged();ImGui::TableNextColumn();int priority=mode.priorities[i];ImGui::SetNextItemWidth(-1);if(ImGui::InputInt("##优先级",&priority)&&priority>=1&&priority<=99){mode.priorities[i]=priority;TransformChanged();}ImGui::PopID();}ImGui::EndTable();
  }ImGui::EndChild();
 }
 ImGui::Separator();ImGui::BeginChild("变身状态列表",ImVec2(0,0));
 if(!mode.parallel)DrawTransformCategory(mode.selected);else{bool any=false;for(int i=0;i<kTransformCategories;i++)if(mode.categories[i]){any=true;ImGui::PushID(i);DrawTransformCategory(i);ImGui::Separator();ImGui::PopID();}if(!any)ImGui::TextWrapped("先勾选要同时使用的条件。");}
 ImGui::TextWrapped("不切换 = 保持基础状态；条件退出后恢复基础预设，默认项不会被变身改写。");ImGui::EndChild();
}
