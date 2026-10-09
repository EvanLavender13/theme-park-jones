#ifndef TPJ_APP_UI_HOURS_PANEL_H
#define TPJ_APP_UI_HOURS_PANEL_H

#include "sim/park/intent.h"

#include <optional>

namespace tpj {

// The Hours panel's controls: the park's hours, and the hours shown, which the player chose.
struct HoursControls {
  OpeningHours Current;
  OpeningHours Chosen;
};

// What the player did in the Hours panel: the hours they chose this frame, and whether they
// pressed Apply.
struct HoursUse {
  std::optional<OpeningHours> Chosen;
  bool Apply = false;
};

// Draws the Hours panel: Opens and Closes, Chosen's two times on a 24-hour clock, each with - and +
// buttons that move it to the quarter hour before or after, and an Apply button below, disabled
// while Chosen is Current. Opens runs from 00:00 to 23:45 and Closes from 00:15 to 24:00, which is
// a closing second of 0. A + on Opens that brings it to or past Closes moves Closes to a quarter
// hour after it, and a - on Closes that brings it to or before Opens moves Opens to a quarter hour
// before it. Call between ImGui::NewFrame and ImGui::Render.
HoursUse drawHoursPanel(const HoursControls &hours);

} // namespace tpj

#endif
