#ifndef TPJ_APP_UI_TIME_PANEL_H
#define TPJ_APP_UI_TIME_PANEL_H

#include "app/input/game_speed.h"
#include "sim/clock.h"

namespace tpj {

// Draws the Time panel: the park's day and its time of day on a 24-hour clock, in hours and
// minutes. Below the clock, radio buttons Pause, 1x, 2x, and 4x, with the current one marked,
// pause the speed or run it at theirs. Call between ImGui::NewFrame and ImGui::Render.
void drawTimePanel(const ParkTime &time, GameSpeed &speed);

} // namespace tpj

#endif
