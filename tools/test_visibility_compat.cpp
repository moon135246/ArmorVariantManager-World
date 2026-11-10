#include "../src/VisibilityQueryHook.h"
#include "../src/RenderPolicy.h"
#include "safetyhook.hpp"
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace worlddiff;
using visibility::QueryFn;
namespace {
QueryFn nativeQuery=nullptr,caller=nullptr,virtualCaller=nullptr,basicOriginal=nullptr,extendedOriginal=nullptr;
thread_local bool useVirtual=false;
bool basicValue=false,extendedValue=false,extendedEnabled=true,extendedMatch=true,nestedQuery=false;
thread_local bool handled=false;
struct Object {uint32_t unused=0;uint16_t id=0,pad=0;};
struct Fixture {bool native=true,fade=true,hidden=false,priority=true,managed=true,active=true;};
bool Filter(uintptr_t ctx,uintptr_t,bool original,bool controlled){
 handled=controlled;auto& f=*reinterpret_cast<Fixture*>(ctx);
 return f.managed&&f.active?ResolveVisibility(original,f.hidden,controlled,f.priority):original;
}
bool __fastcall Basic(uintptr_t ctx,uintptr_t b,uintptr_t q,uintptr_t d){
 auto id=reinterpret_cast<Object*>(q)->id;
 if(id>=30&&id<=35)return basicValue;
 return basicOriginal(ctx,b,q,d);
}
bool __fastcall Extended(uintptr_t ctx,uintptr_t b,uintptr_t q,uintptr_t d){
 if(extendedEnabled&&extendedMatch&&reinterpret_cast<Object*>(q)->id==500){
  if(nestedQuery){Object other;caller(ctx,b,reinterpret_cast<uintptr_t>(&other),d);}
  return extendedValue;
 }
 return extendedOriginal(ctx,b,q,d);
}
bool Run(Fixture& fixture,uint16_t id){
 Object q;q.id=id;auto fn=useVirtual?virtualCaller:caller;return fn(reinterpret_cast<uintptr_t>(&fixture),reinterpret_cast<uintptr_t>(&fixture.native),reinterpret_cast<uintptr_t>(&q),reinterpret_cast<uintptr_t>(&fixture.fade));
}
safetyhook::InlineHook AddController(QueryFn destination,QueryFn& original){
 auto made=safetyhook::InlineHook::create(reinterpret_cast<void*>(nativeQuery),reinterpret_cast<void*>(destination),safetyhook::InlineHook::StartDisabled);
 if(!made)throw std::runtime_error("controller hook creation");
 original=made->original<QueryFn>();if(!made->enable())throw std::runtime_error("controller hook enable");
 return std::move(*made);
}
}
int main(int argc,char** argv){try{
 unsigned checks=0;auto check=[&](bool okay,const char* why){checks++;if(!okay)throw std::runtime_error(why);};
 const bool basicLate=argc>1&&std::strcmp(argv[1],"basic-late")==0;
 const bool early=basicLate||(argc>1&&std::strcmp(argv[1],"early")==0);
 auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
 if(!memory)throw std::runtime_error("fixture allocation");std::memset(memory,0xcc,4096);
 // Synthetic executable code only. A normal Win64 caller, its CALL rel32, and
 // an independent post-query fade gate. No game or third-party binary fixture.
 const unsigned char callerCode[]={0x53,0x48,0x83,0xec,0x20,0x49,0x8b,0xd9,0xe8,0,0,0,0,0x22,0x03,0x48,0x83,0xc4,0x20,0x5b,0xc3};
 std::memcpy(memory,callerCode,sizeof(callerCode));int32_t distance=0x100-13;std::memcpy(memory+9,&distance,4);
 const unsigned char queryCode[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xec,0x20,0x45,0x0f,0xb7,0x50,0x04,
  0x8a,0x02,0x48,0x83,0xc4,0x20,0x41,0x5f,0x41,0x5e,0x5f,0xc3};
 std::memcpy(memory+0x100,queryCode,sizeof(queryCode));FlushInstructionCache(GetCurrentProcess(),memory,4096);
 caller=reinterpret_cast<QueryFn>(memory);nativeQuery=reinterpret_cast<QueryFn>(memory+0x100);
 auto* tableMemory=static_cast<uintptr_t*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));if(!tableMemory)throw std::runtime_error("dispatch table allocation");
 std::span<uintptr_t> dispatch(tableMemory,2);dispatch[0]=dispatch[1]=reinterpret_cast<uintptr_t>(nativeQuery);
 DWORD tableProtection=0;if(!VirtualProtect(tableMemory,4096,PAGE_READONLY,&tableProtection))throw std::runtime_error("dispatch table read-only fixture");
 std::array<uintptr_t*,2> dispatchSlots{&dispatch[0],&dispatch[1]};
 // This route uses CALL [slot], exactly the route used by the player equipment.
 // Keeping this separate from the one direct CALL catches missing coverage.
 const unsigned char virtualCode[]={0x53,0x48,0x83,0xec,0x20,0x49,0x8b,0xd9,0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0x10,0x22,0x03,0x48,0x83,0xc4,0x20,0x5b,0xc3};
 std::memcpy(memory+0x200,virtualCode,sizeof(virtualCode));auto slotAddress=reinterpret_cast<uintptr_t>(&dispatch[0]);std::memcpy(memory+0x20a,&slotAddress,8);FlushInstructionCache(GetCurrentProcess(),memory+0x200,sizeof(virtualCode));virtualCaller=reinterpret_cast<QueryFn>(memory+0x200);
 Fixture f;check(Run(f,0),"unhooked native fixture");
 safetyhook::InlineHook basic,extended;if(!basicLate)basic=AddController(Basic,basicOriginal);
 if(early)extended=AddController(Extended,extendedOriginal);
 check(!visibility::Install(reinterpret_cast<uintptr_t>(memory+8),reinterpret_cast<uintptr_t>(nativeQuery),reinterpret_cast<uintptr_t>(memory+0x100+29),Filter,{}),"missing virtual dispatch coverage is rejected");
 check(visibility::Install(reinterpret_cast<uintptr_t>(memory+8),reinterpret_cast<uintptr_t>(nativeQuery),reinterpret_cast<uintptr_t>(memory+0x100+29),Filter,dispatchSlots),"direct/virtual callers and native-body hooks install");
 check(dispatch[0]==dispatch[1]&&dispatch[0]!=reinterpret_cast<uintptr_t>(nativeQuery),"all dispatch slots redirect without replacing their containing tables");
 MEMORY_BASIC_INFORMATION tableInfo{};VirtualQuery(tableMemory,&tableInfo,sizeof(tableInfo));check(tableInfo.Protect==PAGE_READONLY,"dispatch table page protection is restored after atomic patching");
 if(basicLate)basic=AddController(Basic,basicOriginal);
 if(!early)extended=AddController(Extended,extendedOriginal);
 check(!visibility::Install(reinterpret_cast<uintptr_t>(memory+8),reinterpret_cast<uintptr_t>(nativeQuery),reinterpret_cast<uintptr_t>(memory+0x100+29),Filter,dispatchSlots),"duplicate hook installation rejected");
 for(bool virtualRoute:{false,true}){
 useVirtual=virtualRoute;
 for(bool native:{false,true})for(bool hidden:{false,true})for(bool priority:{false,true}){
  f={};f.native=native;f.hidden=hidden;f.priority=priority;
  check(Run(f,0)==(native&&!hidden)&&!handled,"native pass-through preserves native culling in either mode");
  check(Run(f,65535)==(native&&!hidden)&&!handled,"unconfigured ID is never claimed by a controller");
  for(bool value:{false,true}){
   basicValue=value;check(Run(f,30)==(priority?!hidden:value)&&handled,"basic controller show/hide respects complete priority");
   extendedValue=value;check(Run(f,500)==(priority?!hidden:value)&&handled,"custom extended ID show/hide respects complete priority");
  }
  extendedEnabled=false;check(Run(f,500)==(native&&!hidden)&&!handled,"disabled extended controller passes through");extendedEnabled=true;
  extendedMatch=false;check(Run(f,500)==(native&&!hidden)&&!handled,"unmatched conditions pass through");extendedMatch=true;
 }
 for(bool managed:{false,true})for(bool active:{false,true})for(bool value:{false,true}){
  f={};f.managed=managed;f.active=active;f.hidden=value;f.priority=true;basicValue=value;
  check(Run(f,30)==(managed&&active?!value:value),"pause or unmatched target preserves original controller result");
 }
 f={};f.hidden=true;f.priority=true;basicValue=true;check(!Run(f,30),"manager can hide controller-visible mesh");
 f.hidden=false;basicValue=false;check(Run(f,30),"manager can show controller-hidden mesh");
 f.priority=false;f.hidden=true;basicValue=true;check(Run(f,30),"controller can show manager-hidden mesh");
 f.hidden=false;basicValue=false;check(!Run(f,30),"controller can hide manager-visible mesh");
 f={};f.fade=false;basicValue=false;extendedValue=false;
 check(!Run(f,30)&&handled&&!Run(f,500)&&handled,"post-query game fade gate survives force-show overrides");
 nestedQuery=true;f={};f.priority=false;extendedValue=true;
 check(Run(f,500)&&handled,"nested unrelated native query does not misclassify outer controller result");nestedQuery=false;
 std::atomic<bool> bad=false;std::array<std::thread,4> threads;
 for(size_t i=0;i<threads.size();i++)threads[i]=std::thread([&,i]{useVirtual=i%2==0;Fixture separate;separate.priority=i%2==0;separate.hidden=false;for(int n=0;n<10000;n++)if(Run(separate,30)!=separate.priority||!handled)bad=true;});
 for(auto& t:threads)t.join();check(!bad.load(),"query traces and priorities stay independent on concurrent render threads");
 }
 f={};auto start=std::chrono::steady_clock::now();for(int n=0;n<200000;n++)Run(f,0);
 auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 std::cout<<"PASS "<<checks<<" visibility compatibility checks (basic "<<(basicLate?"after":"before")<<", extended "<<(early?"before":"after")<<" WorldDiff); 200000 native queries "<<elapsed<<" ms\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
