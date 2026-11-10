#pragma once
#include <cstdint>
#include <span>

namespace worlddiff::visibility {
using QueryFn=bool(__fastcall*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
using ResultFilter=bool(*)(uintptr_t,uintptr_t,bool,bool);

// Intercept direct CALL and virtual dispatch references, rather than competing
// for the callee's entry with Visual Controller. Object vtable identities remain
// unchanged. The body observer distinguishes controller return from pass-through.
// filter(context, query, returnedVisibility, controllerHandled).
bool Install(uintptr_t callSite,uintptr_t queryEntry,uintptr_t nativeBody,ResultFilter,std::span<uintptr_t*> dispatchSlots);
}
