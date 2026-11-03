#pragma once
#include "imgui.h"
#include <algorithm>

namespace worlddiff::overlay::paper {
constexpr ImU32 ink = IM_COL32(22,22,22,255), white = IM_COL32_WHITE;
constexpr ImU32 sheet = IM_COL32(245,245,242,255), chip = IM_COL32(228,228,224,255);
constexpr ImU32 rule = IM_COL32(215,215,210,255), muted = IM_COL32(88,88,82,255);
constexpr ImU32 yellow = IM_COL32(255,245,0,255), red = IM_COL32(146,44,41,255);
inline thread_local bool active = false;

// Freeze the choice for one draw pass, including popups opened by the panel.
// Pop everything before the basic theme or another ImGui window is rendered.
class Scope {
    bool previous = active;
    int colors = 0, vars = 0;
    void color(ImGuiCol key, ImU32 value) { ImGui::PushStyleColor(key, value); ++colors; }
    void var(ImGuiStyleVar key, float value) { ImGui::PushStyleVar(key, value); ++vars; }
public:
    explicit Scope(bool enabled) {
        active = enabled;
        if (!enabled) return;
        color(ImGuiCol_Text, ink); color(ImGuiCol_TextDisabled, muted);
        color(ImGuiCol_WindowBg, sheet); color(ImGuiCol_ChildBg, white); color(ImGuiCol_PopupBg, white);
        color(ImGuiCol_TitleBg, ink); color(ImGuiCol_TitleBgActive, ink); color(ImGuiCol_TitleBgCollapsed, ink);
        color(ImGuiCol_Border, IM_COL32(165,165,158,255)); color(ImGuiCol_BorderShadow, 0);
        color(ImGuiCol_Separator, rule); color(ImGuiCol_SeparatorHovered, muted); color(ImGuiCol_SeparatorActive, ink);
        color(ImGuiCol_FrameBg, sheet); color(ImGuiCol_FrameBgHovered, chip); color(ImGuiCol_FrameBgActive, rule);
        color(ImGuiCol_Button, chip); color(ImGuiCol_ButtonHovered, rule); color(ImGuiCol_ButtonActive, IM_COL32(191,191,184,255));
        color(ImGuiCol_Header, chip); color(ImGuiCol_HeaderHovered, rule); color(ImGuiCol_HeaderActive, yellow);
        color(ImGuiCol_CheckboxSelectedBg, ink); color(ImGuiCol_CheckMark, ink); color(ImGuiCol_SliderGrab, muted); color(ImGuiCol_SliderGrabActive, ink);
        color(ImGuiCol_ScrollbarBg, sheet); color(ImGuiCol_ScrollbarGrab, rule); color(ImGuiCol_ScrollbarGrabHovered, muted); color(ImGuiCol_ScrollbarGrabActive, ink);
        color(ImGuiCol_ResizeGrip, rule); color(ImGuiCol_ResizeGripHovered, muted); color(ImGuiCol_ResizeGripActive, ink);
        color(ImGuiCol_TableHeaderBg, chip); color(ImGuiCol_TableBorderLight, rule); color(ImGuiCol_TableBorderStrong, muted);
        color(ImGuiCol_NavCursor, muted); color(ImGuiCol_TextSelectedBg, IM_COL32(255,230,0,110));
        color(ImGuiCol_ModalWindowDimBg, IM_COL32(24,24,20,105));
        var(ImGuiStyleVar_WindowRounding, 0); var(ImGuiStyleVar_ChildRounding, 0);
        var(ImGuiStyleVar_FrameRounding, 0); var(ImGuiStyleVar_PopupRounding, 0);
        var(ImGuiStyleVar_ScrollbarRounding, 0); var(ImGuiStyleVar_GrabRounding, 0);
        var(ImGuiStyleVar_FrameBorderSize, 1); var(ImGuiStyleVar_ChildBorderSize, 0);
        var(ImGuiStyleVar_DisabledAlpha, .48f);
    }
    ~Scope() {
        ImGui::PopStyleVar(vars); ImGui::PopStyleColor(colors); active = previous;
    }
};

inline void CutRect(ImDrawList* draw, ImVec2 lo, ImVec2 hi, ImU32 color, float cut) {
    cut = std::min({cut, (hi.x-lo.x)*.5f, (hi.y-lo.y)*.5f});
    if (cut <= 0) return;
    ImVec2 points[] = {lo, {hi.x,lo.y}, {hi.x,hi.y-cut}, {hi.x-cut,hi.y}, {lo.x,hi.y}};
    draw->AddConvexPolyFilled(points, 5, ImGui::GetColorU32(color));
}
// Draw on the viewport without creating a window or capturing game input.
inline void DrawStartupNotice(const char* menuKey, float scale) {
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

    constexpr const char* title = "世界版差分管理器 1.2.3";
    constexpr const char* status = "已加载";
    constexpr const char* pauseKey = "Ctrl+F9";
    float statusW = width(bold,16,status)+12;
    float menuW = width(bold,17,menuKey)+12, pauseW = width(bold,17,pauseKey)+12;
    float openW = width(regular,18,"打开面板"), stopW = width(regular,18,"暂停控制");
    float cardW = std::max({262.f, 19+width(bold,20,title)+12+statusW+9,
                            9+menuW+5+openW+28+pauseW+5+stopW+9});
    rect(0,0,cardW,35,ink);
    rect(0,35,cardW,33,sheet);
    rect(9,10,4,16,yellow);
    text(19,17.5f,bold,20,white,title);
    rect(cardW-9-statusW,8,statusW,20,yellow);
    text(cardW-9-statusW+6,17.5f,bold,16,ink,status);

    float next = cardW-9-stopW-5-pauseW;
    float divider = (9+menuW+5+openW+next)*.5f;
    auto keycap = [&](float x,float w,const char* key) {
        rect(x,42,w,19,chip);
        rect(x,60,w,1,IM_COL32(168,168,162,255));
        text(x+6,51,bold,17,ink,key);
    };
    keycap(9,menuW,menuKey);
    text(9+menuW+5,51,regular,18,ink,"打开面板");
    rect(divider,44,1,14,IM_COL32(183,183,177,255));
    keycap(next,pauseW,pauseKey);
    text(next+pauseW+5,51,regular,18,ink,"暂停控制");
    draw->AddRect(origin,point(cardW,68),IM_COL32(100,100,94,255),0,scale);
}

}

