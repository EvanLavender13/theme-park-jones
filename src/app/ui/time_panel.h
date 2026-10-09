#ifndef TPJ_APP_UI_TIME_PANEL_H
#define TPJ_APP_UI_TIME_PANEL_H

#include "sim/clock.h"

namespace tpj {

// Draws the Time panel: the park's day and its time of day on a 24-hour clock, in hours and
// minutes. Call between ImGui::NewFrame and ImGui::Render.
void drawTimePanel(const ParkTime &time);

} // namespace tpj

#endif
