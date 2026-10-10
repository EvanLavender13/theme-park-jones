#ifndef TPJ_APP_UI_DEBUG_PANEL_H
#define TPJ_APP_UI_DEBUG_PANEL_H

#include "render/math.h"
#include "sim/guests/guests.h"

#include <optional>
#include <stdint.h>

namespace tpj {

struct DebugStats {
  uint64_t SimTick = 0;
  uint64_t Guests = 0;
  Vec3 Focus;
  float Distance = 0.0f;
};

// Draws the tooling panel with frame rate, simulation tick, the number of guests, camera state,
// the Graph checkbox, which sets showGraph, and the Overlay row of radio buttons Off, Food, Drink,
// Rest, and Toilet, which sets overlay to no need or to that need. Call between ImGui::NewFrame and
// ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph, std::optional<GuestNeed> &overlay);

} // namespace tpj

#endif
