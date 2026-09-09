#pragma once
#include "imgui.h"
#include "implot.h"

#include "skinVars.h"

namespace skins {

inline void gilded() {
    auto& s = ImGui::GetStyle();

    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 0.0f;
    s.FrameRounding     = 0.0f;
    s.PopupRounding     = 0.0f;
    s.ScrollbarRounding = 0.0f;
    s.GrabRounding      = 0.0f;
    s.TabRounding       = 0.0f;

    s.WindowPadding     = ImVec2(6, 6);
    s.FramePadding      = ImVec2(4, 3);
    s.ItemSpacing       = ImVec2(6, 3);
    s.ItemInnerSpacing  = ImVec2(4, 4);
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 10.0f;

    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.WindowTitleAlign  = ImVec2(0.02f, 0.5f);

    auto rgba = [](float r, float g, float b, float a = 1.0f) {
        return ImVec4(r, g, b, a);
    };
    // near black base, terminal-green chrome, gold/amber accent - a Bloomberg
    // terminal-inspired take on toxic's near-black-base + single-accent shape
    const ImVec4 bg0     = rgba(0.020f, 0.031f, 0.020f);
    const ImVec4 bg1     = rgba(0.035f, 0.051f, 0.035f);
    const ImVec4 bg2     = rgba(0.051f, 0.071f, 0.047f);
    const ImVec4 frame   = rgba(0.071f, 0.098f, 0.063f);
    const ImVec4 hover   = rgba(0.102f, 0.145f, 0.086f);
    const ImVec4 active  = rgba(0.141f, 0.196f, 0.106f);
    const ImVec4 border  = rgba(0.353f, 0.282f, 0.086f);
    const ImVec4 grab    = rgba(0.549f, 0.443f, 0.129f);
    const ImVec4 text    = rgba(0.353f, 1.000f, 0.435f);
    const ImVec4 dim     = rgba(0.427f, 0.545f, 0.427f);
    const ImVec4 accent  = rgba(1.000f, 0.784f, 0.196f);
    const ImVec4 accentH = rgba(1.000f, 0.867f, 0.396f);

    auto& c = s.Colors;
    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = dim;
    c[ImGuiCol_WindowBg]              = bg0;
    c[ImGuiCol_ChildBg]               = bg1;
    c[ImGuiCol_PopupBg]               = rgba(0.035f, 0.051f, 0.035f, 0.94f);
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = rgba(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg]               = frame;
    c[ImGuiCol_FrameBgHovered]        = hover;
    c[ImGuiCol_FrameBgActive]         = active;
    c[ImGuiCol_TitleBg]               = bg0;
    c[ImGuiCol_TitleBgActive]         = rgba(0.129f, 0.098f, 0.039f);
    c[ImGuiCol_TitleBgCollapsed]      = rgba(0.016f, 0.024f, 0.016f, 0.75f);
    c[ImGuiCol_MenuBarBg]             = bg2;
    c[ImGuiCol_ScrollbarBg]           = rgba(0.016f, 0.024f, 0.016f, 0.50f);
    c[ImGuiCol_ScrollbarGrab]         = grab;
    c[ImGuiCol_ScrollbarGrabHovered]  = rgba(0.667f, 0.541f, 0.180f);
    c[ImGuiCol_ScrollbarGrabActive]   = rgba(0.784f, 0.643f, 0.235f);
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accentH;
    c[ImGuiCol_Button]                = frame;
    c[ImGuiCol_ButtonHovered]         = hover;
    c[ImGuiCol_ButtonActive]          = active;
    c[ImGuiCol_Header]                = rgba(0.098f, 0.086f, 0.031f);
    c[ImGuiCol_HeaderHovered]         = rgba(0.145f, 0.129f, 0.047f);
    c[ImGuiCol_HeaderActive]          = rgba(0.192f, 0.169f, 0.063f);
    c[ImGuiCol_Separator]             = border;
    c[ImGuiCol_SeparatorHovered]      = accent;
    c[ImGuiCol_SeparatorActive]       = accentH;
    c[ImGuiCol_ResizeGrip]            = rgba(1.000f, 0.784f, 0.196f, 0.20f);
    c[ImGuiCol_ResizeGripHovered]     = rgba(1.000f, 0.784f, 0.196f, 0.55f);
    c[ImGuiCol_ResizeGripActive]      = rgba(1.000f, 0.867f, 0.396f, 0.90f);
    c[ImGuiCol_Tab]                   = bg2;
    c[ImGuiCol_TabHovered]            = hover;
    c[ImGuiCol_TabActive]             = frame;
    c[ImGuiCol_TabUnfocused]          = bg0;
    c[ImGuiCol_TabUnfocusedActive]    = rgba(0.075f, 0.106f, 0.067f);
    c[ImGuiCol_PlotLines]             = text;
    c[ImGuiCol_PlotLinesHovered]      = accent;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentH;
    c[ImGuiCol_TextSelectedBg]        = rgba(accent.x, accent.y, accent.z, 0.25f);
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_TableHeaderBg]         = bg2;
    c[ImGuiCol_TableRowBg]            = bg0;
    c[ImGuiCol_TableRowBgAlt]         = bg1;
    c[ImGuiCol_TableBorderStrong]     = border;
    c[ImGuiCol_TableBorderLight]      = rgba(0.098f, 0.086f, 0.031f);

    auto& p  = ImPlot::GetStyle();
    p.PlotPadding        = ImVec2(10, 10);
    p.LabelPadding       = ImVec2(5, 5);
    p.LegendPadding      = ImVec2(5, 5);
    p.LegendInnerPadding = ImVec2(5, 5);
    p.LegendSpacing      = ImVec2(5, 0);
    p.MousePosPadding    = ImVec2(8, 8);
    p.FitPadding         = ImVec2(0.05f, 0.05f);
    p.PlotBorderSize     = 1.0f;
    p.MinorAlpha         = 0.10f;
    auto& pc = p.Colors;
    pc[ImPlotCol_FrameBg]     = bg0;
    pc[ImPlotCol_PlotBg]      = rgba(0.012f, 0.020f, 0.012f);
    pc[ImPlotCol_PlotBorder]  = border;
    pc[ImPlotCol_LegendBg]    = rgba(0.020f, 0.031f, 0.020f, 0.90f);
    pc[ImPlotCol_LegendBorder]= rgba(0.353f, 0.282f, 0.086f, 0.50f);
    pc[ImPlotCol_LegendText]  = text;
    pc[ImPlotCol_AxisText]    = rgba(0.729f, 0.647f, 0.400f);
    pc[ImPlotCol_AxisGrid]    = rgba(1.000f, 0.784f, 0.196f, 0.10f);
    pc[ImPlotCol_AxisTick]    = rgba(0.549f, 0.443f, 0.129f);

    // window snap grid, gold over the near-black ground
    gridStepX = 20.0f;
    gridStepY = 20.0f;
    gridColor = rgba(1.000f, 0.784f, 0.196f, 0.10f);

    // default series colour, terminal green so a plot added with no
    // explicit colour still reads as part of the skin
    baseColor = {0.353f, 1.000f, 0.435f, 1.0f};
}

} // namespace skins
