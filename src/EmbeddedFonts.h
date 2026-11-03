#pragma once
#include <windows.h>
#include <initializer_list>
#include <filesystem>
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

inline std::filesystem::path WindowsFontDirectory() {
    wchar_t path[MAX_PATH]{};
    const UINT length = GetWindowsDirectoryW(path, MAX_PATH);
    if (!length || length >= MAX_PATH) return {};
    return std::filesystem::path(path) / L"Fonts";
}

enum FontChoice { Unavailable, Embedded, YaHei };

inline FontChoice LoadPanelFonts(ImFontAtlas& atlas, const ImWchar* ranges = nullptr,
                                 const std::filesystem::path& directory = WindowsFontDirectory()) {
    if (!directory.empty()) {
        ImFontConfig config;
        config.Flags |= ImFontFlags_NoLoadError;
        auto addFile = [&](const wchar_t* name) {
            const auto path = (directory / name).u8string();
            return atlas.AddFontFromFileTTF(reinterpret_cast<const char*>(path.c_str()), 22.0f, &config, ranges);
        };
        if (addFile(L"msyh.ttc")) {
            // Version 1.2.5 used the regular face for headings when bold was unavailable.
            addFile(L"msyhbd.ttc");
            return YaHei;
        }
    }
    for (int id : {WORLDDIFF_FONT_REGULAR, WORLDDIFF_FONT_BOLD}) {
        Resource resource = Read(id);
        if (!resource.data || !resource.size) return Unavailable;
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false; // Module resources outlive the atlas.
        if (!atlas.AddFontFromMemoryTTF(resource.data, static_cast<int>(resource.size),
                                        22.0f, &config, ranges)) return Unavailable;
    }
    return Embedded;
}
}
