#ifndef TPJ_APP_UI_NEED_TOOLTIP_H
#define TPJ_APP_UI_NEED_TOOLTIP_H

#include "legible/availability.h"
#include "sim/guests/guests.h"

namespace tpj {

// Draws a tooltip at the cursor attributing the need's availability to its facilities. Call
// between ImGui::NewFrame and ImGui::Render.
void drawNeedTooltip(GuestNeed need, const Availability &availability);

} // namespace tpj

#endif
