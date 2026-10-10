#ifndef TPJ_APP_UI_NEED_TOOLTIP_H
#define TPJ_APP_UI_NEED_TOOLTIP_H

#include "render/paving_overlay.h"
#include "sim/guests/guests.h"
#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// Draws a tooltip at the cursor attributing the need's availability at the overlay place of the
// point under it to its facilities, and nothing when there is no point or it has no overlay place
// in the shape. Call between ImGui::NewFrame and ImGui::Render.
void drawNeedTooltip(const World &world, GuestNeed need, const OverlayShape &shape,
                     std::optional<ParkPoint> point);

} // namespace tpj

#endif
