#include "VisibilityQueryHook.h"
#include <windows.h>
#include "safetyhook.hpp"
#include <memory>
#include <vector>

namespace worlddiff::visibility {
namespace {
QueryFn query=nullptr;
ResultFilter filter=nullptr;
uintptr_t resume=0;
safetyhook::MidHook* callerHook=nullptr;
safetyhook::MidHook* nativeHook=nullptr;
std::vector<uintptr_t*> dispatchHooks;
struct Trace;
thread_local Trace* current=nullptr;
struct Trace {
 uintptr_t context,object;bool native=false;Trace* previous=current;
 Trace(uintptr_t ctx,uintptr_t q):context(ctx),object(q){current=this;}
 ~Trace(){current=previous;}
};
void NativeBody(safetyhook::Context& ctx){
 // A controller may perform unrelated nested queries; those must not classify
 // its outer early return as native pass-through.
 if(current&&current->context==ctx.rcx&&current->object==ctx.r8)current->native=true;
}
bool __fastcall DispatchQuery(uintptr_t ctx,uintptr_t b,uintptr_t q,uintptr_t d){
 Trace trace(ctx,q);
 const bool original=query(ctx,b,q,d);
 return filter(ctx,q,original,!trace.native);
}
void CallQuery(safetyhook::Context& ctx){
 const bool visible=DispatchQuery(ctx.rcx,ctx.rdx,ctx.r8,ctx.r9);
 ctx.rax=(ctx.rax&~uintptr_t{0xff})|(visible?1:0);
 // The displaced instruction is exactly CALL rel32. Skip it after executing
 // the live target above; continue with the caller's ordinary fade checks.
 ctx.rip=resume;
}
bool ReplaceSlot(uintptr_t* slot,uintptr_t expected,uintptr_t value){
 DWORD old=0;if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old))return false;
 auto previous=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(value),reinterpret_cast<void*>(expected));
 DWORD ignored=0;VirtualProtect(slot,sizeof(*slot),old,&ignored);
 return reinterpret_cast<uintptr_t>(previous)==expected;
}
}
bool Install(uintptr_t callSite,uintptr_t queryEntry,uintptr_t nativeBody,ResultFilter resultFilter,std::span<uintptr_t*> dispatchSlots){
 if(callerHook||nativeHook||!callSite||!queryEntry||!nativeBody||!resultFilter||dispatchSlots.empty())return false;
 for(auto* slot:dispatchSlots)if(!slot||reinterpret_cast<uintptr_t>(slot)%alignof(uintptr_t)||*slot!=queryEntry)return false;
 auto observer=safetyhook::MidHook::create((void*)nativeBody,NativeBody,safetyhook::MidHook::StartDisabled);
 if(!observer)return false;
 auto caller=safetyhook::MidHook::create((void*)callSite,CallQuery,safetyhook::MidHook::StartDisabled);
 if(!caller||caller->original_bytes().size()!=5)return false;
 auto observerOwner=std::make_unique<safetyhook::MidHook>(std::move(*observer));
 auto callerOwner=std::make_unique<safetyhook::MidHook>(std::move(*caller));
 std::vector<uintptr_t*> changed;changed.reserve(dispatchSlots.size());
 query=reinterpret_cast<QueryFn>(queryEntry);filter=resultFilter;resume=callSite+5;
 if(!observerOwner->enable())return false;
 auto restore=[&]{for(auto* slot:changed)ReplaceSlot(slot,reinterpret_cast<uintptr_t>(&DispatchQuery),queryEntry);};
 for(auto* slot:dispatchSlots){
  if(!ReplaceSlot(slot,queryEntry,reinterpret_cast<uintptr_t>(&DispatchQuery))){restore();return false;}
  changed.push_back(slot);
 }
 if(!callerOwner->enable()){restore();return false;}
 // The plugin has process lifetime, just like the existing render/input hooks.
 dispatchHooks=std::move(changed);nativeHook=observerOwner.release();callerHook=callerOwner.release();
 return true;
}
}
