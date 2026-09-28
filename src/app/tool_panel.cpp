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

ToolPanelChoice drawToolPanel(ToolKind current, bool drawing, bool dialogShowing) {
  ToolPanelChoice choice;
  ImGui::SetNextWindowPos(ImVec2(12.0f, 140.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::BeginDisabled(dialogShowing);
    if (ImGui::Button("New park")) {
      choice.Park = ParkAction::New;
    }
    ImGui::SameLine();
    if (ImGui::Button("Open park")) {
      choice.Park = ParkAction::Open;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save park")) {
      choice.Park = ParkAction::Save;
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    for (const ToolChoice &option : TOOL_CHOICES) {
      if (ImGui::RadioButton(option.Label, current == option.Kind) && current != option.Kind) {
        choice.Tool = option.Kind;
      }
    }
    if (drawing) {
      ImGui::Separator();
      ImGui::TextUnformatted("Click the last point again to finish.");
      if (ImGui::Button("Cancel path")) {
        choice.Tool = current;
      }
    }
  }
  ImGui::End();
  return choice;
}

} // namespace tpj
