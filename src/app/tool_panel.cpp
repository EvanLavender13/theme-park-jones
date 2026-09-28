#include "app/tool_panel.h"

#include <imgui.h>

namespace tpj {
namespace {

struct ToolChoice {
  const char *Label;
  ToolKind Kind;
};

constexpr ToolChoice TOOL_CHOICES[] = {{"Look", ToolKind::None},
                                       {"Place shop", ToolKind::PlaceShop},
                                       {"Place depot", ToolKind::PlaceDepot},
                                       {"Move box", ToolKind::MoveBox},
                                       {"Delete", ToolKind::Delete}};

} // namespace

bool drawToolPanel(ToolKind &kind) {
  bool changed = false;
  ImGui::SetNextWindowPos(ImVec2(12.0f, 140.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (const ToolChoice &choice : TOOL_CHOICES) {
      if (ImGui::RadioButton(choice.Label, kind == choice.Kind) && kind != choice.Kind) {
        kind = choice.Kind;
        changed = true;
      }
    }
  }
  ImGui::End();
  return changed;
}

} // namespace tpj
