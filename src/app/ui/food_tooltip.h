#ifndef TPJ_APP_UI_FOOD_TOOLTIP_H
#define TPJ_APP_UI_FOOD_TOOLTIP_H

#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// Draws a tooltip at the cursor attributing foodNear's availability for the ground under it, with
// the reach OVERLAY_BAND, to its shops, and nothing when there is no ground or no availability.
// Call between ImGui::NewFrame and ImGui::Render.
void drawFoodTooltip(const World &world, std::optional<ParkPoint> ground);

} // namespace tpj

#endif
