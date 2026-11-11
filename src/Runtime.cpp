#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdarg>
#include "Protocol.h"
#include "RenderPolicy.h"
#include "State.h"
#include "Overlay.h"
#include "VisibilityQueryHook.h"
#include "safetyhook.hpp"

namespace {
using namespace worlddiff;
std::atomic<bool> visualControllerLoaded=false;
SharedState* shared=nullptr;
HANDLE mapping=nullptr;
std::wstring logPath;
std::filesystem::path pluginDirectory;
constexpr uint32_t kSlots=32768;
struct Slot { std::atomic<uint64_t> key{0}; std::atomic<LONG> row{-1}; };
Slot slots[kSlots];
struct Identity {uintptr_t resource,base;uint32_t count,index,materialCount;uint16_t material,group,lod;char path[256],name[128];};

bool ReadIdentity(uintptr_t ctx,uintptr_t query,Identity* out,bool withPath){
 __try {
  if(!ctx||!query)return false;
  auto resource=*(uintptr_t*)(ctx+0x2A0);
  if(resource<0x10000)return false;
  auto base=*(uintptr_t*)(resource+0xC8);
  auto count=*(uint32_t*)(resource+0xD0);
  if(!base||!count||count>65535||query<base)return false;
  auto distance=query-base;if(distance%80||distance/80>=count)return false;
  out->resource=resource;out->base=base;out->count=count;out->index=(uint32_t)(distance/80);
  out->material=*(uint16_t*)(query+6);out->group=*(uint16_t*)(query+4);out->lod=*(uint16_t*)(query+8);
  if(withPath){
   size_t i=0;for(;i<sizeof(out->path)-1;i++){
    char c=*((char*)resource+0x0C+i);if(!c)break;
    if((unsigned char)c<0x20)return false;
    out->path[i]=(c=='/')?'\\':(c>='A'&&c<='Z'?c+32:c);
    if(i==10&&memcmp(out->path,"pl\\f_equip\\",11)&&memcmp(out->path,"pl\\m_equip\\",11))return false;
   }
   if(i==sizeof(out->path)-1)return false;out->path[i]=0;
   if(strncmp(out->path,"pl\\f_equip\\",11)&&strncmp(out->path,"pl\\m_equip\\",11))return false;
   if(!strstr(out->path,"\\mod\\"))return false;
   const size_t n=strlen(out->path);if(n>=5&&!strcmp(out->path+n-5,".mod3"))out->path[n-5]=0;
   out->materialCount=*(uint32_t*)(resource+0xE4);
   if(!out->materialCount||out->materialCount>65535||out->material>=out->materialCount)return false;
   auto names=*(uintptr_t*)(resource+0x400);if(!names)return false;
   size_t j=0;for(;j<127;j++){char c=*((char*)names+out->material*128+j);if(!c)break;if((unsigned char)c<0x20)return false;out->name[j]=c;}
   if(j==127)return false;out->name[j]=0;
  }
  return true;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool PathMatches(uintptr_t resource,const char* path){
 __try{
  for(unsigned i=0;i<255;i++){
   char c=*((char*)resource+0x0C+i);c=c=='/'?'\\':(c>='A'&&c<='Z'?c+32:c);
   if(!path[i])return !c||(c=='.'&&!strcmp((char*)resource+0x0C+i,".mod3"));
   if(c!=path[i])return false;
  }
  return false;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool NameMatches(uintptr_t resource,uint32_t material,const char* name){
 __try{
  if(material>=*(uint32_t*)(resource+0xE4))return false;
  auto names=*(uintptr_t*)(resource+0x400);if(!names)return false;
  return strncmp((char*)names+material*128,name,128)==0;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
LiveRow* MakeRow(Slot& s,uintptr_t ctx,uintptr_t q){
 Identity id{};if(!ReadIdentity(ctx,q,&id,true)){s.row=-1;s.key.store(0,std::memory_order_release);return nullptr;}
 LONG n=InterlockedIncrement(&shared->rowCount)-1;
 if(n>=(LONG)kMaxRows){InterlockedExchange(&shared->overflow,1);s.row=-1;return nullptr;}
 auto* r=&shared->rows[n];strcpy_s(r->modelPath,id.path);r->meshIndex=id.index;r->materialId=id.material;r->groupId=id.group;r->resource=id.resource;r->query=q;
 strcpy_s(r->materialName,id.name);r->meshCount=id.count;r->materialCount=id.materialCount;r->lod=id.lod;
 uint32_t crc=0xFFFFFFFF;for(const unsigned char* p=(const unsigned char*)id.name;*p;p++){crc^=*p;for(int bit=0;bit<8;bit++)crc=(crc>>1)^(0xEDB88320u&-(int32_t)(crc&1));}r->materialHash=crc;
 rowContexts[n].store(ctx,std::memory_order_relaxed);InterlockedExchange(&r->ready,1);s.row.store(n,std::memory_order_release);return r;
}
LiveRow* FindRow(uintptr_t ctx,uintptr_t q){
 Identity id{};if(!ReadIdentity(ctx,q,&id,false))return nullptr;
 uint64_t key=(q^((id.resource<<17)|(id.resource>>47))^((ctx<<31)|(ctx>>33)))|1ull;
 uint32_t slot=(uint32_t)((key>>4)*11400714819323198485ull)&(kSlots-1);
 for(unsigned probe=0;probe<32;probe++,slot=(slot+1)&(kSlots-1)){
  auto& s=slots[slot];uint64_t k=s.key.load(std::memory_order_acquire);
  if(k==key){
   LONG n=s.row.load(std::memory_order_acquire);if(n<0||n>=(LONG)kMaxRows)return nullptr;
   auto* r=&shared->rows[n];
   if(r->query!=q||r->resource!=id.resource||rowContexts[n].load(std::memory_order_relaxed)!=ctx)continue;
   if(r->ready!=1||r->materialId!=id.material||r->meshIndex!=id.index||!PathMatches(id.resource,r->modelPath)||!NameMatches(id.resource,id.material,r->materialName)){
    if(s.row.compare_exchange_strong(n,-2,std::memory_order_acq_rel)){InterlockedExchange(&r->ready,-1);return MakeRow(s,ctx,q);}return nullptr;
   }
   return r;
  }
  if(!k&&s.key.compare_exchange_strong(k,key,std::memory_order_acq_rel)){
   s.row=-2;return MakeRow(s,ctx,q);
  }
 }
 return nullptr;
}
bool PolicyOwnerMatches(const ModelPolicy& p,uintptr_t localEntity){
 __try{
  const auto base=(uintptr_t)GetModuleHandleW(nullptr);auto manager=*(uintptr_t*)(base+0x50139A0);auto local=manager?*(uintptr_t*)(manager+0x50):0;
  if(p.preview){if(local)return false;}else if(local!=localEntity||!local)return false;
  if(p.record){
   if(*(uintptr_t*)(base+0x500ECA0)!=p.container||*(uintptr_t*)p.container!=base+0x33FE5B0||*(uintptr_t*)p.record!=base+0x33FE588||*(uintptr_t*)(p.record+0x18)!=p.actor||*(uintptr_t*)(p.record+8)!=p.token||*(uintptr_t*)p.actor!=base+0x34A6D18)return false;
  }else if(!p.local)return false;
  constexpr uintptr_t offsets[]={0x300,0,0x80,0x100,0x180};if(p.part>=5)return false;
  auto armor=*(uintptr_t*)(p.actor+0xC928);
  return armor&&*(uintptr_t*)armor==p.actor&&*(uintptr_t*)(armor+offsets[p.part])==p.context&&*(uintptr_t*)(p.context+0x2A0)==p.resource&&*(uintptr_t*)(p.resource+0xC8)==p.meshes&&*(uintptr_t*)(p.resource+0x400)==p.names&&*(uint32_t*)(p.resource+0xE4)==p.materialCount;
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool FilterVisibility(uintptr_t ctx,uintptr_t q,bool visible,bool controllerHandled){
 if(!shared||shared->ready!=1)return visible;
 auto policies=renderPolicies.load(std::memory_order_acquire);auto* policy=policies?policies->Find(ctx):nullptr;auto now=GetTickCount();
 if(!policy||(DWORD)(now-policies->tick)>=1500||!PolicyOwnerMatches(*policy,policies->localEntity))return visible;
 Identity id{};if(!ReadIdentity(ctx,q,&id,false))return visible;
 const bool active=policies->enabled&&shared->enabled&&(DWORD)(now-(DWORD)shared->heartbeat)<5000&&PolicyMatches(*policy,id.resource,id.index,id.material,id.count)&&id.index<policy->groups.size()&&policy->groups[id.index]==id.group&&PathMatches(id.resource,policy->path.c_str());
 const bool hidden=active&&policy->hidden[id.index]!=0;
 const bool resolved=active?ResolveVisibility(visible,hidden,controllerHandled&&visualControllerLoaded.load(std::memory_order_relaxed),policies->worldDiffPriority):visible;
 // Telemetry exhaustion must never disable visibility control.
 if(auto* r=FindRow(ctx,q)){InterlockedIncrement(&r->hits);InterlockedExchange(&r->lastTick,(LONG)now);InterlockedExchange(&r->originalVisible,visible?1:0);InterlockedExchange(&r->hide,visible&&!resolved?1:0);if(visible&&!resolved)InterlockedIncrement(&r->hiddenHits);}
 return resolved;
}
void Log(const char* msg){FILE* f=nullptr;_wfopen_s(&f,logPath.c_str(),L"ab");if(f){fprintf(f,"[%llu] %s\n",GetTickCount64(),msg);fclose(f);}}
void SetMessage(const char* s,LONG error=0){strcpy_s(shared->message,s);InterlockedExchange(&shared->lastError,error);Log(s);}
std::vector<uintptr_t*> QueryDispatchSlots(uintptr_t module,uintptr_t target){
 std::vector<uintptr_t*> result;
 auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(module);
 auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(module+dos->e_lfanew);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.NumberOfSections>96)return result;
 auto* section=IMAGE_FIRST_SECTION(nt);
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++)if(!memcmp(section[i].Name,".rdata\0\0",8)){
  const size_t size=section[i].Misc.VirtualSize;
  if(size<sizeof(uintptr_t)||size>nt->OptionalHeader.SizeOfImage||section[i].VirtualAddress>nt->OptionalHeader.SizeOfImage-size)return {};
  std::vector<unsigned char> bytes(size);SIZE_T got=0;uintptr_t start=module+section[i].VirtualAddress;
  if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(start),bytes.data(),size,&got)||got!=size)return {};
  for(size_t at=0;at+sizeof(uintptr_t)<=size;at+=sizeof(uintptr_t)){
   uintptr_t value=0;memcpy(&value,bytes.data()+at,sizeof(value));
   if(value==target)result.push_back(reinterpret_cast<uintptr_t*>(start+at));
  }
 }
 if(result.size()>4096)return {};
 return result;
}
DWORD WINAPI Start(void* arg){
 wchar_t full[32768];GetModuleFileNameW((HMODULE)arg,full,32768);std::wstring folder=full;folder.resize(folder.find_last_of(L"\\/")+1);pluginDirectory=folder;logPath=folder+L"WorldDiff104.log";
 std::wstring name=std::wstring(kMappingPrefix)+std::to_wstring(GetCurrentProcessId());
 mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(SharedState),name.c_str());
 if(!mapping){Log("CreateFileMapping failed");return 0;}
 if(GetLastError()==ERROR_ALREADY_EXISTS){Log("Another WorldDiff104 instance is already active; duplicate DLL remains inert.");CloseHandle(mapping);return 0;}
 shared=(SharedState*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedState));if(!shared){Log("MapViewOfFile failed");CloseHandle(mapping);return 0;}
 memset(shared,0,sizeof(SharedState));shared->magic=kMagic;shared->version=kVersion;shared->pid=GetCurrentProcessId();shared->rowCapacity=kMaxRows;shared->enabled=1;
 uintptr_t target=(uintptr_t)GetModuleHandleW(nullptr)+0x222FC90;
 constexpr unsigned char tail[]={0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20,0x45,0x0F,0xB7,0x50,0x04};
 unsigned char bytes[sizeof(tail)]{};SIZE_T got=0;
 if(!ReadProcessMemory(GetCurrentProcess(),(void*)(target+14),bytes,sizeof(bytes),&got)||got!=sizeof(bytes)||memcmp(bytes,tail,sizeof(tail))){SetMessage("Unsupported executable: visibility signature mismatch. No hook installed.",1);return 0;}
 const uintptr_t caller=(uintptr_t)GetModuleHandleW(nullptr)+0x26DC304;
 // Verified caller: save r9/r8/rcx to rbx/rbp/rdi, CALL the shared query, then
 // movzx esi,al; test al,al. Never patch an unrecognized executable layout.
 constexpr unsigned char callSignature[]={0x49,0x8B,0xD9,0x49,0x8B,0xE8,0x48,0x8B,0xF9,0xE8,0x7E,0x39,0xB5,0xFF,0x0F,0xB6,0xF0,0x84,0xC0};
 unsigned char callerBytes[sizeof(callSignature)]{};
 if(!ReadProcessMemory(GetCurrentProcess(),(void*)caller,callerBytes,sizeof(callerBytes),&got)||got!=sizeof(callerBytes)||memcmp(callerBytes,callSignature,sizeof(callSignature))){SetMessage("Unsupported executable: visibility caller signature mismatch. No hook installed.",2);return 0;}
 constexpr unsigned char bodySignature[]={0x49,0x8B,0xE9,0x41,0x8B,0xC2,0x45,0x8B,0xDA};
 unsigned char bodyBytes[sizeof(bodySignature)]{};
 if(!ReadProcessMemory(GetCurrentProcess(),(void*)(target+29),bodyBytes,sizeof(bodyBytes),&got)||got!=sizeof(bodyBytes)||memcmp(bodyBytes,bodySignature,sizeof(bodySignature))){SetMessage("Unsupported executable: native visibility body signature mismatch. No hook installed.",2);return 0;}
 visualControllerLoaded.store(GetModuleHandleW(L"visual_controller_v5.dll")||GetModuleHandleW(L"MHWI-VisualControllerExtended.dll"),std::memory_order_relaxed);
 auto dispatchSlots=QueryDispatchSlots((uintptr_t)GetModuleHandleW(nullptr),target);
 if(!visibility::Install(caller+9,target,target+29,FilterVisibility,dispatchSlots)){SetMessage("Visibility direct/virtual caller hook installation failed. No visibility overrides are active.",3);return 0;}
 RuntimeLog("Visibility arbitration installed: direct caller plus %zu virtual dispatch references; original vtable identities preserved.",dispatchSlots.size());
 wchar_t gameExe[32768];GetModuleFileNameW(nullptr,gameExe,32768);
 app::Initialize(shared,pluginDirectory,std::filesystem::path(gameExe).parent_path());
 SetMessage("WorldDiff 1.3.3 internal ready; default panel keys: F9 / Home.");InterlockedExchange(&shared->ready,1);
 if(!overlay::Install())RuntimeLog("Overlay initialization failed; visibility controls remain available to diagnostics.");
 for(;;){visualControllerLoaded.store(GetModuleHandleW(L"visual_controller_v5.dll")||GetModuleHandleW(L"MHWI-VisualControllerExtended.dll"),std::memory_order_relaxed);app::Tick();overlay::WorkerTick();Sleep(100);}
}
}
namespace worlddiff {
void RuntimeLog(const char* fmt,...){char b[2048];va_list args;va_start(args,fmt);vsnprintf_s(b,sizeof(b),_TRUNCATE,fmt,args);va_end(args);::Log(b);}
SharedState* RuntimeState(){return shared;}
std::filesystem::path PluginDirectory(){return pluginDirectory;}
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,void*){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);HANDLE t=CreateThread(nullptr,0,Start,h,0,nullptr);if(t)CloseHandle(t);}return TRUE;}
