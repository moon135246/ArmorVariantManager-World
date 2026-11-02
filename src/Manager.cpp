#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "Protocol.h"
#include "ModelCatalog.h"
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"shell32.lib")
using namespace worlddiff;
namespace {
enum { ID_CONNECT=100,ID_SCAN,ID_SEARCH,ID_MODELS,ID_MATERIAL,ID_MESH,ID_LIST,ID_MODELRESET,ID_ALLRESET,ID_ENABLED,ID_LOADED,ID_PROFILE,ID_SAVE,ID_APPLY,ID_FOLDER };
constexpr UINT WM_INJECTED=WM_APP+21;
HWND win,connectBtn,scanBtn,searchBox,modelList,modeMat,modeMesh,detailList,modelReset,allReset,enabledBox,loadedBox,profileBox,saveBtn,applyBtn,folderBtn,titleText,statusText,helpText,modelTitle,footer;
HFONT font,headingFont; HBRUSH background;
UINT systemDpi=96;RECT desktopWork{};
int S(int value){return MulDiv(value,(int)systemDpi,96);}
std::filesystem::path exeDir,gameRoot;
std::vector<Model> catalog;
std::set<std::string> diskPaths;
std::vector<int> visibleModels;
std::vector<uint32_t> displayedTargets;
int selectedModel=-1; bool materialMode=true,updating=false,enabled=true,onlyLoaded=false;
DWORD processId=0; HANDLE mapping=nullptr; SharedState* live=nullptr;
using Key=std::pair<std::string,uint32_t>;
struct Rules {std::set<Key> meshHide,materialHide,meshShow;};
Rules rules;
std::map<std::wstring,Rules> presets;
std::wstring connectStatus=L"正在查找游戏…",diagnosticStatus;
std::atomic<bool> connecting{false};
std::wstring Wide(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring r(n,L' ');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r;}
std::string Utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string r(n,' ');WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;}
std::wstring Text(HWND h){int n=GetWindowTextLengthW(h);std::wstring r(n+1,L' ');GetWindowTextW(h,r.data(),n+1);r.resize(n);return r;}
std::wstring Part(const std::string& p){if(p=="body")return L"身体槽";if(p=="arm")return L"手臂槽";if(p=="helm")return L"头部槽";if(p=="leg")return L"腿部槽";if(p=="wst")return L"腰部槽";return Wide(p);}
std::wstring Hex(uint32_t v){wchar_t s[32];swprintf_s(s,L"%08X",v);return s;}
std::wstring Lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),[](wchar_t c){return (wchar_t)towlower(c);});return s;}
std::string Normalize(std::string s){for(auto& c:s){if(c=='/')c='\\';else if(c>='A'&&c<='Z')c+=32;}if(s.size()>5&&s.substr(s.size()-5)==".mod3")s.resize(s.size()-5);return s;}
std::string RowPath(const LiveRow& r){size_t n=0;while(n<sizeof(r.modelPath)&&r.modelPath[n])++n;return Normalize(std::string(r.modelPath,n));}
std::string ModelPath(const Model& m){return Normalize(m.relativePath);}
void Diagnostic(const std::string& key,const std::string& value){
 static std::map<std::string,std::string> previous;auto it=previous.find(key);if(it!=previous.end()&&it->second==value)return;previous[key]=value;
 std::ofstream f(exeDir/L"WorldDiffManager.log",std::ios::binary|std::ios::app);if(!f)return;SYSTEMTIME t{};GetLocalTime(&t);char stamp[48];sprintf_s(stamp,"%04u-%02u-%02u %02u:%02u:%02u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);f<<'['<<stamp<<"] "<<key<<": "<<value<<'\n';
}
bool IsHidden(const std::string& path,uint32_t mesh,uint32_t materialHash){Key key{path,mesh};if(rules.meshShow.count(key))return false;if(rules.meshHide.count(key))return true;return rules.materialHide.count({path,materialHash})!=0;}
uint32_t MeshMaterialHash(const Model& m,const Mesh& mesh){return mesh.materialId<m.materials.size()?m.materials[mesh.materialId].hash:0;}
size_t RuleCount(const Rules& r){return r.meshHide.size()+r.materialHide.size()+r.meshShow.size();}
bool ConfigWrite(){
 auto tmp=exeDir/L"WorldDiff102.ini.tmp",dst=exeDir/L"WorldDiff102.ini";
 std::ofstream f(tmp,std::ios::binary|std::ios::trunc);if(!f)return false;
 f<<"; WorldDiff 1.0.2 - UTF-8\n; M|path|materialHash = material hide; H|path|meshIndex = mesh hide; S|path|meshIndex = follow-game exception.\n; Priority: S > H > M. Group_ID is not used as the target.\n[Settings]\nEnabled="<<(enabled?1:0)<<"\nGameRoot="<<Utf8(gameRoot.wstring())<<"\n[Current]\n";
 auto write=[&](const Rules& r){for(const auto& k:r.materialHide)f<<"M|"<<k.first<<'|'<<k.second<<'\n';for(const auto& k:r.meshHide)f<<"H|"<<k.first<<'|'<<k.second<<'\n';for(const auto& k:r.meshShow)f<<"S|"<<k.first<<'|'<<k.second<<'\n';};write(rules);
 for(const auto& p:presets){f<<"[Preset:"<<Utf8(p.first)<<"]\n";write(p.second);}f.close();
 return !!MoveFileExW(tmp.c_str(),dst.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
void LoadConfig(){
 std::ifstream f(exeDir/L"WorldDiff102.ini",std::ios::binary);std::string line,section;Rules* into=nullptr;
 while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line[0]==';')continue;
  if(line.front()=='['&&line.back()==']'){section=line.substr(1,line.size()-2);into=nullptr;if(section=="Current")into=&rules;else if(section.rfind("Preset:",0)==0)into=&presets[Wide(section.substr(7))];continue;}
  if(section=="Settings"){if(line.rfind("Enabled=",0)==0)enabled=line.substr(8)!="0";else if(line.rfind("GameRoot=",0)==0)gameRoot=Wide(line.substr(9));continue;}
  if(into){auto p=line.find_last_of('|');if(p==std::string::npos)continue;try{char type='H';size_t start=0;if(line.size()>2&&line[1]=='|'&&(line[0]=='M'||line[0]=='H'||line[0]=='S')){type=line[0];start=2;}size_t used=0;auto value=std::stoull(line.substr(p+1),&used);if(used!=line.size()-p-1||value>0xFFFFFFFFull||(type!='M'&&value>=65536)||p<=start)continue;Key key{Normalize(line.substr(start,p-start)),(uint32_t)value};if(type=='M')into->materialHide.insert(key);else if(type=='S')into->meshShow.insert(key);else into->meshHide.insert(key);}catch(...){}}
 }
}
void Persist(){if(!ConfigWrite())SetWindowTextW(footer,L"设置尚未保存：程序目录不可写。请将整个文件夹放到可写目录后运行。");}
bool Alive(const LiveRow& r){return r.ready==1&&(DWORD)(GetTickCount()-(DWORD)r.lastTick)<5000;}
void Disconnect(){if(live){UnmapViewOfFile(live);live=nullptr;}if(mapping){CloseHandle(mapping);mapping=nullptr;}}
bool OpenShared(){
 if(!processId)return false;if(live)return true;std::wstring name=std::wstring(kMappingPrefix)+std::to_wstring(processId);mapping=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,name.c_str());
 if(!mapping){DWORD error=GetLastError();Diagnostic("OpenShared","pid="+std::to_string(processId)+" OpenFileMapping error="+std::to_string(error));return false;}
 live=(SharedState*)MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(SharedState));if(!live){DWORD error=GetLastError();CloseHandle(mapping);mapping=nullptr;Diagnostic("OpenShared","pid="+std::to_string(processId)+" MapViewOfFile error="+std::to_string(error));return false;}
 if(live->magic!=kMagic||live->version!=kVersion||live->pid!=processId){Diagnostic("OpenShared","pid="+std::to_string(processId)+" protocol mismatch");Disconnect();connectStatus=L"插件协议不匹配，请使用同一文件夹的 EXE 与 DLL。";return false;}
 Diagnostic("OpenShared","pid="+std::to_string(processId)+" connected");return true;
}
bool FindGame(){
 DWORD found=0,snapshotError=0,pathError=0;std::filesystem::path root;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 if(snap==INVALID_HANDLE_VALUE)snapshotError=GetLastError();else{
  PROCESSENTRY32W e{sizeof(e)};BOOL ok=Process32FirstW(snap,&e);if(!ok)snapshotError=GetLastError();for(;ok;ok=Process32NextW(snap,&e))if(!_wcsicmp(e.szExeFile,L"MonsterHunterWorld.exe")){found=e.th32ProcessID;break;}CloseHandle(snap);
 }
 DWORD snapshotPid=found;
 struct WindowSearch{DWORD pid=0;std::wstring title;};WindowSearch windows;
 SetLastError(0);BOOL enumerated=EnumWindows([](HWND window,LPARAM param)->BOOL{
  wchar_t title[512]{};if(!GetWindowTextW(window,title,512))return TRUE;std::wstring value=title;
  if(Lower(value).find(L"monster hunter: world")==std::wstring::npos)return TRUE;DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(!pid)return TRUE;auto* result=(WindowSearch*)param;result->pid=pid;result->title=std::move(value);return FALSE;
 },(LPARAM)&windows);DWORD enumError=(!enumerated&&!windows.pid)?GetLastError():0;
 // A titled game window identifies the active process even when a process
 // snapshot is incomplete or PROCESS_QUERY_LIMITED_INFORMATION is denied.
 if(windows.pid)found=windows.pid;
 if(found){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,found);if(p){wchar_t file[32768];DWORD n=32768;if(QueryFullProcessImageNameW(p,0,file,&n))root=std::filesystem::path(std::wstring(file,n)).parent_path();else pathError=GetLastError();CloseHandle(p);}else pathError=GetLastError();}
 Diagnostic("FindGame","snapshotPid="+std::to_string(snapshotPid)+" snapshotError="+std::to_string(snapshotError)+" windowPid="+std::to_string(windows.pid)+" enumError="+std::to_string(enumError)+" pathError="+std::to_string(pathError)+" selectedPid="+std::to_string(found));
 if(found!=processId){Disconnect();processId=found;}if(!root.empty())gameRoot=root;return processId!=0;
}
void ApplyPolicies(){if(!live||live->ready!=1)return;InterlockedExchange(&live->heartbeat,(LONG)GetTickCount());InterlockedExchange(&live->enabled,enabled?1:0);for(uint32_t i=0;i<kMaxRows;i++){auto& r=live->rows[i];if(r.ready!=1)continue;InterlockedExchange(&r.hide,IsHidden(RowPath(r),r.meshIndex,r.materialHash)?1:0);}}
std::set<std::string> ActiveModels(){std::set<std::string> s;if(live)for(uint32_t i=0;i<kMaxRows;i++)if(Alive(live->rows[i]))s.insert(RowPath(live->rows[i]));return s;}
bool MergeRuntime(){
 if(!live)return false;bool changed=false;DWORD now=GetTickCount();std::map<std::string,const LiveRow*> newest;
 for(uint32_t i=0;i<kMaxRows;i++){const auto& r=live->rows[i];if(r.ready!=1||r.meshIndex>=65536||r.materialId>=65536)continue;auto path=RowPath(r);if(path.empty())continue;auto it=newest.find(path);if(it==newest.end()||(DWORD)(now-(DWORD)r.lastTick)<=(DWORD)(now-(DWORD)it->second->lastTick))newest[path]=&r;}
 std::map<std::string,size_t> index;for(size_t i=0;i<catalog.size();i++)index[ModelPath(catalog[i])]=i;
 for(const auto& p:newest){if(index.count(p.first))continue;Model m;m.relativePath=p.first;auto a=p.first.find_last_of('\\');m.display=a==std::string::npos?p.first:p.first.substr(a+1);auto e=p.first.find("_equip\\");if(e!=std::string::npos){e+=7;auto end=p.first.find('\\',e);if(end!=std::string::npos){m.equip=p.first.substr(e,end-e);auto partEnd=p.first.find('\\',end+1);m.part=p.first.substr(end+1,partEnd-end-1);}}if(m.equip.empty())m.equip=m.display;index[p.first]=catalog.size();catalog.push_back(std::move(m));changed=true;}
 for(const auto& p:newest){auto& m=catalog[index[p.first]];auto count=p.second->meshCount;if(count&&count<=65535){auto end=std::remove_if(m.meshes.begin(),m.meshes.end(),[&](const Mesh& mesh){return mesh.index>=count;});if(end!=m.meshes.end()){m.meshes.erase(end,m.meshes.end());changed=true;}}}
 std::map<Key,DWORD> matAge;
 for(uint32_t i=0;i<kMaxRows;i++){
  const auto& r=live->rows[i];if(r.ready!=1||r.meshIndex>=65536||r.materialId>=65536)continue;auto path=RowPath(r);auto newestIt=newest.find(path);if(newestIt==newest.end()||newestIt->second->resource!=r.resource)continue;auto& m=catalog[index[path]];
  size_t len=0;while(len<sizeof(r.materialName)&&r.materialName[len])++len;std::string name(r.materialName,len);auto materialKey=Key{path,r.materialId};DWORD age=now-(DWORD)r.lastTick;
  if(!name.empty()&&(!matAge.count(materialKey)||age<=matAge[materialKey])){
   matAge[materialKey]=age;int matched=-1;for(const auto& old:m.materials)if(old.hash==r.materialHash&&old.mrlIndex>=0){matched=old.mrlIndex;break;}
   if(m.materials.size()<=r.materialId){m.materials.resize((size_t)r.materialId+1);changed=true;}
   auto& mat=m.materials[r.materialId];if(mat.name!=name||mat.hash!=r.materialHash){mat.name=name;mat.hash=r.materialHash;mat.mrlIndex=matched;changed=true;}
  }
  auto mesh=std::find_if(m.meshes.begin(),m.meshes.end(),[&](const Mesh& x){return x.index==r.meshIndex;});if(mesh==m.meshes.end()){Mesh value;value.index=r.meshIndex;value.materialId=(uint16_t)r.materialId;value.groupId=(uint16_t)r.groupId;value.lod=(uint16_t)r.lod;m.meshes.push_back(value);changed=true;}
  else if(mesh->materialId!=r.materialId||mesh->groupId!=r.groupId||mesh->lod!=r.lod){mesh->materialId=(uint16_t)r.materialId;mesh->groupId=(uint16_t)r.groupId;mesh->lod=(uint16_t)r.lod;changed=true;}
 }
 for(auto& m:catalog)std::sort(m.meshes.begin(),m.meshes.end(),[](const Mesh& a,const Mesh& b){return a.index<b.index;});return changed;
}
void SetCell(int row,int col,const std::wstring& value){ListView_SetItemText(detailList,row,col,const_cast<wchar_t*>(value.c_str()));}
std::vector<const Mesh*> TargetMeshes(const Model& m,uint32_t target){std::vector<const Mesh*> out;for(auto& mesh:m.meshes)if(materialMode?MeshMaterialHash(m,mesh)==target:mesh.index==target)out.push_back(&mesh);return out;}
void UpdateTableValues(){
 if(selectedModel<0||selectedModel>=(int)catalog.size())return;updating=true;const auto& m=catalog[selectedModel];auto path=ModelPath(m);
 for(int row=0;row<(int)displayedTargets.size();row++){
  auto meshes=TargetMeshes(m,displayedTargets[row]);size_t count=0;for(auto mesh:meshes)if(IsHidden(path,mesh->index,MeshMaterialHash(m,*mesh)))count++;
  ListView_SetCheckState(detailList,row,!meshes.empty()&&count==meshes.size());
  SetCell(row,2,count==0?L"跟随游戏":count==meshes.size()?L"已隐藏":L"部分隐藏");
  uint64_t hits=0,hiddenHits=0;bool active=false,original=false;
  if(live)for(uint32_t k=0;k<kMaxRows;k++){auto& r=live->rows[k];if(r.ready!=1||RowPath(r)!=path)continue;if(materialMode?r.materialHash!=displayedTargets[row]:r.meshIndex!=displayedTargets[row])continue;hits+=(uint32_t)r.hits;hiddenHits+=(uint32_t)r.hiddenHits;if(Alive(r)){active=true;if(r.originalVisible)original=true;}}
  std::wstring state=!active?L"未观测到":(!enabled?L"总开关已关闭":count==meshes.size()&&count?L"正在拦截隐藏":original?L"游戏允许显示":L"游戏当前不显示");SetCell(row,5,state);SetCell(row,6,std::to_wstring(hits));SetCell(row,7,std::to_wstring(hiddenHits));
 }updating=false;
}
void BuildTable(){
 updating=true;ListView_DeleteAllItems(detailList);displayedTargets.clear();if(selectedModel<0||selectedModel>=(int)catalog.size()){SetWindowTextW(modelTitle,L"选择左侧模型，查看它的材质与子网格");updating=false;return;}
 const auto& m=catalog[selectedModel];SetWindowTextW(modelTitle,(Wide(m.equip)+L"  /  "+Part(m.part)+L"   ·   "+Wide(m.display)+L"   （"+std::to_wstring(m.meshes.size())+L" 个子网格 · "+(diskPaths.count(ModelPath(m))?L"散装模型":L"运行时模型")+L"）").c_str());
 std::set<uint32_t> targets;for(auto& mesh:m.meshes)targets.insert(materialMode?MeshMaterialHash(m,mesh):mesh.index);
 for(auto target:targets){auto meshes=TargetMeshes(m,target);if(meshes.empty())continue;uint32_t mid=meshes[0]->materialId;const Material* mat=mid<m.materials.size()?&m.materials[mid]:nullptr;
  int row=(int)displayedTargets.size();displayedTargets.push_back(target);std::wstring label=materialMode?L"材质 "+std::to_wstring(mid):L"网格 "+std::to_wstring(target);
  LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=row;item.pszText=label.data();ListView_InsertItem(detailList,&item);SetCell(row,1,mat?Wide(mat->name):L"（材质索引无效）");
  SetCell(row,3,materialMode?std::to_wstring(meshes.size())+L" 个网格":L"LOD "+std::to_wstring(meshes[0]->lod)+L" / G "+std::to_wstring(meshes[0]->groupId));SetCell(row,4,mat?L"0x"+Hex(mat->hash)+(mat->mrlIndex>=0?L"  #"+std::to_wstring(mat->mrlIndex):L"  MRL未知"):L"—");
 }
 updating=false;UpdateTableValues();
}
void FilterModels(bool preserve=true){
 std::string selected;if(preserve&&selectedModel>=0&&selectedModel<(int)catalog.size())selected=ModelPath(catalog[selectedModel]);auto active=ActiveModels();auto find=Lower(Text(searchBox));visibleModels.clear();
 for(int i=0;i<(int)catalog.size();i++){const auto& m=catalog[i];bool loaded=active.count(ModelPath(m));if(onlyLoaded&&!loaded)continue;if(!find.empty()&&Lower(Wide(m.relativePath)).find(find)==std::wstring::npos&&Lower(Part(m.part)).find(find)==std::wstring::npos)continue;visibleModels.push_back(i);}
 std::stable_sort(visibleModels.begin(),visibleModels.end(),[&](int a,int b){return active.count(ModelPath(catalog[a]))>active.count(ModelPath(catalog[b]));});
 updating=true;SendMessageW(modelList,WM_SETREDRAW,FALSE,0);SendMessageW(modelList,LB_RESETCONTENT,0,0);int select=-1;
 for(int row=0;row<(int)visibleModels.size();row++){const auto& m=catalog[visibleModels[row]];std::wstring label=(active.count(ModelPath(m))?L"● ":L"   ")+Wide(m.equip)+L"  ·  "+Part(m.part)+L"  ·  "+Wide(m.display);SendMessageW(modelList,LB_ADDSTRING,0,(LPARAM)label.c_str());if(ModelPath(m)==selected)select=row;}
 if(select<0&&!visibleModels.empty())select=0;SendMessageW(modelList,LB_SETCURSEL,select,0);selectedModel=select>=0?visibleModels[select]:-1;SendMessageW(modelList,WM_SETREDRAW,TRUE,0);InvalidateRect(modelList,nullptr,TRUE);updating=false;BuildTable();
}
void Scan(){if(gameRoot.empty())gameRoot=L"F:\\SteamLibrary\\steamapps\\common\\Monster Hunter World";std::vector<std::string> errors;catalog=LoadCatalog(gameRoot,&errors);diskPaths.clear();for(const auto& m:catalog)diskPaths.insert(ModelPath(m));MergeRuntime();diagnosticStatus=L"磁盘目录："+gameRoot.wstring()+L"  |  "+std::to_wstring(diskPaths.size())+L" 个散装模型；已连接时自动合并运行时模型";if(!errors.empty())diagnosticStatus+=L"，"+std::to_wstring(errors.size())+L" 条解析提示";SetWindowTextW(footer,diagnosticStatus.c_str());FilterModels();}
void RefreshProfiles(){std::wstring old=Text(profileBox);SendMessageW(profileBox,CB_RESETCONTENT,0,0);for(auto& p:presets)SendMessageW(profileBox,CB_ADDSTRING,0,(LPARAM)p.first.c_str());SetWindowTextW(profileBox,old.c_str());}
void StartConnection(){
 if(connecting.exchange(true))return;if(!FindGame()){connecting=false;connectStatus=L"未找到运行中的 Monster Hunter: World。请进入游戏后连接。";return;}if(OpenShared()){connecting=false;connectStatus=L"已连接游戏";ApplyPolicies();return;}
 auto dll=exeDir/L"WorldDiff.dll";if(!std::filesystem::exists(dll)){connecting=false;connectStatus=L"找不到同目录的 WorldDiff.dll。请保留完整发布文件夹。";return;}
 EnableWindow(connectBtn,FALSE);connectStatus=L"正在加载插件，请稍候…";DWORD pid=processId;std::thread([dll,pid]{
  DWORD code=0;HANDLE p=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);if(!p){code=GetLastError();PostMessageW(win,WM_INJECTED,code,0);return;}
  auto s=dll.wstring();size_t bytes=(s.size()+1)*sizeof(wchar_t);void* remote=VirtualAllocEx(p,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);SIZE_T written=0;
  if(!remote||!WriteProcessMemory(p,remote,s.c_str(),bytes,&written)||written!=bytes){code=GetLastError();if(remote)VirtualFreeEx(p,remote,0,MEM_RELEASE);CloseHandle(p);PostMessageW(win,WM_INJECTED,code?code:299,0);return;}
  auto load=(LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");HANDLE t=CreateRemoteThread(p,nullptr,0,load,remote,0,nullptr);
  if(!t){code=GetLastError();VirtualFreeEx(p,remote,0,MEM_RELEASE);}else{DWORD wait=WaitForSingleObject(t,10000),rc=0;if(wait==WAIT_OBJECT_0){GetExitCodeThread(t,&rc);VirtualFreeEx(p,remote,0,MEM_RELEASE);if(!rc)code=1114;}else code=wait==WAIT_TIMEOUT?1460:GetLastError();CloseHandle(t);}CloseHandle(p);PostMessageW(win,WM_INJECTED,code,0);
 }).detach();
}
HWND Control(const wchar_t* cls,const wchar_t* text,DWORD style,int id,DWORD ex=0){HWND h=CreateWindowExW(ex,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,1,1,win,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);return h;}
void Layout(){RECT r;GetClientRect(win,&r);int w=r.right,h=r.bottom,p=S(20),left=S(340),gap=S(18),right=p+left+gap,rw=w-right-p;
 MoveWindow(titleText,p,S(16),w-S(40),S(32),TRUE);MoveWindow(statusText,p,S(54),w-S(40),S(26),TRUE);MoveWindow(connectBtn,p,S(88),S(100),S(32),TRUE);MoveWindow(scanBtn,p+S(110),S(88),S(110),S(32),TRUE);MoveWindow(enabledBox,p+S(240),S(91),S(180),S(28),TRUE);MoveWindow(allReset,w-p-S(128),S(88),S(128),S(32),TRUE);MoveWindow(folderBtn,w-p-S(260),S(88),S(122),S(32),TRUE);
 MoveWindow(searchBox,p,S(138),left,S(30),TRUE);MoveWindow(loadedBox,p,S(176),left,S(26),TRUE);MoveWindow(modelList,p,S(210),left,std::max(S(100),h-S(302)),TRUE);
 MoveWindow(modeMat,right,S(138),S(150),S(30),TRUE);MoveWindow(modeMesh,right+S(158),S(138),S(150),S(30),TRUE);MoveWindow(modelReset,w-p-S(130),S(138),S(130),S(30),TRUE);MoveWindow(modelTitle,right,S(179),rw,S(26),TRUE);MoveWindow(detailList,right,S(210),rw,std::max(S(100),h-S(344)),TRUE);
 MoveWindow(helpText,right,h-S(122),rw,S(36),TRUE);MoveWindow(profileBox,right,h-S(77),S(200),S(220),TRUE);MoveWindow(saveBtn,right+S(212),h-S(77),S(116),S(28),TRUE);MoveWindow(applyBtn,right+S(340),h-S(77),S(116),S(28),TRUE);MoveWindow(footer,p,h-S(35),w-S(40),S(24),TRUE);
 if(rw>0){int widths[]={S(92),std::max(S(170),rw-S(762)),S(88),S(132),S(172),S(118),S(76),S(80)};for(int i=0;i<8;i++)ListView_SetColumnWidth(detailList,i,widths[i]);}
}
void Status(){std::wstring s=connectStatus;if(live){s=L"● 已连接  ·  PID "+std::to_wstring(processId)+L"  ·  "+(live->ready==1?L"拦截器就绪":L"拦截器未就绪")+L"  ·  "+(enabled?L"显隐控制已启用":L"全部跟随游戏（总开关关闭）")+L"  ·  已发现 "+std::to_wstring(live->rowCount)+L" 个运行时子网格";if(live->overflow)s+=L"  ·  观测记录已满，请重启游戏";if(live->lastError)s+=L"  ·  "+Wide(live->message);}SetWindowTextW(statusText,s.c_str());}
void SetTarget(int row,bool hide){
 if(selectedModel<0||row<0||row>=(int)displayedTargets.size())return;const auto& m=catalog[selectedModel];auto path=ModelPath(m);auto target=displayedTargets[row];auto meshes=TargetMeshes(m,target);
 if(materialMode){Key material{path,target};if(hide)rules.materialHide.insert(material);else rules.materialHide.erase(material);for(auto mesh:meshes){rules.meshShow.erase({path,mesh->index});if(!hide)rules.meshHide.erase({path,mesh->index});}}
 else for(auto mesh:meshes){Key key{path,mesh->index};if(hide){rules.meshHide.insert(key);rules.meshShow.erase(key);}else{rules.meshHide.erase(key);if(rules.materialHide.count({path,MeshMaterialHash(m,*mesh)}))rules.meshShow.insert(key);else rules.meshShow.erase(key);}}
 ApplyPolicies();Persist();UpdateTableValues();
}
LRESULT CALLBACK WndProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){case WM_CREATE:{win=h;LOGFONTW lf{};lf.lfHeight=-S(16);wcscpy_s(lf.lfFaceName,L"Microsoft YaHei UI");font=CreateFontIndirectW(&lf);lf.lfHeight=-S(24);lf.lfWeight=FW_SEMIBOLD;headingFont=CreateFontIndirectW(&lf);background=CreateSolidBrush(RGB(245,247,250));
  titleText=Control(L"STATIC",L"差分管理器  ·  世界版 1.0.2",SS_LEFT,0);SendMessageW(titleText,WM_SETFONT,(WPARAM)headingFont,TRUE);statusText=Control(L"STATIC",L"正在查找游戏…",SS_LEFT,0);
  connectBtn=Control(L"BUTTON",L"连接游戏",BS_PUSHBUTTON,ID_CONNECT);scanBtn=Control(L"BUTTON",L"刷新模型",BS_PUSHBUTTON,ID_SCAN);enabledBox=Control(L"BUTTON",L"启用显隐控制",BS_AUTOCHECKBOX,ID_ENABLED);Button_SetCheck(enabledBox,enabled?BST_CHECKED:BST_UNCHECKED);
  allReset=Control(L"BUTTON",L"全部恢复显示",BS_PUSHBUTTON,ID_ALLRESET);folderBtn=Control(L"BUTTON",L"打开程序目录",BS_PUSHBUTTON,ID_FOLDER);
  searchBox=Control(L"EDIT",L"",ES_AUTOHSCROLL,ID_SEARCH,WS_EX_CLIENTEDGE);SendMessageW(searchBox,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索装备编号、部位、模型路径");loadedBox=Control(L"BUTTON",L"只看当前观测到的模型",BS_AUTOCHECKBOX,ID_LOADED);
  modelList=Control(L"LISTBOX",L"",LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_HSCROLL,ID_MODELS,WS_EX_CLIENTEDGE);SendMessageW(modelList,LB_SETHORIZONTALEXTENT,S(800),0);
  modeMat=Control(L"BUTTON",L"按材质选择",BS_AUTORADIOBUTTON|WS_GROUP,ID_MATERIAL);modeMesh=Control(L"BUTTON",L"按子网格选择",BS_AUTORADIOBUTTON,ID_MESH);Button_SetCheck(modeMat,BST_CHECKED);modelReset=Control(L"BUTTON",L"恢复此模型",BS_PUSHBUTTON,ID_MODELRESET);modelTitle=Control(L"STATIC",L"选择左侧模型，查看它的材质与子网格",SS_LEFT,0);
  detailList=Control(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,ID_LIST,WS_EX_CLIENTEDGE);ListView_SetExtendedListViewStyle(detailList,LVS_EX_FULLROWSELECT|LVS_EX_CHECKBOXES|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES|LVS_EX_LABELTIP);
  const wchar_t* cols[]={L"勾选隐藏",L"材质名称",L"设置",L"关联 / 原始值",L"材质哈希 / MRL序号",L"运行时状态",L"查询次数",L"隐藏命中"};for(int i=0;i<8;i++){LVCOLUMNW c{};c.mask=LVCF_TEXT|LVCF_WIDTH;c.pszText=const_cast<wchar_t*>(cols[i]);c.cx=S(100);ListView_InsertColumn(detailList,i,&c);}
  helpText=Control(L"STATIC",L"勾选 = 隐藏；取消勾选 = 跟随游戏。材质规则按名称哈希保存，自动覆盖新 LOD 与重载模型。\n子网格可单独恢复，作为材质隐藏的例外；无需 nativePC 散装模型也可读取当前运行时材质。",SS_LEFT,0);
  profileBox=Control(L"COMBOBOX",L"",CBS_DROPDOWN|CBS_AUTOHSCROLL|WS_VSCROLL,ID_PROFILE);saveBtn=Control(L"BUTTON",L"保存命名预设",BS_PUSHBUTTON,ID_SAVE);applyBtn=Control(L"BUTTON",L"应用此预设",BS_PUSHBUTTON,ID_APPLY);footer=Control(L"STATIC",L"",SS_LEFT,0);RefreshProfiles();FindGame();Scan();Status();SetTimer(win,1,800,nullptr);return 0;}
 case WM_SIZE:Layout();return 0;
 case WM_GETMINMAXINFO:((MINMAXINFO*)lp)->ptMinTrackSize={std::min<int>(S(1120),desktopWork.right-desktopWork.left-S(40)),std::min<int>(S(650),desktopWork.bottom-desktopWork.top-S(40))};return 0;
 case WM_CTLCOLORSTATIC:{auto dc=(HDC)wp;SetBkMode(dc,TRANSPARENT);SetTextColor(dc,(HWND)lp==titleText?RGB(20,40,60):RGB(60,73,86));return (LRESULT)background;}
 case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);FillRect((HDC)wp,&r,background);return 1;}
 case WM_NOTIFY:{auto n=(NMHDR*)lp;if(n->idFrom==ID_LIST&&n->code==LVN_ITEMCHANGED&&!updating){auto change=(NMLISTVIEW*)lp;if((change->uChanged&LVIF_STATE)&&((change->uOldState^change->uNewState)&LVIS_STATEIMAGEMASK)&&change->iItem>=0)SetTarget(change->iItem,(change->uNewState&LVIS_STATEIMAGEMASK)==INDEXTOSTATEIMAGEMASK(2));}return 0;}
 case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);if(updating)return 0;
  if(id==ID_CONNECT){StartConnection();Status();}
  else if(id==ID_SCAN){FindGame();Scan();Persist();}
  else if(id==ID_SEARCH&&event==EN_CHANGE)FilterModels();
  else if(id==ID_LOADED){onlyLoaded=Button_GetCheck(loadedBox)==BST_CHECKED;FilterModels();}
  else if(id==ID_MODELS&&event==LBN_SELCHANGE){int row=(int)SendMessageW(modelList,LB_GETCURSEL,0,0);selectedModel=row>=0&&row<(int)visibleModels.size()?visibleModels[row]:-1;BuildTable();}
  else if(id==ID_MATERIAL||id==ID_MESH){materialMode=id==ID_MATERIAL;Button_SetCheck(modeMat,materialMode?BST_CHECKED:BST_UNCHECKED);Button_SetCheck(modeMesh,materialMode?BST_UNCHECKED:BST_CHECKED);BuildTable();}
  else if(id==ID_ENABLED){enabled=Button_GetCheck(enabledBox)==BST_CHECKED;ApplyPolicies();Persist();Status();UpdateTableValues();}
  else if(id==ID_MODELRESET&&selectedModel>=0){auto path=ModelPath(catalog[selectedModel]);auto erase=[&](std::set<Key>& keys){for(auto it=keys.begin();it!=keys.end();)if(it->first==path)it=keys.erase(it);else ++it;};erase(rules.meshHide);erase(rules.materialHide);erase(rules.meshShow);ApplyPolicies();Persist();UpdateTableValues();}
  else if(id==ID_ALLRESET){rules={};enabled=true;Button_SetCheck(enabledBox,BST_CHECKED);ApplyPolicies();Persist();Status();UpdateTableValues();}
  else if(id==ID_FOLDER)ShellExecuteW(win,L"open",exeDir.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
  else if(id==ID_SAVE){auto name=Text(profileBox);if(name.empty()||name.size()>80||name.find_first_of(L"[]\r\n")!=std::wstring::npos){MessageBoxW(win,L"请先输入预设名称（1–80 字，不含方括号或换行）。",L"保存预设",MB_OK|MB_ICONINFORMATION);return 0;}presets[name]=rules;Persist();RefreshProfiles();SetWindowTextW(profileBox,name.c_str());SetWindowTextW(footer,(L"已保存预设："+name+L"  ·  "+std::to_wstring(RuleCount(rules))+L" 条材质 / 子网格规则").c_str());}
  else if(id==ID_APPLY){auto name=Text(profileBox);auto it=presets.find(name);if(it==presets.end()){MessageBoxW(win,L"请从列表选择一个已保存的预设。",L"应用预设",MB_OK|MB_ICONINFORMATION);return 0;}rules=it->second;ApplyPolicies();Persist();UpdateTableValues();SetWindowTextW(footer,(L"已应用预设："+name).c_str());}return 0;}
 case WM_INJECTED:{connecting=false;EnableWindow(connectBtn,TRUE);DWORD error=(DWORD)wp;if(error==5)connectStatus=L"连接被拒绝（错误 5）。请用发布目录的“启动管理器.cmd”运行，再连接游戏。";else if(error)connectStatus=L"加载失败，Windows 错误 "+std::to_wstring(error)+L"。详见程序目录中的运行日志。";else connectStatus=L"插件已加载，等待运行时连接…";Status();return 0;}
 case WM_TIMER:{bool was=live!=nullptr;DWORD old=processId;FindGame();if(processId)OpenShared();bool changed=MergeRuntime();ApplyPolicies();Status();UpdateTableValues();static unsigned tick=0;if(changed||(++tick%5)==0||was!=(live!=nullptr)||old!=processId)FilterModels(was&&old==processId);return 0;}
 case WM_DESTROY:KillTimer(h,1);Persist();if(live)InterlockedExchange(&live->enabled,0);Disconnect();DeleteObject(font);DeleteObject(headingFont);DeleteObject(background);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\WorldDiffManager102");if(singleton&&GetLastError()==ERROR_ALREADY_EXISTS){HWND existing=FindWindowW(L"WorldDiffManager102",nullptr);if(existing){if(IsIconic(existing))ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}CloseHandle(singleton);return 0;}
 SetProcessDPIAware();using DpiFn=UINT(WINAPI*)();auto dpi=(DpiFn)GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetDpiForSystem");if(dpi)systemDpi=dpi();if(systemDpi<96)systemDpi=96;SystemParametersInfoW(SPI_GETWORKAREA,0,&desktopWork,0);if(desktopWork.right<=desktopWork.left)desktopWork={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};
 wchar_t exe[32768];GetModuleFileNameW(nullptr,exe,32768);exeDir=std::filesystem::path(exe).parent_path();LoadConfig();INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wc.lpszClassName=L"WorldDiffManager102";RegisterClassExW(&wc);
 int width=std::min<int>(S(1400),desktopWork.right-desktopWork.left-S(80)),height=std::min<int>(S(860),desktopWork.bottom-desktopWork.top-S(80));int x=desktopWork.left+(desktopWork.right-desktopWork.left-width)/2,y=desktopWork.top+(desktopWork.bottom-desktopWork.top-height)/2;
 HWND h=CreateWindowExW(0,wc.lpszClassName,L"差分管理器 · 世界版 1.0.2",WS_OVERLAPPEDWINDOW,x,y,width,height,nullptr,nullptr,instance,nullptr);if(!h){if(singleton)CloseHandle(singleton);return 1;}ShowWindow(h,show);UpdateWindow(h);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){if(!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}if(singleton)CloseHandle(singleton);return (int)m.wParam;
}
