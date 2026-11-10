#pragma once
#include "imgui.h"
#include "UiTheme.h"
#include <algorithm>

namespace worlddiff::overlay::paper {
constexpr ImU32 rgb(unsigned hex) {
    return IM_COL32((hex>>16)&255,(hex>>8)&255,hex&255,255);
}

// Named roles keep signal-colored text, interactive states and surfaces independent.
struct Palette {
    ImU32 text, muted, danger, window;
    ImU32 panel, actionArea, dialog, list;
    ImU32 field, input, title, onTitle;
    ImU32 caption, signal, onSignal, signalHover;
    ImU32 signalHeld, primary, onPrimary, primaryHover;
    ImU32 primaryHeld, button, hover, held;
    ImU32 selection, onSelection, header, fileIdle;
    ImU32 fileActive, rule, edge, topRule;
    ImU32 radio, scrollbar, toast, keycap;
};
inline constexpr Palette light = {
    .text=rgb(0x161616), .muted=rgb(0x585852), .danger=rgb(0x922c29),
    .window=rgb(0xf5f5f2), .panel=rgb(0xffffff), .actionArea=rgb(0xf5f5f2),
    .dialog=rgb(0xf5f5f2), .list=rgb(0xffffff), .field=rgb(0xf5f5f2),
    .input=rgb(0xffffff), .title=rgb(0x161616), .onTitle=rgb(0xffffff),
    .caption=rgb(0xe4e4e0), .signal=rgb(0xfff500), .onSignal=rgb(0x161616),
    .signalHover=rgb(0xf6e800), .signalHeld=rgb(0xe6d000), .primary=rgb(0x161616),
    .onPrimary=rgb(0xffffff), .primaryHover=rgb(0x3a3a36), .primaryHeld=rgb(0x585852),
    .button=rgb(0xe4e4e0), .hover=rgb(0xd7d7d2), .held=rgb(0xbfbfb8),
    .selection=rgb(0x161616), .onSelection=rgb(0xffffff), .header=rgb(0x161616),
    .fileIdle=rgb(0xededea), .fileActive=rgb(0x161616), .rule=rgb(0xd7d7d2),
    .edge=rgb(0xa5a59e), .topRule=rgb(0x161616), .radio=rgb(0x161616),
    .scrollbar=rgb(0xd7d7d2), .toast=rgb(0xf5f5f2), .keycap=rgb(0xe4e4e0),
};
inline constexpr Palette night = {
    .text=rgb(0xf2f3ec), .muted=rgb(0xb1b4aa), .danger=rgb(0xffa6a0),
    .window=rgb(0x191919), .panel=rgb(0x222222), .actionArea=rgb(0x292a27),
    .dialog=rgb(0x222222), .list=rgb(0x191919), .field=rgb(0x20211f),
    .input=rgb(0x1b1c19), .title=rgb(0x101110), .onTitle=rgb(0xf5f6ed),
    .caption=rgb(0xbbbdaf), .signal=rgb(0xb8ff3d), .onSignal=rgb(0x151614),
    .signalHover=rgb(0xc8ff69), .signalHeld=rgb(0x9de334), .primary=rgb(0xdadcd3),
    .onPrimary=rgb(0x141613), .primaryHover=rgb(0xe6e8df), .primaryHeld=rgb(0xbfc2b7),
    .button=rgb(0x2e2e2e), .hover=rgb(0x45473f), .held=rgb(0x53564b),
    .selection=rgb(0x383a35), .onSelection=rgb(0xf5f6ed), .header=rgb(0x3c3e37),
    .fileIdle=rgb(0x2c2d29), .fileActive=rgb(0x40423a), .rule=rgb(0x30312e),
    .edge=rgb(0x65685d), .topRule=rgb(0x696b62), .radio=rgb(0xb8ff3d),
    .scrollbar=rgb(0x64665d), .toast=rgb(0x242521), .keycap=rgb(0x393b35),
};
inline constexpr Palette glass = {
    .text=rgb(0x182c49), .muted=rgb(0x526780), .danger=rgb(0x954b51),
    .window=IM_COL32(198,215,232,240), .panel=IM_COL32(247,250,255,225), .actionArea=rgb(0xf6faff),
    .dialog=rgb(0xe8f2f5), .list=rgb(0xf4f8fc), .field=rgb(0xfcfdff),
    .input=rgb(0xfbfdff), .title=rgb(0xe5effa), .onTitle=rgb(0x213e5e),
    .caption=rgb(0x496380), .signal=rgb(0x3079ed), .onSignal=rgb(0xffffff),
    .signalHover=rgb(0x438bf4), .signalHeld=rgb(0x245dbf), .primary=rgb(0x3176dc),
    .onPrimary=rgb(0xffffff), .primaryHover=rgb(0x478ef6), .primaryHeld=rgb(0x275fb8),
    .button=rgb(0xf0f6fb), .hover=rgb(0xdcebf9), .held=rgb(0xcadff4),
    .selection=rgb(0xbddaf6), .onSelection=rgb(0x153b69), .header=rgb(0xdbeaf7),
    .fileIdle=rgb(0xeff4f8), .fileActive=rgb(0xc3e0fc), .rule=rgb(0xc5d4e3),
    .edge=rgb(0xa9bfd5), .topRule=rgb(0xb8cddd), .radio=rgb(0x246dd8),
    .scrollbar=rgb(0xaabbcd), .toast=rgb(0xe4f0f5), .keycap=rgb(0xf4f8fc),
};
inline const Palette& PaletteFor(app::UiTheme theme) {
    if(theme==app::UiTheme::Glass)return glass;
    return theme==app::UiTheme::ClassicBlack ? night : light;
}
inline thread_local const Palette* colors = &light;
inline bool IsGlass() { return colors==&glass; }
inline bool IsNight() { return colors==&night; }
inline thread_local bool active = false;

// Freeze the choice for one draw pass, including popups opened by the panel.
// Pop everything before the basic theme or another ImGui window is rendered.
class Scope {
    bool previous = active;
    const Palette* previousColors = colors;
    int pushedColors = 0, vars = 0;
    void color(ImGuiCol key, ImU32 value) { ImGui::PushStyleColor(key, value); ++pushedColors; }
    void var(ImGuiStyleVar key, float value) { ImGui::PushStyleVar(key, value); ++vars; }
public:
    explicit Scope(app::UiTheme theme,float scale=1.f) {
        active = theme!=app::UiTheme::Basic;
        if (!active) return;
        const auto& c = PaletteFor(theme);
        colors = &c;
        color(ImGuiCol_Text, c.text); color(ImGuiCol_TextDisabled, c.muted);
        color(ImGuiCol_WindowBg, c.window); color(ImGuiCol_ChildBg, IsGlass()?0:c.panel);
        color(ImGuiCol_PopupBg, c.panel); color(ImGuiCol_TitleBg, c.title);
        color(ImGuiCol_TitleBgActive, c.title); color(ImGuiCol_TitleBgCollapsed, c.title);
        color(ImGuiCol_Border, c.edge); color(ImGuiCol_Separator, c.rule);
        color(ImGuiCol_SeparatorHovered, c.muted); color(ImGuiCol_SeparatorActive, c.text);
        color(ImGuiCol_FrameBg, c.field); color(ImGuiCol_FrameBgHovered, IsNight()?c.hover:c.button);
        color(ImGuiCol_FrameBgActive, IsNight()?c.held:c.rule); color(ImGuiCol_Button, c.button);
        color(ImGuiCol_ButtonHovered, c.hover); color(ImGuiCol_ButtonActive, c.held);
        color(ImGuiCol_Header, c.button); color(ImGuiCol_HeaderHovered, c.hover);
        color(ImGuiCol_HeaderActive, IsGlass()?c.held:IsNight()?c.hover:c.signal); color(ImGuiCol_CheckboxSelectedBg, c.primary);
        color(ImGuiCol_CheckMark, c.radio); color(ImGuiCol_SliderGrab, c.muted);
        color(ImGuiCol_SliderGrabActive, c.text); color(ImGuiCol_ScrollbarBg, c.window);
        color(ImGuiCol_ScrollbarGrab, c.scrollbar); color(ImGuiCol_ScrollbarGrabHovered, c.muted);
        color(ImGuiCol_ScrollbarGrabActive, c.text); color(ImGuiCol_ResizeGrip, c.rule);
        color(ImGuiCol_ResizeGripHovered, c.muted); color(ImGuiCol_ResizeGripActive, c.text);
        color(ImGuiCol_TableHeaderBg, c.button); color(ImGuiCol_TableBorderLight, c.rule);
        color(ImGuiCol_TableBorderStrong, c.muted); color(ImGuiCol_NavCursor, c.muted);
        color(ImGuiCol_BorderShadow, 0);
        color(ImGuiCol_TextSelectedBg, IsGlass()?IM_COL32(62,134,229,85):IsNight()?IM_COL32(142,190,60,90):IM_COL32(255,230,0,110));
        color(ImGuiCol_ModalWindowDimBg, IsGlass()?IM_COL32(40,63,89,122):IsNight()?IM_COL32(0,0,0,155):IM_COL32(24,24,20,105));
        var(ImGuiStyleVar_WindowRounding, IsGlass()?14*scale:0); var(ImGuiStyleVar_ChildRounding, IsGlass()?14*scale:0);
        var(ImGuiStyleVar_FrameRounding, IsGlass()?9*scale:0); var(ImGuiStyleVar_PopupRounding, IsGlass()?14*scale:0);
        var(ImGuiStyleVar_ScrollbarRounding, IsGlass()?8*scale:0); var(ImGuiStyleVar_GrabRounding, IsGlass()?6*scale:0);
        var(ImGuiStyleVar_FrameBorderSize, 1); var(ImGuiStyleVar_ChildBorderSize, 0);
        var(ImGuiStyleVar_DisabledAlpha, .48f);
    }
    ~Scope() {
        ImGui::PopStyleVar(vars); ImGui::PopStyleColor(pushedColors); active = previous; colors = previousColors;
    }
};

inline void CutRect(ImDrawList* draw, ImVec2 lo, ImVec2 hi, ImU32 color, float cut) {
    cut = std::min({cut, (hi.x-lo.x)*.5f, (hi.y-lo.y)*.5f});
    if (cut <= 0) return;
    ImVec2 points[] = {lo, {hi.x,lo.y}, {hi.x,hi.y-cut}, {hi.x-cut,hi.y}, {lo.x,hi.y}};
    draw->AddConvexPolyFilled(points, 5, ImGui::GetColorU32(color));
}
// Tint a rounded ImGui primitive without losing its antialiased edge coverage.
inline ImU32 Blend(ImU32 a,ImU32 b,float t) {
    auto first=ImGui::ColorConvertU32ToFloat4(a),last=ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32({first.x+(last.x-first.x)*t,first.y+(last.y-first.y)*t,
                                          first.z+(last.z-first.z)*t,1.f});
}
inline void GlassSurface(ImDrawList* d,ImVec2 lo,ImVec2 hi,float r,ImU32 top,ImU32 bottom,ImU32 border,float opacity=1.f) {
    if(hi.x<=lo.x||hi.y<=lo.y)return;
    int begin=d->VtxBuffer.Size;
    d->AddRectFilled(lo,hi,ImGui::GetColorU32(IM_COL32_WHITE,opacity),r);
    for(int i=begin;i<d->VtxBuffer.Size;++i){
        auto& v=d->VtxBuffer[i];
        float t=std::clamp((v.pos.y-lo.y)/(hi.y-lo.y),0.f,1.f);
        v.col=(Blend(top,bottom,t)&~IM_COL32_A_MASK)|(v.col&IM_COL32_A_MASK);
    }
    d->AddRect(lo,hi,ImGui::GetColorU32(border),r);
}
// Draw on the viewport without creating a window or capturing game input.
inline void DrawStartupNotice(const char* menuKey, float scale,const Palette& c) {
    auto* draw = ImGui::GetForegroundDrawList();
    auto& fonts = ImGui::GetIO().Fonts->Fonts;
    ImFont* regular = fonts[0];
    ImFont* bold = fonts.Size > 1 ? fonts[1] : regular;
    const ImVec2 origin(26, 28);
    auto point = [&](float x, float y) { return ImVec2(origin.x+x*scale, origin.y+y*scale); };
    auto width = [&](ImFont* font, float size, const char* label) {
        return font->CalcTextSizeA(size*scale, FLT_MAX, 0, label).x/scale;
    };
    auto rect = [&](float x, float y, float w, float h, ImU32 color) {
        draw->AddRectFilled(point(x,y), point(x+w,y+h), color);
    };
    auto text = [&](float x, float centerY, ImFont* font, float size, ImU32 color, const char* label) {
        draw->AddText(font, size*scale, point(x,centerY-size*.5f), color, label);
    };

    constexpr const char* title = "世界版差分管理器 1.3.2";
    constexpr const char* status = "已加载";
    constexpr const char* pauseKey = "Ctrl+F9";
    float statusW = width(bold,16,status)+12;
    float menuW = width(bold,17,menuKey)+12, pauseW = width(bold,17,pauseKey)+12;
    float openW = width(regular,18,"打开面板"), stopW = width(regular,18,"暂停控制");
    float cardW = std::max({262.f, 19+width(bold,20,title)+12+statusW+9,
                            9+menuW+5+openW+28+pauseW+5+stopW+9});
    if(&c==&glass){
        GlassSurface(draw,origin,point(cardW,68),14*scale,rgb(0xf0f8ff),rgb(0xd9ebeb),rgb(0xffffff));
        rect(1,34,cardW-2,1,c.edge);
        text(12,17.5f,bold,20,c.onTitle,"世界版差分管理器");
        float versionX=12+width(bold,20,"世界版差分管理器")+7;
        text(versionX,17.5f,regular,15,c.muted,"1.3.2");
        GlassSurface(draw,point(cardW-9-statusW,8),point(cardW-9,28),10*scale,rgb(0x478ff8),rgb(0x255fc2),c.primary);
    }else{
    rect(0,0,cardW,35,c.title);
    rect(0,35,cardW,33,c.toast);
    rect(9,10,4,16,c.signal);
    text(19,17.5f,bold,20,c.onTitle,title);
    rect(cardW-9-statusW,8,statusW,20,c.signal);
    }
    text(cardW-9-statusW+6,17.5f,bold,16,c.onSignal,status);

    float next = cardW-9-stopW-5-pauseW;
    float divider = (9+menuW+5+openW+next)*.5f;
    auto keycap = [&](float x,float w,const char* key) {
        if(&c==&glass)GlassSurface(draw,point(x,42),point(x+w,61),5*scale,rgb(0xffffff),c.keycap,c.edge);
        else{
        rect(x,42,w,19,c.keycap);
        rect(x,60,w,1,(&c==&night?c.edge:IM_COL32(168,168,162,255)));
        }
        text(x+6,51,bold,17,c.text,key);
    };
    keycap(9,menuW,menuKey);
    text(9+menuW+5,51,regular,18,c.text,"打开面板");
    rect(divider,44,1,14,(&c==&night?c.edge:IM_COL32(183,183,177,255)));
    keycap(next,pauseW,pauseKey);
    text(next+pauseW+5,51,regular,18,c.text,"暂停控制");
    if(&c!=&glass)draw->AddRect(origin,point(cardW,68),IM_COL32(100,100,94,255),0,scale);
}

}

