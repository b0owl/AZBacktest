#pragma once
#include "imgui.h"

/// style values imgui and implot have no slot for. every skin sets all of them
/// so switching skins carries the whole look, not just the imgui palette

namespace skins {

/// @brief rgba colour, all -1 means "let the renderer pick"
struct RGBA {
    float r=-1, g=-1, b=-1, a=-1;
    bool isSet() const { return r >= 0; }
};

/// @brief default colour handed to any series added without an explicit one
inline RGBA baseColor = {};

inline float gridStepX = 20.0f; ///< snap grid pitch across, in pixels, <=0 disables snapping on x
inline float gridStepY = 20.0f; ///< snap grid pitch down, in pixels, <=0 disables snapping on y
inline ImVec4 gridColor = ImVec4(1.0f, 1.0f, 1.0f, 0.08f); ///< grid line colour


} // namespace skins
