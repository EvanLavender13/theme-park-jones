#ifndef TPJ_APP_UI_TOOL_PANEL_H
#define TPJ_APP_UI_TOOL_PANEL_H

#include "app/session/park_file_requests.h"
#include "tools/tools.h"

#include <optional>
#include <stdint.h>

namespace tpj {

// What the player chose in the Tools panel this frame.
struct ToolPanelChoice {
  // The tool to select: a different one, or the current one again when they cancelled a path.
  std::optional<ToolKind> Tool;
  ParkAction Park = ParkAction::None;
};

// Draws the Tools panel: the park buttons, disabled while a dialog shows, a choice for each tool,
// and while the tool is drawing a path, a hint and a Cancel path button. Call between
// ImGui::NewFrame and ImGui::Render.
ToolPanelChoice drawToolPanel(ToolKind current, bool drawing, bool dialogShowing);

} // namespace tpj

#endif
