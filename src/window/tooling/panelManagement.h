#pragma once
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "implot.h"
#include "string"
#include "vector"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "seriesPool.h"

namespace panelManagement {

/// @brief kind of series a panel can host
enum SeriesKind { Line, Bar };

/// @brief a single series belonging to a panel
struct Series {
    SeriesKind kind;
    std::string label;
    std::vector<float> data;
    bool unbound = false;
    seriesPool::RGBA color;  ///< -1 = let ImPlot pick
    bool legendHidden = false; ///< true = plot but don't show in legend
    bool onY2 = false;         ///< true = plot against the secondary (right) y-axis
};

/// @brief a panel window that owns any number of child series
struct Panel {
    std::string id;                  ///< unique id; also drives the ImGui window title
    std::vector<Series> children;    ///< series currently attached to this panel
    bool axesLocked = false;         ///< true = x/y axes can't be panned or zoomed
    bool axesAutoFit = true;         ///< true = x/y axes auto-fit to data extents every frame
};

/// @brief all active panels, rendered each frame by renderPanels()
inline std::vector<Panel> panels;

/// @brief find a panel by id, or nullptr if it doesn't exist
inline Panel* findPanel(const std::string& id) {
    for (auto& p : panels) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

/// @brief register a new panel; no-op if `id` already exists
inline void newPanel(std::string id) {
    if (findPanel(id) == nullptr) {
        panels.push_back({id, {}});
    }
}

/// @brief render every registered panel for the current ImGui frame
inline void renderPanels() {
    for (int pi = 0; pi < (int)panels.size(); ) {
        auto& p = panels[pi];
        // Title from series names, fall back to id
        std::string winTitle;
        if (p.children.empty()) {
            winTitle = "Plot " + p.id;
        } else {
            for (size_t i = 0; i < p.children.size(); i++) {
                if (p.children[i].legendHidden) continue;
                if (!winTitle.empty()) winTitle += ", ";
                winTitle += p.children[i].label;
            }
            if (winTitle.empty()) winTitle = "Plot " + p.id;
        }
        winTitle += "###panel_" + p.id;
        bool open = true;
        ImGui::Begin(winTitle.c_str(), &open);

        std::string btnId = "+ Add Series##" + p.id;
        if (ImGui::Button(btnId.c_str())) ImGui::OpenPopup(btnId.c_str());
        if (ImGui::BeginPopup(btnId.c_str())) {
            if (seriesPool::pool.empty()) {
                ImGui::TextDisabled("(no series loaded)"); // if no series was initilized, show this text
            } else { // if a series was initlized, ->
                for (auto& s : seriesPool::pool) {
                    if (ImGui::Selectable(s.name.c_str())) {
                        for (int col = 0; col < s.cols(); col++) {
                            std::string label = s.cols() == 1
                                ? s.name
                                : s.name + " [" + s.colName(col) + "]";
                            // for large multi-column series (clouds), hide individual legend entries
                            bool hide = s.cols() > 10 && col > 0;
                            if (hide) label = "##" + s.name + "_" + std::to_string(col);
                            p.children.push_back({s.type == 1 ? Bar : Line, label, s.data[col], false, s.color, hide, s.onY2});
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        std::string lockId = "Lock axes##lock_" + p.id;
        ImGui::Checkbox(lockId.c_str(), &p.axesLocked);

        ImGui::SameLine();
        std::string autoFitId = "Auto-fit axes##autofit_" + p.id;
        ImGui::Checkbox(autoFitId.c_str(), &p.axesAutoFit);

        // Per-series toggle for which y-axis (left/Y1 vs right/Y2) each
        // series plots against, only shown once there's more than one
        // series to make the choice meaningful
        if (p.children.size() > 1) {
            ImGui::SameLine();
            ImGui::TextDisabled("Y2:");
            for (size_t i = 0; i < p.children.size(); i++) {
                auto& c = p.children[i];
                if (c.legendHidden) continue;
                ImGui::SameLine();
                std::string cbId = c.label + "##y2_" + p.id + "_" + std::to_string(i);
                ImGui::Checkbox(cbId.c_str(), &c.onY2);
            }
        }

        if (ImPlot::BeginPlot(p.id.c_str(), ImVec2(-1, -1))) {
            // "Lock axes" / "Auto-fit axes" above toggle these per-panel;
            // axes start unlocked + auto-fitting by default
            ImPlotAxisFlags lockFlags = p.axesLocked ? ImPlotAxisFlags_Lock : ImPlotAxisFlags_None;
            ImPlotAxisFlags fitFlags  = p.axesAutoFit ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None;
            ImPlotAxisFlags baseFlags = lockFlags | fitFlags;

            // Y2 stays hidden/auto-fit unless a series actually opts into it
            ImPlotAxisFlags y2Flags = ImPlotAxisFlags_AuxDefault | baseFlags;
            bool anyOnY2 = false;
            for (auto& c : p.children) if (c.onY2) { anyOnY2 = true; break; }
            if (!anyOnY2) y2Flags |= ImPlotAxisFlags_NoDecorations;

            ImPlot::SetupAxis(ImAxis_X1, nullptr, baseFlags);
            ImPlot::SetupAxis(ImAxis_Y1, nullptr, baseFlags);
            ImPlot::SetupAxis(ImAxis_Y2, nullptr, y2Flags);

            for (auto& c : p.children) {
                if (c.unbound) continue;
                if (c.color.isSet()) {
                    ImVec4 cv(c.color.r, c.color.g, c.color.b, c.color.a);
                    ImPlot::SetNextLineStyle(cv);
                    ImPlot::SetNextFillStyle(cv);
                }
                ImPlot::SetAxes(ImAxis_X1, c.onY2 ? ImAxis_Y2 : ImAxis_Y1);
                if (c.kind == Line)
                    ImPlot::PlotLine(c.label.c_str(), c.data.data(), (int)c.data.size());
                else
                    ImPlot::PlotBars(c.label.c_str(), c.data.data(), (int)c.data.size());
            }
            ImPlot::EndPlot();
        }

        ImGui::End();
        if (!open) { panels.erase(panels.begin() + pi); }
        else { ++pi; }
    }
}

} // namespace panelManagement
