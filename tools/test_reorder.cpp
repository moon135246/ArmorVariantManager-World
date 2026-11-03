#include "imgui.h"
#include "imgui_internal.h"
#include <fstream>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <string>
namespace reorderqa {
struct Row {ImGuiID id;ImRect bounds;};std::vector<Row> groups,presets,meshes;
}
namespace ImGui {
bool TrackedSelectable(const char* label,bool selected=false,ImGuiSelectableFlags flags=0,const ImVec2& size=ImVec2(0,0)){
 bool result=Selectable(label,selected,flags,size);auto* window=GetCurrentWindow();reorderqa::Row row{GetItemID(),ImRect(GetItemRectMin(),GetItemRectMax())};
 if(std::strstr(window->Name,"/分组列表_"))reorderqa::groups.push_back(row);
 else if(std::strstr(window->Name,"/预设列表_"))reorderqa::presets.push_back(row);
 else if(std::strstr(window->Name,"/材质滚动区_"))reorderqa::meshes.push_back(row);return result;
}
}
#define Selectable TrackedSelectable
#include "../src/Panel.cpp"
#undef Selectable
namespace worlddiff::overlay {
void NativeInputClose(){}
void SetVisible(bool){}const char* BackendStatus(){return "reorder verifier";}
bool TextField(const char* label,const char* hint,char* value,size_t capacity){return ImGui::InputTextWithHint(label,hint,value,capacity);}
}
void ExportFrame(const std::filesystem::path& folder){
 std::filesystem::create_directories(folder);unsigned char* atlas;int w,h;ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas,&w,&h);
 {std::ofstream rgba(folder/L"atlas.rgba",std::ios::binary);rgba.write((const char*)atlas,(std::streamsize)w*h*4);}
 std::ofstream output(folder/L"drag_preview.json");auto* data=ImGui::GetDrawData();output<<"{\"width\":"<<data->DisplaySize.x<<",\"height\":"<<data->DisplaySize.y<<",\"atlas_width\":"<<w<<",\"atlas_height\":"<<h<<",\"lists\":[";
 for(int i=0;i<data->CmdListsCount;i++){if(i)output<<',';auto* list=data->CmdLists[i];output<<"{\"vertices\":[";
  for(int n=0;n<list->VtxBuffer.Size;n++){if(n)output<<',';auto& v=list->VtxBuffer[n];output<<'['<<v.pos.x<<','<<v.pos.y<<','<<v.uv.x<<','<<v.uv.y<<','<<v.col<<']';}output<<"],\"indices\":[";
  for(int n=0;n<list->IdxBuffer.Size;n++){if(n)output<<',';output<<list->IdxBuffer[n];}output<<"],\"commands\":[";
  for(int n=0;n<list->CmdBuffer.Size;n++){if(n)output<<',';auto& c=list->CmdBuffer[n];output<<'['<<c.ElemCount<<','<<c.IdxOffset<<','<<c.VtxOffset<<','<<c.ClipRect.x<<','<<c.ClipRect.y<<','<<c.ClipRect.z<<','<<c.ClipRect.w<<']';}output<<"]}";
 }output<<"]}";
}
#include "../src/EmbeddedFonts.h"
int main(int argc,char** argv){try{
 using namespace worlddiff;using namespace worlddiff::app;namespace ui=worlddiff::overlay;
 unsigned checks=0;auto check=[&](bool good,const char* reason){++checks;if(!good)throw std::runtime_error(reason);};
 auto& s=Get();s.loaded=true;s.data.hideDragHint=true;s.data.scale=1;s.data.autoDefault=true;std::string scope="pl105_0000";
 auto mainId=Main(scope).id;std::vector<uint64_t> groupIds;for(auto name:{"衣服","手套","袖子","黑丝","鞋子","身材"}){Group g;g.id=s.data.nextId++;g.scope=scope;g.name=name;g.global=g.name=="身材";groupIds.push_back(g.id);s.data.groups.push_back(g);}
 uint64_t globalId=groupIds.back();Group foreign;foreign.id=s.data.nextId++;foreign.scope="pl106_0000";foreign.name="其他装备";auto foreignId=foreign.id;s.data.groups.insert(s.data.groups.begin()+3,foreign);
 for(auto name:{"方案一","方案二","方案三","方案四"}){Preset p;p.id=s.data.nextId++;p.name=name;Find(globalId)->presets.push_back(p);}auto* global=Find(globalId);global->selected=global->defaultPreset=global->presets[0].id;
 Model model;model.equip=scope;model.part="arm";model.display="网格拖动验证";model.relativePath="pl\\f_equip\\pl105_0000\\arm\\mod\\f_arm105_0000";model.signature=123;
 for(int i=0;i<6;i++){Material material;material.name="网格"+std::to_string(i);material.hash=MaterialNameHash(material.name);material.mrlIndex=i;model.materials.push_back(material);Mesh mesh;mesh.index=(uint32_t)i;mesh.materialId=(uint16_t)i;model.meshes.push_back(mesh);}s.models.push_back(model);Find(globalId)->presets[1].rules.hidden.insert({model.relativePath,2,true});
 auto groupOrder=[&]{std::vector<uint64_t> ids;for(auto& g:s.data.groups)if(g.scope==scope&&!g.main)ids.push_back(g.id);return ids;};
 auto presetOrder=[&]{std::vector<uint64_t> ids;for(auto& p:Find(globalId)->presets)ids.push_back(p.id);return ids;};
 check(!MoveGroupBefore(mainId,globalId),"main list pinned");check(!MoveGroupBefore(globalId,foreignId),"groups cannot cross equipment scopes");
 check(MoveGroupBefore(globalId,groupIds[0]),"global group participates in insertion");check(groupOrder().front()==globalId,"global group moved before ordinary group");check(Find(globalId)->global,"global flag retained");
 MoveGroupBefore(globalId);check(groupOrder()==groupIds,"global group can return to last position");check(Find(foreignId)->scope=="pl106_0000","unrelated equipment preserved");
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.DisplaySize={760,850};if(!worlddiff::fonts::LoadPanelFonts(*io.Fonts,io.Fonts->GetGlyphRangesChineseSimplifiedCommon()))return 1;unsigned char* pixels;int fontWidth,fontHeight;io.Fonts->GetTexDataAsRGBA32(&pixels,&fontWidth,&fontHeight);
 auto& style=ImGui::GetStyle();style.WindowPadding={18,14};style.FramePadding={9,6};style.ItemSpacing={10,8};style.WindowRounding=12;style.ChildRounding=8;style.FrameRounding=6;style.ScrollbarSize=14;
 style.Colors[ImGuiCol_WindowBg]={.078f,.094f,.125f,1};style.Colors[ImGuiCol_ChildBg]={.122f,.149f,.188f,1};style.Colors[ImGuiCol_Border]={.24f,.29f,.36f,.8f};style.Colors[ImGuiCol_Button]={.13f,.30f,.35f,1};style.Colors[ImGuiCol_Text]={.945f,.957f,.972f,1};
 ui::outfit=scope;ui::groupId=mainId;ui::part=2;ui::creating=false;ui::meshMode=true;
 int page=0;float paneHeight=750;
 auto draw=[&]{reorderqa::groups.clear();reorderqa::presets.clear();reorderqa::meshes.clear();ImGui::NewFrame();ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({700,810});ImGui::Begin("Drag QA",nullptr,ImGuiWindowFlags_NoDecoration);
  if(page==0)ui::GroupsPane(430,paneHeight);else if(page==1)ui::PresetsPane(430,paneHeight);else ui::TargetsPanel();ImGui::End();ImGui::Render();
 };
 auto settle=[&](int frames=20){for(int i=0;i<frames;i++)draw();};
 auto press=[&](ImVec2 pos){io.AddMousePosEvent(pos.x,pos.y);draw();io.AddMouseButtonEvent(0,true);draw();io.AddMousePosEvent(pos.x+10,pos.y-10);draw();};
 auto hover=[&](ImVec2 pos,int frames=3){io.AddMousePosEvent(pos.x,pos.y);settle(frames);};
 auto release=[&]{io.AddMouseButtonEvent(0,false);draw();settle();};
 settle();s.dirty=false;auto original=groupOrder();auto selection=ui::groupId;
 float sleeveY=reorderqa::groups[3].bounds.Min.y;auto from=reorderqa::groups[2].bounds.GetCenter(),to=reorderqa::groups[5].bounds.GetCenter();to.y=reorderqa::groups[5].bounds.Min.y+5;press(from);hover(to,3);
 check(groupOrder()==original&&!s.dirty,"hover preview never persists intermediate group order");check(ui::reorderDrag.list!=0,"actual group widget starts shared drag");check(ui::reorderDrag.placeholderMax.x-ui::reorderDrag.placeholderMin.x>300,"placeholder spans list width");
 check(ui::reorderDrag.cardMax.x>ui::reorderDrag.cardMin.x,"floating card is rendered");check(reorderqa::groups[3].bounds.Min.y<sleeveY-1&&reorderqa::groups[3].bounds.Min.y>sleeveY-47,"neighbor rows animate between original and insertion positions");check(ui::reorderDrag.cardMax.y<ui::reorderDrag.placeholderMin.y+12,"floating card leaves drop label visible");
 if(argc>1){settle(16);ExportFrame(PathUtf8(argv[1]));}
 release();auto expected=groupIds;expected.erase(expected.begin()+1);expected.insert(expected.begin()+3,groupIds[1]);check(groupOrder()==expected,"ordinary group inserts instead of swapping");check(ui::groupId==selection,"dragging unselected group does not select it");
 settle();original=groupOrder();auto globalPosition=std::find(original.begin(),original.end(),globalId)-original.begin();from=reorderqa::groups[(size_t)globalPosition+1].bounds.GetCenter();to=reorderqa::groups[1].bounds.GetCenter();to.y=reorderqa::groups[1].bounds.Min.y+5;
 press(from);hover(to);release();check(groupOrder().front()==globalId,"actual global group drag works");check(Find(globalId)->global&&ui::groupId==selection,"global dragging preserves type and selection");
 original=groupOrder();s.dirty=false;from=reorderqa::groups[1].bounds.GetCenter();press(from);hover({745,800});release();check(groupOrder()==original&&!s.dirty,"outside drop cancels without saving");
 from=reorderqa::groups[1].bounds.GetCenter();to=reorderqa::groups[4].bounds.GetCenter();press(from);hover(to);io.AddKeyEvent(ImGuiKey_Escape,true);draw();io.AddKeyEvent(ImGuiKey_Escape,false);draw();release();check(groupOrder()==original&&!s.dirty,"Escape cancels without saving");
 page=1;ui::groupId=globalId;settle();auto beforePresets=presetOrder();auto selected=Find(globalId)->selected,defaultPreset=Find(globalId)->defaultPreset;auto working=Find(globalId)->working;
 from=reorderqa::presets[1].bounds.GetCenter();to=reorderqa::presets.back().bounds.GetCenter();to.y=reorderqa::presets.back().bounds.Max.y-6;press(from);hover(to);check(presetOrder()==beforePresets,"global preset preview is transient");release();
 auto desired=beforePresets;desired.erase(desired.begin()+1);desired.push_back(beforePresets[1]);check(presetOrder()==desired,"global presets use insert-to-end");
 check(Find(globalId)->selected==selected&&Find(globalId)->defaultPreset==defaultPreset,"preset drag does not apply or set a new default");check(Find(globalId)->working.hidden==working.hidden&&Find(globalId)->working.shown==working.shown,"preset drag preserves working visibility");
 auto ordinaryId=groupIds[0];for(auto name:{"普通一","普通二","普通三"}){Preset p;p.id=s.data.nextId++;p.name=name;Find(ordinaryId)->presets.push_back(p);}Find(ordinaryId)->selected=Find(ordinaryId)->defaultPreset=Find(ordinaryId)->presets[0].id;
 ui::groupId=ordinaryId;settle();auto ordinarySelected=Find(ordinaryId)->selected;std::vector<uint64_t> ordinaryBefore;for(auto& p:Find(ordinaryId)->presets)ordinaryBefore.push_back(p.id);
 from=reorderqa::presets[2].bounds.GetCenter();to=reorderqa::presets[0].bounds.GetCenter();to.y=reorderqa::presets[0].bounds.Min.y+5;press(from);hover(to);release();
 check(Find(ordinaryId)->presets.front().id==ordinaryBefore[2],"ordinary presets use the same insertion effect");check(Find(ordinaryId)->selected==ordinarySelected&&Find(ordinaryId)->defaultPreset==ordinarySelected,"ordinary preset dragging does not select or change default");
 check(!MovePresetBefore(*Find(ordinaryId),ordinaryBefore[1],beforePresets[0]),"presets cannot move between groups");
 page=2;ui::groupId=mainId;settle();SetMeshNote(s.models[0],4,"鞋子备注");auto beforeNote=MeshView(s.models[0],4)->note;s.dirty=false;
 from=reorderqa::meshes[4].bounds.GetCenter();to=reorderqa::meshes[0].bounds.GetCenter();to.y=reorderqa::meshes[0].bounds.Min.y+3;press(from);hover(to);check(OrderedMeshes(s.models[0]).front()->index==0&&!s.dirty,"mesh preview does not persist before release");release();
 check(OrderedMeshes(s.models[0]).front()->index==4,"mesh uses shared insertion effect");check(MeshView(s.models[0],4)->note==beforeNote,"mesh drag preserves note identity");
 s.dirty=false;std::ostringstream beforeIdentity;WriteDocument(beforeIdentity,s.data);from=reorderqa::meshes[1].bounds.GetCenter();press(from);s.models[0].signature++;draw();release();std::ostringstream afterIdentity;WriteDocument(afterIdentity,s.data);check(!s.dirty&&beforeIdentity.str()==afterIdentity.str(),"changing model identity cancels an existing drag");s.models[0].signature--;settle();
 float topY=reorderqa::meshes.front().bounds.Min.y;strcpy_s(ui::search,"网格5");draw();check(reorderqa::meshes.size()==1&&std::abs(reorderqa::meshes.front().bounds.Min.y-topY)<1,"filter changes do not animate results from offscreen positions");ui::search[0]=0;settle();
 // The actual scroll child must keep scrolling while the source row moves out
 // of the viewport. No intermediate data writes are allowed.
 page=0;ui::groupId=mainId;for(int i=0;i<35;i++){Group g;g.id=s.data.nextId++;g.scope=scope;g.name="更多分组 "+std::to_string(i);s.data.groups.push_back(g);}paneHeight=600;settle();original=groupOrder();s.dirty=false;
 from=reorderqa::groups[1].bounds.GetCenter();press(from);ImGuiWindow* list=nullptr;for(auto* window:ImGui::GetCurrentContext()->Windows)if(window->Active&&std::strstr(window->Name,"/分组列表_"))list=window;
 check(list!=nullptr,"actual group scroll child found");hover({list->InnerClipRect.GetCenter().x,list->InnerClipRect.Max.y-3},90);if(list->Scroll.y<=40)std::cerr<<"scroll="<<list->Scroll.y<<" max="<<list->ScrollMax.y<<" clip="<<list->InnerClipRect.Min.y<<','<<list->InnerClipRect.Max.y<<" mouse="<<io.MousePos.y<<" drag="<<ui::reorderDrag.list<<" cancel="<<ui::reorderDrag.cancelled<<" active="<<ImGui::GetCurrentContext()->ActiveId<<'\n';check(list->Scroll.y>40,"drag near viewport edge auto-scrolls");check(groupOrder()==original&&!s.dirty,"auto-scroll preview does not write order");hover({745,800});release();check(groupOrder()==original,"auto-scroll drag can cancel");
 std::ostringstream serialized;WriteDocument(serialized,s.data);Document restored;std::istringstream saved(serialized.str());check(ReadDocument(saved,restored),"reordered document remains readable");
 std::vector<uint64_t> restoredGroups;for(auto& g:restored.groups)if(g.scope==scope&&!g.main)restoredGroups.push_back(g.id);check(restoredGroups==groupOrder(),"group insertion order survives persistence");
 auto restoredGlobal=std::find_if(restored.groups.begin(),restored.groups.end(),[&](auto& g){return g.id==globalId;});check(restoredGlobal!=restored.groups.end()&&restoredGlobal->presets.front().id==beforePresets[0]&&restoredGlobal->presets.back().id==beforePresets[1],"global preset order survives persistence");
 std::cout<<"PASS "<<checks<<" insertion drag checks: global groups/presets, transient placeholder, floating card, cancellation, auto-scroll, mesh notes and unchanged selection.\n";ImGui::DestroyContext();return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
