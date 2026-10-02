#ifndef TPJ_APP_UI_DEBUG_PANEL_H
#define TPJ_APP_UI_DEBUG_PANEL_H

#include "legible/park_summary.h"
#include "render/math.h"

#include <stdint.h>

namespace tpj {

struct DebugStats {
  uint64_t SimTick = 0;
  Vec3 Focus;
  float Distance = 0.0f;
  ParkSummary Park;
};

// Draws the tooling panel with frame rate, simulation tick, camera state, the Graph checkbox,
// which sets showGraph, the Food overlay checkbox, which sets showFoodOverlay, and the park
// summary's lines. Call between ImGui::NewFrame and ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph, bool &showFoodOverlay);

} // namespace tpj

#endif
