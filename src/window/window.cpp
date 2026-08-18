/**
 * @file window.cpp
 * @brief upstream exposure to internal panel / window management systems
 */

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "implot.h"
#include "vector"
#include "string"

#include "tooling/windowManagement.h"
#include "tooling/panels.h"
#include "tooling/widgets.h"

///@name Panel Series API
///@{

/// @brief append a line series with data already in hand
/// @param panelId panel to add into (no-op if not found)
/// @param label   legend label
/// @param data    values plotted in order
void newLineSeries(std::string panelId, std::string label, std::vector<float> data) {
    auto* panel = panelManagement::findPanel(panelId);
    if (panel) panel->children.push_back({panelManagement::Line, label, data});
}

/// @brief append a bar series with data already in hand
/// @param panelId panel to add into (no-op if not found)
/// @param label   legend label
/// @param data    bar heights plotted in order
void newBarSeries(std::string panelId, std::string label, std::vector<float> data) {
    auto* panel = panelManagement::findPanel(panelId);
    if (panel) panel->children.push_back({panelManagement::Bar, label, data});
}

/// @brief unbound line overload, renders a picker button until you choose a series from the pool
/// @param panelId panel to add into (no-op if not found)
void newLineSeries(std::string panelId) {
    auto* panel = panelManagement::findPanel(panelId);
    if (panel) panel->children.push_back({panelManagement::Line, "", {}, true});
}

/// @brief unbound bar overload, renders a picker button until you choose a series from the pool
/// @param panelId panel to add into (no-op if not found)
void newBarSeries(std::string panelId) {
    auto* panel = panelManagement::findPanel(panelId);
    if (panel) panel->children.push_back({panelManagement::Bar, "", {}, true});
}

///@}

/// @brief open the main console window and run the render loop until close
/// @param title window title (currently unused, window gets titled at creation)
void showConsole(const char* title, void (skin)()) {
    static GLFWwindow* window = windowManagement::createWindow(skin);
    // must run before the first ImGui::NewFrame() below, since that's when
    // ImGui loads the .ini and fires these handlers to restore panels/widgets
    panelManagement::registerSettingsHandler();
    widgetManagement::registerSettingsHandler();

    while (!glfwWindowShouldClose(window)) {
        windowManagement::startFrame();

        panelManagement::renderPanels();
        widgetManagement::renderWindows();
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("New")) {
                if (ImGui::MenuItem("Panel")) {
                    std::string id = std::to_string(panelManagement::nextPanelId());
                    panelManagement::newPanel(id);
                    newLineSeries(id);
                }
                if (ImGui::MenuItem("Widget")) {
                    widgetManagement::newWindow(std::to_string(widgetManagement::nextWindowId()));
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        windowManagement::endFrame(window);
    }
    windowManagement::onClose(window);
}
