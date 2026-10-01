#ifndef TPJ_APP_GRAPH_VIEW_H
#define TPJ_APP_GRAPH_VIEW_H

#include "render/renderer.h"
#include "sim/world.h"

namespace tpj {

// Draws the networks over the scene and behind every panel, on ImGui's background draw list:
// buildGraphOverlay of the world, the view, and ImGui's display size, lines in their kind's graph
// color, or the connector color, then nodes, anchored ones larger in the anchor color. Call
// between ImGui::NewFrame and ImGui::Render.
void drawGraphView(const World &world, const CameraView &view);

} // namespace tpj

#endif
