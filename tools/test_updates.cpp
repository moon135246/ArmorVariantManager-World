#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <cstring>
#include <map>
#include <vector>
#include <stdexcept>
namespace updateqa {std::map<std::string,ImRect> items;std::vector<std::string> texts;}
namespace ImGui {
bool UpdateTrackedButton(const char* label,const ImVec2& size=ImVec2(0,0)){bool hit=Button(label,size);updateqa::items[label]={GetItemRectMin(),GetItemRectMax()};return hit;}
bool UpdateTrackedCheckbox(const char* label,bool* value){bool hit=Checkbox(label,value);updateqa::items[label]={GetItemRectMin(),GetItemRectMax()};return hit;}
void UpdateTrackedText(const char* text,const char* end=nullptr){updateqa::texts.emplace_back(text,end?end:text+std::strlen(text));TextUnformatted(text,end);}
}
#define Button UpdateTrackedButton
#define Checkbox UpdateTrackedCheckbox
#define TextUnformatted UpdateTrackedText
#include "../src/Panel.cpp"
#undef Button
#undef Checkbox
#undef TextUnformatted
#include "save_imgui_frame.inl"
namespace worlddiff::overlay {
void NativeInputClose(){}void SetVisible(bool){}const char* BackendStatus(){return "update verifier";}
bool TextField(const char* label,const char* hint,char* value,size_t capacity){return ImGui::InputTextWithHint(label,hint,value,capacity);}
}
using namespace worlddiff;using namespace worlddiff::app;namespace ui=worlddiff::overlay;namespace fs=std::filesystem;
static void Write(const fs::path& path,const std::string& text){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<text;if(!out)throw std::runtime_error("fixture write");}
static std::string Read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool good,const char* reason){checks++;if(!good)throw std::runtime_error(reason);};
 auto root=fs::temp_directory_path()/(L"WorldDiff121Updates_"+std::to_wstring(GetTickCount64()));auto& state=Get();state.directory=root;state.configPath=root/L"Settings.cfg";state.loaded=true;
 const std::string old="WorldDiffSettings 117\nSET 0 0 1 119 1.2 1 0 0\n";Write(state.configPath,old);state.data.showStartupNotice=false;
 check(LoadSettings()&&state.data.showStartupNotice,"old settings default to startup notice enabled");
 check(!state.data.enabled&&!state.data.autoDefault&&state.data.autoOpen&&state.data.menuKey==119&&state.data.scale==1.2f&&state.data.hideDragHint&&!state.data.transformsEnabled&&!state.data.autoFind,"all personal settings survive legacy load");
 check(Read(state.configPath)==old,"reading old settings never rewrites them");
 state.data.showStartupNotice=false;check(SaveSettings(),"save notice preference");auto current=Read(state.configPath);state.data.showStartupNotice=true;
 check(LoadSettings()&&!state.data.showStartupNotice,"disabled notice survives a fresh settings load");
 state.data.showStartupNotice=true;check(SaveSettings(),"enable notice preference");state.data.showStartupNotice=false;check(LoadSettings()&&state.data.showStartupNotice,"enabled notice survives a fresh settings load");
 for(auto broken:{std::string("WorldDiffSettings 121\nSET 1 1 0 120 1 0 1 1\n"),std::string("WorldDiffSettings 121\nSET 1 1 0 120 1 0 1 1 2\n"),current+"UNKNOWN 1\n",std::string("WorldDiffSettings 999\nSET 1 1 0 120 1 0 1 1 0\n")}){
  Write(state.configPath,broken);auto before=state.data;check(!LoadSettings()&&state.data.showStartupNotice==before.showStartupNotice&&state.data.scale==before.scale&&state.data.menuKey==before.menuKey,"invalid settings are rejected without partial replacement");check(Read(state.configPath)==broken,"invalid input file is preserved");
 }
 Write(state.configPath,current);check(LoadSettings(),"valid settings can be reloaded after rejection");
 auto parsed=notes::Parse("\xef\xbb\xbf## V测试\r\n- 100% 中文更新\r\n\r\n普通段落\n* 第二条\n");
 check(parsed.error.empty()&&parsed.lines.size()==4&&parsed.lines[0].kind==notes::Kind::Heading&&parsed.lines[1].kind==notes::Kind::Item&&parsed.lines[1].text=="100% 中文更新"&&parsed.lines[2].kind==notes::Kind::Text,"UTF-8 BOM CRLF headings lists and literal percentages render as text");
 for(auto bad:{std::string("\xff\xfeX"),std::string("\xc0\xaf"),std::string("\xed\xa0\x80"),std::string("\xf4\x90\x80\x80"),std::string("\xe4\xb8"),std::string("a\0b",3)})check(!notes::Parse(bad).error.empty(),"invalid UTF-8 and embedded nulls are rejected");
 check(notes::Parse("中文 👋").error.empty(),"valid four-byte Unicode remains available");check(!notes::Parse(std::string(notes::kMaxBytes+1,'a')).error.empty(),"oversized notes are rejected");
 auto path=root/L"更新日志.md";notes::Cache cache;check(!notes::Read(path).error.empty(),"missing notes show an explicit read error");Write(path,"## V测试\n- 原始内容\n");cache.Load(path);Write(path,"## V测试\n- 手动更新 100%\n");
 check(cache.document.lines.back().text=="原始内容","cached notes do not poll disk every frame");cache.Load(path);check(cache.document.lines.back().text=="手动更新 100%","reload immediately picks up manual edits");Write(path,"\xff");cache.Load(path);check(!cache.document.error.empty()&&cache.document.lines.empty(),"failed reload cannot misreport stale content as current");
 Write(path,"");check(notes::Read(path).error.empty()&&notes::Read(path).lines.empty(),"empty manually edited file is supported");Write(path,std::string(notes::kMaxBytes+2,'a'));check(!notes::Read(path).error.empty(),"bounded reader rejects oversized files");
 check(notes::ShowStartupNotice(true,false,0)&&notes::ShowStartupNotice(true,false,8999),"startup toast is visible inside its time window");check(!notes::ShowStartupNotice(false,false,0)&&!notes::ShowStartupNotice(false,false,8999),"disabled startup toast stays hidden for the entire window");check(!notes::ShowStartupNotice(true,true,100)&&!notes::ShowStartupNotice(true,false,9000),"panel visibility and expiry suppress the toast");
 if(argc>1){auto official=notes::Read(PathUtf8(argv[1]));check(official.error.empty()&&official.lines.size()>20,"shipped update notes are readable");for(int version=0;version<=5;version++){auto heading="V1.0."+std::to_string(version);check(std::count_if(official.lines.begin(),official.lines.end(),[&](auto& line){return line.kind==notes::Kind::Heading&&line.text==heading;})==1,"each requested early release occurs exactly once in the shipped changelog");}fs::copy_file(PathUtf8(argv[1]),path,fs::copy_options::overwrite_existing);}else Write(path,"## V1.2.1\n- 新增启动提示开关和更新日志。\n");
 Model model;model.equip="pl105_0000";model.part="body";model.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";model.display="验证";state.models={model};auto id=Main(model.equip).id;ui::outfit=model.equip;ui::groupId=id;state.data.scale=1;state.data.hideDragHint=true;
 std::ostringstream baseline;WriteDocument(baseline,state.data);auto notesBefore=Read(path);
 state.notice.clear();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;ImFontGlyphRangesBuilder ranges;ranges.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());ranges.AddText("编辑");ranges.AddText(notesBefore.c_str());ImVector<ImWchar> glyphs;ranges.BuildRanges(&glyphs);io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",22,nullptr,glyphs.Data);unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);auto& style=ImGui::GetStyle();style.FramePadding={9,6};style.ItemSpacing={10,8};style.ChildRounding=8;style.FrameRounding=6;
 auto* colors=style.Colors;colors[ImGuiCol_WindowBg]={.078f,.094f,.125f,.97f};colors[ImGuiCol_ChildBg]={.122f,.149f,.188f,.98f};colors[ImGuiCol_Border]={.24f,.29f,.36f,.8f};colors[ImGuiCol_Text]={.945f,.957f,.972f,1};colors[ImGuiCol_TextDisabled]={.60f,.66f,.72f,1};colors[ImGuiCol_FrameBg]={.145f,.18f,.227f,1};colors[ImGuiCol_Button]={.13f,.30f,.35f,1};colors[ImGuiCol_ButtonHovered]={.18f,.45f,.51f,1};colors[ImGuiCol_CheckMark]={.33f,.72f,.78f,1};
 struct Case {float x,y,scale,width,height;};Case cases[]={{1920,1080,.7f,1022,595},{1920,1080,1,1460,850},{1920,1080,1,1050,620},{1280,720,1,1050,620},{3840,2160,1.5f,2190,1275}};
 auto draw=[&](Case c,int tab,bool waiting){io.DisplaySize={c.x,c.y};state.data.scale=c.scale;state.trackingPlayer=waiting;ui::tab=tab;updateqa::items.clear();updateqa::texts.clear();ImGui::NewFrame();ImGui::SetNextWindowSize({c.width,c.height});ImGui::SetNextWindowPos({20,20});ui::DrawPanel();ImGui::Render();};
 for(auto c:cases)for(bool waiting:{false,true}){
  for(int i=0;i<3;i++)draw(c,4,waiting);
  check(updateqa::items.count("关于")&&updateqa::items.count("设置")&&updateqa::items.count("编辑更新日志")&&updateqa::items.count("重新读取"),"new page and editing controls exist with and without a loaded outfit");
  check(std::find(updateqa::texts.begin(),updateqa::texts.end(),"插件作者：明月别枝(moon)")!=updateqa::texts.end(),"about page credits moon");
  bool body=false;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"/更新日志正文_")){body=true;check(window->Size.x>100&&window->Size.y>30&&window->ScrollMax.x<.5f,"update text has a usable wrapped scrolling viewport at every supported size");}check(body,"update body exists");
  auto bounds=updateqa::items.at("关于");
  check(bounds.GetWidth()>30&&bounds.GetHeight()>10,"new tab has a usable click target");
 }
 state.trackingPlayer=false;state.data.scale=1;state.data.showStartupNotice=true;for(int i=0;i<3;i++)draw(cases[1],3,false);check(updateqa::items.count("显示启动提示"),"notice toggle is in Settings");
 auto center=updateqa::items.at("显示启动提示").GetCenter();io.AddMousePosEvent(center.x,center.y);io.AddMouseButtonEvent(0,true);draw(cases[1],3,false);io.AddMouseButtonEvent(0,false);draw(cases[1],3,false);
 check(!state.data.showStartupNotice,"real settings checkbox disables the startup toast");state.data.showStartupNotice=true;check(LoadSettings()&&!state.data.showStartupNotice,"real checkbox persists the preference immediately");
 for(int i=0;i<3;i++)draw(cases[1],4,false);Write(path,"## V手动编辑\n- 新内容已保存\n");auto reload=updateqa::items.at("重新读取").GetCenter();io.AddMousePosEvent(reload.x,reload.y);io.AddMouseButtonEvent(0,true);draw(cases[1],4,false);io.AddMouseButtonEvent(0,false);draw(cases[1],4,false);
 check(ui::updateNotes.document.lines.back().text=="新内容已保存","real Reload button updates the in-game document");
 Write(path,notesBefore);ui::updateNotes.Load(path);draw(cases[1],4,false);if(argc>2){SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",argv[2]);SaveCurrentFrame();SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",nullptr);}
 std::ostringstream after;WriteDocument(after,state.data);check(after.str()==baseline.str()&&Read(path)==notesBefore,"viewing notes and changing global preference preserve equipment definitions and manual notes");
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" update/settings checks\nArtifacts "<<root.string()<<'\n';return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
