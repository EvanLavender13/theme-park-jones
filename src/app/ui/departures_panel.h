#ifndef TPJ_APP_UI_DEPARTURES_PANEL_H
#define TPJ_APP_UI_DEPARTURES_PANEL_H

#include "sim/guests/guests.h"

namespace tpj {

// Draws the Departures panel: a row for each reason in HOME_REASONS order, with its count from the
// tally, and a Total row with their sum. Call between ImGui::NewFrame and ImGui::Render.
void drawDeparturesPanel(const DepartureTally &tally);

} // namespace tpj

#endif
