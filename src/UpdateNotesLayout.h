#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <string>
#include <vector>

namespace worlddiff::notes {
inline bool Cjk(unsigned c) {
    return (c >= 0x2e80 && c <= 0x9fff) || (c >= 0xac00 && c <= 0xd7af) ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0xff01 && c <= 0xffef) ||
           (c >= 0x20000 && c <= 0x323af);
}
inline bool Blank(unsigned c) { return c == ' ' || c == '\t' || c == 0x3000; }
inline bool Closing(unsigned c) {
    switch(c) {
    case ',': case '.': case ';': case ':': case '!': case '?': case ')': case ']': case '}': case '%':
    case 0x3001: case 0x3002: case 0xff0c: case 0xff0e: case 0xff1b: case 0xff1a:
    case 0xff01: case 0xff1f: case 0xff09: case 0x3011: case 0x300b: case 0x300d:
    case 0x300f: case 0x2019: case 0x201d: return true;
    default: return false;
    }
}
inline bool Opening(unsigned c) {
    switch(c) {
    case '(': case '[': case '{': case 0xff08: case 0x3010: case 0x300a:
    case 0x300c: case 0x300e: case 0x2018: case 0x201c: return true;
    default: return false;
    }
}
inline bool BreakBetween(unsigned previous, unsigned current) {
    if (Blank(previous) || Blank(current)) return true;
    if (Closing(current) || Opening(previous)) return false;
    return Cjk(previous) || Cjk(current);
}

// Local to the notes viewport: keep Latin words intact, allow CJK character
// boundaries, and avoid stranded punctuation when possible. Only display text
// receives soft line breaks; the user's Markdown and cache remain untouched.
inline std::string WrapText(const std::string& text, float width, ImFont* font, float size) {
    struct Glyph { size_t offset; unsigned code; float advance; };
    std::vector<Glyph> glyphs;
    const char* begin = text.data();
    const char* end = begin + text.size();
    for (const char* p = begin; p < end;) {
        unsigned c = 0;
        int bytes = ImTextCharFromUtf8(&c, p, end);
        if (bytes <= 0) break;
        glyphs.push_back({size_t(p - begin), c, font->CalcTextSizeA(size, FLT_MAX, 0, p, p + bytes).x});
        p += bytes;
    }
    std::string result;
    result.reserve(text.size());
    size_t start = 0;
    while (start < glyphs.size()) {
        size_t stop = start, lastBreak = start;
        float used = 0;
        bool hardBreak = false;
        for (; stop < glyphs.size(); ++stop) {
            const auto& g = glyphs[stop];
            if (g.code == '\n') { hardBreak = true; break; }
            if (stop > start && BreakBetween(glyphs[stop - 1].code, g.code)) lastBreak = stop;
            if (stop > start && used + g.advance > width) {
                if (lastBreak > start) stop = lastBreak;
                break;
            }
            used += g.advance;
        }
        size_t visibleEnd = stop;
        while (visibleEnd > start && Blank(glyphs[visibleEnd - 1].code)) --visibleEnd;
        const size_t byteEnd = visibleEnd < glyphs.size() ? glyphs[visibleEnd].offset : text.size();
        result.append(text, glyphs[start].offset, byteEnd - glyphs[start].offset);
        start = stop + (hardBreak ? 1 : 0);
        if (!hardBreak) while (start < glyphs.size() && Blank(glyphs[start].code)) ++start;
        if (hardBreak || start < glyphs.size()) result += '\n';
    }
    return result;
}
}
