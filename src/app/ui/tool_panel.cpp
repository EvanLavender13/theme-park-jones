#include "app/ui/tool_panel.h"

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
                                       {"Place bench", ToolKind::PlaceBench},
                                       {"Place toilet", ToolKind::PlaceToilet},
                                       {"Furnish", ToolKind::Furnish},
                                       {"Move box", ToolKind::MoveBox},
                                       {"Edit path", ToolKind::EditPath},
                                       {"Remove point", ToolKind::RemovePoint},
                                       {"Delete", ToolKind::Delete}};

// Draws the Furnish tool's kind, spacing, and sides, and gives the choice when the player changed
// it.
std::optional<FurnishChoice> drawFurnishControls(const FurnishChoice &furnish) {
  FurnishChoice chosen = furnish;
  ImGui::TextUnformatted("Kind");
  if (ImGui::RadioButton("None", !chosen.Kind)) {
    chosen.Kind.reset();
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Benches", chosen.Kind == FurnitureKind::Bench)) {
    chosen.Kind = FurnitureKind::Bench;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Tables", chosen.Kind == FurnitureKind::Table)) {
    chosen.Kind = FurnitureKind::Table;
  }
  const double least = FURNISH_MIN_SPACING;
  const double most = FURNISH_MAX_SPACING;
  ImGui::SliderScalar("Spacing", ImGuiDataType_Double, &chosen.Spacing, &least, &most, "%.1f m",
                      ImGuiSliderFlags_AlwaysClamp);
  ImGui::TextUnformatted("Sides");
  if (ImGui::RadioButton("One side", !chosen.BothSides)) {
    chosen.BothSides = false;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Both sides", chosen.BothSides)) {
    chosen.BothSides = true;
  }
  if (chosen == furnish) {
    return std::nullopt;
  }
  return chosen;
}

} // namespace

ToolPanelChoice drawToolPanel(ToolKind current, bool drawing, bool canUndo, bool canRedo,
                              bool dialogShowing, const FurnishChoice &furnish) {
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
    ImGui::BeginDisabled(!canUndo);
    if (ImGui::Button("Undo")) {
      choice.History = HistoryAction::Undo;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!canRedo);
    if (ImGui::Button("Redo")) {
      choice.History = HistoryAction::Redo;
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    for (const ToolChoice &option : TOOL_CHOICES) {
      if (ImGui::RadioButton(option.Label, current == option.Kind) && current != option.Kind) {
        choice.Tool = option.Kind;
      }
    }
    if (current == ToolKind::Furnish) {
      ImGui::Separator();
      choice.Furnish = drawFurnishControls(furnish);
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
