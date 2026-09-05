#pragma once
#include <GLFW/glfw3.h>
#include "imgui.h"

struct px {
    GLFWwindow* win;
    float px; float py;
    float r; float g; float b; float a;
};

/// @brief draw a pixel on the foreground list, over every imgui window
inline void draw_px(px pix) {
    ImGui::GetForegroundDrawList()->AddRectFilled(
        ImVec2(pix.px, pix.py), ImVec2(pix.px + 1, pix.py + 1),
        ImGui::GetColorU32(ImVec4(pix.r, pix.g, pix.b, pix.a)));
}

/// @brief same pixel but on the background list, so it lands under every imgui
/// window instead of on top of them
inline void draw_px_under(px pix) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(pix.px, pix.py), ImVec2(pix.px + 1, pix.py + 1),
        ImGui::GetColorU32(ImVec4(pix.r, pix.g, pix.b, pix.a)));
}