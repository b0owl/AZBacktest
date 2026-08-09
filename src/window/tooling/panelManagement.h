#pragma once
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_internal.h" // ImGuiSettingsHandler / ImHashStr for ini persistence
#include "implot.h"
#include "string"
#include "vector"
#include <cstdlib>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "seriesPool.h"

namespace panelManagement {

/// @brief kind of series a panel can host
enum SeriesKind { Line, Bar, Heatmap };

/// @brief a single series belonging to a panel
struct Series {
    SeriesKind kind;
    std::string label;
    std::vector<float> data;
    bool unbound = false;
    seriesPool::RGBA color;  ///< -1 = let ImPlot pick
    bool legendHidden = false; ///< true = plot but don't show in legend
    bool onY2 = false;         ///< true = plot against the secondary (right) y-axis
    std::vector<float> xs;     ///< non-empty = explicit x per point (e.g. histogram), else index-based
    float barWidth = 0.67f;    ///< only used when xs is non-empty
    int heatmapRows = 0;       ///< heatmap row count (type 2 only)
    int heatmapCols = 0;       ///< heatmap col count (type 2 only)

    // provenance, used to persist + restore this child from the .ini: which pool
    // series it was pulled from (empty = raw data added via newLine/BarSeries,
    // not restorable) and whether it came from the xyBars or per-column branch
    std::string sourceSeries;
    bool sourceXY = false;
    int sourceCol = -1;
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

/// @brief lowest id not already in use, so newly-created panels don't collide
/// with ones just restored from the .ini
inline int nextPanelId() {
    int maxId = -1;
    for (auto& p : panels) {
        char* endp = nullptr;
        long v = std::strtol(p.id.c_str(), &endp, 10);
        if (endp != p.id.c_str() && *endp == '\0' && v > maxId) maxId = (int)v;
    }
    return maxId + 1;
}

/// @brief build a panel child for one column of a pool series
inline Series buildColumnChild(seriesPool::NamedSeries& s, int col, bool onY2) {
    std::string label = s.cols() == 1 ? s.name : s.name + " [" + s.colName(col) + "]";
    // for large multi-column series (clouds), hide individual legend entries
    bool hide = s.cols() > 10 && col > 0;
    if (hide) label = "##" + s.name + "_" + std::to_string(col);
    SeriesKind kind = s.type == 2 ? Heatmap : (s.type == 1 ? Bar : Line);
    Series c{kind, label, s.data[col], false, s.color, hide, onY2};
    c.heatmapRows = s.heatmapRows;
    c.heatmapCols = s.heatmapCols;
    c.sourceSeries = s.name;
    c.sourceCol = col;
    return c;
}

/// @brief build a panel child for an xyBars pool series (histogram-style)
inline Series buildXYChild(seriesPool::NamedSeries& s, bool onY2) {
    Series c{Bar, s.name, s.data[1], false, s.color, false, onY2, s.data[0], s.barWidth};
    c.sourceSeries = s.name;
    c.sourceXY = true;
    return c;
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
                        if (s.xyBars) {
                            p.children.push_back(buildXYChild(s, s.onY2));
                        } else {
                            for (int col = 0; col < s.cols(); col++)
                                p.children.push_back(buildColumnChild(s, col, s.onY2));
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

        // check if this panel has any non-heatmap children
        bool hasStandard = false, hasHeatmap = false;
        for (auto& c : p.children) {
            if (c.unbound) continue;
            if (c.kind == Heatmap) hasHeatmap = true;
            else hasStandard = true;
        }

        if (hasStandard) {
            if (ImPlot::BeginPlot(p.id.c_str(), ImVec2(-1, hasHeatmap ? 0 : -1))) {
                ImPlotAxisFlags lockFlags = p.axesLocked ? ImPlotAxisFlags_Lock : ImPlotAxisFlags_None;
                ImPlotAxisFlags fitFlags  = p.axesAutoFit ? ImPlotAxisFlags_AutoFit : ImPlotAxisFlags_None;
                ImPlotAxisFlags baseFlags = lockFlags | fitFlags;

                ImPlotAxisFlags y2Flags = ImPlotAxisFlags_AuxDefault | baseFlags;
                bool anyOnY2 = false;
                for (auto& c : p.children) if (c.onY2) { anyOnY2 = true; break; }
                if (!anyOnY2) y2Flags |= ImPlotAxisFlags_NoDecorations;

                ImPlot::SetupAxis(ImAxis_X1, nullptr, baseFlags);
                ImPlot::SetupAxis(ImAxis_Y1, nullptr, baseFlags);
                ImPlot::SetupAxis(ImAxis_Y2, nullptr, y2Flags);

                for (auto& c : p.children) {
                    if (c.unbound || c.kind == Heatmap) continue;
                    if (c.color.isSet()) {
                        ImVec4 cv(c.color.r, c.color.g, c.color.b, c.color.a);
                        ImPlot::SetNextLineStyle(cv);
                        ImPlot::SetNextFillStyle(cv);
                    }
                    ImPlot::SetAxes(ImAxis_X1, c.onY2 ? ImAxis_Y2 : ImAxis_Y1);
                    if (c.kind == Line)
                        ImPlot::PlotLine(c.label.c_str(), c.data.data(), (int)c.data.size());
                    else if (!c.xs.empty())
                        ImPlot::PlotBars(c.label.c_str(), c.xs.data(), c.data.data(), (int)c.data.size(), c.barWidth);
                    else
                        ImPlot::PlotBars(c.label.c_str(), c.data.data(), (int)c.data.size());
                }
                ImPlot::EndPlot();
            }
        }

        for (auto& c : p.children) {
            if (c.kind != Heatmap || c.unbound) continue;
            ImPlot::PushColormap(ImPlotColormap_Viridis);
            if (ImPlot::BeginPlot(c.label.c_str(), ImVec2(-1, -1), ImPlotFlags_NoLegend)) {
                ImPlotAxisFlags hmFlags = ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Lock;
                ImPlot::SetupAxes(nullptr, nullptr, hmFlags, hmFlags);
                ImPlot::PlotHeatmap(c.label.c_str(), c.data.data(),
                    c.heatmapRows, c.heatmapCols, 0, 0, "%.1f");
                ImPlot::EndPlot();
            }
            ImPlot::PopColormap();
        }

        ImGui::End();
        if (!open) { panels.erase(panels.begin() + pi); }
        else { ++pi; }
    }
}

/// @brief split on '|', used to decode the ini's Col=/XY= lines
inline std::vector<std::string> splitPipe(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t p; (p = s.find('|', start)) != std::string::npos; start = p + 1)
        parts.push_back(s.substr(start, p - start));
    parts.push_back(s.substr(start));
    return parts;
}

inline void* iniReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    newPanel(name);
    return (void*)findPanel(name);
}

inline void iniReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
    if (!entry) return;
    Panel& p = *(Panel*)entry;
    std::string ln(line);
    size_t eq = ln.find('=');
    if (eq == std::string::npos) return;
    std::string key = ln.substr(0, eq), val = ln.substr(eq + 1);

    if (key == "Locked")  { p.axesLocked  = (val != "0"); return; }
    if (key == "AutoFit") { p.axesAutoFit = (val != "0"); return; }
    if (key != "Col" && key != "XY") return;

    auto parts = splitPipe(val);
    auto* s = seriesPool::findSeries(parts[0]);
    if (!s) return;

    if (key == "XY") {
        if (!s->xyBars || parts.size() < 2) return;
        p.children.push_back(buildXYChild(*s, parts[1] == "1"));
    } else {
        if (parts.size() < 3) return;
        int col = (int)std::strtol(parts[1].c_str(), nullptr, 10);
        if (col < 0 || col >= s->cols()) return;
        p.children.push_back(buildColumnChild(*s, col, parts[2] == "1"));
    }
}

inline void iniWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    for (auto& p : panels) {
        buf->appendf("[%s][%s]\n", handler->TypeName, p.id.c_str());
        buf->appendf("Locked=%d\n", p.axesLocked ? 1 : 0);
        buf->appendf("AutoFit=%d\n", p.axesAutoFit ? 1 : 0);
        for (auto& c : p.children) {
            if (c.sourceSeries.empty()) continue;
            if (c.sourceXY) buf->appendf("XY=%s|%d\n", c.sourceSeries.c_str(), c.onY2 ? 1 : 0);
            else            buf->appendf("Col=%s|%d|%d\n", c.sourceSeries.c_str(), c.sourceCol, c.onY2 ? 1 : 0);
        }
        buf->appendf("\n");
    }
}

/// @brief hook panels into ImGui's own .ini load/save so they auto-restore on
/// startup; call once, before the first ImGui::NewFrame()
inline void registerSettingsHandler() {
    ImGuiSettingsHandler h;
    h.TypeName   = "AZPanel";
    h.TypeHash   = ImHashStr("AZPanel");
    h.ReadOpenFn = iniReadOpen;
    h.ReadLineFn = iniReadLine;
    h.WriteAllFn = iniWriteAll;
    ImGui::AddSettingsHandler(&h);
}

} // namespace panelManagement
