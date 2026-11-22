#include "imgui.h"
#include "imgui_internal.h"
#include <map>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cstring>
namespace qa {
struct Widget { ImRect rect; bool disabled,visible; };
std::map<std::string,Widget> widgets;
void Record(const char* label){widgets[label]={{ImGui::GetItemRectMin(),ImGui::GetItemRectMax()},(ImGui::GetCurrentContext()->LastItemData.ItemFlags&ImGuiItemFlags_Disabled)!=0,ImGui::IsItemVisible()};}
}
namespace ImGui {
bool TrackedButton(const char* label,const ImVec2& size=ImVec2(0,0)){bool hit=Button(label,size);qa::Record(label);return hit;}
bool TrackedInvisibleButton(const char* label,const ImVec2& size,ImGuiButtonFlags flags=0){bool hit=InvisibleButton(label,size,flags);qa::Record(label);return hit;}
bool TrackedCheckbox(const char* label,bool* value){bool hit=Checkbox(label,value);qa::Record(label);return hit;}
}
#define Button TrackedButton
#define InvisibleButton TrackedInvisibleButton
#define Checkbox TrackedCheckbox
#include "../src/Panel.cpp"
#undef Button
#undef InvisibleButton
#undef Checkbox
#include "save_imgui_frame.inl"
#include "../src/EmbeddedFonts.h"
namespace worlddiff::overlay {
void NativeInputClose(){} void SetVisible(bool){} const char* BackendStatus(){return "mismatch simulation";}
bool TextField(const char* label,const char* hint,char* value,size_t capacity){return ImGui::InputTextWithHint(label,hint,value,capacity);}
}
using namespace worlddiff;using namespace worlddiff::app;namespace fs=std::filesystem;namespace ui=worlddiff::overlay;
static std::string Read(const fs::path& path){std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
static void Write(const fs::path& path,const std::string& text){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<text;if(!out)throw std::runtime_error("fixture write");}
static Model Fixture(){
 Model m;m.equip="pl105_0000";m.part="body";m.display="模拟更换外观 MOD";m.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";
 for(int i=0;i<3;i++){auto name="OldMaterial"+std::to_string(i);m.materials.push_back({name,MaterialNameHash(name),i});Mesh mesh;mesh.index=i;mesh.materialId=(uint16_t)i;mesh.vertexCount=100;mesh.indexCount=300;mesh.lod=65535;m.meshes.push_back(mesh);}m.signature=MeshOrderSignature(m);return m;
}
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool value,const char* why){checks++;if(!value)throw std::runtime_error(why);};auto& s=Get();
 s.loaded=true;s.directory=fs::temp_directory_path()/(L"WorldDiffMismatch_"+std::to_wstring(GetTickCount64()));s.configPath=s.directory/L"Settings.cfg";InitializeProfiles();
 auto old=Fixture();auto absent=old;absent.part="leg";absent.relativePath="pl\\f_equip\\pl105_0000\\leg\\mod\\f_leg105_0000";
 s.models={old,absent};auto& original=Main(old.equip);SetVisible(original,{old.relativePath,0,true},false);SetEquipmentHeight(old.equip,{true,100});check(SavePreset(original,"旧模型隐藏")&&Save(),"save original fixture");
 const auto path=s.directory/L"Presets/pl105_0000.wdp";const auto before=Read(path);const auto modified=fs::last_write_time(path);
 s.models.resize(1);for(int i=0;i<3;i++){auto name="NewMaterial"+std::to_string(i);s.models[0].materials[i]={name,MaterialNameHash(name),i};}s.models[0].signature=MeshOrderSignature(s.models[0]);
 check(!LoadEquipmentProfile(old.equip),"replacement model reports the mismatch");
 s.trackingPlayer=true;s.outfit.ready=true;s.outfit.paths[1]=old.relativePath;
 check(!LoadEquipmentProfile(old.equip),"load mismatch with a tracked outfit");FollowPlayerOutfit({});
 check(Save()&&Read(path)==before,"leaving the actor without edits must not overwrite the old file");
 s.trackingPlayer=false;s.outfitChanging=false;
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.DisplaySize={1920,1080};
 ImFontGlyphRangesBuilder ranges;ranges.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());ranges.AddText("编辑按钮模拟更换外观预设不匹配保存备份");ImVector<ImWchar> glyphs;ranges.BuildRanges(&glyphs);
 check(fonts::LoadPanelFonts(*io.Fonts,glyphs.Data),"load real panel fonts");unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 auto& style=ImGui::GetStyle();style.FramePadding={9,6};style.ItemSpacing={10,8};style.ChildRounding=8;
 s.data.theme=UiTheme::Basic;s.data.hideDragHint=true;ui::tab=0;
 ImVec2 panelSize{1460,850};
 auto draw=[&]{qa::widgets.clear();ImGui::NewFrame();ImGui::SetWindowPos("###WorldDiffInternal",{20,20});ImGui::SetWindowSize("###WorldDiffInternal",panelSize);ui::DrawPanel();ImGui::Render();};
 auto settle=[&]{for(int i=0;i<3;i++)draw();};
 auto capture=[&](const char* name){if(argc>1){auto folder=std::string(argv[1])+"/"+name;SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",folder.c_str());SaveCurrentFrame();SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",nullptr);}};
 auto click=[&](const char* label){if(!qa::widgets.contains(label)||qa::widgets.at(label).disabled)throw std::runtime_error(std::string("missing or disabled widget: ")+label);auto p=qa::widgets.at(label).rect.GetCenter();io.AddMousePosEvent(p.x,p.y);io.AddMouseButtonEvent(0,true);draw();io.AddMouseButtonEvent(0,false);draw();draw();};
 settle();capture("mismatch");
 std::cout<<"Reproduction: editable="<<CanEditScope(old.equip)<<", parts-visible="<<qa::widgets.contains("全隐藏")<<", export-disabled="<<qa::widgets.at("导出预设").disabled<<'\n';
 check(CanEditScope(old.equip)&&qa::widgets.contains("全隐藏")&&!qa::widgets.at("导出预设").disabled,"mismatch must keep parts and preset editing available");
 check(Save()&&Read(path)==before&&fs::last_write_time(path)==modified,"viewing or saving unchanged draft never rewrites old WDP");
 check(!Hidden(old.relativePath,0,s.models[0].materials[0].hash)&&Main(old.equip).presets.empty()&&GetEquipmentHeight(old.equip)==EquipmentHeight{},"old mesh indices, presets and height do not control replacement model");
 click("全隐藏");check(Hidden(old.relativePath,0,s.models[0].materials[0].hash),"actual parts button controls replacement model");
 s.data.theme=UiTheme::ClassicWhite;strcpy_s(ui::presetName,"新外观隐藏");settle();click("保存新预设");
 check(Main(old.equip).presets.size()==1,"actual save button creates a matching preset");
 check(Save()&&Read(path)!=before&&LoadEquipmentProfile(old.equip),"new preset saves and reloads as an exact match");
 check(Hidden(old.relativePath,0,s.models[0].materials[0].hash)&&Main(old.equip).presets[0].name=="新外观隐藏","new visibility and preset survive reload");
 bool backed=false;for(auto& name:ProfileBackups(old.equip))if(Read(s.directory/L"Backups"/PathUtf8(old.equip)/PathUtf8(name))==before)backed=true;
 check(backed,"original mismatched WDP is backed up byte for byte");
 const auto matching=Read(path);
 check(matching.find("OldMaterial")==std::string::npos&&matching.find("f_leg105_0000")==std::string::npos,"new profile excludes stale identities from unworn old model parts");
 const auto current=s.models[0];
 for(auto theme:{UiTheme::Basic,UiTheme::ClassicWhite,UiTheme::ClassicBlack,UiTheme::Glass}){
  Write(path,before);check(!LoadEquipmentProfile(old.equip)&&CanEditScope(old.equip),"all themes start with editable mismatch");s.data.theme=theme;ui::tab=0;settle();
  bool list=false;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"材质滚动区"))list=true;
  check(list,"actual material list remains present in each theme");capture(theme==UiTheme::Basic?"basic":theme==UiTheme::ClassicWhite?"white":theme==UiTheme::ClassicBlack?"black":"glass");
  io.DisplaySize={1280,720};panelSize={1050,620};settle();
  capture(theme==UiTheme::Basic?"basic-small":theme==UiTheme::ClassicWhite?"white-small":theme==UiTheme::ClassicBlack?"black-small":"glass-small");
  check(qa::widgets.at("全隐藏").visible&&qa::widgets.at("重新匹配当前文件").visible,"compact layout keeps warning actions and parts visible");
  ImGuiWindow* pageWindow=nullptr;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"材质滚动区")){check(window->Size.y>=119,"compact material viewport cannot collapse");pageWindow=window->ParentWindow;}
  check(pageWindow!=nullptr,"material list has a scrollable page");ImGui::SetScrollY(pageWindow,pageWindow->ScrollMax.y);settle();
  check(qa::widgets.at("陷地修复").visible,"compact page scroll reaches height footer");ImGui::SetScrollY(pageWindow,0);settle();
  io.DisplaySize={1920,1080};panelSize={1460,850};settle();
  ui::tab=1;settle();check(qa::widgets.contains("启用变身功能")&&!qa::widgets.at("启用变身功能").disabled,"transform page accepts editable scope");
  ui::tab=2;settle();check(qa::widgets.contains("新建方案"),"hotkey page remains accessible");
  check(Save()&&Read(path)==before&&s.mismatchedScopes.count(old.equip),"viewing all three pages leaves mismatched WDP untouched");
  ui::tab=0;settle();click("全隐藏");check(Hidden(old.relativePath,2,current.materials[2].hash),"each theme edits actual new mesh visibility");
 }
 // Failed writes must retain the editable draft and the original file.
 auto backupDir=s.directory/L"Backups"/PathUtf8(old.equip),held=s.directory/L"Backups/held";
 fs::rename(backupDir,held);Write(backupDir,"block directory creation");
 check(!Save()&&Read(path)==before&&s.mismatchedScopes.count(old.equip)&&s.dirty&&Hidden(old.relativePath,0,current.materials[0].hash),"backup failure preserves original plus unsaved draft even after an earlier successful backup");
 fs::remove(backupDir);fs::rename(held,backupDir);
 auto temp=path;temp+=L".tmp";fs::create_directory(temp);
 check(!Save()&&Read(path)==before&&s.mismatchedScopes.count(old.equip)&&s.dirty,"atomic save failure keeps mismatch and draft");fs::remove(temp);
 Write(path,before+"\n");s.data.autoFind=true;s.outfit.paths[1]=old.relativePath;s.profileAttempts.clear();TickProfiles();
 check(Hidden(old.relativePath,0,current.materials[0].hash)&&!Save()&&Read(path)==before+"\n","external edit and auto reload cannot overwrite or discard draft");
 check(ExportProfile(old.equip,"新模型导出")&&Read(path)==before+"\n","draft can be exported even when canonical save conflicts");
 check(Read(s.directory/L"Templates/新模型导出.wdp").find("OldMaterial")==std::string::npos,"draft export uses current identities only");
 Write(path,before);check(Save()&&!s.mismatchedScopes.count(old.equip),"retry commits once backup and file conflicts are resolved");
 // Import failure must not lose draft; successful import and reload clear warning.
 Write(path,before);check(!LoadEquipmentProfile(old.equip),"reset mismatch for template import");
 SetVisible(Main(old.equip),{old.relativePath,1,true},false);fs::create_directory(temp);
 check(!ApplyProfile(old.equip,"新模型导出.wdp")&&s.mismatchedScopes.count(old.equip)&&Hidden(old.relativePath,1,current.materials[1].hash)&&Read(path)==before,"failed import restores mismatch draft and original file");fs::remove(temp);
 check(ApplyProfile(old.equip,"新模型导出.wdp")&&!s.mismatchedScopes.count(old.equip)&&LoadEquipmentProfile(old.equip),"matching template import resolves warning");
 Write(path,before);check(!LoadEquipmentProfile(old.equip),"reset mismatch for backup restore");Write(backupDir/L"matching.wdp",matching);fs::create_directory(temp);
 check(!RestoreProfileBackup(old.equip,"matching.wdp")&&s.mismatchedScopes.count(old.equip)&&CanEditScope(old.equip)&&Read(path)==before,"failed backup restore preserves editable mismatch state");fs::remove(temp);
 check(RestoreProfileBackup(old.equip,"matching.wdp")&&!s.mismatchedScopes.count(old.equip)&&Read(path).find("f_leg105_0000")==std::string::npos&&LoadEquipmentProfile(old.equip),"matching backup restores with fresh current model identity");
 Write(path,before);check(!LoadEquipmentProfile(old.equip),"reset mismatch for authored groups");uint64_t newGroup=0;
 check(CreateGroup("新部位",old.equip,false,{{old.relativePath,current.materials[1].hash,false}},newGroup),"create group from current materials during mismatch");
 SetVisible(*Find(newGroup),{old.relativePath,1,true},false);check(SavePreset(*Find(newGroup),"新部位隐藏"),"save new group preset during mismatch");
 auto hotkey=CreateHotkey(old.equip,"新外观方案");check(hotkey&&SetHotkeyTarget(hotkey,0,{newGroup,Find(newGroup)->selected})&&SetHotkeyBinding(hotkey,'L',0),"new preset can be assigned to a hotkey during mismatch");
 check(AddHealthNode(old.equip)!=0&&SetMeshNote(current,1,"新模型备注"),"conditions and mesh notes can be authored during mismatch");SetEquipmentHeight(old.equip,{true,25});
 check(Save()&&LoadEquipmentProfile(old.equip)&&Find(newGroup)&&FindHotkey(hotkey)&&GetEquipmentHeight(old.equip)==EquipmentHeight{true,25}&&MeshView(current,1),"groups, hotkeys, conditions, notes and height persist with new identity");
 Write(path,before);s.trackingPlayer=true;s.outfit.ready=true;s.outfit.paths[1]=old.relativePath;
 check(!LoadEquipmentProfile(old.equip),"load mismatch before actor transition");SetVisible(Main(old.equip),{old.relativePath,2,true},false);FollowPlayerOutfit({});
 check(Save()&&Read(path).find("NewMaterial")!=std::string::npos,"flush pending edit with cached identity after actor disappears");
 Document peer;std::string revision;check(ReadPeerProfile(old.equip,{&current},peer,revision),"transition save retains verifiable model identity for read-only matching");
 s.trackingPlayer=false;s.outfitChanging=false;
 // Explicit rebind can salvage partial matches without applying unrelated rows.
 Write(path,before);s.models[0].materials[0]=old.materials[0];check(!LoadEquipmentProfile(old.equip),"partial material match also opens a draft");
 check(RebindEquipmentProfile(old.equip)&&!s.mismatchedScopes.count(old.equip)&&Hidden(old.relativePath,0,old.materials[0].hash),"manual rebind retains valid old rules for partial matches");
 s.models={old,absent};Write(path,before);check(LoadEquipmentProfile(old.equip)&&!s.mismatchedScopes.count(old.equip)&&GetEquipmentHeight(old.equip)==EquipmentHeight{true,100},"returning to original model restores its original configuration");
 Write(path,"invalid WDP");check(!LoadEquipmentProfile(old.equip)&&s.blockedScopes.count(old.equip)&&!s.mismatchedScopes.count(old.equip)&&!CanEditScope(old.equip)&&Save()&&Read(path)=="invalid WDP","corrupt files remain protected instead of being silently replaced");
 s.models={current};Write(path,before);check(!LoadEquipmentProfile(old.equip)&&CanEditScope(old.equip)&&!s.blockedScopes.count(old.equip),"replacing corrupt file with a valid mismatched file reopens editing");
 fs::remove(path);check(LoadEquipmentProfile(old.equip)&&CanEditScope(old.equip)&&!s.mismatchedScopes.count(old.equip)&&Save()&&!fs::exists(path),"removed unused old WDP becomes ordinary empty equipment without creating a file");
 ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" mismatch storage and real-panel checks\nArtifacts "<<s.directory.string()<<'\n';return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
