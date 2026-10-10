#include "app/ui/inspector_window.h"

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

// The options the guest would weigh now, one to a row.
void drawChoices(const std::vector<ChoiceRow> &choices) {
  ImGui::TextUnformatted("Options now");
  if (!ImGui::BeginTable("choices", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    return;
  }
  for (const char *heading :
       {"Option", "Need", "Relief", "Distance", "Wait", "Commitment", "Score", "Chance"}) {
    ImGui::TableSetupColumn(heading);
  }
  ImGui::TableHeadersRow();
  for (const ChoiceRow &choice : choices) {
    ImGui::TableNextRow();
    for (const std::string *cell :
         {&choice.Option, &choice.Need, &choice.Relief, &choice.Distance, &choice.Wait,
          &choice.Commitment, &choice.Score, &choice.Probability}) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(cell->c_str());
    }
  }
  ImGui::EndTable();
}

// The Sells radio buttons and Apply, on one line.
void drawStockControls(const StockControls &stock, InspectorUse &use) {
  struct StockChoice {
    const char *Label;
    ShopStock Stock;
  };
  constexpr StockChoice CHOICES[] = {
      {"Food", ShopStock::Food}, {"Drink", ShopStock::Drink}, {"Both", ShopStock::Both}};
  ImGui::TextUnformatted("Sells");
  for (const StockChoice &choice : CHOICES) {
    ImGui::SameLine();
    if (ImGui::RadioButton(choice.Label, stock.Chosen == choice.Stock) &&
        stock.Chosen != choice.Stock) {
      use.Chosen = choice.Stock;
    }
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(stock.Chosen == stock.Current);
  if (ImGui::Button("Apply")) {
    use.Apply = true;
  }
  ImGui::EndDisabled();
}

} // namespace

InspectorUse drawInspector(const Inspection &inspection,
                           const std::optional<StockControls> &stock) {
  InspectorUse use;
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
      if (stock) {
        drawStockControls(*stock, use);
      }
      if (!inspection.Choices.empty()) {
        drawChoices(inspection.Choices);
      }
    }
  }
  ImGui::End();
  use.Open = open;
  return use;
}

} // namespace tpj
