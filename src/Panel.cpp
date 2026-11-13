#include "Overlay.h"
#include "Version.h"
#include "State.h"
#include "ArmorNames.h"
#include "NativeInput.h"
#include "UpdateNotes.h"
#include "UpdateNotesLayout.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ClassicTheme.h"
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
char profileName[256]{};
std::string exportSelection,exportConflict,exportError;
std::vector<std::string> exportFiles;
std::vector<ProfileCandidate> profileFiles;
std::string selectedProfileFile;
std::vector<std::string> backupFiles;std::string displayedOutfit;
uint64_t editingMembers=0,confirmGroup=0,confirmPreset=0,renameGroup=0;
Target meshNoteTarget;char meshNote[192]{};bool openMeshNote=false;
ImVec4 accent{0.33f,0.72f,0.78f,1},muted{0.6f,0.66f,0.72f,1},danger{0.91f,0.51f,0.51f,1};
float PanelScale(){
 const float preferred=Get().data.scale;
 const auto screen=ImGui::GetIO().DisplaySize;
 if(screen.x<=30||screen.y<=30)return preferred;
 return std::min({preferred,(screen.x-30.f)/1050.f,(screen.y-30.f)/620.f});
}
float S(float x){return x*PanelScale();}
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
#include "PaperWidgets.inl"
bool Button(const char* text,float width=0,ButtonTone tone=ButtonTone::Normal){return PanelButton(text,ImVec2(width>0?S(width):0,S(30)),tone);}
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
 if(paper::active){
  auto tone=icon==Icon::Plus||icon==Icon::Star?ButtonTone::Primary:icon==Icon::Trash?ButtonTone::Delete:ButtonTone::Normal;
  return PanelButton(label,ImVec2(width,S(height)),tone);
 }
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
void BeginActions(const char* name,float height){ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(10),S(10)));ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(S(8),S(8)));if(paper::active)ImGui::PushStyleColor(ImGuiCol_ChildBg,paper::colors->actionArea);ImGui::BeginChild(name,ImVec2(0,height),(paper::active?ImGuiChildFlags_AlwaysUseWindowPadding:0)|ImGuiChildFlags_Borders,FixedPane);if(paper::active)ImGui::PopStyleColor();PaperRule(PaneSurface::Inset);}
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
 ImGui::PushStyleColor(ImGuiCol_ChildBg,paper::active?ImGui::ColorConvertU32ToFloat4(paper::colors->panel):ImVec4(.025f,.10f,.15f,.95f));ImGui::PushStyleColor(ImGuiCol_Border,paper::active?ImGui::ColorConvertU32ToFloat4(paper::colors->muted):ImVec4(.13f,.59f,.70f,.95f));
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(padding,S(9)));ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(gap*.5f,0));ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,paper::IsGlass()?S(12):paper::active?0.f:S(8));
 ImGui::BeginChild("拖动提示",ImVec2(width,contentHeight+S(18)),(paper::active?ImGuiChildFlags_AlwaysUseWindowPadding:0)|ImGuiChildFlags_Borders,FixedPane);
 if(paper::active){auto* w=ImGui::GetCurrentWindow();auto* d=w->DrawList;d->PushClipRect(w->OuterRectClipped.Min,w->OuterRectClipped.Max,false);d->AddRect(w->Pos,{w->Pos.x+w->Size.x,w->Pos.y+w->Size.y},paper::IsGlass()?paper::glass.rule:paper::colors->muted,paper::IsGlass()?S(12):0);d->PopClipRect();}
 if(ImGui::BeginTable("提示内容",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX)){
  ImGui::TableSetupColumn("信息",ImGuiTableColumnFlags_WidthFixed,iconWidth);ImGui::TableSetupColumn("内容",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("关闭",ImGuiTableColumnFlags_WidthFixed,closeWidth);ImGui::TableNextColumn();
  ImGui::Dummy(ImVec2(iconWidth,contentHeight));auto p=ImGui::GetItemRectMin();auto* draw=ImGui::GetWindowDrawList();ImVec2 center(p.x+iconWidth*.5f,p.y+contentHeight*.5f);auto ink=IM_COL32(160,218,242,255),dark=IM_COL32(8,37,52,255);
  if(paper::active){ink=paper::colors->text;dark=paper::colors->onSignal;if(paper::IsGlass()){dark=paper::glass.primary;draw->AddCircleFilled(center,S(11),paper::rgb(0xd2e7ff));}else draw->AddRectFilled({center.x-S(11),center.y-S(11)},{center.x+S(11),center.y+S(11)},paper::colors->signal);}else draw->AddCircleFilled(center,S(12),ink);draw->AddCircleFilled(ImVec2(center.x,center.y-S(5)),S(1.5f),dark);draw->AddRectFilled(ImVec2(center.x-S(1.4f),center.y-S(1.5f)),ImVec2(center.x+S(1.4f),center.y+S(6)),dark,S(1));
  ImGui::TableNextColumn();float textWidth=ImGui::GetContentRegionAvail().x;ImGui::Dummy(ImVec2(textWidth,contentHeight));auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
  draw->PushClipRect(min,max,true);draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(min.x,min.y+(contentHeight-textSize.y)*.5f),ink,message,nullptr,textWidth);draw->PopClipRect();
  ImGui::TableNextColumn();bool close=ImGui::InvisibleButton("关闭拖动提示",ImVec2(closeWidth,contentHeight));p=ImGui::GetItemRectMin();center=ImVec2(p.x+closeWidth*.5f,p.y+contentHeight*.5f);auto cross=paper::active?paper::colors->text:ImGui::IsItemHovered()?IM_COL32(218,248,255,255):ink;float r=S(5),stroke=S(1.8f);
  draw->AddLine(ImVec2(center.x-r,center.y-r),ImVec2(center.x+r,center.y+r),cross,stroke);draw->AddLine(ImVec2(center.x-r,center.y+r),ImVec2(center.x+r,center.y-r),cross,stroke);
  if(close){Get().data.hideDragHint=true;Changed("拖动提示已隐藏，下次启动不再显示");Save();}Tip("关闭此提示，下次启动不再显示");ImGui::EndTable();
 }ImGui::EndChild();ImGui::PopStyleVar(3);ImGui::PopStyleColor(2);
}
bool PartButton(const char* name,bool selected,float width){
 if(paper::active)return PanelButton(name,{width,ImGui::GetFrameHeight()},selected?ButtonTone::Selected:ButtonTone::Normal);
 ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,S(8));ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,selected?S(1.5f):0.f);
 ImGui::PushStyleColor(ImGuiCol_Border,ImVec4(.25f,.83f,.9f,.95f));if(selected)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.10f,.43f,.52f,1));
 bool pressed=ImGui::Button(name,ImVec2(width,ImGui::GetFrameHeight()));if(selected){auto* draw=ImGui::GetWindowDrawList();draw->AddRect(ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),IM_COL32(61,214,231,35),S(8),S(4));ImGui::PopStyleColor();}ImGui::PopStyleColor();ImGui::PopStyleVar(2);return pressed;
}
bool ListRow(const char* label,bool selected,bool ghost=false,const char* badge=nullptr,bool templateRow=false){
 // Let Selectable own layout, navigation and drag/drop. Draw the rounded row
 // inside its spacing-expanded item rectangle, with text centered in that row.
 const float height=S(42),inset=S(2),gap=S(6),radius=S(8);
 float width=std::max(1.f,ImGui::GetContentRegionAvail().x);
 ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,gap));
 ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0,0,0,0));
 ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0,0,0,0));
 ImGui::PushStyleColor(ImGuiCol_HeaderActive,ImVec4(0,0,0,0));
 bool pressed=ImGui::Selectable(templateRow?"##template":"##ListRow",selected,templateRow?ImGuiSelectableFlags_NoAutoClosePopups:0,ImVec2(width,height));
 ImGui::PopStyleColor(3);ImGui::PopStyleVar();
 if(ImGui::IsItemVisible()&&!ghost){
  auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
  min.x+=inset;max.x-=inset;min.y+=gap*.5f;max.y-=gap*.5f;
  bool hovered=ImGui::IsItemHovered(),active=selected||pressed;
  auto* draw=ImGui::GetWindowDrawList();
  if(paper::active){PaperRow(draw,min,max,label,active,badge);return pressed;}
  if(active){
   draw->AddRectFilled(min,max,IM_COL32(28,65,76,245),radius);
   draw->AddRect(min,max,IM_COL32(75,208,220,30),radius,S(4));
   draw->AddRect(min,max,IM_COL32(82,216,227,hovered?175:120),radius,S(1));
   draw->AddRect(ImVec2(min.x+S(2),min.y+S(2)),ImVec2(max.x-S(2),max.y-S(2)),IM_COL32(92,223,232,22),S(6),S(1));
   draw->AddRectFilled(ImVec2(min.x+S(1),min.y+S(2)),ImVec2(min.x+S(7),max.y-S(2)),IM_COL32(82,226,236,255),S(3));
  }else if(templateRow){
   draw->AddRectFilled(min,max,hovered?IM_COL32(38,64,76,220):IM_COL32(38,49,61,220),radius);
   draw->AddRect(min,max,ImGui::GetColorU32(ImGuiCol_Border),radius);
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
 auto& s=Get();ImGui::BeginChild("分组面板",ImVec2(width,height),(paper::active?ImGuiChildFlags_AlwaysUseWindowPadding:0)|ImGuiChildFlags_Borders,FixedPane);
 PaperRule();PaperKicker("//  GROUPS");if(paper::active)SectionTitle("分组");else ImGui::TextColored(muted,"分组");ImGui::Separator();
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
  ImGui::BeginDisabled(editingMembers!=0);PanelCheckbox("全局分组",&globalGroup);ImGui::EndDisabled();Tip("全局分组可与其他分组重叠，并叠加隐藏");
  ImGui::Text("已选择 %zu 个材质",staged.size());
  ImGui::BeginDisabled(staged.empty()||!groupName[0]);if(Button("确认")){
   uint64_t id=0;bool ok=false;
   if(editingMembers){auto* edit=Find(editingMembers);if(edit){bool valid=true;for(auto& t:staged){auto* owner=Owner(t.model,t.value);if(!edit->global&&owner&&owner->id!=edit->id)valid=false;}if(valid&&RenameGroup(edit->id,groupName)){edit->members=staged;Changed("分组成员已更新");id=edit->id;ok=true;}}}
   else ok=CreateGroup(groupName,outfit,globalGroup,staged,id);
   if(ok){groupId=id;creating=false;staged.clear();}else s.notice="创建失败：检查重名、选择范围或普通组归属";
  }ImGui::EndDisabled();ImGui::SameLine();if(Button("取消")){creating=false;editingMembers=0;staged.clear();}
 }
 if(PanelModal("删除分组？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("分组内的预设将一起删除，材质控制权归还主列表。");if(Button("删除")){DeleteGroup(confirmGroup);groupId=Main(outfit).id;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##删除组"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 if(PanelModal("重命名分组",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){TextField("名称","分组名称",rename,sizeof(rename));if(Button("保存")){if(RenameGroup(renameGroup,rename))ImGui::CloseCurrentPopup();else s.notice="分组名称为空或重复";}ImGui::SameLine();if(Button("取消##改组名"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 EndActions();
 ImGui::EndChild();
}
void PresetsPane(float width,float height){
 auto& s=Get();auto* g=Find(groupId);ImGui::BeginChild("预设面板",ImVec2(width,height),(paper::active?ImGuiChildFlags_AlwaysUseWindowPadding:0)|ImGuiChildFlags_Borders,FixedPane);PaperRule(PaneSurface::Presets);PaperKicker("//  PRESETS");if(paper::active)SectionTitle(((g?g->name:std::string("主列表"))+" · 预设").c_str());else ImGui::TextColored(muted,"%s · 预设",g?g->name.c_str():"主列表");ImGui::Separator();
 float actionsHeight=S(212);
 ImGui::BeginChild("预设列表",ImVec2(0,std::max(S(20),ImGui::GetContentRegionAvail().y-actionsHeight-ImGui::GetStyle().ItemSpacing.y)));
 PaperContours(g?g->presets.size():0);
 if(g){std::vector<uint64_t> keys;for(auto& p:g->presets)keys.push_back(p.id);ReorderList order("presets/"+std::to_string(g->id),keys,S(42),S(6));
  for(auto key:keys){auto it=std::find_if(g->presets.begin(),g->presets.end(),[&](auto& p){return p.id==key;});if(it==g->presets.end())continue;auto& p=*it;bool ghost=order.BeginRow(key);ImGui::PushID((int)p.id);auto label=p.name+(!paper::active&&g->defaultPreset==p.id?"  [默认]":"");
   auto active=ActivePreset(*g);if(active==p.id)label+="  [变身]";if(ListRow(label.c_str(),(active?active:g->selected)==p.id,ghost,paper::active&&g->defaultPreset==p.id?"默认":nullptr)&&!order.Dragging())ApplyPreset(*g,p.id);
   order.Source(label);ImGui::PopID();order.EndRow();
  }if(auto move=order.End())MovePresetBefore(*g,move->source,move->before);
 }
 if(g&&g->presets.empty())ImGui::TextColored(muted,"尚无预设\n先调整右侧显隐，再保存");ImGui::EndChild();
 BeginActions("预设操作",actionsHeight);
 ImGui::TextColored(muted,"保存当前分组的状态");float saveWidth=S(124),inputWidth=std::max(S(60),ImGui::GetContentRegionAvail().x-saveWidth-S(8));ImGui::SetNextItemWidth(inputWidth);TextField("##预设名称","新预设名称",presetName,sizeof(presetName));ImGui::SameLine(0,S(8));ImGui::BeginDisabled(!g||!presetName[0]);
 if(ActionButton("保存新预设",Icon::Save,saveWidth,ImGui::GetFrameHeight()/PanelScale())){if(SavePreset(*g,presetName)){presetName[0]=0;}else s.notice="预设名为空或已存在，请使用覆盖";}ImGui::EndDisabled();
 Preset* selected=nullptr;if(g)for(auto& p:g->presets)if(p.id==g->selected)selected=&p;
 bool modified=selected&&(selected->rules.hidden!=g->working.hidden||selected->rules.shown!=g->working.shown);
 ImGui::TextColored(muted,g&&ActivePreset(*g)?"基础预设：%s%s":"当前预设：%s%s",selected?selected->name.c_str():"未保存的调整",modified?"（已修改）":"");
 float cell=(ImGui::GetContentRegionAvail().x-S(16))/3.f;
 ImGui::BeginDisabled(!selected);if(ActionButton("设为默认",Icon::Star,cell))SetDefault(*g,selected->id);ImGui::SameLine(0,S(8));if(ActionButton("覆盖预设",Icon::Copy,cell))SavePreset(*g,selected->name,true);
 ImGui::SameLine(0,S(8));if(ActionButton("删除预设",Icon::Trash,cell)){confirmPreset=selected->id;ImGui::OpenPopup("删除预设？");}ImGui::EndDisabled();
 if(PanelCheckbox("选择时自动设为默认",&s.data.autoDefault)){if(s.data.autoDefault&&g&&g->selected)SetDefault(*g,g->selected);Changed();}
 Tip("默认预设会随游戏自动加载；普通组分别记忆，全局组叠加生效。");
 if(PanelModal("删除预设？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("删除此预设？当前画面状态保持不变。");if(Button("删除")){if(g)DeletePreset(*g,confirmPreset);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##删预设"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
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
  if(paper::active)draw->AddLine({min.x,max.y},{max.x,max.y},ImGui::GetColorU32(paper::colors->rule));
  if(ImGui::IsItemHovered()||ImGui::IsItemActive())draw->AddRectFilled(min,max,paper::active?ImGui::GetColorU32(paper::colors->button):ImGui::IsItemActive()?IM_COL32(28,65,76,220):IM_COL32(38,64,76,160),paper::active?0:S(8));
  if(ImGui::IsItemFocused())draw->AddRect(min,max,paper::active?paper::colors->muted:IM_COL32(133,235,242,200),paper::active?0:S(8),S(1));
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
 if(!PanelModal("编辑网格备注",nullptr,ImGuiWindowFlags_AlwaysAutoResize))return;
 auto& s=Get();auto model=std::find_if(s.models.begin(),s.models.end(),[&](auto& m){return Normalize(m.relativePath)==meshNoteTarget.model;});
 ImGui::TextUnformatted("备注只用于显示，留空可恢复原名称。");ImGui::SetNextItemWidth(S(360));TextField("##网格备注","输入网格备注",meshNote,sizeof(meshNote));
 if(Button("保存备注")){
  if(model!=s.models.end()&&SetMeshNote(*model,meshNoteTarget.value,meshNote))ImGui::CloseCurrentPopup();else s.notice="备注过长、包含控制字符或网格已失效";
 }ImGui::SameLine();if(Button("取消##网格备注"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
}
// All controls share the native frame height, including the themed checkbox.
void HeightFooter(bool narrow) {
 ImGui::Separator();
 ImGui::PushID(outfit.c_str());
 auto value=GetEquipmentHeight(outfit);
 const float h=ImGui::GetFrameHeight(), gap=ImGui::GetStyle().ItemSpacing.x;
 const float resetWidth=ImGui::CalcTextSize("归零").x+ImGui::GetStyle().FramePadding.x*2;
 auto help=[&]{
  PanelButton("?##陷地说明",{h,h});
  if(ImGui::IsItemHovered()||ImGui::IsItemFocused())
   ImGui::SetTooltip("开关和高度随当前装备预设保存。混搭时使用腿部外观所属装备的值。");
 };
 if(PanelCheckbox("陷地修复",&value.enabled,false,true))SetEquipmentHeight(outfit,value);
 if(narrow){
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,ImGui::GetContentRegionAvail().x-h));
  help();
 }else ImGui::SameLine();
 ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("高度 (cm)");
 ImGui::SameLine();
 const float inputWidth=std::max(S(58),ImGui::CalcTextSize("-10.0").x+2*ImGui::GetStyle().FramePadding.x);
 float tail=inputWidth+resetWidth+2*gap+(narrow?0.f:h+gap);
 float cm=value.tenthsCm/10.f;
 ImGui::SetNextItemWidth(std::max(S(20),ImGui::GetContentRegionAvail().x-tail));
 auto setCm=[&]{
  if(std::isfinite(cm)){value.tenthsCm=(int)std::lround(std::clamp(cm,-10.f,50.f)*10);SetEquipmentHeight(outfit,value);}
 };
 if(ImGui::SliderFloat("##陷地滑杆",&cm,-10.f,50.f,"",ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput))setCm();
 ImGui::SameLine();ImGui::SetNextItemWidth(inputWidth);
 if(ImGui::InputFloat("##陷地数值",&cm,0,0,"%.1f",ImGuiInputTextFlags_EnterReturnsTrue))setCm();
 ImGui::SameLine();
 if(PanelButton("归零",{resetWidth,h}))SetEquipmentHeight(outfit,{true,0});
 if(!narrow){ImGui::SameLine();help();}
 ImGui::PopID();
}
void TargetsPanel(){
 auto& state=Get();auto* current=Find(groupId);if(current&&ActivePreset(*current)&&!creating){ImGui::TextColored(accent,"变身正在控制当前组；基础预设与默认项保持原样");if(Button("暂停变身并编辑")){state.data.transformsEnabled=false;ResetTransforms();Changed("变身已暂停，可以编辑基础显隐");}}
 auto& s=Get();auto* g=Find(groupId);if(!g){ImGui::TextDisabled("等待装备加载…");return;}
 if(creating)ImGui::TextColored(accent,"分组选择模式：勾选要交给该组管理的材质");
 else{ImGui::BeginDisabled(creating);if(ImGui::RadioButton("按子网格",meshMode))meshMode=true;ImGui::SameLine();if(ImGui::RadioButton("按材质",!meshMode))meshMode=false;ImGui::EndDisabled();}
 if(paper::IsGlass())ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{S(34),ImGui::GetStyle().FramePadding.y});
 ImGui::SetNextItemWidth(-1);TextField("##搜索材质","搜索材质、网格备注或模型名称",search,sizeof(search));
 if(paper::IsGlass()){
  ImGui::PopStyleVar();auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
  ImVec2 center(lo.x+S(16),(lo.y+hi.y)*.5f-S(1));auto* draw=ImGui::GetWindowDrawList();
  draw->AddCircle(center,S(4.5f),paper::glass.muted,0,S(1.5f));
  draw->AddLine({center.x+S(3),center.y+S(3)},{center.x+S(8),center.y+S(8)},paper::glass.muted,S(1.5f));
 }

 const char* parts[]={"全部","头部","身体","手臂","腰部","腿部"};float rowWidth=ImGui::GetContentRegionAvail().x,used=0;for(int p=-1;p<5;p++){float buttonWidth=std::min(rowWidth,ImGui::CalcTextSize(parts[p+1]).x+S(20));if(used&&used+S(8)+buttonWidth<=rowWidth){ImGui::SameLine(0,S(8));used+=S(8);}else used=0;if(PartButton(parts[p+1],part==p,buttonWidth))part=p;used+=buttonWidth;}
 ImGui::Separator();
 const bool narrow=ImGui::GetContentRegionAvail().x<S(445);
 float footer=(ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.y)*(narrow?2:1)+ImGui::GetStyle().ItemSpacing.y*2+S(8);
 ImGui::BeginChild("材质滚动区",ImVec2(0,-footer));
 std::string filter=Lower(search);DWORD now=GetTickCount();bool any=false;
 for(int slot=0;slot<6;slot++)for(auto& model:s.models){if(model.equip!=outfit||!IsCurrentModel(model.relativePath)||PartIndex(model.part)!=slot||(part!=-1&&part!=slot))continue;auto path=Normalize(model.relativePath);bool active=s.trackingPlayer||s.lastSeen.count(path)&&(DWORD)(now-s.lastSeen[path])<1800;std::string head=std::string(PartName(slot))+"  ·  "+model.display+(active?"  [已加载]":"  [待加载]");ImGui::PushID(path.c_str());
  bool expanded;
  if(paper::active){
   auto title=std::string(PartName(slot))+"  |  "+model.display+"###model";
   expanded=PanelHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen);
   auto lo=ImGui::GetItemRectMin(),hi=ImGui::GetItemRectMax();
   if(hi.x-lo.x>ImGui::CalcTextSize(title.c_str(),nullptr,true).x+S(105))
    PaperBadge(ImGui::GetWindowDrawList(),{hi.x-S(8),(hi.y+lo.y)*.5f},S(20),active?"已加载":"待加载",paper::colors->onSelection);
  }else expanded=PanelHeader(head.c_str(),ImGuiTreeNodeFlags_DefaultOpen);
  if(expanded){
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
    ImGui::BeginDisabled(!item.allowed||(!creating&&(locked||ActivePreset(*g))));if(PanelCheckbox("##可见",&shown)){if(creating){if(shown)staged.insert(item.target);else staged.erase(item.target);}else SetVisible(*g,item.target,shown);}ImGui::EndDisabled();
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
 ImGui::TextColored(muted,creating?"勾选 = 加入分组；普通组不能重复占用成员":"勾选 = 显示；取消 = 隐藏；恢复时保留游戏自身显隐");
 if(meshMode&&!creating)ImGui::TextColored(muted,"拖动网格名称调整顺序；右键编辑备注");MeshNotePopup();
 ImGui::EndChild();
 HeightFooter(narrow);
}
#include "HotkeyPanel.inl"
#include "TransformPanel.inl"
notes::Cache updateNotes;
bool updateTabVisible=false;
std::string updateNotice;
// Keep spacing local to the four secondary pages, including the waiting screen.
struct SidebarSpacing {
 bool enabled;
 explicit SidebarSpacing(bool apply):enabled(apply){
  if(!enabled)return;
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{S(14),S(10)});
  ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing,{S(8),S(5)});
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{S(12),S(7)});
 }
 ~SidebarSpacing(){if(enabled)ImGui::PopStyleVar(3);}
};
void PageTabs(bool waiting=false){
 const char* pages[]={"部位与材质","变身与条件","预设快捷键","设置","关于"};float width=ImGui::GetContentRegionAvail().x,used=0;
 for(int i=waiting?3:0;i<5;i++){float itemWidth=ImGui::CalcTextSize(pages[i]).x+S(paper::active?22.f:20.f);if(used&&used+S(8)+itemWidth<=width){ImGui::SameLine(0,S(8));used+=S(8);}else used=0;if(PartButton(pages[i],tab==i||(waiting&&tab<3&&i==3),itemWidth)){tab=i;if(tab!=2)Get().captureHotkey=0;}used+=itemWidth;}
 ImGui::Separator();
}
void AboutPanel(){
 PaperKicker("//  ABOUT",14);
 auto path=Get().directory/L"更新日志.md";
 if(!updateTabVisible||!updateNotes.loaded||updateNotes.path!=path){updateNotes.Load(path);updateNotice.clear();}updateTabVisible=true;
 SectionTitle("差分管理器 · 世界版 " WORLDDIFF_VERSION_TEXT);ImGui::TextUnformatted("插件作者：明月别枝(moon)");ImGui::Separator();
 if(Button("编辑更新日志")){
  wchar_t folder[MAX_PATH]{};STARTUPINFOW start{sizeof(start)};PROCESS_INFORMATION process{};
  if(GetWindowsDirectoryW(folder,MAX_PATH)){
   auto editor=std::filesystem::path(folder)/L"notepad.exe";std::wstring command=L"\""+editor.wstring()+L"\" \""+path.wstring()+L"\"";
   if(CreateProcessW(editor.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&start,&process)){CloseHandle(process.hThread);CloseHandle(process.hProcess);updateNotice="保存文件后，点击“重新读取”查看。";}else updateNotice="未能打开记事本，请手动编辑 WorldDiff/更新日志.md。";
  }else updateNotice="未能打开记事本，请手动编辑 WorldDiff/更新日志.md。";
 }
 float buttonWidth=ImGui::CalcTextSize("重新读取").x+ImGui::GetStyle().FramePadding.x*2;
 if(ImGui::GetItemRectMax().x+buttonWidth+ImGui::GetStyle().ItemSpacing.x<=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x)ImGui::SameLine();
 if(Button("重新读取")){updateNotes.Load(path);updateNotice=updateNotes.document.error.empty()?"更新日志已刷新。":"";}
 ImGui::PushTextWrapPos();ImGui::TextColored(muted,"WorldDiff/更新日志.md · UTF-8 文本，可自行修改");if(!updateNotice.empty())ImGui::TextUnformatted(updateNotice.c_str());ImGui::PopTextWrapPos();
 ImGui::Separator();ImGui::BeginChild("更新日志正文",ImVec2(0,0));ImGui::PushTextWrapPos(-1);
 auto text=[&](const std::string& value){auto wrapped=notes::WrapText(value,ImGui::GetContentRegionAvail().x,ImGui::GetFont(),ImGui::GetFontSize());ImGui::TextUnformatted(wrapped.c_str());};
 auto& document=updateNotes.document;
 if(!document.error.empty())text(document.error);
 else if(document.lines.empty())ImGui::TextColored(muted,"尚无更新日志。可点击“编辑更新日志”添加内容。");
 for(auto& line:document.lines){
  if(line.kind==notes::Kind::Heading){ImGui::Spacing();ImGui::PushStyleColor(ImGuiCol_Text,accent);text(line.text);ImGui::PopStyleColor();}
  else if(line.kind==notes::Kind::Item){ImGui::Bullet();ImGui::SameLine();text(line.text);}
  else text(line.text);
 }
 ImGui::PopTextWrapPos();ImGui::EndChild();
}
void SettingsPanel(){
 PaperKicker("//  SETTINGS",14);
 auto& s=Get();const char* themes[]={"基础","经典白","经典黑","玻璃态"};
 ImGui::SetNextItemWidth(std::min(S(210),ImGui::GetContentRegionAvail().x*.55f));
 if(SidebarCombo("界面风格",themes[static_cast<int>(s.data.theme)])){
  for(int i=0;i<4;i++){
   bool selected=i==static_cast<int>(s.data.theme);ImGui::BeginDisabled(!s.storageReady);
   const char* label=themes[i];
   if(ImGui::Selectable(label,selected)&&!selected){
    auto previous=s.data.theme;s.data.theme=static_cast<UiTheme>(i);
    if(!SaveSettings())s.data.theme=previous;
   }
   if(selected)ImGui::SetItemDefaultFocus();ImGui::EndDisabled();
  }
  ImGui::EndCombo();
 }
 ImGui::Separator();SectionTitle("启动与输入");if(SidebarCheckbox("启用差分控制",&s.data.enabled))Changed();if(SidebarCheckbox("进入游戏后自动打开面板",&s.data.autoOpen))Changed();
 if(SidebarCheckbox("显示启动提示",&s.data.showStartupNotice)){Changed();Save();}Tip("启动时在左上角显示版本与快捷键，约 9 秒后自动消失。");
 ImGui::SetNextItemWidth(S(180));auto panelKey=MenuKeyName(s.data);
 if(ImGui::BeginCombo("开关面板快捷键",panelKey.c_str(),ImGuiComboFlags_HeightLarge)){
  if(ImGui::Selectable("默认：F9 / Home",s.data.menuKey==0))SetMenuKey(0);
  for(int key=VK_F1;key<=VK_F12;key++){auto label=PresetKeyName(key,0);if(ImGui::Selectable(label.c_str(),s.data.menuKey==key))SetMenuKey(key);}
  for(int key:{VK_HOME,VK_END,VK_INSERT,VK_PRIOR,VK_NEXT}){auto label=PresetKeyName(key,0);if(ImGui::Selectable(label.c_str(),s.data.menuKey==key))SetMenuKey(key);}
  ImGui::EndCombo();
 }Tip("默认 F9 或 Home 均可开关面板；选择其他选项后，只使用选定的一个键。不能与预设快捷键相同。");
 ImGui::SetNextItemWidth(S(220));if(ImGui::SliderFloat("界面缩放",&s.data.scale,.7f,1.5f,"%.2f"))Changed();
 ImGui::Separator();SectionTitle("网格溶解");
 ImGui::BeginDisabled(!s.storageReady);
 if(SidebarCheckbox("启用网格溶解",&s.data.dissolveEnabled) && !SaveSettings())s.data.dissolveEnabled=!s.data.dissolveEnabled;
 Tip("在 DX12 场景中，当前角色的网格会随显示和隐藏逐步变化。");
 ImGui::BeginDisabled(!s.data.dissolveEnabled);
 ImGui::SetNextItemWidth(std::min(S(210),ImGui::GetContentRegionAvail().x*.55f));
 if(SidebarCombo("溶解风格",dissolve::StyleNames[static_cast<int>(s.data.dissolveStyle)])){
  for(auto style:{dissolve::Style::Silver,dissolve::Style::Amber,dissolve::Style::Ink}){
   const bool selected=style==s.data.dissolveStyle;
   if(ImGui::Selectable(dissolve::StyleNames[static_cast<int>(style)],selected)&&!selected){
    const auto previous=s.data.dissolveStyle;s.data.dissolveStyle=style;
    if(!SaveSettings())s.data.dissolveStyle=previous;
   }
   if(selected)ImGui::SetItemDefaultFocus();
  }
  ImGui::EndCombo();
 }
 float seconds=s.data.dissolveMs/1000.f;
 ImGui::SetNextItemWidth(S(220));
 if(ImGui::SliderFloat("过渡时长",&seconds,.05f,10.f,"%.2f 秒",ImGuiSliderFlags_Logarithmic|ImGuiSliderFlags_AlwaysClamp))
  s.data.dissolveMs=std::clamp(static_cast<int>(std::lround(seconds*1000.f)),50,10000);
 if(ImGui::IsItemDeactivatedAfterEdit())SaveSettings();
 Tip("时长越短，溶解越快。Ctrl + 单击可直接输入秒数。");
 ImGui::EndDisabled();ImGui::EndDisabled();
 ImGui::Separator();SectionTitle("插件兼容");
 ImGui::BeginDisabled(!s.storageReady);
 if(SidebarCheckbox("差分管理器优先于变身插件",&s.data.worldDiffPriority)){
  if(!SaveSettings())s.data.worldDiffPriority=!s.data.worldDiffPriority;
 }
 ImGui::EndDisabled();
 Tip("默认开启。开启时，已匹配装备的显示与隐藏以差分管理器为准；关闭时，变身插件实际命中的网格以变身结果为准，其余仍由差分管理器控制。支持基础变身插件和进阶版；暂停差分控制后恢复原有显隐。");
 ImGui::Separator();SectionTitle("配置与恢复");if(Button("立即保存配置")){if(Save())s.notice="配置已保存";else s.notice="配置保存失败，请检查目录是否可写";}ImGui::SameLine();ImGui::BeginDisabled(!CanEditScope(outfit)||outfit.empty());if(Button("恢复当前组")){if(auto* g=Find(groupId))Restore(*g);}ImGui::EndDisabled();
 if(Button("暂停全部并恢复游戏显隐"))RestoreAll();Tip("不会删除任何已保存预设；重新启用控制后，可选择预设恢复方案。");
 ImGui::BeginDisabled(!CanEditScope(outfit)||outfit.empty());if(Button("重新读取当前装备文件")){if(ReloadEquipmentProfile(outfit)){groupId=Main(outfit).id;s.notice="已重新读取当前装备文件";}}ImGui::EndDisabled();
 ImGui::TextWrapped("Settings.cfg 只保存插件设置；当前装备的分组、预设、变身、网格顺序与备注保存在 Presets/装备编号.wdp。Templates 中的模板用于查找与分享，日常编辑不会写回模板。");
 if(SidebarCheckbox("换装后自动识别预设文件",&s.data.autoFind))Changed();
 ImGui::Separator();SectionTitle("操作说明");ImGui::TextWrapped("普通分组独占成员，主列表不再管理已分组材质。全局分组可以与其他分组重叠，只叠加隐藏。每个分组保存独立预设和默认预设。列表支持拖动排序。");
 ImGui::TextWrapped("关闭面板后规则仍然生效。Ctrl+F9 可暂停全部控制；Esc 关闭面板。插件随 Stracker's Loader 加载，不需要连接或外置程序。");
 ImGui::TextWrapped("换装时只读取当前主角外观对应的装备文件。手动或自动查找从 Templates 导入独立副本。唯一的高可信模板可自动应用。混搭时只列出当前实际穿着的装备。");
 ImGui::TextColored(accent,"联机队友：%zu 人，已匹配预设：%zu 人",s.peerCount,s.matchedPeers);
 ImGui::TextWrapped("面板始终编辑本地主角。队友按自身装备只读匹配本机预设；同装备复用当前基础选择，变身按每个角色独立计算。未匹配时保留游戏显隐，不生成队友配置。效果只影响本机画面。");
 ImGui::TextWrapped("远端动作缺失或为无效值时不触发动作条件。武器强化等条件取决于游戏同步的数据，各武器仍需实机核对。");
 ImGui::BeginDisabled(!CanEditScope(outfit)||outfit.empty());if(Button("恢复预设备份")){backupFiles=ProfileBackups(outfit);ImGui::OpenPopup("恢复当前装备备份");}ImGui::EndDisabled();
 if(PanelModal("恢复当前装备备份",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
  ImGui::TextUnformatted("每份装备文件保留最近五份不同内容的备份。文件名中的时间为 UTC。");
  for(auto& name:backupFiles)if(ImGui::Selectable(name.c_str())){if(RestoreProfileBackup(outfit,name)){groupId=Main(outfit).id;ImGui::CloseCurrentPopup();}}
  if(backupFiles.empty())ImGui::TextDisabled("当前装备尚无备份");if(Button("取消##恢复备份"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
 }
 ImGui::TextWrapped("名称输入使用原生 Unicode/IME 文本框，支持中文、日文和粘贴。回车完成输入，Esc 取消本次编辑。关闭面板后恢复游戏的鼠标与键盘输入。");
 ImGui::TextColored(muted,"渲染：%s",BackendStatus());if(s.live)ImGui::TextColored(muted,"运行时记录 %ld / %u · %s",s.live->rowCount,kMaxRows,s.live->overflow?"已满，请重启游戏":"正常");
}
}
// Keep the last manual size in pixels. Constraints and display changes only affect
// the current window, so returning from a smaller screen restores the preference.
static bool BeginSizedManager(bool* open,bool waiting){
 auto& s=app::Get();
 const auto display=ImGui::GetIO().DisplaySize;
 const ImVec2 minimum(S(waiting?420.f:1050.f),S(waiting?360.f:620.f));
 const ImVec2 maximum(std::max(minimum.x,display.x),std::max(minimum.y,display.y));
 const ImVec2 defaults(std::min(S(waiting?750.f:1460.f),display.x-S(waiting?30.f:60.f)),
                       std::min(S(waiting?650.f:850.f),display.y-S(waiting?30.f:80.f)));
 static ImVec2 lastDisplay{};
 static bool lastWaiting=false;
 auto* window=ImGui::FindWindowByName("###WorldDiffInternal");
 const bool restore=!window||lastDisplay.x!=display.x||lastDisplay.y!=display.y||lastWaiting!=waiting;
 const ImVec2 previous=window?window->SizeFull:ImVec2();
 if(restore){
  const ImVec2 wanted=s.panelWidth>0?ImVec2((float)s.panelWidth,(float)s.panelHeight):defaults;
  ImGui::SetNextWindowSize({std::clamp(wanted.x,minimum.x,maximum.x),std::clamp(wanted.y,minimum.y,maximum.y)});
  ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});
 }
 lastDisplay=display;lastWaiting=waiting;
 ImGui::SetNextWindowSizeConstraints(minimum,maximum);
 bool visible=BeginManager(open);
 window=ImGui::GetCurrentWindow();
 const auto size=window->SizeFull;
 const auto& ctx=*ImGui::GetCurrentContext();
 const auto cursor=ImGui::GetMouseCursor();
 const bool mouseResize=cursor==ImGuiMouseCursor_ResizeEW||cursor==ImGuiMouseCursor_ResizeNS||
                        cursor==ImGuiMouseCursor_ResizeNESW||cursor==ImGuiMouseCursor_ResizeNWSE;
 if(!restore&&s.storageReady&&(size.x!=previous.x||size.y!=previous.y)&&
    ((mouseResize&&ImGui::IsMouseDown(0))||ctx.NavWindowingTarget==window)){
  s.panelWidth=(int)size.x;s.panelHeight=(int)size.y;
  s.panelSizeDirty=true;s.panelSizeChangedAt=GetTickCount();
 }
 if(s.panelSizeDirty&&ImGui::IsMouseReleased(0))s.panelSizeDirty=!app::SaveSettings();
 return visible;
}
void DrawPanel(){
 auto& s=app::Get();std::lock_guard lock(s.mutex);if(!s.loaded||ImGui::GetIO().DisplaySize.x<=30||ImGui::GetIO().DisplaySize.y<=30)return;
 const float scale=PanelScale();
 paper::Scope theme(s.data.theme,scale);
 accent=paper::active?ImGui::ColorConvertU32ToFloat4(paper::colors->text):ImVec4(.33f,.72f,.78f,1);
 muted=paper::active?ImGui::ColorConvertU32ToFloat4(paper::colors->muted):ImVec4(.6f,.66f,.72f,1);
 danger=paper::active?ImGui::ColorConvertU32ToFloat4(paper::colors->danger):ImVec4(.91f,.51f,.51f,1);
 if(tab!=4)updateTabVisible=false;if(tab!=2)s.captureHotkey=0;
 std::set<std::string> scopes,active;DWORD now=GetTickCount();for(auto& m:s.models)if(IsCurrentModel(m.relativePath)){scopes.insert(m.equip);if(s.trackingPlayer||s.lastSeen.count(m.relativePath)&&(DWORD)(now-s.lastSeen[m.relativePath])<1800)active.insert(m.equip);}
 std::string identity=outfit;if(s.trackingPlayer){identity=std::to_string(s.outfit.entity)+(s.outfit.savePreview?"/save":"/play");for(size_t i=0;i<5;i++)identity+=s.outfit.paths[i]+std::to_string(s.outfit.resources[i]);}
 if(displayedOutfit!=identity){displayedOutfit=identity;creating=false;staged.clear();editingMembers=0;openMeshNote=false;confirmGroup=confirmPreset=renameGroup=0;reorderDrag.cancelled=true;if(ImGui::GetCurrentContext()->OpenPopupStack.Size)ImGui::ClosePopupToLevel(0,true);if(s.trackingPlayer)NativeInputClose();}
 if(s.trackingPlayer&&(!s.outfit.ready||s.outfitChanging||!s.storageReady))scopes.clear();
 if(outfit.empty()||(!active.empty()&&!active.count(outfit))){if(!active.empty())outfit=*active.begin();else if(!scopes.empty())outfit=*scopes.begin();}
 if(scopes.empty()||outfit.empty()){
  ImGui::PushFont(nullptr,(paper::active?20.f:22.f)*scale);bool open=true;if(BeginSizedManager(&open,true)){ImGui::TextWrapped("%s",s.storageReady?"正在等待主角外观部位稳定加载…":s.notice.c_str());SidebarSpacing spacing(paper::active);PageTabs(true);if(tab==4)AboutPanel();else SettingsPanel();AutomationConflictPopup();}ImGui::End();ImGui::PopFont();if(!open)SetVisible(false);return;
 }
 if(!scopes.count(outfit))outfit=*scopes.begin();
 if(!Find(groupId)||Find(groupId)->scope!=outfit){groupId=Main(outfit).id;creating=false;staged.clear();}
 ImGui::PushFont(nullptr,(paper::active?20.f:22.f)*scale);bool open=true;ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(18),S(14)));
 if(BeginSizedManager(&open,false)){
  if(ImGui::BeginTable("装备与控制",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX)){
  float controlsWidth=ImGui::GetFrameHeight()+ImGui::GetStyle().ItemInnerSpacing.x+ImGui::CalcTextSize("启用").x+ImGui::GetStyle().ItemSpacing.x+ImGui::CalcTextSize("暂停并恢复").x+ImGui::GetStyle().FramePadding.x*2;
  ImGui::TableSetupColumn("装备状态",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("控制",ImGuiTableColumnFlags_WidthFixed,controlsWidth);ImGui::TableNextColumn();
  auto equipment=EquipmentLabel(outfit);const char* status=s.data.enabled?"显隐控制运行中":"已暂停 · 全部跟随游戏";
  float statusWidth=ImGui::CalcTextSize(status).x+ImGui::CalcTextSize("装备").x+ImGui::GetStyle().ItemSpacing.x*3;
  float comboWidth=std::min(std::max(S(140),ImGui::GetContentRegionAvail().x-statusWidth),std::max(S(225),ImGui::CalcTextSize(equipment.c_str()).x+ImGui::GetFrameHeight()+ImGui::GetStyle().FramePadding.x*2));
  if(scopes.size()==1){if(paper::active)ImGui::PushFont(PaperBold(),S(21));ImGui::TextUnformatted(equipment.c_str());if(paper::active)ImGui::PopFont();EquipmentTip(outfit);}
  else{ImGui::SetNextItemWidth(comboWidth);if(ImGui::BeginCombo("当前混搭部位",equipment.c_str())){for(auto& scope:scopes){auto label=EquipmentLabel(scope);if(ImGui::Selectable(label.c_str(),outfit==scope)){outfit=scope;groupId=Main(outfit).id;creating=false;staged.clear();}EquipmentTip(scope);}ImGui::EndCombo();}}
  ImGui::SameLine();if(paper::active){
   auto badge=std::string(s.outfit.savePreview?"存档预览 · ":"")+status;float h=S(25);
   float w=PaperBold()->CalcTextSizeA(S(15),FLT_MAX,0,badge.c_str()).x+S(14);
   ImGui::Dummy({w,h});auto hi=ImGui::GetItemRectMax();PaperBadge(ImGui::GetWindowDrawList(),{hi.x,hi.y-h*.5f},h,badge.c_str(),paper::colors->text);
  }else ImGui::TextColored(muted,"%s%s",s.outfit.savePreview?"存档预览 · ":"",status);
  ImGui::TableNextColumn();if(PanelCheckbox("启用",&s.data.enabled))Changed();ImGui::SameLine();if(Button("暂停并恢复",0,ButtonTone::Primary))RestoreAll();ImGui::EndTable();
  }ImGui::Separator();
  bool requestFind=false,requestExport=false,headerTable=!s.data.hideDragHint&&ImGui::BeginTable("查找与提示",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoPadOuterX);if(headerTable){ImGui::TableSetupColumn("查找",ImGuiTableColumnFlags_WidthFixed,S(550));ImGui::TableSetupColumn("提示",ImGuiTableColumnFlags_WidthStretch);ImGui::TableNextColumn();}
  if(Button("手动查找",0,ButtonTone::Primary)){profileFiles=FindProfiles(outfit);selectedProfileFile.clear();requestFind=true;}ImGui::SameLine();if(Button("自动查找")){if(AutoFindProfile(outfit)){groupId=Main(outfit).id;creating=false;staged.clear();}}
  ImGui::SameLine();ImGui::BeginDisabled(!CanEditScope(outfit));if(Button("导出预设")){profileName[0]=0;exportSelection.clear();exportConflict.clear();exportError.clear();requestExport=true;}ImGui::EndDisabled();ImGui::SameLine();if(PanelCheckbox("换装自动识别",&s.data.autoFind))Changed();
  if(headerTable){ImGui::TableNextColumn();DragHint();ImGui::EndTable();}if(requestFind)ImGui::OpenPopup("手动查找预设文件");if(requestExport)ImGui::OpenPopup("导出预设文件");
  auto bound=s.data.bindings.find(outfit);
  const std::string profileStatus=s.profileStatus.count(outfit)?s.profileStatus[outfit]:(bound==s.data.bindings.end()?"当前装备尚无预设，可查找模板或直接编辑":bound->second);
  const std::string_view loadedPrefix="已载入装备预设：";
  if((paper::IsNight()||paper::IsGlass())&&profileStatus.starts_with(loadedPrefix)){
   ImGui::TextColored(muted,"已载入装备预设");ImGui::SameLine();InlineChip(profileStatus.c_str()+loadedPrefix.size());
  }else ImGui::TextColored(muted,"%s",profileStatus.c_str());
  if(std::any_of(s.data.inactiveMeshes.begin(),s.data.inactiveMeshes.end(),[&](auto& b){return Equip(b.model)==outfit;})&&ImGui::TreeNode("待匹配网格")){
   ImGui::TextWrapped("这些网格在当前版本中缺失或无法唯一对应。已保留相关规则，换回匹配模型后可恢复。");
   ImGui::BeginChild("待匹配列表",{0,S(85)});
   for(auto& b:s.data.inactiveMeshes)if(Equip(b.model)==outfit)ImGui::Text("%s  ·  #%u  ·  %u 顶点 / %u 索引",PartName(PartIndex(ProfilePart(b.model))),b.index,b.vertices,b.indices);
   ImGui::EndChild();ImGui::TreePop();
  }
  if(PresetFileModal("手动查找预设文件","//  FIND PRESET",716)){
   auto equipmentName=EquipmentLabel(outfit);
   if(paper::active){
    ImGui::TextColored(muted,"当前装备：");ImGui::SameLine();
    ImGui::PushFont(PaperBold(),ImGui::GetFontSize());ImGui::TextWrapped("%s",equipmentName.c_str());ImGui::PopFont();
    std::string count="找到 "+std::to_string(profileFiles.size())+" 份预设文件";
    if(ImGui::GetItemRectMax().x+S(32)+ImGui::CalcTextSize(count.c_str()).x<ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x){
     ImGui::SameLine();ImGui::TextDisabled("|");ImGui::SameLine();
    }
    ImGui::TextUnformatted(count.c_str());
   }else ImGui::Text("当前装备：%s · 找到 %zu 份预设文件",equipmentName.c_str(),profileFiles.size());
   auto childFlags=ImGuiChildFlags_Borders|(paper::active?(ImGuiChildFlags_AlwaysUseWindowPadding|ImGuiChildFlags_NavFlattened):0);
   float listHeight=paper::active?std::min(S(340),std::max(S(50),ImGui::GetIO().DisplaySize.y-S(170))):S(340);
   if(paper::active){ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize,S(1));ImGui::PushStyleColor(ImGuiCol_Border,paper::colors->rule);}
   if(paper::active)ImGui::PushStyleColor(ImGuiCol_ChildBg,paper::colors->list);
   ImGui::BeginChild("预设文件列表",{paper::active?0.f:S(680),listHeight},childFlags);
   if(paper::active){ImGui::PopStyleColor(2);ImGui::PopStyleVar();}
   PaperRule(PaneSurface::Inset);
   int step=0;
   if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&!profileFiles.empty()){
    step=int(ImGui::IsKeyPressed(ImGuiKey_DownArrow))-int(ImGui::IsKeyPressed(ImGuiKey_UpArrow));
    if(step){
     // The list owns vertical movement. Tab still navigates the footer buttons.
     ImGui::NavMoveRequestCancel();
     auto current=std::find_if(profileFiles.begin(),profileFiles.end(),[](const auto& f){return f.file==selectedProfileFile;});
     int last=static_cast<int>(profileFiles.size())-1;
     int next=current==profileFiles.end()?(step>0?0:last):std::clamp(static_cast<int>(current-profileFiles.begin())+step,0,last);
     selectedProfileFile=profileFiles[next].file;
    }
   }
   for(const auto& f:profileFiles){
    ImGui::PushID(f.file.c_str());
    bool selected=selectedProfileFile==f.file;
    std::string rate="匹配率 "+std::to_string((int)(f.materialCoverage*100))+"%";
    bool chosen=paper::active?PaperTemplateRow(f,rate,selected):ListRow((f.name+"  · "+rate).c_str(),selected,false,nullptr,true);
    if(chosen)selectedProfileFile=f.file;
    if(step&&selected){
     ImGui::SetFocusID(ImGui::GetItemID(),ImGui::GetCurrentWindow());
     ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleEdgeY);
    }
    ImGui::PopID();
   }
   if(profileFiles.empty())ImGui::TextWrapped("尚无模板。可将分享的 .wdp 放入 WorldDiff/Templates，再刷新列表。");
   ImGui::EndChild();
   float right=ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x;
   if(Button("刷新列表",0,ButtonTone::Primary)){
    profileFiles=FindProfiles(outfit);
    if(std::none_of(profileFiles.begin(),profileFiles.end(),[](const auto& f){return f.file==selectedProfileFile;}))selectedProfileFile.clear();
   }
   ImGui::SameLine();if(Button("取消##查找"))ImGui::CloseCurrentPopup();
   float applyWidth=ImGui::CalcTextSize("应用预设").x+2*ImGui::GetStyle().FramePadding.x+S(2);
   if(ImGui::GetItemRectMax().x+ImGui::GetStyle().ItemSpacing.x+applyWidth<=ImGui::GetWindowPos().x+right)ImGui::SameLine();
   ImGui::SetCursorPosX(right-applyWidth);
   ImGui::BeginDisabled(selectedProfileFile.empty());
   if(PanelButton("应用预设",{applyWidth,S(30)},ButtonTone::Primary)&&ApplyProfile(outfit,selectedProfileFile)){
    groupId=Main(outfit).id;creating=false;staged.clear();ImGui::CloseCurrentPopup();
   }
   ImGui::EndDisabled();ImGui::EndPopup();
  }
  if(PresetFileModal("导出预设文件","//  EXPORT PRESET",540)){
   bool save=false;
   if(exportConflict.empty()){
    const char* hint="输入新文件名，或从右侧列表选择要覆盖的预设文件。";
    if(paper::active)ImGui::TextWrapped("%s",hint);else ImGui::TextUnformatted(hint);
    float rowWidth=paper::active?ImGui::GetContentRegionAvail().x:S(470);
    ImGui::SetNextItemWidth(rowWidth-ImGui::GetFrameHeight());
    if(paper::active)ImGui::PushStyleColor(ImGuiCol_FrameBg,paper::colors->input);
    if(TextField("##预设文件名","如：黎明巫女 或 pl105_0000",profileName,sizeof(profileName))){
     exportSelection.clear();exportError.clear();
    }
    if(paper::active)ImGui::PopStyleColor();
    ImGui::SameLine(0,0);
    ImGui::SetNextWindowSizeConstraints({rowWidth,0},{FLT_MAX,ImGui::GetTextLineHeightWithSpacing()*10});
    if(SidebarCombo("##已有预设文件",nullptr,ImGuiComboFlags_NoPreview|ImGuiComboFlags_PopupAlignLeft)){
     if(ImGui::IsWindowAppearing()){NativeInputClose();exportFiles=ListExportTemplates();}
     if(exportFiles.empty())ImGui::TextDisabled("暂无预设文件");
     for(auto& file:exportFiles){
      if(ImGui::Selectable(file.c_str(),exportSelection==file)){
       strcpy_s(profileName,Utf8(PathUtf8(file).stem().wstring()).c_str());exportSelection=file;exportError.clear();
      }
     }
     ImGui::EndCombo();
    }
    ImGui::BeginDisabled(!profileName[0]);
    save=Button(exportSelection.empty()?"另存为":"覆盖保存",0,ButtonTone::Primary);
    ImGui::EndDisabled();ImGui::SameLine();
    if(Button("取消##导出"))ImGui::CloseCurrentPopup();
   }else{
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+S(460));
    ImGui::TextWrapped("文件「%s」已存在，是否覆盖？",exportConflict.c_str());
    ImGui::PopTextWrapPos();
    save=Button("确认覆盖",0,ButtonTone::Primary);ImGui::SameLine();
    if(Button("返回修改")){exportConflict.clear();exportError.clear();}
   }
   if(save){
    NativeInputClose();
    bool createOnly=exportSelection.empty()&&exportConflict.empty();
    std::string collision;
    std::string filename=profileName;
    if(!exportSelection.empty())filename=exportSelection;
    if(!exportConflict.empty())filename=exportConflict;
    if(ExportProfile(outfit,filename,createOnly?&collision:nullptr)){
     Changed(createOnly?"预设文件已另存":"预设文件已覆盖保存");ImGui::CloseCurrentPopup();
    }else if(!collision.empty()){exportConflict=collision;exportError.clear();}
    else exportError=s.notice.empty()?"导出失败，请检查文件名和目录。":s.notice;
   }
   if(!exportError.empty())ImGui::TextWrapped("%s",exportError.c_str());
   ImGui::EndPopup();
  }
  ImGui::Separator();
  float height=ImGui::GetContentRegionAvail().y-S(43);float available=ImGui::GetContentRegionAvail().x;float left=std::max(S(240),available*.22f),middle=std::max(S(310),available*.25f);
  ImGui::BeginDisabled(!CanEditScope(outfit));GroupsPane(left,height);ImGui::SameLine();PresetsPane(middle,height);ImGui::EndDisabled();ImGui::SameLine();
  if(paper::IsGlass())ImGui::PushStyleColor(ImGuiCol_ChildBg,paper::glass.panel);
  const bool themed=paper::active;
  const float contentInset=ImGui::GetStyle().WindowPadding.x;
  if(themed)ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{S(21),S(14)});
  ImGui::BeginChild("右侧面板",ImVec2(0,height),ImGuiChildFlags_Borders|(themed?ImGuiChildFlags_AlwaysUseWindowPadding:0),
                    themed?FixedPane:ImGuiWindowFlags_None);
  if(paper::IsGlass())ImGui::PopStyleColor();
  if(themed)ImGui::PopStyleVar();
  PaperRule(PaneSurface::Detail);
  {
   SidebarSpacing header(themed);
   PageTabs();
  }
  // Only the page body scrolls. Its scrollbar and padding cannot reflow the tab strip.
  if(themed){
   const float extra=tab==0?ImGui::GetCurrentWindow()->WindowPadding.x-contentInset:0.f;
   const float bodyWidth=ImGui::GetContentRegionAvail().x+extra*2;
   ImGui::SetCursorPosX(ImGui::GetCursorPosX()-extra);
   ImGui::PushID(tab);
   ImGui::BeginChild("页面内容",{bodyWidth,0},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
  }
  {
   SidebarSpacing body(themed&&tab!=0);
   if(tab==4)AboutPanel();else if(tab==3)SettingsPanel();else if(!CanEditScope(outfit)){ImGui::TextWrapped("当前预设与模型不匹配。可重新匹配当前文件，或使用上方查找按钮导入模板。原文件会先备份。");if(Button("重新匹配当前文件"))if(RebindEquipmentProfile(outfit))groupId=Main(outfit).id;}else if(tab==0)TargetsPanel();else if(tab==2)HotkeyPanel();else TransformPanel();
  }
  if(themed){ImGui::EndChild();ImGui::PopID();}
  ImGui::EndChild();AutomationConflictPopup();
  ImGui::Separator();
  if((paper::IsNight()||paper::IsGlass())&&s.notice.empty()){
   ImGui::TextColored(muted,"配置自动保存   ·");ImGui::SameLine();
   auto menuKey=MenuKeyName(s.data);InlineChip(menuKey.c_str());ImGui::SameLine();
   ImGui::TextColored(muted,"开关面板   ·");ImGui::SameLine();InlineChip("Ctrl+F9");ImGui::SameLine();ImGui::TextColored(muted,"紧急恢复");
  }else{auto footer="配置自动保存 · "+MenuKeyName(s.data)+" 开关面板 · Ctrl+F9 紧急恢复";ImGui::TextColored(muted,"%s",s.notice.empty()?footer.c_str():s.notice.c_str());}
 }
 ImGui::End();ImGui::PopStyleVar();ImGui::PopFont();if(!open)SetVisible(false);
}
}
