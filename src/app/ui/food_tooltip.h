#ifndef TPJ_APP_UI_FOOD_TOOLTIP_H
#define TPJ_APP_UI_FOOD_TOOLTIP_H

#include "render/paving_overlay.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// Draws a tooltip at the cursor attributing the food availability at the overlay place of the
// point under it to its shops, and nothing when there is no point or it has no overlay place in
// the shape. Call between ImGui::NewFrame and ImGui::Render.
void drawFoodTooltip(const World &world, const OverlayShape &shape, std::optional<ParkPoint> point);

} // namespace tpj

#endif
