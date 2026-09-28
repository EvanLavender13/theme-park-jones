#ifndef TPJ_APP_TOOL_PANEL_H
#define TPJ_APP_TOOL_PANEL_H

#include "tools/tools.h"

#include <optional>

namespace tpj {

// Draws the Tools panel, with a choice for each tool, and while the tool is drawing a path, a hint
// and a Cancel path button. Returns the tool to select: a different one the player chose, or the
// current one again when they cancelled. Call between ImGui::NewFrame and ImGui::Render.
std::optional<ToolKind> drawToolPanel(ToolKind current, bool drawing);

} // namespace tpj

#endif
