#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

/// @brief tiling mode, windows get laid out for you and can't be dragged or resized

namespace tiling {

inline bool enabled = false;
inline int gridCols = 0, gridRows = 0; ///< 0 = pick for me

/// @brief a forced cell, in grid units
struct Pin { int col, row, colSpan, rowSpan; };

inline std::unordered_map<std::string, Pin> pins;
inline std::unordered_map<std::string, ImVec4> layout; ///< key -> x, y, w, h for this frame

/// @brief lay out every window in keys over the work area, call once a frame before any Begin
/// pinned ones go where they're told, the rest fill free cells in order
inline void plan(const std::vector<std::string>& keys) {
    layout.clear();
    if (!enabled || keys.empty()) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 o = vp->WorkPos, sz = vp->WorkSize;

    std::vector<std::string> loose;
    int needCols = 0, needRows = 0;
    for (auto& k : keys) {
        auto it = pins.find(k);
        if (it == pins.end()) { loose.push_back(k); continue; }
        needCols = std::max(needCols, it->second.col + it->second.colSpan);
        needRows = std::max(needRows, it->second.row + it->second.rowSpan);
    }

    const int n = (int)keys.size();
    auto cell = [&](const std::string& k, int c, int r, int cs, int rs, int cols, int rows) {
        const float x0 = std::floor(o.x + sz.x * c / cols), x1 = std::floor(o.x + sz.x * (c + cs) / cols);
        const float y0 = std::floor(o.y + sz.y * r / rows), y1 = std::floor(o.y + sz.y * (r + rs) / rows);
        layout[k] = ImVec4(x0, y0, x1 - x0, y1 - y0);
    };

    // nothing forced, near square grid and the last row stretches to fill
    if (loose.size() == keys.size() && gridCols <= 0 && gridRows <= 0) {
        const int cols = (int)std::ceil(std::sqrt((double)n));
        const int rows = (n + cols - 1) / cols;
        for (int i = 0; i < n; i++) {
            const int r = i / cols;
            const int inRow = r == rows - 1 ? n - r * cols : cols;
            cell(keys[i], i % cols, r, 1, 1, inRow, rows);
        }
        return;
    }

    int cols = gridCols > 0 ? gridCols
             : gridRows > 0 ? (n + gridRows - 1) / gridRows
             : (int)std::ceil(std::sqrt((double)n));
    cols = std::max({cols, needCols, 1});
    int rows = std::max({gridRows > 0 ? gridRows : (n + cols - 1) / cols, needRows, 1});

    // grow rows till the loose ones fit
    std::vector<bool> taken;
    for (;;) {
        taken.assign(cols * rows, false);
        for (auto& k : keys) {
            auto it = pins.find(k);
            if (it == pins.end()) continue;
            const Pin& p = it->second;
            for (int r = p.row; r < p.row + p.rowSpan; r++)
                for (int c = p.col; c < p.col + p.colSpan; c++) taken[r * cols + c] = true;
        }
        if ((int)std::count(taken.begin(), taken.end(), false) >= (int)loose.size()) break;
        rows++;
    }

    for (auto& k : keys) {
        auto it = pins.find(k);
        if (it == pins.end()) continue;
        const Pin& p = it->second;
        cell(k, p.col, p.row, p.colSpan, p.rowSpan, cols, rows);
    }
    size_t li = 0;
    for (int i = 0; i < cols * rows && li < loose.size(); i++)
        if (!taken[i]) cell(loose[li++], i % cols, i / cols, 1, 1, cols, rows);
}

/// @brief call right before Begin, forces the slot and hands back the flags to lock it
inline ImGuiWindowFlags apply(const std::string& key) {
    auto it = layout.find(key);
    if (it == layout.end()) return 0;
    ImGui::SetNextWindowPos(ImVec2(it->second.x, it->second.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(it->second.z, it->second.w), ImGuiCond_Always);
    return ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
}

} // namespace tiling
