#ifndef TPJ_APP_UI_INSPECTOR_WINDOW_H
#define TPJ_APP_UI_INSPECTOR_WINDOW_H

#include "legible/inspect.h"

namespace tpj {

// Draws the Inspector window: the inspection's title, then that its entity is gone or its rows,
// and the options a guest would weigh now as a table, one to a row. Returns false when the player
// closes it. Call between ImGui::NewFrame and ImGui::Render.
bool drawInspector(const Inspection &inspection);

} // namespace tpj

#endif
