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
 unsigned checks=0;auto check=[&](bool yes,const char* reason){checks++;if(!yes)throw std::runtime_error(reason);};auto& s=Get();s.loaded=true;s.directory=std::filesystem::temp_directory_path()/(L"WorldDiff122KeyUI_"+std::to_wstring(GetTickCount64()));s.configPath=s.directory/L"Settings.cfg";std::filesystem::create_directories(s.directory);
 Model model;model.equip="pl105_0000";model.part="body";model.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";s.models={model};auto main=Main(model.equip).id;Preset p;p.id=s.data.nextId++;p.name="隐藏";p.rules.hidden.insert({model.relativePath,0,true});Preset shown;shown.id=s.data.nextId++;shown.name="显示";Find(main)->presets={p,shown};Find(main)->selected=shown.id;ui::outfit=model.equip;ui::groupId=main;s.data.hideDragHint=true;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;ImFontGlyphRangesBuilder range;range.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());range.AddText("编辑明月别枝预设快捷键互斥录制");ImVector<ImWchar> glyphs;range.BuildRanges(&glyphs);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",22,nullptr,glyphs.Data);unsigned char* atlas;int w,h;io.Fonts->GetTexDataAsRGBA32(&atlas,&w,&h);auto& style=ImGui::GetStyle();style.FramePadding={9,6};style.ItemSpacing={10,8};style.FrameRounding=6;style.ChildRounding=8;style.Colors[ImGuiCol_WindowBg]={.078f,.094f,.125f,.97f};style.Colors[ImGuiCol_ChildBg]={.122f,.149f,.188f,.98f};style.Colors[ImGuiCol_Button]={.13f,.30f,.35f,1};style.Colors[ImGuiCol_Text]={.945f,.957f,.972f,1};style.Colors[ImGuiCol_CheckMark]={.33f,.72f,.78f,1};
 struct Case {float x,y,scale,width,height;};Case cases[]={{1920,1080,.7f,1022,595},{1920,1080,1,1460,850},{1920,1080,1,1050,620},{1280,720,1,1050,620},{3840,2160,1.5f,2190,1275}};auto active=cases[1];int page=2;
 auto draw=[&]{io.DisplaySize={active.x,active.y};s.data.scale=active.scale;ui::tab=page;keyui::items.clear();keyui::text.clear();ImGui::NewFrame();ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({active.width,active.height});ui::DrawPanel();ImGui::Render();};
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
 page=4;settle();check(std::find(keyui::text.begin(),keyui::text.end(),"插件作者：明月别枝(moon)")!=keyui::text.end(),"About credits the requested author name");ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" real shortcut-page UI checks\nArtifacts "<<s.directory.string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
