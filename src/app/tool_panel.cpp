#include "app/tool_panel.h"

#include <imgui.h>

namespace tpj {
namespace {

struct ToolChoice {
  const char *Label;
  ToolKind Kind;
};

constexpr ToolChoice TOOL_CHOICES[] = {{"Look", ToolKind::None},
                                       {"Guest path", ToolKind::GuestPath},
                                       {"Backstage path", ToolKind::BackstagePath},
                                       {"Place shop", ToolKind::PlaceShop},
                                       {"Place depot", ToolKind::PlaceDepot},
                                       {"Move box", ToolKind::MoveBox},
                                       {"Delete", ToolKind::Delete}};

} // namespace

std::optional<ToolKind> drawToolPanel(ToolKind current, bool drawing) {
  std::optional<ToolKind> chosen;
  ImGui::SetNextWindowPos(ImVec2(12.0f, 140.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    for (const ToolChoice &choice : TOOL_CHOICES) {
      if (ImGui::RadioButton(choice.Label, current == choice.Kind) && current != choice.Kind) {
        chosen = choice.Kind;
      }
    }
    if (drawing) {
      ImGui::Separator();
      ImGui::TextUnformatted("Click the last point again to finish.");
      if (ImGui::Button("Cancel path")) {
        chosen = current;
      }
    }
  }
  ImGui::End();
  return chosen;
}

} // namespace tpj
