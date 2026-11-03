#include "NativeInput.h"
#include "EmbeddedFonts.h"
#include "Overlay.h"
#include "State.h"
#include "imgui.h"
#include <imm.h>
#include <objbase.h>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstring>
namespace worlddiff::overlay {
namespace {
constexpr UINT kUpdate=WM_APP+0x441,kClose=WM_APP+0x442;
std::mutex mutex;std::atomic<DWORD> threadId{0};std::atomic<bool> started{false},shown{false};
HWND owner=nullptr,host=nullptr,edit=nullptr;WNDPROC editOriginal=nullptr;HIMC ime=nullptr;HFONT font=nullptr;HBRUSH brush=nullptr;
struct Session {ImGuiID id=0;std::string value,initial;RECT screen{};size_t capacity=0;int height=22;float rounding=6,paddingX=9;COLORREF background=RGB(37,46,58),foreground=RGB(241,244,248);bool focus=false;};
Session session;bool frameSawActive=false;ImGuiID nativeId=0;
int fontHeight=0,regionWidth=0,regionHeight=0,regionRadius=-1;COLORREF background=RGB(37,46,58),foreground=RGB(241,244,248);
std::map<ImGuiID,std::string> lastValues;
std::wstring Wide(const std::string& text){return app::PathUtf8(text).wstring();}
std::string Limit(const std::string& text,size_t cap){size_t n=0;while(n<text.size()&&n<cap){unsigned char c=text[n];size_t bytes=c<128?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:4;if(n+bytes>cap||n+bytes>text.size())break;n+=bytes;}return text.substr(0,n);}
void Publish(){if(!edit)return;int length=GetWindowTextLengthW(edit);std::wstring text(length+1,L' ');GetWindowTextW(edit,text.data(),length+1);text.resize(length);auto value=app::Utf8(text);std::lock_guard lock(mutex);if(session.id==nativeId)session.value=Limit(value,session.capacity?session.capacity-1:0);}
void Hide(bool cancel){
 if(cancel){std::lock_guard lock(mutex);session.value=session.initial;}else Publish();
 {std::lock_guard lock(mutex);session.focus=false;}
 shown=false;if(host)ShowWindow(host,SW_HIDE);if(owner&&GetForegroundWindow()==host)SetForegroundWindow(owner);
}
LRESULT CALLBACK EditProc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
 if(message==WM_KEYDOWN){int menu;{std::lock_guard lock(app::Get().mutex);menu=app::Get().data.menuKey;}
  if(w==VK_RETURN){Hide(false);return 0;}if(w==VK_ESCAPE){Hide(true);return 0;}if(w==(WPARAM)menu){Hide(false);SetVisible(false);return 0;}
 }
 if(message==WM_KILLFOCUS){LRESULT result=CallWindowProcW(editOriginal,hwnd,message,w,l);Publish();shown=false;ShowWindow(host,SW_HIDE);return result;}
 return CallWindowProcW(editOriginal,hwnd,message,w,l);
}
LRESULT CALLBACK HostProc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
 if(message==kUpdate){Session request;{std::lock_guard lock(mutex);request=session;session.focus=false;}
  if(!edit)return 0;bool changed=request.id!=nativeId;nativeId=request.id;
  if(!shown&&!request.focus)return 0; // Ignore geometry messages queued before commit/cancel.
  if(changed||!shown){auto text=Wide(request.value);SetWindowTextW(edit,text.c_str());SendMessageW(edit,EM_LIMITTEXT,(WPARAM)std::min<size_t>(request.capacity,1000),0);SendMessageW(edit,EM_SETSEL,(WPARAM)-1,-1);}
  // The outer rounded frame belongs to ImGui in every state. The borderless
  // native surface occupies only its interior, leaving that exact outline
  // visible and avoiding Windows' rectangular client-edge focus decoration.
  int outerWidth=std::max(1L,request.screen.right-request.screen.left),outerHeight=std::max(1L,request.screen.bottom-request.screen.top);
  int inset=std::min(1,std::min((outerWidth-1)/2,(outerHeight-1)/2));
  int width=outerWidth-inset*2,height=outerHeight-inset*2;
  int radius=std::max(0,(int)std::lround(request.rounding-inset));radius=std::min(radius,std::min(width,height)/2);
  if(!brush||background!=request.background){if(brush)DeleteObject(brush);background=request.background;brush=CreateSolidBrush(background);}foreground=request.foreground;
  SetWindowPos(hwnd,HWND_TOP,request.screen.left+inset,request.screen.top+inset,width,height,SWP_NOACTIVATE);
  if(width!=regionWidth||height!=regionHeight||radius!=regionRadius){
   HRGN region=radius?CreateRoundRectRgn(0,0,width+1,height+1,radius*2,radius*2):CreateRectRgn(0,0,width,height);
   if(region&&!SetWindowRgn(hwnd,region,TRUE))DeleteObject(region);regionWidth=width;regionHeight=height;regionRadius=radius;
  }
  if(!font||fontHeight!=request.height){
   auto next=CreateFontW(-request.height,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,fonts::Family);
   if(next){auto previous=font;font=next;fontHeight=request.height;SendMessageW(edit,WM_SETFONT,(WPARAM)font,FALSE);if(previous)DeleteObject(previous);}
  }
  HDC dc=GetDC(edit);auto previous=SelectObject(dc,font);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);SelectObject(dc,previous);ReleaseDC(edit,dc);
  int textHeight=std::clamp((int)metrics.tmHeight,1,height),padding=std::clamp((int)std::lround(request.paddingX)-inset,0,(width-1)/2);
  MoveWindow(edit,padding,(height-textHeight)/2,width-padding*2,textHeight,FALSE);SendMessageW(edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
  InvalidateRect(hwnd,nullptr,FALSE);InvalidateRect(edit,nullptr,FALSE);
  if(request.focus||!shown){ShowWindow(hwnd,SW_SHOWNORMAL);shown=true;SetForegroundWindow(hwnd);SetFocus(edit);}
  return 0;
 }
 if(message==kClose){Hide(false);return 0;}
 if(message==WM_COMMAND&&HIWORD(w)==EN_CHANGE){Publish();return 0;}
 if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(hwnd,&paint);FillRect(dc,&paint.rcPaint,brush);EndPaint(hwnd,&paint);return 0;}
 if(message==WM_ERASEBKGND)return 1;
 if(message==WM_CTLCOLOREDIT){SetTextColor((HDC)w,foreground);SetBkColor((HDC)w,background);return (LRESULT)brush;}
 if(message==WM_MOUSEACTIVATE)return MA_ACTIVATE;
 return DefWindowProcW(hwnd,message,w,l);
}
DWORD WINAPI Thread(void*){
 auto regular = fonts::Read(WORLDDIFF_FONT_REGULAR);
 DWORD count = 0;
 HANDLE privateFont = regular.data ? AddFontMemResourceEx(regular.data, regular.size, nullptr, &count) : nullptr;
 if (!privateFont) RuntimeLog("Native editor could not register the embedded font");
 threadId=GetCurrentThreadId();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.lpfnWndProc=HostProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WorldDiff104NativeText";wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32513));RegisterClassW(&wc);brush=CreateSolidBrush(RGB(37,46,58));
 host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"WorldDiff 中文输入",WS_POPUP,0,0,200,30,owner,nullptr,wc.hInstance,nullptr);
 edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT,0,0,200,30,host,(HMENU)1,wc.hInstance,nullptr);
 if(edit){editOriginal=(WNDPROC)SetWindowLongPtrW(edit,GWLP_WNDPROC,(LONG_PTR)EditProc);ime=ImmCreateContext();ImmAssociateContext(edit,ime);}
 RuntimeLog("Native Unicode/IME editor initialized (dedicated Win32 EDIT context)");
 MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
 if(host)DestroyWindow(host);
 if(font){DeleteObject(font);font=nullptr;}
 if(privateFont)RemoveFontMemResourceEx(privateFont);
 return 0;
}
}
void NativeInputInitialize(HWND game){owner=game;if(started.exchange(true))return;HANDLE thread=CreateThread(nullptr,0,Thread,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
bool NativeInputThread(){return GetCurrentThreadId()==threadId.load();}
bool NativeInputActive(){return shown.load();}
bool NativeInputOwnWindow(HWND h){return h&&(h==host||h==edit);}
void NativeInputClose(){if(host)PostMessageW(host,kClose,0,0);}
void NativeInputBeginFrame(){frameSawActive=false;}
void NativeInputEndFrame(){if(shown&&!frameSawActive)NativeInputClose();}
bool TextField(const char* label,const char* hint,char* value,size_t capacity){
 if(!value||capacity<2)return false;ImGuiID id=ImGui::GetID(label);bool changed=false;bool active=false;
 {std::lock_guard lock(mutex);if(session.id==id){active=shown;if(!active&&lastValues.count(id)&&lastValues[id]!=value)session.value=value;auto text=Limit(session.value,capacity-1);if(text!=value){strcpy_s(value,capacity,text.c_str());changed=true;}if(active)frameSawActive=true;}}
 ImGui::PushStyleColor(ImGuiCol_NavCursor,ImVec4(0,0,0,0));
 ImGui::InputTextWithHint(label,hint?hint:"",value,capacity,ImGuiInputTextFlags_ReadOnly);ImGui::PopStyleColor();
 bool clicked=ImGui::IsItemClicked();bool keyboard=ImGui::IsItemFocused()&&ImGui::IsKeyPressed(ImGuiKey_Enter);
 auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();auto& style=ImGui::GetStyle();
 if(active||clicked||keyboard||ImGui::IsItemHovered()||ImGui::IsItemFocused()){
  auto color=style.Colors[ImGuiCol_CheckMark];color.w*=active||clicked||keyboard?.95f:.45f;
  ImGui::GetWindowDrawList()->AddRect(min,max,ImGui::GetColorU32(color),style.FrameRounding,0,1.f);
 }
 if(clicked||keyboard||active){POINT pt{(LONG)min.x,(LONG)min.y};ClientToScreen(owner,&pt);Session request;
  auto rgb=[](ImU32 color){return RGB(color&255,(color>>8)&255,(color>>16)&255);};
  request.screen={pt.x,pt.y,pt.x+(LONG)(max.x-min.x),pt.y+(LONG)(max.y-min.y)};request.height=(int)ImGui::GetFontSize();request.rounding=style.FrameRounding;request.paddingX=style.FramePadding.x;request.background=rgb(ImGui::GetColorU32(ImGuiCol_FrameBg));request.foreground=rgb(ImGui::GetColorU32(ImGuiCol_Text));
  bool update=false;{std::lock_guard lock(mutex);
   if(clicked||keyboard){request.id=id;request.value=value;request.initial=value;request.capacity=capacity;request.focus=true;session=request;update=true;}
   else if(session.id==id&&(memcmp(&session.screen,&request.screen,sizeof(RECT))||session.height!=request.height||session.rounding!=request.rounding||session.paddingX!=request.paddingX||session.background!=request.background||session.foreground!=request.foreground)){
    session.screen=request.screen;session.height=request.height;session.rounding=request.rounding;session.paddingX=request.paddingX;session.background=request.background;session.foreground=request.foreground;update=true;
   }
  }
  if(update&&host)PostMessageW(host,kUpdate,0,0);frameSawActive=true;
 }
 lastValues[id]=value;return changed;
}
}
