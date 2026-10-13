#ifndef TPJ_APP_UI_GRAPH_VIEW_H
#define TPJ_APP_UI_GRAPH_VIEW_H

#include "render/graph_overlay.h"

namespace tpj {

// Draws the networks over the scene and behind every panel, on ImGui's background draw list: the
// overlay's lines in their kind's graph color, or the connector color, then its nodes, anchored
// ones larger in the anchor color. Call between ImGui::NewFrame and ImGui::Render.
void drawGraphView(const GraphOverlay &overlay);

} // namespace tpj

#endif
