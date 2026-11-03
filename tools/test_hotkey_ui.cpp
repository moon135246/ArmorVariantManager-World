#include "imgui.h"
#include "imgui_internal.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace keyui {std::map<std::string,ImRect> items;std::vector<std::string> text;}
namespace ImGui {
bool KeyTrackedButton(const char* label,const ImVec2& size=ImVec2(0,0)){auto hit=Button(label,size);keyui::items[label]={GetItemRectMin(),GetItemRectMax()};return hit;}
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
namespace worlddiff::overlay {void NativeInputClose(){}void SetVisible(bool){}const char* BackendStatus(){return "shortcut UI verifier";}bool TextField(const char* label,const char* hint,char* value,size_t capacity){return ImGui::InputTextWithHint(label,hint,value,capacity);}}
using namespace worlddiff;using namespace worlddiff::app;namespace ui=worlddiff::overlay;
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool yes,const char* reason){checks++;if(!yes)throw std::runtime_error(reason);};auto& s=Get();s.loaded=true;s.directory=std::filesystem::temp_directory_path()/(L"WorldDiff124KeyUI_"+std::to_wstring(GetTickCount64()));s.configPath=s.directory/L"Settings.cfg";std::filesystem::create_directories(s.directory);
 Model model;model.equip="pl105_0000";model.part="body";model.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";s.models={model};auto main=Main(model.equip).id;Preset p;p.id=s.data.nextId++;p.name="隐藏";p.rules.hidden.insert({model.relativePath,0,true});Preset shown;shown.id=s.data.nextId++;shown.name="显示";Find(main)->presets={p,shown};Find(main)->selected=shown.id;ui::outfit=model.equip;ui::groupId=main;s.data.hideDragHint=true;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;ImFontGlyphRangesBuilder range;range.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());range.AddText("编辑明月别枝预设快捷键互斥录制");ImVector<ImWchar> glyphs;range.BuildRanges(&glyphs);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",22,nullptr,glyphs.Data);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyhbd.ttc",22,nullptr,glyphs.Data);unsigned char* atlas;int w,h;io.Fonts->GetTexDataAsRGBA32(&atlas,&w,&h);auto& style=ImGui::GetStyle();style.FramePadding={9,6};style.ItemSpacing={10,8};style.FrameRounding=6;style.ChildRounding=8;style.Colors[ImGuiCol_WindowBg]={.078f,.094f,.125f,.97f};style.Colors[ImGuiCol_ChildBg]={.122f,.149f,.188f,.98f};style.Colors[ImGuiCol_Button]={.13f,.30f,.35f,1};style.Colors[ImGuiCol_Text]={.945f,.957f,.972f,1};style.Colors[ImGuiCol_CheckMark]={.33f,.72f,.78f,1};
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
 click("##组合预设");settle();click("隐藏");check(FindHotkey(id)->targets[0].preset==p.id,"real preset picker records the referenced preset ID");settle();click("立即应用");check(Find(main)->selected==p.id&&Find(main)->working.hidden==p.rules.hidden,"real Apply button applies the configured combination");
 for(auto c:cases){active=c;page=2;settle();check(keyui::items.count("启用预设快捷键")&&keyui::items.count("##快捷键方案"),"master and scheme selector exist across supported sizes");bool editor=false;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"/快捷键方案编辑区_")){editor=true;check(window->Size.x>100&&window->Size.y>80&&window->ScrollMax.x<.5f,"shortcut editor has usable scroll space with no horizontal clipping");}check(editor,"editor viewport remains present");}
 active=cases[1];settle();if(argc>1){SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",argv[1]);SaveCurrentFrame();SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",nullptr);}
 click("启用此方案");check(!FindHotkey(id)->enabled,"per-scheme enable is independent of the feature master");click("删除方案");click("取消##快捷键");check(FindHotkey(id)!=nullptr,"canceling deletion preserves the scheme");click("删除方案");click("删除##快捷键");check(!FindHotkey(id)&&Find(main)&&Find(main)->presets.size()==2,"deleting a plan leaves its source groups and presets intact");
 page=4;settle();check(std::find(keyui::text.begin(),keyui::text.end(),"插件作者：明月别枝(moon)")!=keyui::text.end(),"About credits the requested author name");// Use the real settings combo, then render every page with the saved theme.
 page=3;settle();click("界面风格");settle();click("经典白");settle();
 check(s.data.theme==UiTheme::ClassicWhite&&LoadSettings()&&s.data.theme==UiTheme::ClassicWhite,"real theme picker saves classic white");
 click("界面风格");settle();click("玻璃态（待推出）");
 check(s.data.theme==UiTheme::ClassicWhite,"planned themes cannot be selected");
 click("基础");settle();check(s.data.theme==UiTheme::Basic,"switching back restores the basic UI");
 bool black=argc>3&&std::string(argv[3])=="black";
 const char* choice=black?"经典黑":"经典白";auto wanted=black?UiTheme::ClassicBlack:UiTheme::ClassicWhite;
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
 for(auto& partModel:s.models)for(auto& mesh:partModel.meshes){mesh.vertexCount=3;mesh.indexCount=3;}
 page=0;settle();click("导出模板");settle();capture("save_file");
 strcpy_s(ui::profileName,"Mei_梅");click("保存文件");
 check(std::filesystem::exists(s.directory/L"Templates/Mei_梅.wdp"),"save dialog exports the named template");
 click("手动查找");settle();click("刷新列表");settle();
 const char* result="##paperTemplate";
 check(keyui::items.count(result)==1,"classic search shows the exported template");
 auto yellowFlag=[&]{
  for(auto* w:ImGui::GetCurrentContext()->Windows)if(w->Active&&std::strstr(w->Name,"/预设文件列表_"))
   for(const auto& v:w->DrawList->VtxBuffer)if(v.col==ui::paper::PaletteFor(wanted).signal)return true;
  return false;
 };
 io.AddMousePosEvent(-100,-100);settle();check(!yellowFlag(),"idle search row has no yellow flag");capture("find");
 auto hit=keyui::items.at(result).GetCenter();io.AddMousePosEvent(hit.x,hit.y);settle();
 check(yellowFlag(),"hover highlights the classic template");capture("find_selected");click(result);
 auto closed=[] {return !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);};
 check(closed()&&s.data.bindings[model.equip]=="pl105_0000.wdp","single click imports the original template");
 io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
 click("手动查找");io.AddMousePosEvent(-100,-100);settle();
 for(int i=0;i<8&&!yellowFlag();++i){
  io.AddKeyEvent(ImGuiKey_Tab,true);draw();io.AddKeyEvent(ImGuiKey_Tab,false);settle();
 }
 check(yellowFlag(),"keyboard focus highlights the template with the mouse away");
 io.AddKeyEvent(ImGuiKey_Enter,true);draw();io.AddKeyEvent(ImGuiKey_Enter,false);settle();
 check(closed(),"Enter imports the focused result");
 page=3;settle();click("界面风格");settle();capture("picker");click(choice);
 s.trackingPlayer=true;s.outfit.ready=false;page=3;settle();click("界面风格");settle();click("基础");settle();
 check(s.data.theme==UiTheme::Basic,"waiting-for-player settings can switch back to basic");
 s.trackingPlayer=false;page=0;settle();click("手动查找");settle();
 auto templateRow=keyui::items.at("##template");
 check(templateRow.GetHeight()>=42,"basic template results use the group-row height");
 auto center=templateRow.GetCenter();io.AddMousePosEvent(center.x,center.y);settle();
 bool stripe=false;for(auto* window:ImGui::GetCurrentContext()->Windows)
  if(window->Active&&std::strstr(window->Name,"/预设文件列表_"))
   for(const auto& vertex:window->DrawList->VtxBuffer)if(vertex.col==IM_COL32(82,226,236,255))stripe=true;
 check(stripe,"hovered basic template has the group selection stripe");capture("find_basic");click("##template");
 check(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel),"single click imports from the basic rounded row");
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" real shortcut-page UI checks\nArtifacts "<<s.directory.string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
