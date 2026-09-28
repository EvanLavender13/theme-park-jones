#ifndef TPJ_APP_TOOL_PANEL_H
#define TPJ_APP_TOOL_PANEL_H

#include "tools/tools.h"

#include <optional>
#include <stdint.h>

namespace tpj {

// A park button the player pressed in the Tools panel.
enum class ParkAction : uint8_t { None, New, Open, Save };

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
