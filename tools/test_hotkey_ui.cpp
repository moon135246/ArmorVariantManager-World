#include "imgui.h"
#include "imgui_internal.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace keyui {bool applyDisabled=false;std::map<std::string,ImRect> items;std::vector<std::string> text;}
namespace ImGui {
bool KeyTrackedButton(const char* label,const ImVec2& size=ImVec2(0,0)){auto hit=Button(label,size);if(std::strcmp(label,"应用预设")==0)keyui::applyDisabled=(GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled)!=0;keyui::items[label]={GetItemRectMin(),GetItemRectMax()};return hit;}
bool KeyTrackedCheckbox(const char* label,bool* value){auto hit=Checkbox(label,value);keyui::items[label]={GetItemRectMin(),GetItemRectMax()};return hit;}
bool KeyTrackedCombo(const char* label,const char* preview,ImGuiComboFlags flags=0){auto bounds=ImRect(GetCursorScreenPos(),GetCursorScreenPos());auto hit=BeginCombo(label,preview,flags);if(!hit)bounds={GetItemRectMin(),GetItemRectMax()};else{auto& last=GetCurrentContext()->LastItemData;bounds=last.Rect;}keyui::items[label]=bounds;return hit;}
bool KeyTrackedSelectable(const char* label,bool selected=false,ImGuiSelectableFlags flags=0,const ImVec2& size=ImVec2(0,0)){auto hit=Selectable(label,selected,flags,size);keyui::items[label]={GetItemRectMin(),GetItemRectMax()};return hit;}
void KeyTrackedText(const char* text,const char* end=nullptr){keyui::text.emplace_back(text,end?end:text+std::strlen(text));TextUnformatted(text,end);}
}
#define Button KeyTrackedButton
#define Checkbox KeyTrackedCheckbox
#define BeginCombo KeyTrackedCombo
#define Selectable KeyTrackedSelectable
#define TextUnformatted KeyTrackedText
#include "../src/Panel.cpp"
#undef Button
#undef Checkbox
#undef BeginCombo
#undef Selectable
#undef TextUnformatted
#include "save_imgui_frame.inl"
#include "../src/EmbeddedFonts.h"
namespace worlddiff::overlay {void NativeInputClose(){}void SetVisible(bool){}const char* BackendStatus(){return "shortcut UI verifier";}bool TextField(const char* label,const char* hint,char* value,size_t capacity){auto changed=ImGui::InputTextWithHint(label,hint,value,capacity);keyui::items[label]={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};return changed;}}
using namespace worlddiff;using namespace worlddiff::app;namespace ui=worlddiff::overlay;
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool yes,const char* reason){checks++;if(!yes)throw std::runtime_error(reason);};auto& s=Get();s.loaded=true;s.directory=std::filesystem::temp_directory_path()/(L"WorldDiff124KeyUI_"+std::to_wstring(GetTickCount64()));s.configPath=s.directory/L"Settings.cfg";std::filesystem::create_directories(s.directory);
 Model model;model.equip="pl105_0000";model.part="body";model.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";s.models={model};auto main=Main(model.equip).id;Preset p;p.id=s.data.nextId++;p.name="隐藏";p.rules.hidden.insert({model.relativePath,0,true});Preset shown;shown.id=s.data.nextId++;shown.name="显示";Find(main)->presets={p,shown};Find(main)->selected=shown.id;ui::outfit=model.equip;ui::groupId=main;s.data.hideDragHint=true;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;ImFontGlyphRangesBuilder range;range.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());range.AddText("编辑明月别枝预设快捷键互斥录制黎明巫女");ImVector<ImWchar> glyphs;range.BuildRanges(&glyphs);if(!worlddiff::fonts::LoadPanelFonts(*io.Fonts,glyphs.Data))return 1;unsigned char* atlas;int w,h;io.Fonts->GetTexDataAsRGBA32(&atlas,&w,&h);auto& style=ImGui::GetStyle();style.FramePadding={9,6};style.ItemSpacing={10,8};style.FrameRounding=6;style.ChildRounding=8;style.Colors[ImGuiCol_WindowBg]={.078f,.094f,.125f,.97f};style.Colors[ImGuiCol_ChildBg]={.122f,.149f,.188f,.98f};style.Colors[ImGuiCol_Button]={.13f,.30f,.35f,1};style.Colors[ImGuiCol_Text]={.945f,.957f,.972f,1};style.Colors[ImGuiCol_CheckMark]={.33f,.72f,.78f,1};
 struct Case {float x,y,scale,width,height;};Case cases[]={{1920,1080,.7f,1022,595},{1920,1080,1,1460,850},{1920,1080,1,1050,620},{1280,720,1,1050,620},{3840,2160,1.5f,2190,1275}};auto active=cases[1];int page=2;
 auto draw=[&]{io.DisplaySize={active.x,active.y};s.data.scale=active.scale;ui::tab=page;keyui::items.clear();keyui::text.clear();ImGui::NewFrame();ImGui::SetWindowPos("###WorldDiffInternal",{20,20});ImGui::SetWindowSize("###WorldDiffInternal",{active.width,active.height});ui::DrawPanel();ImGui::Render();};
 auto settle=[&]{for(int i=0;i<3;i++)draw();};auto click=[&](const char* label){auto it=keyui::items.find(label);if(it==keyui::items.end())throw std::runtime_error(std::string("missing widget: ")+label);auto point=it->second.GetCenter();io.AddMousePosEvent(point.x,point.y);io.AddMouseButtonEvent(0,true);draw();io.AddMouseButtonEvent(0,false);draw();draw();};
 auto modalActive=[&]{for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"功能互斥提示"))return true;return false;};
 settle();check(keyui::items.count("预设快捷键")&&keyui::items.count("设置")&&keyui::items.count("关于"),"five tabs use the requested ordering and abbreviated names");check(keyui::items.at("预设快捷键").Min.x>keyui::items.at("变身与条件").Min.x&&keyui::items.at("设置").Min.x>keyui::items.at("预设快捷键").Min.x,"shortcut tab sits between transformation and settings at standard width");
 click("新建方案");check(s.data.hotkeys.size()==1&&ui::hotkeyId&&FindHotkey(ui::hotkeyId),"real New button creates a scoped scheme");auto id=ui::hotkeyId;
 click("启用预设快捷键");check(modalActive()&&!s.data.hotkeysEnabled&&s.data.transformsEnabled,"conflicting enable opens a real modal and leaves both switches unchanged");click("知道了");check(!modalActive(),"acknowledging conflict closes the modal");
 page=1;settle();click("启用变身功能");check(!s.data.transformsEnabled,"real transform master can be explicitly disabled");page=2;settle();click("启用预设快捷键");check(s.data.hotkeysEnabled&&!s.data.transformsEnabled,"real shortcut master can then be enabled");
 page=1;settle();click("启用变身功能");check(modalActive()&&s.data.hotkeysEnabled&&!s.data.transformsEnabled,"reverse enable also opens conflict modal and preserves the active mode");click("知道了");page=2;settle();
 strcpy_s(ui::hotkeyName,"月下搭配");click("保存名称");check(FindHotkey(id)->name=="月下搭配","real name action updates only the scheme");std::array<bool,256> keys{};ProcessPresetKeys(keys,false,true);click("录制快捷键");check(s.captureHotkey==id,"real record button arms capture");keys['L']=keys[VK_CONTROL]=true;ProcessPresetKeys(keys,false,true);settle();check(!s.captureHotkey&&FindHotkey(id)->key=='L'&&FindHotkey(id)->modifiers==1,"capture result reaches the real shortcut editor");
 click("添加目标分组");check(FindHotkey(id)->targets.size()==1&&FindHotkey(id)->targets[0].group==main&&!FindHotkey(id)->targets[0].preset,"target editor starts with an existing group and waits for a preset");
 // Both selectors fill their cell. Repeated frames must not feed their
 // measured widths back into an increasingly uneven split.
 auto originalTheme=s.data.theme;
 for(auto theme:{UiTheme::ClassicWhite,UiTheme::ClassicBlack,UiTheme::Glass,UiTheme::Basic}){
  s.data.theme=theme;
  for(auto size:{cases[1],cases[2],cases[4]}){
   active=size;
   for(int frame=0;frame<180;++frame)draw();
   float groupWidth=keyui::items.at("##组合分组").GetWidth();
   float presetWidth=keyui::items.at("##组合预设").GetWidth();
   std::cout<<"target widths "<<groupWidth<<" / "<<presetWidth<<"\n";
   check(groupWidth>60*size.scale&&presetWidth>60*size.scale&&groupWidth>presetWidth*.75f&&presetWidth>groupWidth*.75f,"both target selectors retain usable width after repeated frames and resize");
  }
 }
 active=cases[1];s.data.theme=originalTheme;settle();
 click("##组合分组");settle();click(Find(main)->name.c_str());settle();
 click("##组合预设");settle();click("隐藏");check(FindHotkey(id)->targets[0].preset==p.id,"real preset picker records the referenced preset ID");settle();click("立即应用");check(Find(main)->selected==p.id&&Find(main)->working.hidden==p.rules.hidden,"real Apply button applies the configured combination");
 for(auto c:cases){active=c;page=2;settle();check(keyui::items.count("启用预设快捷键")&&keyui::items.count("##快捷键方案"),"master and scheme selector exist across supported sizes");bool editor=false;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"/快捷键方案编辑区_")){editor=true;check(window->Size.x>100&&window->Size.y>80&&window->ScrollMax.x<.5f,"shortcut editor has usable scroll space with no horizontal clipping");}check(editor,"editor viewport remains present");}
 active=cases[1];settle();if(argc>1){SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",argv[1]);SaveCurrentFrame();SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",nullptr);}
 click("启用此方案");check(!FindHotkey(id)->enabled,"per-scheme enable is independent of the feature master");click("删除方案");click("取消##快捷键");check(FindHotkey(id)!=nullptr,"canceling deletion preserves the scheme");click("删除方案");click("删除##快捷键");check(!FindHotkey(id)&&Find(main)&&Find(main)->presets.size()==2,"deleting a plan leaves its source groups and presets intact");
 page=4;settle();check(std::find(keyui::text.begin(),keyui::text.end(),"插件作者：明月别枝(moon)")!=keyui::text.end(),"About credits the requested author name");// Use the real settings combo, then render every page with the saved theme.
 page=3;settle();click("界面风格");settle();click("经典白");settle();
 check(s.data.theme==UiTheme::ClassicWhite&&LoadSettings()&&s.data.theme==UiTheme::ClassicWhite,"real theme picker saves classic white");
 click("界面风格");settle();click("基础");settle();check(s.data.theme==UiTheme::Basic,"switching back restores the basic UI");
 std::string variant=argc>3?argv[3]:"white";
 auto wanted=variant=="glass"?UiTheme::Glass:variant=="black"?UiTheme::ClassicBlack:UiTheme::ClassicWhite;
 const char* choice=variant=="glass"?"玻璃态":variant=="black"?"经典黑":"经典白";
 click("界面风格");settle();click(choice);settle();
 check(LoadSettings()&&s.data.theme==wanted,"selected classic theme persists");
 auto& body=s.models[0];body.display="f_body105_0000";
 for(auto name:{"Meira_3_Antenna","Meira_3_Back","Meira_3_Front","Meira_3_Ponytail","Meira_3_Head","Meira_4_Hairclip 1","Meira_4_Hairclip 2"}){
  Mesh mesh;mesh.materialId=(uint16_t)body.materials.size();mesh.index=mesh.materialId;body.meshes.push_back(mesh);body.materials.push_back({name,MaterialNameHash(name)});
 }
 auto head=body;head.part="helm";head.display="f_helm105_0000";head.relativePath="pl\\f_equip\\pl105_0000\\helm\\mod\\f_helm105_0000";
 head.meshes.resize(1);head.materials={{"iris",MaterialNameHash("iris")}};s.lastSeen[Normalize(body.relativePath)]=s.lastSeen[Normalize(head.relativePath)]=GetTickCount();s.models.push_back(head);
 auto* group=Find(main);group->presets={shown};group->presets.front().name="Mei_梅";group->selected=group->defaultPreset=shown.id;
 for(auto name:{"身材","外套","衣服","袖子","黑丝","手套","鞋子"}){Group g;g.id=s.data.nextId++;g.scope=model.equip;g.name=name;g.global=g.name=="身材";s.data.groups.push_back(g);}
 group=Find(main);group->working=shown.rules;s.data.transformsEnabled=true;
 s.data.hotkeysEnabled=false;s.blockedScopes.clear();s.data.hideDragHint=false;s.profileStatus[model.equip]="已载入装备预设：pl105_0000.wdp";s.notice.clear();
 ui::hotkeyId=CreateHotkey(model.equip,"整备方案");s.notice.clear();
 active={1642,1048,1,1602,1008};io.AddMousePosEvent(-100,-100);
 auto capture=[&](const char* name){if(argc>2){auto dir=std::string(argv[2])+"/"+name;SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",dir.c_str());SaveCurrentFrame();SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",nullptr);}};
 auto notesFile=std::filesystem::path(L"发布/nativePC/plugins/WorldDiff/更新日志.md");
 if(std::filesystem::exists(notesFile))std::filesystem::copy_file(notesFile,s.directory/L"更新日志.md",std::filesystem::copy_options::overwrite_existing);
 const char* names[]={"main","conditions","hotkeys","settings","about"};
 for(page=0;page<5;page++){settle();check(ImGui::GetDrawData()->TotalVtxCount>1000,"classic page rendered");capture(names[page]);}
 const char* nav[]={"部位与材质","变身与条件","预设快捷键","设置","关于"};
 for(const auto& size:cases){
  active=size;page=0;settle();
  std::array<ImRect,5> baseline;
  for(size_t i=0;i<baseline.size();++i)baseline[i]=keyui::items.at(nav[i]);
  auto verifyTabs=[&]{
   for(size_t i=0;i<baseline.size();++i){
    const auto& box=keyui::items.at(nav[i]);const auto& ref=baseline[i];
    if(box.Min.x!=ref.Min.x||box.Min.y!=ref.Min.y||box.Max.x!=ref.Max.x||box.Max.y!=ref.Max.y){
     std::cerr<<"tab geometry changed: "<<nav[i]<<" page="<<page<<" width="<<size.width<<" scale="<<size.scale<<" rect "<<ref.Min.x<<","<<ref.Min.y<<","<<ref.Max.x<<","<<ref.Max.y<<" -> "<<box.Min.x<<","<<box.Min.y<<","<<box.Max.x<<","<<box.Max.y<<"\n";
     throw std::runtime_error("tab geometry must stay fixed through the click and subsequent frames");
    }
   }
  };
  for(int next:{1,2,3,4,0}){
   const auto pt=keyui::items.at(nav[next]).GetCenter();io.AddMousePosEvent(pt.x,pt.y);
   io.AddMouseButtonEvent(0,true);draw();verifyTabs();
   io.AddMouseButtonEvent(0,false);draw();page=ui::tab;verifyTabs();
   check(page==next,"clicked page became active without moving the tab strip");
   for(int frame=0;frame<3;++frame){draw();verifyTabs();}
  }
 }
 active={1642,1048,1,1602,1008};page=4;settle();

 for(auto& partModel:s.models)for(auto& mesh:partModel.meshes){mesh.vertexCount=3;mesh.indexCount=3;}
 page=0;settle();
 auto sourceFile=s.directory/L"Templates/Mei_梅.wdp";auto secondFile=s.directory/L"Templates/Z_备用.wdp";
 auto openExport=[&]{click("导出预设");settle();};
 auto chooseExport=[&]{
  auto field=keyui::items.at("##预设文件名"),arrow=keyui::items.at("##已有预设文件");
  check(std::abs(arrow.Min.x-field.Max.x)<2&&std::abs(arrow.Min.y-field.Min.y)<2&&field.GetWidth()>arrow.GetWidth()*4,"arrow directly adjoins filename on the right");
  click("##已有预设文件");settle();check(keyui::items.count("Mei_梅.wdp")!=0,"existing Unicode template listed");
  click("Mei_梅.wdp");settle();check(ui::exportSelection=="Mei_梅.wdp"&&keyui::items.count("覆盖保存"),"selecting a file arms direct overwrite");
 };
 for(auto theme:{UiTheme::Basic,wanted}){
  s.data.theme=theme;settle();openExport();click("另存为");check(keyui::items.count("##已有预设文件"),"empty save keeps dialog open");
  if(!std::filesystem::exists(sourceFile)){
   strcpy_s(ui::profileName,"Mei_梅");click("另存为");check(std::filesystem::exists(sourceFile),"save as adds wdp extension");
   openExport();
  }
  capture(theme==UiTheme::Basic?"save_basic":"save_file");chooseExport();click("##预设文件名");
  io.AddKeyEvent(ImGuiKey_End,true);draw();io.AddKeyEvent(ImGuiKey_End,false);draw();
  io.AddInputCharactersUTF8("X");settle();
  check(ui::exportSelection.empty()&&keyui::items.count("另存为"),"typing after selection disarms overwrite");
  click("另存为");settle();
  auto renamed=s.directory/L"Templates/Mei_梅X.wdp";
  check(std::filesystem::exists(renamed)&&std::filesystem::exists(sourceFile),"renaming selected template creates a separate file");
  std::filesystem::remove(renamed);openExport();chooseExport();
  {std::ofstream out(sourceFile);out<<"direct-overwrite marker";}
  click("覆盖保存");check(!keyui::items.count("确认覆盖")&&std::filesystem::file_size(sourceFile)>40,"selected file updated immediately");
  openExport();strcpy_s(ui::profileName,"mei_梅");{std::ofstream out(sourceFile);out<<"confirmation marker";}
  click("另存为");settle();check(keyui::items.count("确认覆盖")&&std::filesystem::file_size(sourceFile)==19,"manual same name differing in case and extension requires confirmation");
  click("返回修改");settle();check(keyui::items.count("另存为")&&std::filesystem::file_size(sourceFile)==19,"back preserves existing bytes");
  click("另存为");settle();click("确认覆盖");check(std::filesystem::file_size(sourceFile)>40,"explicit confirmation replaces template");
 }
 io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 auto closed=[] {return !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);};
 auto press=[&](ImGuiKey key){io.AddKeyEvent(key,true);draw();io.AddKeyEvent(key,false);settle();};
 auto verifyFind=[&](bool basic){
  auto rows=basic?"##template":"##paperTemplate";
  auto firstId=Main(model.equip).id;
  std::filesystem::copy_file(sourceFile,secondFile,std::filesystem::copy_options::overwrite_existing);
  click("手动查找");settle();
  check(keyui::applyDisabled&&ui::profileFiles.size()==2&&ui::selectedProfileFile.empty(),"open with two templates and no selection");
  auto apply=keyui::items.at("应用预设"),cancel=keyui::items.at("取消##查找"),refresh=keyui::items.at("刷新列表");
  check(apply.Min.x>cancel.Max.x&&apply.Min.y==refresh.Min.y,"Apply is right of Cancel on the footer");
  click("应用预设");check(!closed()&&Main(model.equip).id==firstId,"disabled Apply cannot import");
  io.AddMousePosEvent(-100,-100);settle();capture(basic?"find_basic_idle":"find");
  press(ImGuiKey_DownArrow);check(ui::selectedProfileFile=="Mei_梅.wdp","Down selects first without clicking");
  press(ImGuiKey_DownArrow);check(ui::selectedProfileFile=="Z_备用.wdp","Down selects next");
  press(ImGuiKey_UpArrow);check(ui::selectedProfileFile=="Mei_梅.wdp","Up selects previous");
  auto lastRow=keyui::items.at(rows).GetCenter();io.AddMousePosEvent(lastRow.x,lastRow.y);settle();
  check(ui::selectedProfileFile=="Mei_梅.wdp","hover over another row does not replace selection");
  click(rows);io.AddMousePosEvent(-100,-100);settle();
  check(!keyui::applyDisabled&&ui::selectedProfileFile=="Z_备用.wdp"&&!closed()&&Main(model.equip).id==firstId,"click selects without importing");
  click("刷新列表");check(ui::selectedProfileFile=="Z_备用.wdp","refresh retains selected filename");
  std::filesystem::remove(secondFile);click("刷新列表");
  check(keyui::applyDisabled&&ui::selectedProfileFile.empty()&&ui::profileFiles.size()==1,"refresh clears missing selection");
  click(rows);settle();capture(basic?"find_basic":"find_selected");click("取消##查找");
  check(closed()&&Main(model.equip).id==firstId,"cancel leaves equipment unchanged");
  click("手动查找");settle();check(ui::selectedProfileFile.empty(),"reopen clears selection");
  std::filesystem::copy_file(sourceFile,secondFile);click("刷新列表");click(rows);press(ImGuiKey_Enter);check(!closed()&&Main(model.equip).id==firstId,"Enter on a row never imports");
  click("应用预设");
  check(closed()&&s.data.bindings[model.equip]=="pl105_0000.wdp"&&s.notice.find("Z_备用.wdp")!=std::string::npos&&Main(model.equip).id!=firstId,"Apply imports selected file and closes");std::filesystem::remove(secondFile);
 };
 verifyFind(false);
 page=3;settle();click("界面风格");settle();capture("picker");click(choice);
 s.trackingPlayer=true;s.outfit.ready=false;page=3;settle();click("界面风格");settle();click("基础");settle();
 check(s.data.theme==UiTheme::Basic,"waiting-for-player settings can switch back to basic");
 s.trackingPlayer=false;page=0;settle();verifyFind(true);

 if(argc>5){
  s.directory=s.directory/L"MeiUI";s.configPath=s.directory/L"Settings.cfg";s.data={};
  s.loadedFingerprints.clear();s.profileSnapshots.clear();InitializeProfiles();
  std::filesystem::copy_file(PathUtf8(argv[5]),s.directory/L"Templates/Mei_梅.wdp");
  s.models=LoadCatalog(PathUtf8(argv[4]));s.data.hideDragHint=true;
  for(auto theme:{wanted,UiTheme::Basic}){
   s.data.theme=theme;page=0;active={1642,1048,1,1602,1008};settle();click("手动查找");settle();
   check(ui::profileFiles.size()==1&&ui::profileFiles[0].resolved==93&&ui::profileFiles[0].controlled==93,"actual Mei UI uses 93 resolved targets");
   capture(theme==UiTheme::Basic?"mei_basic":"mei");
   click("取消##查找");
   active={1280,720,.7f,1022,595};settle();click("手动查找");settle();
   auto row=keyui::items.at(theme==UiTheme::Basic?"##template":"##paperTemplate");
   check(row.GetHeight()>10&&row.GetHeight()<40&&row.GetWidth()>400,"single-line template row fits the compact layout");
   capture(theme==UiTheme::Basic?"mei_basic_small":"mei_small");click("取消##查找");
  }
 }
 // Exercise the actual settings dropdown and conflict rejection in every theme.
 for(auto theme:{UiTheme::ClassicWhite,UiTheme::ClassicBlack,UiTheme::Glass,UiTheme::Basic}){
  s.data.theme=theme;s.data.menuKey=0;page=3;active={1280,720,.7f,1022,595};settle();
  click("开关面板快捷键");settle();check(keyui::items.count("默认：F9 / Home")&&keyui::items.count("F9")&&keyui::items.count("Home"),"default and single-key options are available");
  click("F8");check(s.data.menuKey==VK_F8&&!IsMenuKey(VK_F9,s.data)&&!IsMenuKey(VK_HOME,s.data),"settings selection replaces both defaults");
  PresetHotkey blocker;blocker.id=s.data.nextId++;blocker.name="冲突测试";blocker.scope="pl105_0000";blocker.key=VK_F7;s.data.hotkeys.push_back(blocker);
  click("开关面板快捷键");settle();click("F7");check(s.data.menuKey==VK_F8&&s.notice.find("冲突")!=std::string::npos,"settings refuses a key already used by a preset");s.data.hotkeys.pop_back();
  click("开关面板快捷键");settle();click("F9");check(s.data.menuKey==VK_F9&&!IsMenuKey(VK_HOME,s.data),"selecting F9 explicitly makes it a single key");
  click("开关面板快捷键");settle();click("Home");check(s.data.menuKey==VK_HOME&&!IsMenuKey(VK_F9,s.data),"selecting Home explicitly makes it a single key");
  click("开关面板快捷键");settle();click("默认：F9 / Home");check(s.data.menuKey==0&&IsMenuKey(VK_HOME,s.data)&&IsMenuKey(VK_F9,s.data),"default option restores both keys");
  io.AddMousePosEvent(-100,-100);s.notice.clear();settle();capture(theme==UiTheme::Basic?"panel_basic":theme==UiTheme::ClassicWhite?"panel_white":theme==UiTheme::ClassicBlack?"panel_black":"panel_glass");
 }
 // Drive a real resize grip, then recreate the ImGui context as on game restart.
 auto newContext=[&]{
  ImGui::DestroyContext();ImGui::CreateContext();
  auto& testIO=ImGui::GetIO();testIO.IniFilename=nullptr;testIO.DisplaySize={3840,2160};
  testIO.DeltaTime=1.f/60;
  testIO.Fonts->AddFontDefault();
  unsigned char* pixels;int w,h;testIO.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 };
 newContext();s.data.scale=1;s.data.theme=wanted;ui::tab=0;
 auto panelFrame=[&]{ImGui::NewFrame();ui::DrawPanel();ImGui::Render();return ImGui::FindWindowByName("###WorldDiffInternal");};
 auto* manager=panelFrame();panelFrame();panelFrame();
 auto& input=ImGui::GetIO();
 ImVec2 grip(manager->Pos.x+manager->Size.x-3,manager->Pos.y+manager->Size.y-3);
 input.AddMousePosEvent(grip.x,grip.y);panelFrame();panelFrame();
 input.AddMouseButtonEvent(0,true);panelFrame();
 input.AddMousePosEvent(grip.x+640,grip.y+390);panelFrame();panelFrame();
 input.AddMouseButtonEvent(0,false);panelFrame();
 const int savedWidth=s.panelWidth,savedHeight=s.panelHeight;
 check(savedWidth>=2050&&savedHeight>=1200&&!s.panelSizeDirty,"manual resize writes the chosen dimensions on release");
 s.panelWidth=s.panelHeight=0;
 check(LoadSettings()&&s.panelWidth==savedWidth&&s.panelHeight==savedHeight,"window size round trips through player settings");
 newContext();manager=panelFrame();
 check(manager->Size.x==savedWidth&&manager->Size.y==savedHeight,"fresh context restores the chosen pixel size");
 ImGui::GetIO().DisplaySize={1920,1080};manager=panelFrame();
 check(manager->Size.x<=1920&&manager->Size.y<=1080&&manager->Pos.x>=0&&manager->Pos.y>=0,"4K preference fits a smaller screen");
 check(s.panelWidth==savedWidth&&s.panelHeight==savedHeight,"screen constraint does not replace stored preference");
 ImGui::GetIO().DisplaySize={3840,2160};manager=panelFrame();
 check(manager->Size.x==savedWidth&&manager->Size.y==savedHeight,"returning to 4K restores the saved size");
 s.trackingPlayer=true;s.outfit.ready=false;manager=panelFrame();
 check(manager->Size.x==savedWidth&&manager->Size.y==savedHeight,"waiting panel uses the same remembered size");
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" real shortcut-page UI checks\nArtifacts "<<s.directory.string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
