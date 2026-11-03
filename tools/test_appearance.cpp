#include "../src/Panel.cpp"
#include "../src/NativeInput.cpp"
#include "save_imgui_frame.inl"
#include "../src/EmbeddedFonts.h"
#include <iostream>
#include <stdexcept>
namespace worlddiff {void RuntimeLog(const char*,...){} }
namespace worlddiff::overlay {void SetVisible(bool){}const char* BackendStatus(){return "appearance verifier";} }
using namespace worlddiff;using namespace worlddiff::app;namespace ui=worlddiff::overlay;
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool good,const char* reason){checks++;if(!good)throw std::runtime_error(reason);};
 auto pump=[](){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}};
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,500};io.DeltaTime=1.f/60;if(!worlddiff::fonts::LoadPanelFonts(*io.Fonts,io.Fonts->GetGlyphRangesChineseSimplifiedCommon()))return 1;unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 auto& style=ImGui::GetStyle();style.WindowRounding=12;style.FrameRounding=6;style.FramePadding={9,6};style.ItemSpacing={10,8};style.Colors[ImGuiCol_WindowBg]={.122f,.149f,.188f,1};style.Colors[ImGuiCol_FrameBg]={.145f,.18f,.227f,1};style.Colors[ImGuiCol_CheckMark]={.33f,.72f,.78f,1};
 Model m;m.equip="pl105_0000";m.relativePath="pl\\f_equip\\pl105_0000\\body\\mod\\f_body105_0000";Target target{m.relativePath,0,true};ImRect row,checkbox,field;ImGuiID fieldId=0;char text[192]{};
 auto draw=[&](bool mesh){ImGui::NewFrame();ImGui::SetNextWindowPos({20,20});ImGui::SetNextWindowSize({600,260});ImGui::Begin("外观检查###Appearance",nullptr,ImGuiWindowFlags_NoSavedSettings);ImGui::PushFont(nullptr,22*Get().data.scale);ui::NativeInputBeginFrame();
  if(mesh){ImGui::TextUnformatted("身体 · f_body105_0000");bool visible=true;ImGui::Checkbox("##可见",&visible);checkbox={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};ImGui::SameLine();ui::MeshNameRow(m,target,"Meira_3_Antenna",true,nullptr);row={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};}
  ImGui::SetNextItemWidth(240*Get().data.scale);ui::TextField("##appearance_input","新预设名称",text,sizeof(text));field={ImGui::GetItemRectMin(),ImGui::GetItemRectMax()};fieldId=ImGui::GetItemID();ui::NativeInputEndFrame();ImGui::PopFont();ImGui::End();ImGui::Render();
 };
 auto exportFrame=[&](const char* name){if(argc>1){auto folder=PathUtf8(argv[1])/PathUtf8(name);auto value=Utf8(folder.wstring());SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",value.c_str());SaveCurrentFrame();SetEnvironmentVariableA("WORLDDIFF_QA_PREVIEW",nullptr);}};
 Get().data.scale=1;draw(true);draw(true);auto rowWidth=row.GetWidth();check(rowWidth>300,"mesh label hover spans the available row, beyond just its text");
 check(row.Min.y==checkbox.Min.y&&row.Max.y==checkbox.Max.y,"rounded hover is vertically aligned with its visibility checkbox");
 io.AddMousePosEvent(row.GetCenter().x,row.GetCenter().y);draw(true);draw(true);size_t hoverVertices=0;
 for(auto* list:ImGui::GetDrawData()->CmdLists)for(auto& vertex:list->VtxBuffer)if(vertex.col==IM_COL32(38,64,76,160))hoverVertices++;
 check(hoverVertices>8,"mesh hover is a rounded tessellated surface instead of a rectangular selectable fill");exportFrame("mesh_hover");
 io.AddMousePosEvent(-100,-100);draw(false);draw(false);exportFrame("input_idle");
 // Exercise the actual dedicated native editor while keeping its popup hidden;
 // this fixture never focuses a window or sends input to the user's game.
 HWND testOwner=CreateWindowExW(0,L"STATIC",L"WorldDiff appearance owner",WS_POPUP,100,100,900,500,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);check(testOwner!=nullptr,"create hidden native owner fixture");ui::NativeInputInitialize(testOwner);
 for(int i=0;i<200&&!ui::edit;i++){pump();Sleep(5);}check(ui::edit&&ui::host,"real native editor thread initialized");check(!(GetWindowLongPtrW(ui::edit,GWL_EXSTYLE)&WS_EX_CLIENTEDGE)&&!(GetWindowLongPtrW(ui::edit,GWL_STYLE)&WS_BORDER),"native editor has no system rectangular border");
 check(ImmGetContext(ui::edit)!=nullptr,"dedicated IME context remains available");
 ui::shown=true;{std::lock_guard lock(ui::mutex);ui::session.id=fieldId;ui::session.value="圆角中文输入";ui::session.initial="初始名称";ui::session.capacity=sizeof(text);}
 for(float scale:{.7f,1.f,1.5f}){
  Get().data.scale=scale;draw(false);PostMessageW(ui::host,ui::kUpdate,0,0);RECT bounds{};POINT origin{(LONG)field.Min.x,(LONG)field.Min.y};ClientToScreen(testOwner,&origin);
  // Updates normally enter the dedicated UI thread's message queue. Sending
  // synchronously can re-enter an older pending update and observe it midway.
  for(int i=0;i<100;i++){
   pump();GetWindowRect(ui::host,&bounds);HRGN clip=CreateRectRgn(0,0,0,0);int kind=GetWindowRgn(ui::host,clip);RECT clipBounds{};GetRgnBox(clip,&clipBounds);
   bool ready=kind!=ERROR&&!PtInRegion(clip,0,0)&&clipBounds.right>=bounds.right-bounds.left&&clipBounds.right<=bounds.right-bounds.left+1&&clipBounds.bottom>=bounds.bottom-bounds.top&&clipBounds.bottom<=bounds.bottom-bounds.top+1;DeleteObject(clip);
   if(ready&&bounds.left==origin.x+1&&bounds.top==origin.y+1&&bounds.right-bounds.left==(LONG)field.GetWidth()-2&&bounds.bottom-bounds.top==(LONG)field.GetHeight()-2)break;Sleep(5);
  }
  if(bounds.right-bounds.left!=(LONG)field.GetWidth()-2||bounds.bottom-bounds.top!=(LONG)field.GetHeight()-2)std::cerr<<"geometry scale="<<scale<<" field="<<field.GetWidth()<<'x'<<field.GetHeight()<<" native="<<bounds.right-bounds.left<<'x'<<bounds.bottom-bounds.top<<'\n';
  check(bounds.left==origin.x+1&&bounds.top==origin.y+1,"active native surface stays inside the original field outline");check(bounds.right-bounds.left==(LONG)field.GetWidth()-2&&bounds.bottom-bounds.top==(LONG)field.GetHeight()-2,"active field keeps the original width and height at each UI scale");
  HRGN region=CreateRectRgn(0,0,0,0);check(GetWindowRgn(ui::host,region)!=ERROR,"native surface has a window clipping region");check(!PtInRegion(region,0,0)&&PtInRegion(region,(bounds.right-bounds.left)/2,(bounds.bottom-bounds.top)/2),"active native surface clips the corners while preserving its center");DeleteObject(region);
  check(!IsWindowVisible(ui::host),"native test does not show or focus an interactive popup");
 }
 HDC editDC=GetDC(ui::edit);auto oldFont=SelectObject(editDC,ui::font);
 wchar_t family[LF_FACESIZE]{};GetTextFaceW(editDC,LF_FACESIZE,family);
 WORD glyphs[3]{};DWORD found=GetGlyphIndicesW(editDC,L"猎龍霞",3,glyphs,GGI_MARK_NONEXISTING_GLYPHS);
 check(wcscmp(family,fonts::Family)==0,"native editor selected embedded Noto Sans SC");
 check(found==3&&glyphs[0]!=0xffff&&glyphs[1]!=0xffff&&glyphs[2]!=0xffff,"embedded native font covers Chinese input");
 SelectObject(editDC,oldFont);ReleaseDC(ui::edit,editDC);
 Get().data.scale=1;draw(false);SendMessageW(ui::host,ui::kUpdate,0,0);exportFrame("input_active");
 check(std::string(text)=="圆角中文输入","Unicode native value reaches the real TextField buffer");SetWindowTextW(ui::edit,L"提交的中文名称");SendMessageW(ui::edit,WM_KEYDOWN,VK_RETURN,0);draw(false);check(std::string(text)=="提交的中文名称"&&!ui::shown,"Enter commits Unicode text and closes the native session");
 SendMessageW(ui::host,ui::kUpdate,0,0);check(!ui::shown&&!IsWindowVisible(ui::host),"queued geometry cannot reopen a committed input");
 ui::shown=true;{std::lock_guard lock(ui::mutex);ui::session.initial="初始名称";}SetWindowTextW(ui::edit,L"取消的修改");SendMessageW(ui::edit,WM_KEYDOWN,VK_ESCAPE,0);draw(false);check(std::string(text)=="初始名称"&&!ui::shown,"Escape restores the initial Unicode value");
 SendMessageW(ui::host,ui::kUpdate,0,0);check(!ui::shown&&!IsWindowVisible(ui::host),"queued geometry cannot reopen a cancelled input");
 PostThreadMessageW(ui::threadId.load(),WM_QUIT,0,0);DestroyWindow(testOwner);ImGui::DestroyContext();std::cout<<"PASS "<<checks<<" appearance checks: rounded hover, borderless native input, exact field geometry, scale, Unicode commit/cancel and IME context\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
