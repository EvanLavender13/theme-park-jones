#ifndef TPJ_APP_DEBUG_PANEL_H
#define TPJ_APP_DEBUG_PANEL_H

#include "render/math.h"
#include "sim/entity_key.h"
#include "sim/operations/operations.h"

#include <stdint.h>
#include <vector>

namespace tpj {

// A shop box's key and its inspection record, as the Debug panel lists them.
struct ShopLine {
  EntityKey Shop = NULL_KEY;
  ShopRecord Record;
};

struct DebugStats {
  uint64_t SimTick = 0;
  Vec3 Focus;
  float Distance = 0.0f;
  std::vector<ShopLine> Shops;
};

// Draws the tooling panel with frame rate, simulation tick, camera state, the Graph checkbox,
// which sets showGraph, and a line for each shop's record. Call between ImGui::NewFrame and
// ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph);

} // namespace tpj

#endif
