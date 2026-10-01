#include "app/inspector_window.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace tpj {
namespace {

void drawRows(const std::vector<InspectorRow> &rows) {
  if (!ImGui::BeginTable("rows", 2, ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  for (const InspectorRow &row : rows) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.Label.c_str());
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(row.Value.c_str());
  }
  ImGui::EndTable();
}

// The guest's last choice, one option to a row, the picked one marked in the first column.
void drawChoices(const std::vector<ChoiceRow> &choices) {
  ImGui::TextUnformatted("Last choice");
  if (!ImGui::BeginTable("choices", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  for (const char *heading :
       {"", "Option", "Relief", "Distance", "Wait", "Commitment", "Score", "Chance"}) {
    ImGui::TableSetupColumn(heading);
  }
  ImGui::TableHeadersRow();
  for (const ChoiceRow &choice : choices) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(choice.Picked ? ">" : "");
    for (const std::string *cell : {&choice.Option, &choice.Relief, &choice.Distance, &choice.Wait,
                                    &choice.Commitment, &choice.Score, &choice.Probability}) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(cell->c_str());
    }
  }
  ImGui::EndTable();
}

} // namespace

bool drawInspector(const Inspection &inspection) {
  bool open = true;
  // At the bottom left, clear of the Tools panel above it and the Debug panel at the top right.
  ImGui::SetNextWindowPos(ImVec2(12.0f, ImGui::GetIO().DisplaySize.y - 12.0f),
                          ImGuiCond_FirstUseEver, ImVec2(0.0f, 1.0f));
  if (ImGui::Begin("Inspector", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(inspection.Title.c_str());
    if (inspection.Gone) {
      ImGui::TextUnformatted("No longer in the park");
    } else {
      drawRows(inspection.Rows);
      if (!inspection.Choices.empty()) {
        drawChoices(inspection.Choices);
      }
    }
  }
  ImGui::End();
  return open;
}

} // namespace tpj
