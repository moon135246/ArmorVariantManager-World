#pragma once
#include <windows.h>
#include <initializer_list>
#include "imgui.h"
#include "FontIds.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace worlddiff::fonts {
inline constexpr wchar_t Family[] = L"Noto Sans SC";

struct Resource {
    void* data;
    DWORD size;
};

inline Resource Read(int id) {
    HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    HRSRC entry = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!entry) return {};
    HGLOBAL loaded = LoadResource(module, entry);
    return {LockResource(loaded), SizeofResource(module, entry)};
}

inline bool LoadPanelFonts(ImFontAtlas& atlas, const ImWchar* ranges = nullptr) {
    for (int id : {WORLDDIFF_FONT_REGULAR, WORLDDIFF_FONT_BOLD}) {
        Resource resource = Read(id);
        if (!resource.data || !resource.size) return false;
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false; // Module resources outlive the atlas.
        if (!atlas.AddFontFromMemoryTTF(resource.data, static_cast<int>(resource.size),
                                        22.0f, &config, ranges)) return false;
    }
    return true;
}
}
