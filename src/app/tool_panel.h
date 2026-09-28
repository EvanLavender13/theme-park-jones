#ifndef TPJ_APP_TOOL_PANEL_H
#define TPJ_APP_TOOL_PANEL_H

#include "tools/tools.h"

namespace tpj {

// Draws the Tools panel, with a choice for each tool. Returns true when the player chose a
// different tool, which it writes to kind. Call between ImGui::NewFrame and ImGui::Render.
bool drawToolPanel(ToolKind &kind);

} // namespace tpj

#endif
