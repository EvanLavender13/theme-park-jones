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
  uint64_t Guests = 0;
  double MeanHunger = 0.0;
  uint64_t Waiting = 0;
  int64_t MealsEaten = 0;
};

// Draws the tooling panel with frame rate, simulation tick, camera state, the Graph checkbox,
// which sets showGraph, the Food overlay checkbox, which sets showFoodOverlay, a line for each
// shop's record, the guest count and mean hunger, and the guests waiting and meals eaten. Call
// between ImGui::NewFrame and ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph, bool &showFoodOverlay);

} // namespace tpj

#endif
