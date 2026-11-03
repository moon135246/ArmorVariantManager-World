#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <cstring>
#include <map>
#include <string>
#include <vector>
namespace layoutqa {
struct ButtonRecord {ImRect bounds,clip;bool visible=false;ImGuiWindow* window=nullptr;};
std::map<std::string,ButtonRecord> buttons;
std::vector<ButtonRecord> healthDeletes;
bool recordMeshes=false;
std::vector<std::string> meshLabels;
std::vector<ImRect> meshCheckboxes;
std::vector<ImRect> meshRows;ImRect noteField;
}
namespace ImGui {
bool LayoutTrackedSelectable(const char* label,bool selected=false,ImGuiSelectableFlags flags=0,const ImVec2& size=ImVec2(0,0)){
 bool pressed=Selectable(label,selected,flags,size);
 if(layoutqa::recordMeshes&&std::strstr(label,"Meira_")){layoutqa::meshLabels.emplace_back(label);layoutqa::meshRows.emplace_back(GetItemRectMin(),GetItemRectMax());}
 return pressed;
}
bool LayoutTrackedMenuItem(const char* label,const char* shortcut=nullptr,bool selected=false,bool enabled=true){
 bool pressed=MenuItem(label,shortcut,selected,enabled);if(std::strcmp(label,"编辑备注")==0){auto* window=GetCurrentWindow();layoutqa::buttons[label]={ImRect(GetItemRectMin(),GetItemRectMax()),window->ClipRect,IsItemVisible(),window};}return pressed;
}
void LayoutTrackedTextUnformatted(const char* text,const char* end=nullptr){
 if(layoutqa::recordMeshes&&std::strncmp(text,"Meira_",6)==0)layoutqa::meshLabels.emplace_back(text,end?end:text+std::strlen(text));
 TextUnformatted(text,end);
}
bool LayoutTrackedCheckbox(const char* label,bool* value){
 bool pressed=Checkbox(label,value);
 if(layoutqa::recordMeshes&&std::strcmp(label,"##可见")==0)layoutqa::meshCheckboxes.emplace_back(GetItemRectMin(),GetItemRectMax());
 return pressed;
}
// Instrument the actual widgets, without replacing their layout or behavior.
// This records footer bounds and lets the test click the real rule Edit button.
bool LayoutTrackedButton(const char* label,const ImVec2& size=ImVec2(0,0)){
 bool pressed=Button(label,size);
 if(std::strcmp(label,"删除##血量节点")==0){auto* window=GetCurrentWindow();layoutqa::healthDeletes.push_back({ImRect(GetItemRectMin(),GetItemRectMax()),window->ClipRect,IsItemVisible(),window});}
 if(std::strcmp(label,"编辑")==0||std::strcmp(label,"保存规则")==0||std::strcmp(label,"取消##变身编辑")==0||std::strcmp(label,"保存备注")==0||std::strcmp(label,"恢复材质顺序")==0){
  auto* window=GetCurrentWindow();layoutqa::buttons[label]={ImRect(GetItemRectMin(),GetItemRectMax()),window->ClipRect,IsItemVisible(),window};
 }
 return pressed;
}
}
#define Button LayoutTrackedButton
#define TextUnformatted LayoutTrackedTextUnformatted
#define Checkbox LayoutTrackedCheckbox
#define Selectable LayoutTrackedSelectable
#define MenuItem LayoutTrackedMenuItem
#include "../src/Panel.cpp"
#undef Button
#undef TextUnformatted
#undef Checkbox
#undef Selectable
#undef MenuItem
#include "save_imgui_frame.inl"
namespace worlddiff::overlay {
void NativeInputClose(){}
void SetVisible(bool){}
const char* BackendStatus(){return "layout verifier";}
bool TextField(const char* label,const char* hint,char* value,size_t capacity){bool changed=ImGui::InputTextWithHint(label,hint,value,capacity);if(std::strcmp(label,"##网格备注")==0)layoutqa::noteField={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};return changed;}
}
int main(int argc,char** argv){
 using namespace worlddiff::app;
 auto& state=Get();state.loaded=true;worlddiff::Model model;model.equip="pl105_0000";model.part="body";model.display="布局验证模型";model.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";state.models.push_back(model);
 auto main=Main(model.equip).id;for(int i=0;i<24;i++){Group group;group.id=state.data.nextId++;group.name="测试分组 "+std::to_string(i);group.scope=model.equip;state.data.groups.push_back(group);}
 auto* group=Find(main);for(int i=0;i<30;i++){Preset preset;preset.id=state.data.nextId++;preset.name="包含中文与较长名称的预设 "+std::to_string(i);group->presets.push_back(preset);}group->selected=group->defaultPreset=group->presets[0].id;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",22,nullptr,io.Fonts->GetGlyphRangesChineseSimplifiedCommon());unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 auto& style=ImGui::GetStyle();style.FramePadding={9,6};style.ItemSpacing={10,8};style.ChildRounding=8;style.ScrollbarSize=14;
 struct Case {float screenX,screenY,scale,windowX,windowY;};
 Case cases[]={{1920,1080,.7f,1022,595},{1920,1080,1,1460,850},{1920,1080,1,1050,620},{1280,720,1,1050,620},{3840,2160,1.5f,2190,1275}};
 unsigned checks=0;
 for(auto theme:{UiTheme::Basic,UiTheme::ClassicWhite})for(auto c:cases)for(bool hidden:{false,true})for(bool editing:{false,true})for(int page=0;page<5;page++){
  state.data.theme=theme;io.DisplaySize={c.screenX,c.screenY};state.data.scale=c.scale;state.data.hideDragHint=hidden;worlddiff::overlay::creating=editing;worlddiff::overlay::tab=page;
  for(int frame=0;frame<3;frame++){ImGui::NewFrame();ImGui::SetWindowSize("差分管理器 · 世界版 1.1.8###WorldDiffInternal",{c.windowX,c.windowY});ImGui::SetWindowPos("差分管理器 · 世界版 1.1.8###WorldDiffInternal",{(c.screenX-c.windowX)*.5f,(c.screenY-c.windowY)*.5f});worlddiff::overlay::DrawPanel();ImGui::Render();}
  unsigned panes=0;for(auto* window:ImGui::GetCurrentContext()->Windows){if(!window->Active)continue;const char* leaf=std::strrchr(window->Name,'/');if(!leaf)continue;
   if(std::strstr(leaf,"/分组面板_")||std::strstr(leaf,"/预设面板_")||std::strstr(leaf,"/分组操作_")||std::strstr(leaf,"/预设操作_")||std::strstr(leaf,"/拖动提示_")){
    panes++;if(window->ScrollMax.y>.5f){std::cerr<<"FAIL clipped fixed pane: "<<window->Name<<" overflow="<<window->ScrollMax.y<<" scale="<<c.scale<<" size="<<c.windowX<<'x'<<c.windowY<<" editing="<<editing<<" hidden="<<hidden<<'\n';return 1;}checks++;
   }
  }if(panes!=(hidden?4u:5u)){std::cerr<<"FAIL missing pane metrics: "<<panes<<'\n';return 2;}
 }

 state.data.theme=UiTheme::Basic;
 unsigned modeChecks=0;auto& mode=EnsureTransformMode(state.data,model.equip);mode.enabled=true;mode.categories.fill(true);
 for(auto c:cases)for(int selection=0;selection<=kTransformCategories;selection++){
  io.DisplaySize={c.screenX,c.screenY};state.data.scale=c.scale;state.data.hideDragHint=true;worlddiff::overlay::creating=false;worlddiff::overlay::tab=1;mode.parallel=selection==kTransformCategories;mode.selected=selection%kTransformCategories;
  auto before=state.data.transforms.size();
  for(int frame=0;frame<3;frame++){ImGui::NewFrame();ImGui::SetWindowSize("差分管理器 · 世界版 1.1.8###WorldDiffInternal",{c.windowX,c.windowY});ImGui::SetWindowPos("差分管理器 · 世界版 1.1.8###WorldDiffInternal",{(c.screenX-c.windowX)*.5f,(c.screenY-c.windowY)*.5f});worlddiff::overlay::DrawPanel();ImGui::Render();}
  if(state.data.transforms.size()!=before){std::cerr<<"FAIL viewing predefined states created rules without user selection\n";return 3;}modeChecks++;
  bool found=false;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"/变身状态列表_")){found=true;if(window->Size.y<15||window->Size.x<150){std::cerr<<"FAIL collapsed predefined-state viewport "<<window->Size.x<<'x'<<window->Size.y<<" mode="<<selection<<" screen="<<c.windowX<<'x'<<c.windowY<<'\n';return 4;}modeChecks++;}
  if(!found){std::cerr<<"FAIL missing predefined-state viewport\n";return 5;}
 }
 worlddiff::overlay::outfit=model.equip;worlddiff::overlay::groupId=main;auto slot=TransformSlots(0).front();auto id=worlddiff::overlay::StoreTransformTarget(0,slot,0,{main,Find(main)->presets[1].id});auto* assigned=worlddiff::overlay::TransformById(id);
 if(!assigned||assigned->condition.kind!=ConditionKind::WeaponState||assigned->condition.state!=0||assigned->targets[0].preset!=Find(main)->presets[1].id){std::cerr<<"FAIL predefined target assignment\n";return 6;}modeChecks++;
 // The actual per-node delete widget must remain visible at the top of the
 // state list at every supported size, including after adding duplicate nodes.
 auto firstHealth=AddHealthNode(model.equip),secondHealth=AddHealthNode(model.equip);if(!firstHealth||!secondHealth)return 7;unsigned healthChecks=0;
 auto drawHealth=[&](Case c){
  io.DisplaySize={c.screenX,c.screenY};state.data.scale=c.scale;state.data.hideDragHint=true;worlddiff::overlay::creating=false;worlddiff::overlay::tab=1;mode.enabled=true;mode.parallel=false;mode.selected=4;layoutqa::healthDeletes.clear();
  ImGui::NewFrame();ImGui::SetWindowSize("差分管理器 · 世界版 1.1.8###WorldDiffInternal",{c.windowX,c.windowY});ImGui::SetWindowPos("差分管理器 · 世界版 1.1.8###WorldDiffInternal",{(c.screenX-c.windowX)*.5f,(c.screenY-c.windowY)*.5f});worlddiff::overlay::DrawPanel();ImGui::Render();
 };
 for(auto c:cases){for(int frame=0;frame<3;frame++)drawHealth(c);if(layoutqa::healthDeletes.size()!=2){std::cerr<<"FAIL missing per-node delete widgets\n";return 8;}healthChecks++;
  auto first=layoutqa::healthDeletes.front();if(!first.visible||first.bounds.Min.x<first.clip.Min.x||first.bounds.Max.x>first.clip.Max.x||first.bounds.Min.y<first.clip.Min.y||first.bounds.Max.y>first.clip.Max.y){std::cerr<<"FAIL clipped health delete widget at "<<c.windowX<<'x'<<c.windowY<<" visible="<<first.visible<<" bounds="<<first.bounds.Min.x<<','<<first.bounds.Min.y<<'-'<<first.bounds.Max.x<<','<<first.bounds.Max.y<<" clip="<<first.clip.Min.x<<','<<first.clip.Min.y<<'-'<<first.clip.Max.x<<','<<first.clip.Max.y<<'\n';return 9;}healthChecks++;
 }
 auto desktop=cases[1];for(int frame=0;frame<3;frame++)drawHealth(desktop);auto firstDelete=layoutqa::healthDeletes.front();auto center=firstDelete.bounds.GetCenter();auto beforeGroups=state.data.groups.size(),beforePresets=Find(main)->presets.size();io.AddMousePosEvent(center.x,center.y);io.AddMouseButtonEvent(0,true);drawHealth(desktop);io.AddMouseButtonEvent(0,false);drawHealth(desktop);
 if(std::any_of(state.data.transforms.begin(),state.data.transforms.end(),[&](auto& r){return r.id==firstHealth;})||std::none_of(state.data.transforms.begin(),state.data.transforms.end(),[&](auto& r){return r.id==secondHealth;})||state.data.groups.size()!=beforeGroups||Find(main)->presets.size()!=beforePresets){std::cerr<<"FAIL actual health delete button did not remove exactly its own node\n";return 10;}healthChecks++;
 unsigned meshChecks=0;
 if(argc>1){
  // Use actual Panel.cpp widgets and real MOD3/MRL3 files. The first
  // displayed row must toggle its original runtime index, not its list position.
  state.models=worlddiff::LoadCatalog(PathUtf8(argv[1]));
  for(auto& m:state.models)m.relativePath=Normalize(m.relativePath);
  auto arm=std::find_if(state.models.begin(),state.models.end(),[](auto& m){return m.equip=="pl105_0000"&&m.part=="arm";});
  if(arm==state.models.end()||arm->materialOrder.empty()){std::cerr<<"FAIL missing real MRL material fixture\n";return 11;}
  const unsigned expected[]={1,3,4,5,9,10,6,11,12,2,7,8,0,13,14,15};
  state.data.transformsEnabled=false;Find(main)->working={};state.data.scale=1;
  worlddiff::overlay::outfit=model.equip;worlddiff::overlay::groupId=main;worlddiff::overlay::part=2;
  worlddiff::overlay::creating=false;worlddiff::overlay::meshMode=true;worlddiff::overlay::search[0]=0;
  auto drawMeshes=[&]{
   layoutqa::meshLabels.clear();layoutqa::meshCheckboxes.clear();layoutqa::meshRows.clear();layoutqa::recordMeshes=true;
   io.DisplaySize={1920,1400};ImGui::NewFrame();ImGui::SetNextWindowPos({50,50});ImGui::SetNextWindowSize({1000,1300});
   ImGui::Begin("Mesh order QA");worlddiff::overlay::TargetsPanel();ImGui::End();ImGui::Render();layoutqa::recordMeshes=false;
  };
  for(int frame=0;frame<3;frame++)drawMeshes();
  if(layoutqa::meshLabels.size()!=16||layoutqa::meshCheckboxes.size()!=16){std::cerr<<"FAIL missing actual mesh rows\n";return 12;}
  for(unsigned i=0;i<16;i++){
   auto& mesh=arm->meshes[expected[i]];
   if(layoutqa::meshLabels[i]!=arm->materials[mesh.materialId].name){std::cerr<<"FAIL actual panel row order at "<<i<<'\n';return 13;}meshChecks++;
  }
  auto firstBox=layoutqa::meshCheckboxes.front().GetCenter();io.AddMousePosEvent(firstBox.x,firstBox.y);
  io.AddMouseButtonEvent(0,true);drawMeshes();io.AddMouseButtonEvent(0,false);drawMeshes();
  auto faceHash=arm->materials[arm->meshes[1].materialId].hash;
  auto bodyHash=arm->materials[arm->meshes[0].materialId].hash;
  if(!RuleHidden(Find(main)->working,arm->relativePath,1,faceHash)||RuleHidden(Find(main)->working,arm->relativePath,0,bodyHash)){
   std::cerr<<"FAIL reordered checkbox changed wrong mesh ID\n";return 14;
  }meshChecks++;
  // Check the material view too: it must not sort by material CRC/hash.
  worlddiff::overlay::meshMode=false;drawMeshes();
  for(unsigned i=0;i<16;i++){if(layoutqa::meshLabels[i]!=arm->materials[arm->meshes[expected[i]].materialId].name){std::cerr<<"FAIL material view order\n";return 15;}meshChecks++;}
  worlddiff::overlay::meshMode=true;drawMeshes();
  auto click=[&](ImVec2 point,int button=0){io.AddMousePosEvent(point.x,point.y);io.AddMouseButtonEvent(button,true);drawMeshes();io.AddMouseButtonEvent(button,false);drawMeshes();};
  click(layoutqa::meshRows.front().GetCenter(),1);drawMeshes();
  if(!layoutqa::buttons.count("编辑备注")){std::cerr<<"FAIL missing mesh context menu\n";return 16;}meshChecks++;
  click(layoutqa::buttons.at("编辑备注").bounds.GetCenter());drawMeshes();drawMeshes();
  if(!layoutqa::buttons.count("保存备注")){std::cerr<<"FAIL missing note modal\n";return 17;}meshChecks++;
  click(layoutqa::noteField.GetCenter());io.AddInputCharactersUTF8("面部备注");drawMeshes();
  click(layoutqa::buttons.at("保存备注").bounds.GetCenter());drawMeshes();
  if(!MeshView(*arm,1)||MeshView(*arm,1)->note!="面部备注"||layoutqa::meshLabels.front().find("面部备注")==std::string::npos){std::cerr<<"FAIL actual Unicode note editor\n";return 18;}meshChecks++;
  auto source=layoutqa::meshRows[12].GetCenter();auto destination=layoutqa::meshRows.front().GetCenter();destination.y=layoutqa::meshRows.front().Min.y+2;
  auto traceDrag=[&](const char* stage){if(argc>2){auto& ctx=*ImGui::GetCurrentContext();std::cerr<<stage<<" active="<<ctx.ActiveId<<" hovered="<<ctx.HoveredId<<" dnd="<<ctx.DragDropActive<<" accept="<<ctx.DragDropAcceptIdCurr<<" prev="<<ctx.DragDropAcceptIdPrev<<" mouse="<<io.MousePos.x<<','<<io.MousePos.y<<" down="<<io.MouseDown[0]<<" notice="<<state.notice<<'\n';}};
  io.AddMousePosEvent(source.x,source.y);io.AddMouseButtonEvent(0,true);drawMeshes();traceDrag("press");io.AddMousePosEvent(source.x+10,source.y-12);drawMeshes();traceDrag("start");
  io.AddMousePosEvent(destination.x,destination.y);drawMeshes();traceDrag("target1");drawMeshes();traceDrag("target2");io.AddMouseButtonEvent(0,false);drawMeshes();traceDrag("release");drawMeshes();traceDrag("after");
  if(OrderedMeshes(*arm).front()->index!=0||layoutqa::meshLabels.front()!=arm->materials[arm->meshes[0].materialId].name){std::cerr<<"FAIL actual drag/drop ordering\n";return 19;}meshChecks++;
  if(!RuleHidden(Find(main)->working,arm->relativePath,1,faceHash)||RuleHidden(Find(main)->working,arm->relativePath,0,bodyHash)){std::cerr<<"FAIL drag/note changed visibility IDs\n";return 20;}meshChecks++;
  click(layoutqa::buttons.at("恢复材质顺序").bounds.GetCenter());drawMeshes();
  if(OrderedMeshes(*arm).front()->index!=1||!MeshView(*arm,1)||MeshView(*arm,1)->note!="面部备注"){std::cerr<<"FAIL reset material order preserving note\n";return 21;}meshChecks++;
  auto currentPath=arm->relativePath;auto foreign=*arm;foreign.equip="pl106_0000";foreign.relativePath="pl\\f_equip\\pl106_0000\\leg\\mod\\f_leg106_0000";foreign.part="leg";auto existingForeign=std::find_if(state.models.begin(),state.models.end(),[&](auto& m){return m.relativePath==foreign.relativePath;});if(existingForeign==state.models.end())state.models.push_back(foreign);else *existingForeign=foreign;
  auto drawCurrent=[&]{layoutqa::meshLabels.clear();layoutqa::recordMeshes=true;io.DisplaySize={1920,1400};ImGui::NewFrame();ImGui::SetNextWindowSize({1460,1200});worlddiff::overlay::DrawPanel();ImGui::Render();layoutqa::recordMeshes=false;};
  state.trackingPlayer=true;state.outfit.ready=true;state.outfit.entity=123;state.outfit.paths[2]=currentPath;worlddiff::overlay::outfit="pl106_0000";worlddiff::overlay::part=2;worlddiff::overlay::tab=0;drawCurrent();drawCurrent();
  if(worlddiff::overlay::outfit!="pl105_0000"||layoutqa::meshLabels.size()!=16){std::cerr<<"FAIL current appearance UI includes historical equipment or unworn parts\n";return 22;}meshChecks++;
  SaveCurrentFrame();
  std::ostringstream beforeSavePreview;WriteDocument(beforeSavePreview,state.data);state.outfit.savePreview=true;drawCurrent();drawCurrent();
  if(worlddiff::overlay::displayedOutfit.find("/save")==std::string::npos){std::cerr<<"FAIL save preview UI identity not updated\n";return 24;}meshChecks++;
  std::ostringstream afterSavePreview;WriteDocument(afterSavePreview,state.data);if(afterSavePreview.str()!=beforeSavePreview.str()){std::cerr<<"FAIL save preview UI rewrote equipment definitions\n";return 25;}meshChecks++;
  state.outfit.paths[4]=foreign.relativePath;worlddiff::overlay::outfit="pl106_0000";worlddiff::overlay::part=4;drawCurrent();drawCurrent();
  if(worlddiff::overlay::outfit!="pl106_0000"||layoutqa::meshLabels.size()!=16){std::cerr<<"FAIL mixed appearance UI cannot select an actually worn part: outfit="<<worlddiff::overlay::outfit<<" rows="<<layoutqa::meshLabels.size()<<" group="<<worlddiff::overlay::groupId<<" part="<<worlddiff::overlay::part<<" path="<<state.outfit.paths[4]<<'\n';return 23;}meshChecks++;
  state.outfit.savePreview=false;state.trackingPlayer=false;
 }
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" fixed-pane layout checks, "<<modeChecks<<" state/mode checks, "<<healthChecks<<" health-node UI checks and "<<meshChecks<<" mesh-order UI checks; ImGui assertions enabled.\n";return 0;
}
