#include "Overlay.h"
#include "State.h"
#include "ArmorNames.h"
#include "NativeInput.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <set>
#include <optional>
namespace worlddiff::overlay {
namespace {
using namespace app;
std::string outfit;uint64_t groupId=0;int tab=0,part=-1;bool meshMode=true,creating=false,globalGroup=false;
char groupName[192]{},presetName[192]{},search[192]{},rename[192]{};std::set<Target> staged;
char profileName[192]{};std::vector<ProfileCandidate> profileFiles;
std::vector<std::string> backupFiles;std::string displayedOutfit;
uint64_t editingMembers=0,confirmGroup=0,confirmPreset=0,renameGroup=0;
Target meshNoteTarget;char meshNote[192]{};bool openMeshNote=false;
ImVec4 accent{0.33f,0.72f,0.78f,1},muted{0.6f,0.66f,0.72f,1},danger{0.91f,0.51f,0.51f,1};
float S(float x){return x*Get().data.scale;}
std::string Lower(std::string x){for(auto& c:x)if(c>='A'&&c<='Z')c+=32;return x;}
int PartIndex(const std::string& p){if(p=="helm")return 0;if(p=="body")return 1;if(p=="arm")return 2;if(p=="wst")return 3;if(p=="leg")return 4;return 5;}
const char* PartName(int p){const char* names[]={"头部","身体","手臂","腰部","腿部","其他"};return names[std::clamp(p,0,5)];}
void Tip(const char* text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",text);}
void EquipmentTip(const std::string& scope){
 if(!ImGui::IsItemHovered())return;auto label=EquipmentLabel(scope);auto aliases=EquipmentAliases(scope);
 ImGui::BeginTooltip();ImGui::PushTextWrapPos(ImGui::GetFontSize()*34);ImGui::TextUnformatted(label.c_str());
 if(aliases.find('\n')!=std::string_view::npos){ImGui::Separator();ImGui::TextDisabled("共用此模型编号的装备系列：");ImGui::TextUnformatted(aliases.data(),aliases.data()+aliases.size());}
 ImGui::PopTextWrapPos();ImGui::EndTooltip();
}
bool Button(const char* text,float width=0){return ImGui::Button(text,ImVec2(width>0?S(width):0,S(30)));}
enum class Icon {Plus,Members,Edit,Trash,Save,Star,Copy};
void DrawIcon(ImDrawList* draw,Icon icon,ImVec2 p,float size,ImU32 color){
 auto point=[&](float x,float y){return ImVec2(p.x+x*size,p.y+y*size);};
 float stroke=std::max(1.f,size*.09f);
 auto line=[&](float x,float y,float a,float b){draw->AddLine(point(x,y),point(a,b),color,stroke);};
 if(icon==Icon::Plus){line(.5f,.12f,.5f,.88f);line(.12f,.5f,.88f,.5f);}
 else if(icon==Icon::Star){ImVec2 points[10];for(int i=0;i<10;i++){float angle=-1.5707963f+i*.6283185f,r=(i%2?.21f:.48f);points[i]=point(.5f+std::cos(angle)*r,.5f+std::sin(angle)*r);}draw->AddPolyline(points,10,color,ImDrawFlags_Closed,stroke);}
 else if(icon==Icon::Trash){line(.15f,.24f,.85f,.24f);line(.35f,.12f,.65f,.12f);line(.24f,.29f,.29f,.88f);line(.76f,.29f,.71f,.88f);line(.29f,.88f,.71f,.88f);line(.43f,.4f,.43f,.74f);line(.57f,.4f,.57f,.74f);}
 else if(icon==Icon::Save){draw->AddRect(point(.12f,.1f),point(.88f,.9f),color,size*.06f,stroke);draw->AddRect(point(.3f,.1f),point(.7f,.4f),color,0,stroke);draw->AddRect(point(.3f,.6f),point(.7f,.9f),color,0,stroke);}
 else if(icon==Icon::Copy){draw->AddRect(point(.12f,.12f),point(.65f,.7f),color,size*.05f,stroke);draw->AddRectFilled(point(.35f,.3f),point(.89f,.9f),color,size*.06f);}
 else if(icon==Icon::Edit){line(.2f,.72f,.7f,.2f);line(.3f,.84f,.82f,.32f);line(.7f,.2f,.82f,.32f);line(.2f,.72f,.16f,.88f);line(.16f,.88f,.3f,.84f);}
 else{draw->AddCircleFilled(point(.38f,.3f),size*.17f,color);draw->AddCircleFilled(point(.78f,.35f),size*.13f,color);draw->AddRectFilled(point(.12f,.58f),point(.64f,.91f),color,size*.12f);draw->AddRectFilled(point(.68f,.61f),point(.95f,.91f),color,size*.09f);}
}
bool ActionButton(const char* label,Icon icon,float width,float height=38){
 ImGui::PushID(label);bool pressed=ImGui::Button("##action",ImVec2(width,S(height)));ImGui::PopID();
 auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();float fontSize=S(18),iconSize=S(17),gap=S(7),padding=S(7);
 auto* font=ImGui::GetFont();float textWidth=font->CalcTextSizeA(fontSize,FLT_MAX,0,label).x;
 bool hasIcon=textWidth+iconSize+gap+padding*2<=width;
 if(!hasIcon&&textWidth+padding*2>width){fontSize*=std::max(.65f,(width-padding*2)/textWidth);textWidth=font->CalcTextSizeA(fontSize,FLT_MAX,0,label).x;}
 float content=textWidth+(hasIcon?iconSize+gap:0),x=min.x+std::max(padding,(width-content)*.5f);auto color=ImGui::GetColorU32(ImGuiCol_Text);auto* draw=ImGui::GetWindowDrawList();
 draw->PushClipRect(min,max,true);if(hasIcon){DrawIcon(draw,icon,ImVec2(x,min.y+(max.y-min.y-iconSize)*.5f),iconSize,color);x+=iconSize+gap;}
 draw->AddText(font,fontSize,ImVec2(x,min.y+(max.y-min.y-fontSize)*.5f),color,label);draw->PopClipRect();return pressed;
}
constexpr ImGuiWindowFlags FixedPane=ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
void BeginActions(const char* name,float height){ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(10),S(10)));ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(S(8),S(8)));ImGui::BeginChild(name,ImVec2(0,height),ImGuiChildFlags_Borders,FixedPane);}
void EndActions(){ImGui::EndChild();ImGui::PopStyleVar(2);}
void DragHint(){
 constexpr const char* message="提示：可拖动左侧的【分组列表】和【预设列表】调整顺序。";
 // Reserve every visual with a normal item; drawing never moves the layout cursor.
 const float padding=S(12),gap=S(10),iconWidth=S(26),closeWidth=S(26);
 float available=ImGui::GetContentRegionAvail().x,chrome=padding*2+iconWidth+closeWidth+gap*2;
 float width=std::min(available,ImGui::CalcTextSize(message).x+chrome+S(2));
 float wrapWidth=std::max(1.f,width-chrome);
 auto textSize=ImGui::CalcTextSize(message,nullptr,false,wrapWidth);float contentHeight=std::max(S(28),textSize.y);
 if(available>width){ImGui::Dummy(ImVec2(available-width,0));ImGui::SameLine(0,0);}
 ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.025f,.10f,.15f,.95f));ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.13f,.59f,.70f,.95f));
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(padding,S(9)));ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(gap*.5f,0));ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,S(8));
 ImGui::BeginChild("拖动提示",ImVec2(width,contentHeight+S(18)),ImGuiChildFlags_Borders,FixedPane);
 if(ImGui::BeginTable("提示内容",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX)){
  ImGui::TableSetupColumn("信息",ImGuiTableColumnFlags_WidthFixed,iconWidth);ImGui::TableSetupColumn("内容",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("关闭",ImGuiTableColumnFlags_WidthFixed,closeWidth);ImGui::TableNextColumn();
  ImGui::Dummy(ImVec2(iconWidth,contentHeight));auto p=ImGui::GetItemRectMin();auto* draw=ImGui::GetWindowDrawList();ImVec2 center(p.x+iconWidth*.5f,p.y+contentHeight*.5f);auto ink=IM_COL32(160,218,242,255),dark=IM_COL32(8,37,52,255);
  draw->AddCircleFilled(center,S(12),ink);draw->AddCircleFilled(ImVec2(center.x,center.y-S(5)),S(1.5f),dark);draw->AddRectFilled(ImVec2(center.x-S(1.4f),center.y-S(1.5f)),ImVec2(center.x+S(1.4f),center.y+S(6)),dark,S(1));
  ImGui::TableNextColumn();float textWidth=ImGui::GetContentRegionAvail().x;ImGui::Dummy(ImVec2(textWidth,contentHeight));auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
  draw->PushClipRect(min,max,true);draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(min.x,min.y+(contentHeight-textSize.y)*.5f),ink,message,nullptr,textWidth);draw->PopClipRect();
  ImGui::TableNextColumn();bool close=ImGui::InvisibleButton("关闭拖动提示",ImVec2(closeWidth,contentHeight));p=ImGui::GetItemRectMin();center=ImVec2(p.x+closeWidth*.5f,p.y+contentHeight*.5f);auto cross=ImGui::IsItemHovered()?IM_COL32(218,248,255,255):ink;float r=S(5),stroke=S(1.8f);
  draw->AddLine(ImVec2(center.x-r,center.y-r),ImVec2(center.x+r,center.y+r),cross,stroke);draw->AddLine(ImVec2(center.x-r,center.y+r),ImVec2(center.x+r,center.y-r),cross,stroke);
  if(close){Get().data.hideDragHint=true;Changed("拖动提示已隐藏，下次启动不再显示");Save();}Tip("关闭此提示，下次启动不再显示");ImGui::EndTable();
 }ImGui::EndChild();ImGui::PopStyleVar(3);ImGui::PopStyleColor(2);
}
bool PartButton(const char* name,bool selected,float width){
 ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,S(8));ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,selected?S(1.5f):0.f);
 ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.25f,.83f,.9f,.95f));if(selected)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.10f,.43f,.52f,1));
 bool pressed=ImGui::Button(name,ImVec2(width,ImGui::GetFrameHeight()));if(selected){auto* draw=ImGui::GetWindowDrawList();draw->AddRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),IM_COL32(61,214,231,35),S(8),S(4));ImGui::PopStyleColor();}ImGui::PopStyleColor();ImGui::PopStyleVar(2);return pressed;
}
bool ListRow(const char* label,bool selected,bool ghost=false){
 // Let Selectable own layout, navigation and drag/drop. Draw the rounded row
 // inside its spacing-expanded item rectangle, with text centered in that row.
 const float height=S(42),inset=S(2),gap=S(6),radius=S(8);
 float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
 ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,gap));
 ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0,0,0,0));
 ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0,0,0,0));
 ImGui::PushStyleColor(ImGuiCol_HeaderActive,ImVec4(0,0,0,0));
 bool pressed=ImGui::Selectable("##ListRow",selected,0,ImVec2(width,height));
 ImGui::PopStyleColor(3);ImGui::PopStyleVar();
 if(ImGui::IsItemVisible()&&!ghost){
  auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
  min.x+=inset;max.x-=inset;min.y+=gap*.5f;max.y-=gap*.5f;
  bool hovered=ImGui::IsItemHovered(),active=selected||pressed;
  auto* draw=ImGui::GetWindowDrawList();
  if(active){
   draw->AddRectFilled(min,max,IM_COL32(28,65,76,245),radius);
   draw->AddRect(min,max,IM_COL32(75,208,220,30),radius,S(4));
   draw->AddRect(min,max,IM_COL32(82,216,227,hovered?175:120),radius,S(1));
   draw->AddRect(ImVec2(min.x+S(2),min.y+S(2)),ImVec2(max.x-S(2),max.y-S(2)),IM_COL32(92,223,232,22),S(6),S(1));
   draw->AddRectFilled(ImVec2(min.x+S(1),min.y+S(2)),ImVec2(min.x+S(7),max.y-S(2)),IM_COL32(82,226,236,255),S(3));
  }else if(hovered)draw->AddRectFilled(min,max,IM_COL32(38,64,76,160),radius);
  if(ImGui::IsItemFocused())draw->AddRect(min,max,IM_COL32(133,235,242,200),radius,S(1));
  ImVec2 textMin(min.x+S(14),min.y),textMax(max.x-S(10),max.y);
  draw->PushClipRect(textMin,textMax,true);
  draw->AddText(ImVec2(textMin.x,min.y+(max.y-min.y-ImGui::GetFontSize())*.5f),ImGui::GetColorU32(ImGuiCol_Text),label);
  draw->PopClipRect();
  if(hovered&&ImGui::CalcTextSize(label).x>textMax.x-textMin.x)ImGui::SetTooltip("%s",label);
 }
 return pressed;
}
#include "ReorderList.inl"
void GroupsPane(float width,float height){
 auto& s=Get();ImGui::BeginChild("分组面板",ImVec2(width,height),ImGuiChildFlags_Borders,FixedPane);
 ImGui::TextColored(muted,"分组");ImGui::Separator();
 float actionsHeight=S(creating?222.f:146.f);
 ImGui::BeginChild("分组列表",ImVec2(0,std::max(S(20),ImGui::GetContentRegionAvail().y-actionsHeight-ImGui::GetStyle().ItemSpacing.y)));
 for(auto& g:s.data.groups)if(g.scope==outfit&&g.main){ImGui::PushID((int)g.id);if(ListRow("主列表",groupId==g.id)){groupId=g.id;creating=false;editingMembers=0;staged.clear();}ImGui::PopID();}
 std::vector<uint64_t> keys;for(auto& g:s.data.groups)if(g.scope==outfit&&!g.main)keys.push_back(g.id);
 ReorderList order("groups/"+outfit,keys,S(42),S(6));
 for(auto key:keys){auto* g=Find(key);if(!g)continue;bool ghost=order.BeginRow(key);ImGui::PushID((int)g->id);std::string label=g->global?"[全局] "+g->name:g->name;
  if(ListRow(label.c_str(),groupId==g->id,ghost)&&!order.Dragging()){groupId=g->id;creating=false;editingMembers=0;staged.clear();}
  order.Source(label);ImGui::PopID();order.EndRow();
 }
 if(auto move=order.End())MoveGroupBefore(move->source,move->before);
 ImGui::EndChild();
 BeginActions("分组操作",actionsHeight);auto* g=Find(groupId);
 if(!creating){
  ImGui::TextUnformatted("分组操作");float cell=(ImGui::GetContentRegionAvail().x-S(8))*.5f;
  if(ActionButton("新建分组",Icon::Plus,cell)){creating=true;editingMembers=0;globalGroup=false;staged.clear();groupName[0]=0;}
  ImGui::SameLine(0,S(8));ImGui::BeginDisabled(!g||g->main);if(ActionButton("编辑成员",Icon::Members,cell)){creating=true;editingMembers=g->id;globalGroup=g->global;staged=g->members;strcpy_s(groupName,g->name.c_str());}
  if(ActionButton("重命名",Icon::Edit,cell)){renameGroup=g->id;strcpy_s(rename,g->name.c_str());ImGui::OpenPopup("重命名分组");}
  ImGui::SameLine(0,S(8));if(ActionButton("删除分组",Icon::Trash,cell)){confirmGroup=g->id;ImGui::OpenPopup("删除分组？");}ImGui::EndDisabled();
 }else{
  ImGui::TextColored(accent,editingMembers?"编辑分组成员":"选择材质并创建分组");ImGui::SetNextItemWidth(-1);TextField("##组名","分组名称",groupName,sizeof(groupName));
  ImGui::BeginDisabled(editingMembers!=0);ImGui::Checkbox("全局分组",&globalGroup);ImGui::EndDisabled();Tip("全局分组可与其他分组重叠，并叠加隐藏");
  ImGui::Text("已选择 %zu 个材质",staged.size());
  ImGui::BeginDisabled(staged.empty()||!groupName[0]);if(Button("确认")){
   uint64_t id=0;bool ok=false;
   if(editingMembers){auto* edit=Find(editingMembers);if(edit){bool valid=true;for(auto& t:staged){auto* owner=Owner(t.model,t.value);if(!edit->global&&owner&&owner->id!=edit->id)valid=false;}if(valid&&RenameGroup(edit->id,groupName)){edit->members=staged;Changed("分组成员已更新");id=edit->id;ok=true;}}}
   else ok=CreateGroup(groupName,outfit,globalGroup,staged,id);
   if(ok){groupId=id;creating=false;staged.clear();}else s.notice="创建失败：检查重名、选择范围或普通组归属";
  }ImGui::EndDisabled();ImGui::SameLine();if(Button("取消")){creating=false;editingMembers=0;staged.clear();}
 }
 if(ImGui::BeginPopupModal("删除分组？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("分组内的预设将一起删除，材质控制权归还主列表。");if(Button("删除")){DeleteGroup(confirmGroup);groupId=Main(outfit).id;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##删除组"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 if(ImGui::BeginPopupModal("重命名分组",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){TextField("名称","分组名称",rename,sizeof(rename));if(Button("保存")){if(RenameGroup(renameGroup,rename))ImGui::CloseCurrentPopup();else s.notice="分组名称为空或重复";}ImGui::SameLine();if(Button("取消##改组名"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 EndActions();
 ImGui::EndChild();
}
void PresetsPane(float width,float height){
 auto& s=Get();auto* g=Find(groupId);ImGui::BeginChild("预设面板",ImVec2(width,height),ImGuiChildFlags_Borders,FixedPane);ImGui::TextColored(muted,"%s · 预设",g?g->name.c_str():"主列表");ImGui::Separator();
 float actionsHeight=S(212);
 ImGui::BeginChild("预设列表",ImVec2(0,std::max(S(20),ImGui::GetContentRegionAvail().y-actionsHeight-ImGui::GetStyle().ItemSpacing.y)));
 if(g){std::vector<uint64_t> keys;for(auto& p:g->presets)keys.push_back(p.id);ReorderList order("presets/"+std::to_string(g->id),keys,S(42),S(6));
  for(auto key:keys){auto it=std::find_if(g->presets.begin(),g->presets.end(),[&](auto& p){return p.id==key;});if(it==g->presets.end())continue;auto& p=*it;bool ghost=order.BeginRow(key);ImGui::PushID((int)p.id);auto label=p.name+(g->defaultPreset==p.id?"  [默认]":"");
   auto active=ActivePreset(*g);if(active==p.id)label+="  [变身]";if(ListRow(label.c_str(),(active?active:g->selected)==p.id,ghost)&&!order.Dragging())ApplyPreset(*g,p.id);
   order.Source(label);ImGui::PopID();order.EndRow();
  }if(auto move=order.End())MovePresetBefore(*g,move->source,move->before);
 }
 if(g&&g->presets.empty())ImGui::TextColored(muted,"尚无预设\n先调整右侧显隐，再保存");ImGui::EndChild();
 BeginActions("预设操作",actionsHeight);
 ImGui::TextColored(muted,"保存当前分组的状态");float saveWidth=S(124),inputWidth=std::max(S(60),ImGui::GetContentRegionAvail().x-saveWidth-S(8));ImGui::SetNextItemWidth(inputWidth);TextField("##预设名称","新预设名称",presetName,sizeof(presetName));ImGui::SameLine(0,S(8));ImGui::BeginDisabled(!g||!presetName[0]);
 if(ActionButton("保存新预设",Icon::Save,saveWidth,ImGui::GetFrameHeight()/s.data.scale)){if(SavePreset(*g,presetName)){presetName[0]=0;}else s.notice="预设名为空或已存在，请使用覆盖";}ImGui::EndDisabled();
 Preset* selected=nullptr;if(g)for(auto& p:g->presets)if(p.id==g->selected)selected=&p;
 bool modified=selected&&(selected->rules.hidden!=g->working.hidden||selected->rules.shown!=g->working.shown);
 ImGui::TextColored(muted,g&&ActivePreset(*g)?"基础预设：%s%s":"当前预设：%s%s",selected?selected->name.c_str():"未保存的调整",modified?"（已修改）":"");
 float cell=(ImGui::GetContentRegionAvail().x-S(16))/3.f;
 ImGui::BeginDisabled(!selected);if(ActionButton("设为默认",Icon::Star,cell))SetDefault(*g,selected->id);ImGui::SameLine(0,S(8));if(ActionButton("覆盖预设",Icon::Copy,cell))SavePreset(*g,selected->name,true);
 ImGui::SameLine(0,S(8));if(ActionButton("删除预设",Icon::Trash,cell)){confirmPreset=selected->id;ImGui::OpenPopup("删除预设？");}ImGui::EndDisabled();
 if(ImGui::Checkbox("选择时自动设为默认",&s.data.autoDefault)){if(s.data.autoDefault&&g&&g->selected)SetDefault(*g,g->selected);Changed();}
 Tip("默认预设会随游戏自动加载；普通组分别记忆，全局组叠加生效。");
 if(ImGui::BeginPopupModal("删除预设？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("删除此预设？当前画面状态保持不变。");if(Button("删除")){if(g)DeletePreset(*g,confirmPreset);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##删预设"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 EndActions();
 ImGui::EndChild();
}
void MeshNameRow(const Model& model,const Target& target,const std::string& label,bool allowed,ReorderList* order,float trailingWidth=0){
 if(!allowed)ImGui::PushStyleColor(ImGuiCol_Text,muted);
 // Keep the real selectable for keyboard, context menus and reordering, but
 // give its full name area the same rounded hover treatment as group rows.
 float width=std::max(1.f,ImGui::GetContentRegionAvail().x-trailingWidth);auto ink=ImGui::GetColorU32(ImGuiCol_Text);
 auto* window=ImGui::GetCurrentWindow();float baseline=window->DC.CurrLineTextBaseOffset;window->DC.CurrLineTextBaseOffset=0;
 for(auto color:{ImGuiCol_Header,ImGuiCol_HeaderHovered,ImGuiCol_HeaderActive,ImGuiCol_NavCursor,ImGuiCol_Text})ImGui::PushStyleColor(color,ImVec4(0,0,0,0));
 ImGui::Selectable(label.c_str(),false,ImGuiSelectableFlags_NoPadWithHalfSpacing,ImVec2(width,ImGui::GetFrameHeight()));
 window->DC.PrevLineTextBaseOffset=std::max(window->DC.PrevLineTextBaseOffset,baseline);
 ImGui::PopStyleColor(5);
 if(ImGui::IsItemVisible()){
  auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();auto* draw=ImGui::GetWindowDrawList();
  if(ImGui::IsItemHovered()||ImGui::IsItemActive())draw->AddRectFilled(min,max,ImGui::IsItemActive()?IM_COL32(28,65,76,220):IM_COL32(38,64,76,160),S(8));
  if(ImGui::IsItemFocused())draw->AddRect(min,max,IM_COL32(133,235,242,200),S(8),S(1));
  ImVec2 textMin(min.x+S(8),min.y),textMax(max.x-S(8),max.y);draw->PushClipRect(textMin,textMax,true);
  draw->AddText(ImVec2(textMin.x,min.y+(max.y-min.y-ImGui::GetFontSize())*.5f),ink,label.c_str());draw->PopClipRect();
 }
 if(!allowed)ImGui::PopStyleColor();
 if(order)order->Source(label);
 if((!order||!order->Dragging())&&ImGui::BeginPopupContextItem("网格操作")){
  if(ImGui::MenuItem("编辑备注")){meshNoteTarget=target;auto* view=MeshView(model,target.value);strcpy_s(meshNote,view?view->note.c_str():"");openMeshNote=true;}
  if(ImGui::MenuItem("清除备注",nullptr,false,MeshView(model,target.value)&&!MeshView(model,target.value)->note.empty()))SetMeshNote(model,target.value,{});
  ImGui::EndPopup();
 }
}
void MeshNotePopup(){
 if(openMeshNote){ImGui::OpenPopup("编辑网格备注");openMeshNote=false;}
 if(!ImGui::BeginPopupModal("编辑网格备注",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
 auto& s=Get();auto model=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return Normalize(m.relativePath)==meshNoteTarget.model;});
 ImGui::TextUnformatted("备注只用于显示，留空可恢复原名称。");ImGui::SetNextItemWidth(S(360));TextField("##网格备注","输入网格备注",meshNote,sizeof(meshNote));
 if(Button("保存备注")){
  if(model!=s.models.end()&&SetMeshNote(*model,meshNoteTarget.value,meshNote))ImGui::CloseCurrentPopup();else s.notice="备注过长、包含控制字符或网格已失效";
 }ImGui::SameLine();if(Button("取消##网格备注"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
}
void TargetsPanel(){
 auto& state=Get();auto* current=Find(groupId);if(current&&ActivePreset(*current)&&!creating){ImGui::TextColored(accent,"变身正在控制当前组；基础预设与默认项保持原样");if(Button("暂停变身并编辑")){state.data.transformsEnabled=false;ResetTransforms();Changed("变身已暂停，可以编辑基础显隐");}}
 auto& s=Get();auto* g=Find(groupId);if(!g){ImGui::TextDisabled("等待装备加载…");return;}
 if(creating)ImGui::TextColored(accent,"分组选择模式：勾选要交给该组管理的材质");
 else{ImGui::BeginDisabled(creating);if(ImGui::RadioButton("按子网格",meshMode))meshMode=true;ImGui::SameLine();if(ImGui::RadioButton("按材质",!meshMode))meshMode=false;ImGui::EndDisabled();}
 ImGui::SetNextItemWidth(-1);TextField("##搜索材质","搜索材质、网格备注或模型名称",search,sizeof(search));
 const char* parts[]={"全部","头部","身体","手臂","腰部","腿部"};float rowWidth=ImGui::GetContentRegionAvail().x,used=0;for(int p=-1;p<5;p++){float buttonWidth=std::min(rowWidth,ImGui::CalcTextSize(parts[p+1]).x+S(20));if(used&&used+S(8)+buttonWidth<=rowWidth){ImGui::SameLine(0,S(8));used+=S(8);}else used=0;if(PartButton(parts[p+1],part==p,buttonWidth))part=p;used+=buttonWidth;}
 ImGui::Separator();
 ImGui::BeginChild("材质滚动区",ImVec2(0,-S(meshMode&&!creating?52.f:28.f)));
 std::string filter=Lower(search);DWORD now=GetTickCount();bool any=false;
 for(int slot=0;slot<6;slot++)for(auto& model:s.models){if(model.equip!=outfit||!IsCurrentModel(model.relativePath)||PartIndex(model.part)!=slot||(part!=-1&&part!=slot))continue;auto path=Normalize(model.relativePath);bool active=s.trackingPlayer||s.lastSeen.count(path)&&(DWORD)(now-s.lastSeen[path])<1800;std::string head=std::string(PartName(slot))+"  ·  "+model.display+(active?"  [已加载]":"  [待加载]");ImGui::PushID(path.c_str());
  if(ImGui::CollapsingHeader(head.c_str(),ImGuiTreeNodeFlags_DefaultOpen)){
   std::map<uint32_t,std::vector<const Mesh*>> targets;for(auto& m:model.meshes){if(m.materialId>=model.materials.size())continue;targets[meshMode&&!creating?m.index:model.materials[m.materialId].hash].push_back(&m);}
   struct Item {Target target;const Material* mat;std::vector<const Mesh*> meshes;bool allowed,member;Group* owner;std::string label;};std::vector<Item> items;
   for(auto& [key,meshes]:targets){auto* mat=&model.materials[meshes[0]->materialId];auto* view=meshMode&&!creating?MeshView(model,key):nullptr;std::string label=view&&!view->note.empty()?view->note+"  ·  "+mat->name:mat->name;if(!filter.empty()&&Lower(label+model.display).find(filter)==std::string::npos)continue;auto* owner=Owner(path,mat->hash);bool allowed=creating?(globalGroup||!owner||owner->id==editingMembers):InGroup(*g,path,mat->hash);Target t{path,key,meshMode&&!creating};items.push_back({t,mat,meshes,allowed,staged.count(t)!=0,owner,label});}
   if(meshMode&&!creating){std::map<uint32_t,size_t> ranks;size_t rank=0;for(auto* mesh:OrderedMeshes(model))ranks[mesh->index]=rank++;std::stable_sort(items.begin(),items.end(),[&](const Item& a,const Item& b){return ranks.at(a.target.value)<ranks.at(b.target.value);});}
   else std::stable_sort(items.begin(),items.end(),[](const Item& a,const Item& b){return MaterialListOrder(*a.mat,a.meshes.front()->materialId)<MaterialListOrder(*b.mat,b.meshes.front()->materialId);});
   ImGui::BeginDisabled(!creating&&ActivePreset(*g));if(Button(creating?"全选":"全显示")){for(auto& item:items)if(item.allowed){if(creating)staged.insert(item.target);else SetVisible(*g,item.target,true);}}
   ImGui::SameLine();if(Button(creating?"清空选择":"全隐藏")){for(auto& item:items)if(item.allowed){if(creating)staged.erase(item.target);else SetVisible(*g,item.target,false);}}
   ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled("%zu 项",items.size());
   if(meshMode&&!creating){if(ImGui::GetContentRegionAvail().x>S(420))ImGui::SameLine();if(Button("恢复材质顺序"))ResetMeshOrder(model);}
   std::optional<ReorderList> order;if(meshMode&&!creating){std::vector<uint64_t> keys;for(auto& item:items)keys.push_back(item.target.value);order.emplace("meshes/"+path+"/"+std::to_string(model.signature),std::move(keys),ImGui::GetFrameHeight(),ImGui::GetStyle().ItemSpacing.y);}
   for(auto& item:items){if(order)order->BeginRow(item.target.value);ImGui::PushID((int)item.target.value);bool shown=true;size_t hidden=0;bool locked=false;for(auto* mesh:item.meshes){if(RuleHidden(EffectiveRules(*g),path,mesh->index,item.mat->hash))hidden++;if(!g->global&&GlobalHidden(path,mesh->index,item.mat->hash))locked=true;}shown=hidden==0;if(creating)shown=staged.count(item.target)!=0;if(locked&&!creating)shown=false;
    ImGui::BeginDisabled(!item.allowed||(!creating&&(locked||ActivePreset(*g))));if(ImGui::Checkbox("##可见",&shown)){if(creating){if(shown)staged.insert(item.target);else staged.erase(item.target);}else SetVisible(*g,item.target,shown);}ImGui::EndDisabled();
    float trailingWidth=0;
    if(locked&&!creating)trailingWidth=ImGui::CalcTextSize("全局隐藏").x+ImGui::GetStyle().ItemSpacing.x;
    else if(!item.allowed&&item.owner){auto tag="["+item.owner->name+"]";trailingWidth=ImGui::CalcTextSize(tag.c_str()).x+ImGui::GetStyle().ItemSpacing.x;}
    else if(!creating&&hidden&&hidden<item.meshes.size())trailingWidth=ImGui::CalcTextSize("部分隐藏").x+ImGui::GetStyle().ItemSpacing.x;
    ImGui::SameLine();if(item.target.mesh)MeshNameRow(model,item.target,item.label,item.allowed,order?&*order:nullptr,trailingWidth);else if(!item.allowed)ImGui::TextDisabled("%s",item.label.c_str());else ImGui::TextUnformatted(item.label.c_str());
    if(locked&&!creating){ImGui::SameLine();ImGui::TextColored(danger,"全局隐藏");}
    else if(!item.allowed&&item.owner){ImGui::SameLine();ImGui::TextColored(accent,"[%s]",item.owner->name.c_str());}
    else if(!creating&&hidden&&hidden<item.meshes.size()){ImGui::SameLine();ImGui::TextColored(muted,"部分隐藏");}
    ImGui::PopID();if(order)order->EndRow();any=true;
   }
   if(order)if(auto move=order->End()){
    uint32_t before=move->before==UINT64_MAX?UINT32_MAX:(uint32_t)move->before;
    if(before==UINT32_MAX){auto last=std::find_if(items.rbegin(),items.rend(),[&](auto& item){return item.target.value!=move->source;});if(last!=items.rend()){
      auto full=OrderedMeshes(model);auto at=std::find_if(full.begin(),full.end(),[&](auto* mesh){return mesh->index==last->target.value;});
      if(at!=full.end()){for(++at;at!=full.end();++at)if((*at)->index!=move->source){before=(*at)->index;break;}}
    }}MoveMeshBefore(model,(uint32_t)move->source,before);
   }
  }ImGui::PopID();
 }
 if(!any)ImGui::TextColored(muted,"尚无匹配材质。\n请进入场景、选择装备，或清空搜索。");
 ImGui::EndChild();ImGui::TextColored(muted,creating?"勾选 = 加入分组；普通组不能重复占用成员":"勾选 = 显示；取消 = 隐藏；恢复时保留游戏自身显隐");
 if(meshMode&&!creating)ImGui::TextColored(muted,"拖动网格名称调整顺序；右键编辑备注");MeshNotePopup();
}
#include "TransformPanel.inl"
void SettingsPanel(){
 auto& s=Get();ImGui::TextColored(accent,"启动与输入");if(ImGui::Checkbox("启用差分控制",&s.data.enabled))Changed();if(ImGui::Checkbox("进入游戏后自动打开面板",&s.data.autoOpen))Changed();
 const char* keys[]={"F5","F6","F7","F8","F9","F10","F11","F12"};int key=std::clamp(s.data.menuKey-VK_F5,0,7);ImGui::SetNextItemWidth(S(180));if(ImGui::Combo("开关面板快捷键",&key,keys,8)){s.data.menuKey=VK_F5+key;Changed();}
 ImGui::SetNextItemWidth(S(220));if(ImGui::SliderFloat("界面缩放",&s.data.scale,.7f,1.5f,"%.2f"))Changed();
 ImGui::Separator();ImGui::TextColored(accent,"配置与恢复");if(Button("立即保存配置")){if(Save())s.notice="配置已保存";else s.notice="配置保存失败，请检查目录是否可写";}ImGui::SameLine();if(Button("恢复当前组")){if(auto* g=Find(groupId))Restore(*g);}
 if(Button("暂停全部并恢复游戏显隐"))RestoreAll();Tip("不会删除任何已保存预设；重新启用控制后，可选择预设恢复方案。");
 if(Button("重新读取当前装备文件")){if(ReloadEquipmentProfile(outfit)){groupId=Main(outfit).id;s.notice="已重新读取当前装备文件";}}
 ImGui::TextWrapped("Settings.cfg 只保存插件设置；当前装备的分组、预设、变身、网格顺序与备注保存在 Presets/装备编号.wdp。Templates 中的模板用于查找与分享，日常编辑不会写回模板。");
 if(ImGui::Checkbox("换装后自动识别预设文件",&s.data.autoFind))Changed();
 ImGui::Separator();ImGui::TextColored(accent,"操作说明");ImGui::TextWrapped("普通分组独占成员，主列表不再管理已分组材质。全局分组可以与其他分组重叠，只叠加隐藏。每个分组保存独立预设和默认预设。列表支持拖动排序。");
 ImGui::TextWrapped("关闭面板后规则仍然生效。Ctrl+F9 可暂停全部控制；Esc 关闭面板。插件随 Stracker's Loader 加载，不需要连接或外置程序。");
 ImGui::TextWrapped("换装时只读取当前主角外观对应的装备文件。手动或自动查找从 Templates 导入独立副本。自动查找要求当前部位完整匹配且候选唯一；混搭时只列出当前实际穿着的装备。");
 ImGui::TextColored(accent,"联机队友：%zu 人，已匹配预设：%zu 人",s.peerCount,s.matchedPeers);
 ImGui::TextWrapped("面板始终编辑本地主角。队友按自身装备只读匹配本机预设；同装备复用当前基础选择，变身按每个角色独立计算。未匹配时保留游戏显隐，不生成队友配置。效果只影响本机画面。");
 ImGui::TextWrapped("远端动作缺失或为无效值时不触发动作条件。武器强化等条件取决于游戏同步的数据，各武器仍需实机核对。");
 if(Button("恢复预设备份")){backupFiles=ProfileBackups(outfit);ImGui::OpenPopup("恢复当前装备备份");}
 if(ImGui::BeginPopupModal("恢复当前装备备份",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::TextUnformatted("每份装备文件保留最近五份不同内容的备份。文件名中的时间为 UTC。");
  for(auto& name:backupFiles)if(ImGui::Selectable(name.c_str())){if(RestoreProfileBackup(outfit,name)){groupId=Main(outfit).id;ImGui::CloseCurrentPopup();}}
  if(backupFiles.empty())ImGui::TextDisabled("当前装备尚无备份");if(Button("取消##恢复备份"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
 }
 ImGui::TextWrapped("名称输入使用原生 Unicode/IME 文本框，支持中文、日文和粘贴。回车完成输入，Esc 取消本次编辑。关闭面板后恢复游戏的鼠标与键盘输入。");
 ImGui::TextColored(muted,"渲染：%s",BackendStatus());if(s.live)ImGui::TextColored(muted,"运行时记录 %ld / %u · %s",s.live->rowCount,kMaxRows,s.live->overflow?"已满，请重启游戏":"正常");
}
}
void DrawPanel(){
 auto& s=app::Get();std::lock_guard lock(s.mutex);if(!s.loaded)return;
 std::set<std::string> scopes,active;DWORD now=GetTickCount();for(auto& m:s.models)if(IsCurrentModel(m.relativePath)){scopes.insert(m.equip);if(s.trackingPlayer||s.lastSeen.count(m.relativePath)&&(DWORD)(now-s.lastSeen[m.relativePath])<1800)active.insert(m.equip);}
 std::string identity=outfit;if(s.trackingPlayer){identity=std::to_string(s.outfit.entity)+(s.outfit.savePreview?"/save":"/play");for(size_t i=0;i<5;i++)identity+=s.outfit.paths[i]+std::to_string(s.outfit.resources[i]);}
 if(displayedOutfit!=identity){displayedOutfit=identity;creating=false;staged.clear();editingMembers=0;openMeshNote=false;confirmGroup=confirmPreset=renameGroup=0;reorderDrag.cancelled=true;if(ImGui::GetCurrentContext()->OpenPopupStack.Size)ImGui::ClosePopupToLevel(0,true);if(s.trackingPlayer)NativeInputClose();}
 if(s.trackingPlayer&&(!s.outfit.ready||s.outfitChanging||!s.storageReady))scopes.clear();
 if(outfit.empty()||(!active.empty()&&!active.count(outfit))){if(!active.empty())outfit=*active.begin();else if(!scopes.empty())outfit=*scopes.begin();}
 if(scopes.empty()||outfit.empty()){if(ImGui::Begin("差分管理器 · 世界版 1.2.0###WorldDiffInternal")){ImGui::TextUnformatted(s.storageReady?"正在等待主角外观部位稳定加载…":s.notice.c_str());if(Button("关闭面板"))SetVisible(false);}ImGui::End();return;}
 if(!scopes.count(outfit))outfit=*scopes.begin();
 if(!Find(groupId)||Find(groupId)->scope!=outfit){groupId=Main(outfit).id;creating=false;staged.clear();}
 auto& io=ImGui::GetIO();float scale=s.data.scale;ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x-S(60),S(1460)),std::min(io.DisplaySize.y-S(80),S(850))),ImGuiCond_FirstUseEver);ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_FirstUseEver,ImVec2(.5f,.5f));ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(S(1050),io.DisplaySize.x-30),std::min(S(620),io.DisplaySize.y-30)),ImVec2(io.DisplaySize.x,io.DisplaySize.y));
 ImGui::PushFont(nullptr,22*scale);bool open=true;ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(18),S(14)));
 if(ImGui::Begin("差分管理器 · 世界版 1.2.0###WorldDiffInternal",&open,ImGuiWindowFlags_NoCollapse)){
  if(ImGui::BeginTable("装备与控制",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX)){
  float controlsWidth=ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+ImGui::CalcTextSize("启用").x+ImGui::GetStyle().ItemSpacing.x+ImGui::CalcTextSize("暂停并恢复").x+ImGui::GetStyle().FramePadding.x*2;
  ImGui::TableSetupColumn("装备状态",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("控制",ImGuiTableColumnFlags_WidthFixed,controlsWidth);ImGui::TableNextColumn();
  auto equipment=EquipmentLabel(outfit);const char* status=s.data.enabled?"显隐控制运行中":"已暂停 · 全部跟随游戏";
  float statusWidth=ImGui::CalcTextSize(status).x+ImGui::CalcTextSize("装备").x+ImGui::GetStyle().ItemSpacing.x*3;
  float comboWidth=std::min(std::max(S(140),ImGui::GetContentRegionAvail().x-statusWidth),std::max(S(225),ImGui::CalcTextSize(equipment.c_str()).x+ImGui::GetFrameHeight()+ImGui::GetStyle().FramePadding.x*2));
  if(scopes.size()==1){ImGui::TextUnformatted(equipment.c_str());EquipmentTip(outfit);}
  else{ImGui::SetNextItemWidth(comboWidth);if(ImGui::BeginCombo("当前混搭部位",equipment.c_str())){for(auto& scope:scopes){auto label=EquipmentLabel(scope);if(ImGui::Selectable(label.c_str(),outfit==scope)){outfit=scope;groupId=Main(outfit).id;creating=false;staged.clear();}EquipmentTip(scope);}ImGui::EndCombo();}}
  ImGui::SameLine();ImGui::TextColored(muted,"%s%s",s.outfit.savePreview?"存档预览 · ":"",s.data.enabled?"显隐控制运行中":"已暂停 · 全部跟随游戏");
  ImGui::TableNextColumn();if(ImGui::Checkbox("启用",&s.data.enabled))Changed();ImGui::SameLine();if(Button("暂停并恢复"))RestoreAll();ImGui::EndTable();
  }ImGui::Separator();
  bool requestFind=false,requestExport=false,headerTable=!s.data.hideDragHint&&ImGui::BeginTable("查找与提示",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX);if(headerTable){ImGui::TableSetupColumn("查找",ImGuiTableColumnFlags_WidthFixed,S(550));ImGui::TableSetupColumn("提示",ImGuiTableColumnFlags_WidthStretch);ImGui::TableNextColumn();}
  if(Button("手动查找")){profileFiles=FindProfiles(outfit);requestFind=true;}ImGui::SameLine();if(Button("自动查找")){if(AutoFindProfile(outfit)){groupId=Main(outfit).id;creating=false;staged.clear();}}
  ImGui::SameLine();ImGui::BeginDisabled(!CanEditScope(outfit));if(Button("导出模板")){profileName[0]=0;requestExport=true;}ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Checkbox("换装自动识别",&s.data.autoFind))Changed();
  if(headerTable){ImGui::TableNextColumn();DragHint();ImGui::EndTable();}if(requestFind)ImGui::OpenPopup("手动查找预设文件");if(requestExport)ImGui::OpenPopup("另存预设文件");
  auto bound=s.data.bindings.find(outfit);ImGui::TextColored(muted,"%s",s.profileStatus.count(outfit)?s.profileStatus[outfit].c_str():(bound==s.data.bindings.end()?"当前装备尚无预设，可查找模板或直接编辑":bound->second.c_str()));
  if(ImGui::BeginPopupModal("手动查找预设文件",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
   ImGui::Text("当前装备：%s · 找到 %zu 份预设文件",EquipmentLabel(outfit).c_str(),profileFiles.size());
   ImGui::TextWrapped("从 Templates 导入到当前装备，源模板保持原样。已有装备文件会先备份，再保存独立副本。");
   ImGui::BeginChild("预设文件列表",ImVec2(S(680),S(340)),ImGuiChildFlags_Borders);
   for(auto& f:profileFiles){ImGui::PushID(f.file.c_str());std::string label=f.file+"  · 当前网格覆盖 "+std::to_string((int)(f.coverage*100))+"%"+(f.strong?"  [完整匹配]":"");if(ImGui::Selectable(label.c_str())){if(ApplyProfile(outfit,f.file)){groupId=Main(outfit).id;creating=false;staged.clear();ImGui::CloseCurrentPopup();}}ImGui::PopID();}
   if(profileFiles.empty())ImGui::TextWrapped("尚无模板。可将分享的 .wdp 放入 WorldDiff/Templates，再刷新列表。");ImGui::EndChild();if(Button("刷新列表"))profileFiles=FindProfiles(outfit);ImGui::SameLine();if(Button("取消##查找"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
  }
  if(ImGui::BeginPopupModal("另存预设文件",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("保存当前装备的全部分组与预设，可供同款 MOD 的其他装备识别。");ImGui::SetNextItemWidth(S(400));TextField("##预设文件名","如：黎明巫女 或 pl105_0000",profileName,sizeof(profileName));if(Button("保存文件")){if(ExportProfile(outfit,profileName)){Changed("预设文件已另存，换装可通过材质识别复用");ImGui::CloseCurrentPopup();}}ImGui::SameLine();if(Button("取消##另存"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
  ImGui::Separator();
  float height=ImGui::GetContentRegionAvail().y-S(43);float available=ImGui::GetContentRegionAvail().x;float left=std::max(S(240),available*.22f),middle=std::max(S(310),available*.25f);
  ImGui::BeginDisabled(!CanEditScope(outfit));GroupsPane(left,height);ImGui::SameLine();PresetsPane(middle,height);ImGui::EndDisabled();ImGui::SameLine();
  ImGui::BeginChild("右侧面板",ImVec2(0,height),ImGuiChildFlags_Borders);
  const char* pages[]={"部位与材质","变身与条件","设置与说明"};float pageWidth=ImGui::GetContentRegionAvail().x,used=0;
  for(int i=0;i<3;i++){float width=ImGui::CalcTextSize(pages[i]).x+S(20);if(used&&used+S(8)+width<=pageWidth){ImGui::SameLine(0,S(8));used+=S(8);}else used=0;if(PartButton(pages[i],tab==i,width))tab=i;used+=width;}
  ImGui::Separator();if(tab==2)SettingsPanel();else if(!CanEditScope(outfit)){ImGui::TextWrapped("当前预设与模型不匹配。可重新匹配当前文件，或使用上方查找按钮导入模板。原文件会先备份。");if(Button("重新匹配当前文件"))if(RebindEquipmentProfile(outfit))groupId=Main(outfit).id;}else if(tab==0)TargetsPanel();else TransformPanel();ImGui::EndChild();
  ImGui::Separator();ImGui::TextColored(muted,"%s",s.notice.empty()?"配置自动保存 · F9 开关面板 · Ctrl+F9 紧急恢复":s.notice.c_str());
 }
 ImGui::End();ImGui::PopStyleVar();ImGui::PopFont();if(!open)SetVisible(false);
}
}
