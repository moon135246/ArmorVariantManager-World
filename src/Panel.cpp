#include "Overlay.h"
#include "State.h"
#include "NativeInput.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace worlddiff::overlay {
namespace {
using namespace app;
std::string outfit;uint64_t groupId=0;int tab=0,part=-1;bool meshMode=false,creating=false,globalGroup=false;
char groupName[192]{},presetName[192]{},search[192]{},rename[192]{};std::set<Target> staged;
char profileName[192]{};std::vector<ProfileCandidate> profileFiles;
uint64_t editingMembers=0,confirmGroup=0,confirmPreset=0,renameGroup=0;
ImVec4 accent{0.33f,0.72f,0.78f,1},muted{0.6f,0.66f,0.72f,1},danger{0.91f,0.51f,0.51f,1};
float S(float x){return x*Get().data.scale;}
std::string Lower(std::string x){for(auto& c:x)if(c>='A'&&c<='Z')c+=32;return x;}
int PartIndex(const std::string& p){if(p=="helm")return 0;if(p=="body")return 1;if(p=="arm")return 2;if(p=="wst")return 3;if(p=="leg")return 4;return 5;}
const char* PartName(int p){const char* names[]={"头部","身体","手臂","腰部","腿部","其他"};return names[std::clamp(p,0,5)];}
void Tip(const char* text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",text);}
bool Button(const char* text,float width=0){return ImGui::Button(text,ImVec2(width>0?S(width):0,S(30)));}
bool ListRow(const char* label,bool selected){
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
 if(ImGui::IsItemVisible()){
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
void DragGroup(Group& g){
 if(g.main||g.global)return;
 if(ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("WD_GROUP",&g.id,sizeof(g.id));ImGui::TextUnformatted(g.name.c_str());ImGui::EndDragDropSource();}
 if(ImGui::BeginDragDropTarget()){if(auto* payload=ImGui::AcceptDragDropPayload("WD_GROUP")){auto id=*(const uint64_t*)payload->Data;auto& all=Get().data.groups;auto from=std::find_if(all.begin(),all.end(),[&](auto& a){return a.id==id;});auto to=std::find_if(all.begin(),all.end(),[&](auto& a){return a.id==g.id;});if(from!=all.end()&&to!=all.end()&&from->scope==to->scope){std::iter_swap(from,to);Changed("分组顺序已保存");}}ImGui::EndDragDropTarget();}
}
void GroupsPane(float width,float height){
 auto& s=Get();ImGui::BeginChild("分组面板",ImVec2(width,height),ImGuiChildFlags_Borders);
 ImGui::TextColored(muted,"分组");ImGui::Separator();
 ImGui::BeginChild("分组列表",ImVec2(0,-S(206)));
 for(auto& g:s.data.groups){if(g.scope!=outfit)continue;ImGui::PushID((int)g.id);std::string label=(g.main?"主列表":g.global?"[全局] "+g.name:g.name);
  if(ListRow(label.c_str(),groupId==g.id)){groupId=g.id;creating=false;editingMembers=0;staged.clear();}
  DragGroup(g);ImGui::PopID();
 }ImGui::EndChild();
 ImGui::Separator();auto* g=Find(groupId);
 if(!creating){
  if(Button("新建分组",-1)){creating=true;editingMembers=0;globalGroup=false;meshMode=false;staged.clear();groupName[0]=0;}
  ImGui::BeginDisabled(!g||g->main);if(Button("编辑成员")){creating=true;editingMembers=g->id;globalGroup=g->global;meshMode=false;staged=g->members;strcpy_s(groupName,g->name.c_str());}
  ImGui::SameLine();if(Button("重命名")){renameGroup=g->id;strcpy_s(rename,g->name.c_str());ImGui::OpenPopup("重命名分组");}
  if(Button("删除分组")){confirmGroup=g->id;ImGui::OpenPopup("删除分组？");}ImGui::EndDisabled();
  ImGui::TextColored(muted,"普通组独占成员\n全局组可重叠并叠加隐藏\n拖动列表可以调整顺序");
 }else{
  ImGui::TextColored(accent,editingMembers?"编辑分组成员":"选择材质并创建分组");ImGui::SetNextItemWidth(-1);TextField("##组名","分组名称",groupName,sizeof(groupName));
  ImGui::BeginDisabled(editingMembers!=0);ImGui::Checkbox("全局分组（叠加隐藏）",&globalGroup);ImGui::EndDisabled();
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
 ImGui::EndChild();
}
void PresetsPane(float width,float height){
 auto& s=Get();auto* g=Find(groupId);ImGui::BeginChild("预设面板",ImVec2(width,height),ImGuiChildFlags_Borders);ImGui::TextColored(muted,"%s · 预设",g?g->name.c_str():"主列表");ImGui::Separator();
 ImGui::BeginChild("预设列表",ImVec2(0,-S(249)));
 if(g)for(size_t i=0;i<g->presets.size();i++){auto& p=g->presets[i];ImGui::PushID((int)p.id);auto label=p.name+(g->defaultPreset==p.id?"  [默认]":"");
  if(ListRow(label.c_str(),g->selected==p.id))ApplyPreset(*g,p.id);
  if(ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("WD_PRESET",&p.id,sizeof(p.id));ImGui::TextUnformatted(p.name.c_str());ImGui::EndDragDropSource();}
  if(ImGui::BeginDragDropTarget()){if(auto* payload=ImGui::AcceptDragDropPayload("WD_PRESET")){auto id=*(const uint64_t*)payload->Data;auto it=std::find_if(g->presets.begin(),g->presets.end(),[&](auto& a){return a.id==id;});if(it!=g->presets.end()){std::iter_swap(it,g->presets.begin()+i);Changed();}}ImGui::EndDragDropTarget();}ImGui::PopID();
 }
 if(g&&g->presets.empty())ImGui::TextColored(muted,"尚无预设\n先调整右侧显隐，再保存");ImGui::EndChild();ImGui::Separator();
 ImGui::TextColored(muted,"保存当前分组的状态");ImGui::SetNextItemWidth(-1);TextField("##预设名称","新预设名称",presetName,sizeof(presetName));ImGui::BeginDisabled(!g||!presetName[0]);
 if(Button("保存新预设")){if(SavePreset(*g,presetName)){presetName[0]=0;}else s.notice="预设名为空或已存在，请使用覆盖";}ImGui::EndDisabled();
 Preset* selected=nullptr;if(g)for(auto& p:g->presets)if(p.id==g->selected)selected=&p;
 bool modified=selected&&(selected->rules.hidden!=g->working.hidden||selected->rules.shown!=g->working.shown);
 ImGui::TextColored(muted,"当前：%s%s",selected?selected->name.c_str():"未保存的调整",modified?"（已修改）":"");
 ImGui::BeginDisabled(!selected);if(Button("设为默认"))SetDefault(*g,selected->id);ImGui::SameLine();if(Button("覆盖预设"))SavePreset(*g,selected->name,true);
 if(Button("删除预设")){confirmPreset=selected->id;ImGui::OpenPopup("删除预设？");}ImGui::EndDisabled();
 if(ImGui::Checkbox("选择时自动设为默认",&s.data.autoDefault)){if(s.data.autoDefault&&g&&g->selected)SetDefault(*g,g->selected);Changed();}
 Tip("默认预设会随游戏自动加载；普通组分别记忆，全局组叠加生效。");
 if(ImGui::BeginPopupModal("删除预设？",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("删除此预设？当前画面状态保持不变。");if(Button("删除")){if(g)DeletePreset(*g,confirmPreset);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(Button("取消##删预设"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 ImGui::EndChild();
}
void TargetsPanel(){
 auto& s=Get();auto* g=Find(groupId);if(!g){ImGui::TextDisabled("等待装备加载…");return;}
 if(creating)ImGui::TextColored(accent,"分组选择模式：勾选要交给该组管理的材质");
 else{ImGui::BeginDisabled(creating);if(ImGui::RadioButton("按材质",!meshMode))meshMode=false;ImGui::SameLine();if(ImGui::RadioButton("按子网格",meshMode))meshMode=true;ImGui::EndDisabled();}
 ImGui::SetNextItemWidth(-1);TextField("##搜索材质","搜索材质或模型名称",search,sizeof(search));
 const char* parts[]={"全部","头部","身体","手臂","腰部","腿部"};for(int p=-1;p<5;p++){if(p!=-1)ImGui::SameLine();bool on=part==p;if(on)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(.13f,.3f,.35f,1));if(Button(parts[p+1]))part=p;if(on)ImGui::PopStyleColor();}
 ImGui::Separator();
 ImGui::BeginChild("材质滚动区",ImVec2(0,-S(28)));
 std::string filter=Lower(search);DWORD now=GetTickCount();bool any=false;
 for(int slot=0;slot<6;slot++)for(auto& model:s.models){if(model.equip!=outfit||PartIndex(model.part)!=slot||(part!=-1&&part!=slot))continue;auto path=Normalize(model.relativePath);bool active=s.lastSeen.count(path)&&(DWORD)(now-s.lastSeen[path])<1800;std::string head=std::string(PartName(slot))+"  ·  "+model.display+(active?"  [已加载]":"  [待加载]");ImGui::PushID(path.c_str());
  if(ImGui::CollapsingHeader(head.c_str(),ImGuiTreeNodeFlags_DefaultOpen)){
   std::map<uint32_t,std::vector<const Mesh*>> targets;for(auto& m:model.meshes){if(m.materialId>=model.materials.size())continue;targets[meshMode&&!creating?m.index:model.materials[m.materialId].hash].push_back(&m);}
   struct Item {Target target;const Material* mat;std::vector<const Mesh*> meshes;bool allowed,member;Group* owner;};std::vector<Item> items;
   for(auto& [key,meshes]:targets){auto* mat=&model.materials[meshes[0]->materialId];if(!filter.empty()&&Lower(mat->name+model.display).find(filter)==std::string::npos)continue;auto* owner=Owner(path,mat->hash);bool allowed=creating?(globalGroup||!owner||owner->id==editingMembers):InGroup(*g,path,mat->hash);Target t{path,key,meshMode&&!creating};items.push_back({t,mat,meshes,allowed,staged.count(t)!=0,owner});}
   if(Button(creating?"全选":"全显示")){for(auto& item:items)if(item.allowed){if(creating)staged.insert(item.target);else SetVisible(*g,item.target,true);}}
   ImGui::SameLine();if(Button(creating?"清空选择":"全隐藏")){for(auto& item:items)if(item.allowed){if(creating)staged.erase(item.target);else SetVisible(*g,item.target,false);}}
   ImGui::SameLine();ImGui::TextDisabled("%zu 项",items.size());
   for(auto& item:items){ImGui::PushID((int)item.target.value);bool shown=true;size_t hidden=0;bool locked=false;for(auto* mesh:item.meshes){if(RuleHidden(g->working,path,mesh->index,item.mat->hash))hidden++;if(!g->global&&GlobalHidden(path,mesh->index,item.mat->hash))locked=true;}shown=hidden==0;if(creating)shown=staged.count(item.target)!=0;if(locked&&!creating)shown=false;
    ImGui::BeginDisabled(!item.allowed||(!creating&&locked));if(ImGui::Checkbox("##可见",&shown)){if(creating){if(shown)staged.insert(item.target);else staged.erase(item.target);}else SetVisible(*g,item.target,shown);}ImGui::EndDisabled();
    ImGui::SameLine();if(!item.allowed)ImGui::TextDisabled("%s",item.mat->name.c_str());else ImGui::TextUnformatted(item.mat->name.c_str());
    if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::Text("材质：%s",item.mat->name.c_str());ImGui::Text("哈希：%08X · %zu 个子网格",item.mat->hash,item.meshes.size());for(auto* mesh:item.meshes)ImGui::Text("网格 #%u · 原 Group_ID %u · LOD %u",mesh->index,mesh->groupId,mesh->lod);ImGui::TextUnformatted(path.c_str());ImGui::EndTooltip();}
    if(meshMode&&!creating){ImGui::SameLine();ImGui::TextColored(muted,"#%u",item.target.value);}
    if(locked&&!creating){ImGui::SameLine();ImGui::TextColored(danger,"全局隐藏");}
    else if(!item.allowed&&item.owner){ImGui::SameLine();ImGui::TextColored(accent,"[%s]",item.owner->name.c_str());}
    else if(!creating&&hidden&&hidden<item.meshes.size()){ImGui::SameLine();ImGui::TextColored(muted,"部分隐藏");}
    ImGui::PopID();any=true;
   }
  }ImGui::PopID();
 }
 if(!any)ImGui::TextColored(muted,"尚无匹配材质。\n请进入场景、选择装备，或清空搜索。");
 ImGui::EndChild();ImGui::TextColored(muted,creating?"勾选 = 加入分组；普通组不能重复占用成员":"勾选 = 显示；取消 = 隐藏；恢复时保留游戏自身显隐");
}
void SettingsPanel(){
 auto& s=Get();ImGui::TextColored(accent,"启动与输入");if(ImGui::Checkbox("启用差分控制",&s.data.enabled))Changed();if(ImGui::Checkbox("进入游戏后自动打开面板",&s.data.autoOpen))Changed();
 const char* keys[]={"F5","F6","F7","F8","F9","F10","F11","F12"};int key=std::clamp(s.data.menuKey-VK_F5,0,7);ImGui::SetNextItemWidth(S(180));if(ImGui::Combo("开关面板快捷键",&key,keys,8)){s.data.menuKey=VK_F5+key;Changed();}
 ImGui::SetNextItemWidth(S(220));if(ImGui::SliderFloat("界面缩放",&s.data.scale,.7f,1.5f,"%.2f"))Changed();
 ImGui::Separator();ImGui::TextColored(accent,"配置与恢复");if(Button("立即保存配置")){if(Save())s.notice="配置已保存";else s.notice="配置保存失败，请检查目录是否可写";}ImGui::SameLine();if(Button("恢复当前组")){if(auto* g=Find(groupId))Restore(*g);}
 if(Button("暂停全部并恢复游戏显隐"))RestoreAll();Tip("不会删除任何已保存预设；重新启用控制后，可选择预设恢复方案。");
 if(Button("导入 1.0.2 配置"))Import102(s.directory/L"WorldDiff102.ini");
 ImGui::TextWrapped("配置：nativePC/plugins/WorldDiff/WorldDiff104.cfg。可分享的分组与预设文件在 WorldDiff/Presets/*.wdp，可改名或复制后用手动/自动查找载入。");
 if(ImGui::Checkbox("换装后自动识别预设文件",&s.data.autoFind))Changed();
 ImGui::Separator();ImGui::TextColored(accent,"操作说明");ImGui::TextWrapped("普通分组独占成员，主列表不再管理已分组材质。全局分组可以与其他分组重叠，只叠加隐藏。每个分组保存独立预设和默认预设。列表支持拖动排序。");
 ImGui::TextWrapped("关闭面板后规则仍然生效。Ctrl+F9 可暂停全部控制；Esc 关闭面板。插件随 Stracker's Loader 加载，不需要连接或外置程序。");
 ImGui::TextWrapped("查找会比较全部部位的材质，包含分组成员与所有预设，同分时优先装备同名文件。载入时重新绑定材质和子网格，因此同款 MOD 可跨装备编号复用。无法确定的网格会跳过并提示。");
 ImGui::TextWrapped("名称输入使用原生 Unicode/IME 文本框，支持中文、日文和粘贴。回车完成输入，Esc 取消本次编辑。关闭面板后恢复游戏的鼠标与键盘输入。");
 ImGui::TextColored(muted,"渲染：%s",BackendStatus());if(s.live)ImGui::TextColored(muted,"运行时记录 %ld / %u · %s",s.live->rowCount,kMaxRows,s.live->overflow?"已满，请重启游戏":"正常");
}
}
void DrawPanel(){
 auto& s=app::Get();std::lock_guard lock(s.mutex);if(!s.loaded)return;
 std::set<std::string> scopes,active;DWORD now=GetTickCount();for(auto& m:s.models){scopes.insert(m.equip);if(s.lastSeen.count(m.relativePath)&&(DWORD)(now-s.lastSeen[m.relativePath])<1800)active.insert(m.equip);}
 if(outfit.empty()||(!active.empty()&&!active.count(outfit))){if(!active.empty())outfit=*active.begin();else if(!scopes.empty())outfit=*scopes.begin();}
 if(outfit.empty()){if(ImGui::Begin("差分管理器 · 世界版 1.0.5###WorldDiffInternal")){ImGui::TextUnformatted("插件已随游戏加载，正在等待角色装备进入绘制流程。");if(Button("关闭面板"))SetVisible(false);}ImGui::End();return;}
 if(!Find(groupId)||Find(groupId)->scope!=outfit){groupId=Main(outfit).id;creating=false;staged.clear();}
 auto& io=ImGui::GetIO();float scale=s.data.scale;ImGui::SetNextWindowSize(ImVec2(std::min(io.DisplaySize.x-S(60),S(1460)),std::min(io.DisplaySize.y-S(80),S(850))),ImGuiCond_FirstUseEver);ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_FirstUseEver,ImVec2(.5f,.5f));ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(S(1050),io.DisplaySize.x-30),std::min(S(620),io.DisplaySize.y-30)),ImVec2(io.DisplaySize.x,io.DisplaySize.y));
 ImGui::PushFont(nullptr,22*scale);bool open=true;ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(S(18),S(14)));
 if(ImGui::Begin("差分管理器 · 世界版 1.0.5###WorldDiffInternal",&open,ImGuiWindowFlags_NoCollapse)){
  ImGui::TextColored(accent,"差分管理器");ImGui::SameLine();ImGui::TextDisabled("世界版  1.0.5");ImGui::SameLine();ImGui::TextDisabled("| 游戏内置");
  ImGui::SameLine(ImGui::GetWindowWidth()-S(350));if(ImGui::Checkbox("启用",&s.data.enabled))Changed();ImGui::SameLine();if(Button("暂停并恢复"))RestoreAll();
  ImGui::SetNextItemWidth(S(225));if(ImGui::BeginCombo("装备",outfit.empty()?"等待装备加载":outfit.c_str())){for(auto& scope:scopes){auto label=scope+(active.count(scope)?" · 已加载":"");if(ImGui::Selectable(label.c_str(),outfit==scope)){outfit=scope;groupId=Main(outfit).id;creating=false;staged.clear();}}ImGui::EndCombo();}
  ImGui::SameLine();ImGui::TextColored(muted,"%s",s.data.enabled?"显隐控制运行中":"已暂停 · 全部跟随游戏");ImGui::Separator();
  if(Button("手动查找")){profileFiles=FindProfiles(outfit);ImGui::OpenPopup("手动查找预设文件");}ImGui::SameLine();if(Button("自动查找")){if(AutoFindProfile(outfit)){groupId=Main(outfit).id;creating=false;staged.clear();}}
  ImGui::SameLine();if(Button("另存预设文件")){profileName[0]=0;ImGui::OpenPopup("另存预设文件");}ImGui::SameLine();if(ImGui::Checkbox("换装自动识别",&s.data.autoFind))Changed();
  auto bound=s.data.bindings.find(outfit);ImGui::TextColored(muted,"预设文件：%s",bound==s.data.bindings.end()?"尚未关联，编辑后自动创建":bound->second.c_str());
  if(ImGui::BeginPopupModal("手动查找预设文件",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
   ImGui::Text("当前装备：%s · 找到 %zu 份预设文件",outfit.c_str(),profileFiles.size());
   ImGui::TextWrapped("载入整个文件的分组、预设与默认项，按当前材质重新绑定。已有设置会先自动备份。");
   ImGui::BeginChild("预设文件列表",ImVec2(S(680),S(340)),ImGuiChildFlags_Borders);
   for(auto& f:profileFiles){ImGui::PushID(f.file.c_str());std::string label=f.file+"  · 材质 "+std::to_string(f.hits)+"/"+std::to_string(f.total)+(f.filenameMatch?"  [装备同名]":"");if(ImGui::Selectable(label.c_str())){if(ApplyProfile(outfit,f.file)){groupId=Main(outfit).id;creating=false;staged.clear();ImGui::CloseCurrentPopup();}}ImGui::PopID();}
   if(profileFiles.empty())ImGui::TextWrapped("尚无预设文件。保存分组/预设后会自动生成，也可把 .wdp 文件放入 WorldDiff/Presets。");ImGui::EndChild();if(Button("刷新列表"))profileFiles=FindProfiles(outfit);ImGui::SameLine();if(Button("取消##查找"))ImGui::CloseCurrentPopup();ImGui::EndPopup();
  }
  if(ImGui::BeginPopupModal("另存预设文件",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextUnformatted("保存当前装备的全部分组与预设，可供同款 MOD 的其他装备识别。");ImGui::SetNextItemWidth(S(400));TextField("##预设文件名","如：黎明巫女 或 pl105_0000",profileName,sizeof(profileName));if(Button("保存文件")){if(ExportProfile(outfit,profileName)){Changed("预设文件已另存，换装可通过材质识别复用");ImGui::CloseCurrentPopup();}}ImGui::SameLine();if(Button("取消##另存"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
  ImGui::Separator();
  float height=ImGui::GetContentRegionAvail().y-S(43);float available=ImGui::GetContentRegionAvail().x;float left=std::max(S(215),available*.21f),middle=std::max(S(240),available*.23f);
  GroupsPane(left,height);ImGui::SameLine();PresetsPane(middle,height);ImGui::SameLine();
  ImGui::BeginChild("右侧面板",ImVec2(0,height),ImGuiChildFlags_Borders);
  if(ImGui::BeginTabBar("右侧标签")){if(ImGui::BeginTabItem("部位与材质")){tab=0;TargetsPanel();ImGui::EndTabItem();}if(ImGui::BeginTabItem("设置与说明")){tab=1;SettingsPanel();ImGui::EndTabItem();}ImGui::EndTabBar();}ImGui::EndChild();
  ImGui::Separator();ImGui::TextColored(muted,"%s",s.notice.empty()?"配置自动保存 · F9 开关面板 · Ctrl+F9 紧急恢复":s.notice.c_str());
 }
 ImGui::End();ImGui::PopStyleVar();ImGui::PopFont();if(!open)SetVisible(false);
}
}
