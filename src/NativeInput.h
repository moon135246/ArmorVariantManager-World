#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>
namespace worlddiff::overlay {
void NativeInputInitialize(HWND game);
void NativeInputBeginFrame();
void NativeInputEndFrame();
void NativeInputClose();
bool TextField(const char* label,const char* hint,char* value,size_t capacity);
bool NativeInputOwnWindow(HWND);
bool NativeInputThread();
bool NativeInputActive();
}
