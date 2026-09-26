#ifndef TPJ_APP_DEBUG_PANEL_H
#define TPJ_APP_DEBUG_PANEL_H

#include "render/math.h"

#include <stdint.h>

namespace tpj {

struct DebugStats {
  uint64_t SimTick = 0;
  Vec3 Focus;
  float Distance = 0.0f;
};

// Draws the tooling panel with frame rate, simulation tick, and camera state. Call between
// ImGui::NewFrame and ImGui::Render.
void drawDebugPanel(const DebugStats &stats);

} // namespace tpj

#endif
