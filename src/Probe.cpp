#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <atomic>
#include <cstdio>
#include <string>
#include "safetyhook.hpp"
namespace {
using Fn=bool(__fastcall*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
Fn original=nullptr;
safetyhook::InlineHook* hook=nullptr;
struct Row { uint64_t obj,ctx,b,d,caller,hits,result; unsigned char q[128],c[0x500],bb[256],dd[256]; };
constexpr unsigned N=8192;
Row rows[N]{};
std::atomic<uint64_t> keys[N]{};
std::atomic<bool> capture{false};
std::atomic<uint64_t> hidden{0}, matched{0};
std::wstring dir;
bool Read(uintptr_t p,void* dst,size_t n){__try{memcpy(dst,(void*)p,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool __fastcall Detour(uintptr_t a,uintptr_t b,uintptr_t q,uintptr_t d){
 bool ret=original(a,b,q,d);
 if(capture.load(std::memory_order_relaxed)){
  auto key=q ? q:1; unsigned slot=(unsigned)((q>>4)*11400714819323198485ull)&(N-1);
  for(unsigned k=0;k<16;k++,slot=(slot+1)&(N-1)){
   auto v=keys[slot].load(std::memory_order_relaxed);
   if(!v && keys[slot].compare_exchange_strong(v,key)){
    auto& r=rows[slot];r.obj=q;r.ctx=a;r.b=b;r.d=d;r.caller=(uintptr_t)_ReturnAddress();r.result=ret;
    Read(q,r.q,sizeof(r.q));Read(a,r.c,sizeof(r.c));Read(b,r.bb,sizeof(r.bb));Read(d,r.dd,sizeof(r.dd));
    InterlockedIncrement64((volatile LONG64*)&r.hits);break;
   }
   if(v==key){InterlockedIncrement64((volatile LONG64*)&rows[slot].hits);break;}
  }
 }
 if(hidden.load(std::memory_order_relaxed)==q){matched++;return false;}
 return ret;
}
void Log(const char* s){FILE* f=nullptr;_wfopen_s(&f,(dir+L"probe.log").c_str(),L"ab");if(f){fprintf(f,"%s\n",s);fclose(f);}}
DWORD WINAPI Work(void* arg){
 wchar_t path[32768];GetModuleFileNameW((HMODULE)arg,path,32768);dir=path;dir.resize(dir.find_last_of(L"\\/")+1);
 uintptr_t target=(uintptr_t)GetModuleHandleW(nullptr)+0x222FC90;
 unsigned char sig[15]={0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20,0x45,0x0F,0xB7,0x50,0x04},got[15]{};
 if(!Read(target+14,got,15)||memcmp(sig,got,15)){Log("SIGNATURE_MISMATCH");return 0;}
 auto h=safetyhook::InlineHook::create((void*)target,(void*)Detour,safetyhook::InlineHook::StartDisabled);
 if(!h){Log("HOOK_CREATE_FAILED");return 0;}
 hook=new safetyhook::InlineHook(std::move(*h));original=hook->original<Fn>();
 if(!hook->enable()){Log("HOOK_ENABLE_FAILED");return 0;}Log("READY");
 std::string last;
 for(;;){
  Sleep(150);FILE* f=nullptr;_wfopen_s(&f,(dir+L"probe.command").c_str(),L"rb");if(!f)continue;
  char line[256]{};fgets(line,256,f);fclose(f);std::string s=line;if(s==last)continue;last=s;
  if(s.starts_with("capture")){capture=false;Sleep(100);memset(rows,0,sizeof(rows));for(auto& k:keys)k=0;capture=true;Log("CAPTURING");}
  else if(s.starts_with("dump")){capture=false;Sleep(100);_wfopen_s(&f,(dir+L"probe.bin").c_str(),L"wb");if(f){fwrite(rows,sizeof(rows),1,f);fclose(f);}Log("DUMPED");}
  else if(s.starts_with("hide")){unsigned long long p=0;sscanf_s(line,"hide %llx",&p);hidden=p;matched=0;Log("HIDE");}
  else if(s.starts_with("reset")){hidden=0;Log("RESET");}
  else if(s.starts_with("status")){char bfr[100];sprintf_s(bfr,"HITS %llu",matched.load());Log(bfr);}
  else if(s.starts_with("disable")){capture=false;hidden=0;hook->disable();Log("DISABLED");return 0;}
 }
}
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD why,void*){if(why==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(h);CloseHandle(CreateThread(nullptr,0,Work,h,0,nullptr));}return TRUE;}
