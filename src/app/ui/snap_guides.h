#ifndef TPJ_APP_UI_SNAP_GUIDES_H
#define TPJ_APP_UI_SNAP_GUIDES_H

#include "render/renderer.h"
#include "tools/tools.h"

#include <span>

namespace tpj {

// Draws the guides over the scene and behind every panel, on ImGui's background draw list, each
// kind in its color, and each measured length at its segment's middle, in meters to two decimals.
// Call between ImGui::NewFrame and ImGui::Render.
void drawSnapGuides(std::span<const SnapGuide> guides, std::span<const MeasuredLength> lengths,
                    const CameraView &view);

} // namespace tpj

#endif
