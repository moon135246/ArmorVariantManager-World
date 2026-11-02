#include "NativeInput.h"
#include "Overlay.h"
#include "State.h"
#include "imgui.h"
#include <imm.h>
#include <objbase.h>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>
namespace worlddiff::overlay {
namespace {
constexpr UINT kUpdate=WM_APP+0x441,kClose=WM_APP+0x442;
std::mutex mutex;std::atomic<DWORD> threadId{0};std::atomic<bool> started{false},shown{false};
HWND owner=nullptr,host=nullptr,edit=nullptr;WNDPROC editOriginal=nullptr;HIMC ime=nullptr;HFONT font=nullptr;HBRUSH brush=nullptr;
struct Session {ImGuiID id=0;std::string value,initial;RECT screen{};size_t capacity=0;int height=22;bool focus=false;};
Session session;bool frameSawActive=false;ImGuiID nativeId=0;
std::map<ImGuiID,std::string> lastValues;
std::wstring Wide(const std::string& text){return app::PathUtf8(text).wstring();}
std::string Limit(const std::string& text,size_t cap){size_t n=0;while(n<text.size()&&n<cap){unsigned char c=text[n];size_t bytes=c<128?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:4;if(n+bytes>cap||n+bytes>text.size())break;n+=bytes;}return text.substr(0,n);}
void Publish(){if(!edit)return;int length=GetWindowTextLengthW(edit);std::wstring text(length+1,L' ');GetWindowTextW(edit,text.data(),length+1);text.resize(length);auto value=app::Utf8(text);std::lock_guard lock(mutex);if(session.id==nativeId)session.value=Limit(value,session.capacity?session.capacity-1:0);}
void Hide(bool cancel){
 if(cancel){std::lock_guard lock(mutex);session.value=session.initial;}else Publish();
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
  if(changed||!shown){auto text=Wide(request.value);SetWindowTextW(edit,text.c_str());SendMessageW(edit,EM_LIMITTEXT,(WPARAM)std::min<size_t>(request.capacity,1000),0);SendMessageW(edit,EM_SETSEL,(WPARAM)-1,-1);}
  int width=std::max(40L,request.screen.right-request.screen.left),height=std::max(24L,request.screen.bottom-request.screen.top);
  SetWindowPos(hwnd,HWND_TOP,request.screen.left,request.screen.top,width,height,SWP_NOACTIVATE);MoveWindow(edit,0,0,width,height,TRUE);
  if(font)DeleteObject(font);font=CreateFontW(-request.height,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");SendMessageW(edit,WM_SETFONT,(WPARAM)font,TRUE);
  if(request.focus||!shown){ShowWindow(hwnd,SW_SHOWNORMAL);shown=true;SetForegroundWindow(hwnd);SetFocus(edit);}
  return 0;
 }
 if(message==kClose){Hide(false);return 0;}
 if(message==WM_COMMAND&&HIWORD(w)==EN_CHANGE){Publish();return 0;}
 if(message==WM_CTLCOLOREDIT){SetTextColor((HDC)w,RGB(241,244,248));SetBkColor((HDC)w,RGB(37,46,58));return (LRESULT)brush;}
 if(message==WM_MOUSEACTIVATE)return MA_ACTIVATE;
 return DefWindowProcW(hwnd,message,w,l);
}
DWORD WINAPI Thread(void*){
 threadId=GetCurrentThreadId();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);WNDCLASSW wc{};wc.lpfnWndProc=HostProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"WorldDiff104NativeText";wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32513));RegisterClassW(&wc);brush=CreateSolidBrush(RGB(37,46,58));
 host=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,wc.lpszClassName,L"WorldDiff 中文输入",WS_POPUP,0,0,200,30,owner,nullptr,wc.hInstance,nullptr);
 edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_LEFT,0,0,200,30,host,(HMENU)1,wc.hInstance,nullptr);
 if(edit){editOriginal=(WNDPROC)SetWindowLongPtrW(edit,GWLP_WNDPROC,(LONG_PTR)EditProc);ime=ImmCreateContext();ImmAssociateContext(edit,ime);}
 RuntimeLog("Native Unicode/IME editor initialized (dedicated Win32 EDIT context)");
 MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}return 0;
}
}
void NativeInputInitialize(HWND game){owner=game;if(started.exchange(true))return;HANDLE thread=CreateThread(nullptr,0,Thread,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
bool NativeInputThread(){return GetCurrentThreadId()==threadId.load();}
bool NativeInputOwnWindow(HWND h){return h&&(h==host||h==edit);}
void NativeInputClose(){if(host)PostMessageW(host,kClose,0,0);}
void NativeInputBeginFrame(){frameSawActive=false;}
void NativeInputEndFrame(){if(shown&&!frameSawActive)NativeInputClose();}
bool TextField(const char* label,const char* hint,char* value,size_t capacity){
 if(!value||capacity<2)return false;ImGuiID id=ImGui::GetID(label);bool changed=false;bool active=false;
 {std::lock_guard lock(mutex);if(session.id==id){active=shown;if(!active&&lastValues.count(id)&&lastValues[id]!=value)session.value=value;auto text=Limit(session.value,capacity-1);if(text!=value){strcpy_s(value,capacity,text.c_str());changed=true;}if(active)frameSawActive=true;}}
 ImGui::InputTextWithHint(label,hint?hint:"",value,capacity,ImGuiInputTextFlags_ReadOnly);
 bool clicked=ImGui::IsItemClicked();bool keyboard=ImGui::IsItemFocused()&&ImGui::IsKeyPressed(ImGuiKey_Enter);
 if(clicked||keyboard){auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();POINT pt{(LONG)min.x,(LONG)min.y};ClientToScreen(owner,&pt);Session request;request.id=id;request.value=value;request.initial=value;request.capacity=capacity;request.screen={pt.x,pt.y,pt.x+(LONG)(max.x-min.x),pt.y+(LONG)(max.y-min.y)};request.height=(int)ImGui::GetFontSize();request.focus=true;{std::lock_guard lock(mutex);session=request;}if(host)PostMessageW(host,kUpdate,0,0);frameSawActive=true;
 }
 lastValues[id]=value;return changed;
}
}
