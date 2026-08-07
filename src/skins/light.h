#pragma once
#include "imgui.h"
#include "implot.h"

namespace skins {

inline void light() {
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
    const ImVec4 bg0     = rgba(0.98f, 0.98f, 0.98f);
    const ImVec4 bg1     = rgba(0.95f, 0.95f, 0.95f);
    const ImVec4 bg2     = rgba(0.92f, 0.92f, 0.92f);
    const ImVec4 frame   = rgba(0.88f, 0.88f, 0.88f);
    const ImVec4 hover   = rgba(0.80f, 0.80f, 0.80f);
    const ImVec4 active  = rgba(0.72f, 0.72f, 0.72f);
    const ImVec4 border  = rgba(0.75f, 0.75f, 0.75f);
    const ImVec4 grab    = rgba(0.65f, 0.65f, 0.65f);
    const ImVec4 text    = rgba(0.10f, 0.10f, 0.10f);
    const ImVec4 dim     = rgba(0.45f, 0.45f, 0.45f);
    const ImVec4 accent  = rgba(0.20f, 0.42f, 0.68f);
    const ImVec4 accentH = rgba(0.30f, 0.52f, 0.78f);

    auto& c = s.Colors;
    c[ImGuiCol_Text]                  = text;
    c[ImGuiCol_TextDisabled]          = dim;
    c[ImGuiCol_WindowBg]              = bg0;
    c[ImGuiCol_ChildBg]               = bg1;
    c[ImGuiCol_PopupBg]               = rgba(0.95f, 0.95f, 0.95f, 0.94f);
    c[ImGuiCol_Border]                = border;
    c[ImGuiCol_BorderShadow]          = rgba(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg]               = frame;
    c[ImGuiCol_FrameBgHovered]        = hover;
    c[ImGuiCol_FrameBgActive]         = active;
    c[ImGuiCol_TitleBg]               = bg2;
    c[ImGuiCol_TitleBgActive]         = bg1;
    c[ImGuiCol_TitleBgCollapsed]      = rgba(0.85f, 0.85f, 0.85f, 0.75f);
    c[ImGuiCol_MenuBarBg]             = bg2;
    c[ImGuiCol_ScrollbarBg]           = rgba(0.90f, 0.90f, 0.90f, 0.50f);
    c[ImGuiCol_ScrollbarGrab]         = grab;
    c[ImGuiCol_ScrollbarGrabHovered]  = rgba(0.55f, 0.55f, 0.55f);
    c[ImGuiCol_ScrollbarGrabActive]   = rgba(0.45f, 0.45f, 0.45f);
    c[ImGuiCol_CheckMark]             = accent;
    c[ImGuiCol_SliderGrab]            = accent;
    c[ImGuiCol_SliderGrabActive]      = accentH;
    c[ImGuiCol_Button]                = frame;
    c[ImGuiCol_ButtonHovered]         = hover;
    c[ImGuiCol_ButtonActive]          = active;
    c[ImGuiCol_Header]                = rgba(0.82f, 0.82f, 0.82f);
    c[ImGuiCol_HeaderHovered]         = rgba(0.74f, 0.74f, 0.74f);
    c[ImGuiCol_HeaderActive]          = rgba(0.66f, 0.66f, 0.66f);
    c[ImGuiCol_Separator]             = border;
    c[ImGuiCol_SeparatorHovered]      = accent;
    c[ImGuiCol_SeparatorActive]       = accentH;
    c[ImGuiCol_ResizeGrip]            = rgba(0.60f, 0.60f, 0.60f, 0.25f);
    c[ImGuiCol_ResizeGripHovered]     = rgba(0.45f, 0.45f, 0.45f, 0.67f);
    c[ImGuiCol_ResizeGripActive]      = rgba(0.35f, 0.35f, 0.35f, 0.95f);
    c[ImGuiCol_Tab]                   = bg2;
    c[ImGuiCol_TabHovered]            = hover;
    c[ImGuiCol_TabActive]             = frame;
    c[ImGuiCol_TabUnfocused]          = bg1;
    c[ImGuiCol_TabUnfocusedActive]    = rgba(0.86f, 0.86f, 0.86f);
    c[ImGuiCol_PlotLines]             = accent;
    c[ImGuiCol_PlotLinesHovered]      = accentH;
    c[ImGuiCol_PlotHistogram]         = accent;
    c[ImGuiCol_PlotHistogramHovered]  = accentH;
    c[ImGuiCol_TextSelectedBg]        = rgba(accent.x, accent.y, accent.z, 0.25f);
    c[ImGuiCol_NavHighlight]          = accent;
    c[ImGuiCol_DragDropTarget]        = accent;
    c[ImGuiCol_TableHeaderBg]         = bg2;
    c[ImGuiCol_TableRowBg]            = bg0;
    c[ImGuiCol_TableRowBgAlt]         = bg1;
    c[ImGuiCol_TableBorderStrong]     = border;
    c[ImGuiCol_TableBorderLight]      = rgba(0.85f, 0.85f, 0.85f);

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
    pc[ImPlotCol_PlotBg]      = rgba(1.0f, 1.0f, 1.0f);
    pc[ImPlotCol_PlotBorder]  = border;
    pc[ImPlotCol_LegendBg]    = rgba(0.98f, 0.98f, 0.98f, 0.90f);
    pc[ImPlotCol_LegendBorder]= rgba(0.75f, 0.75f, 0.75f, 0.50f);
    pc[ImPlotCol_LegendText]  = text;
    pc[ImPlotCol_AxisText]    = rgba(0.30f, 0.30f, 0.30f);
    pc[ImPlotCol_AxisGrid]    = rgba(0.0f, 0.0f, 0.0f, 0.08f);
    pc[ImPlotCol_AxisTick]    = rgba(0.55f, 0.55f, 0.55f);
}

} // namespace skins
