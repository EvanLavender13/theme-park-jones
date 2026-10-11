#include "app/ui/departures_panel.h"

#include <imgui.h>

#include <array>
#include <stddef.h>
#include <stdint.h>

namespace tpj {
namespace {

// Each reason's label, in HOME_REASONS order.
constexpr std::array<const char *, HOME_REASONS.size()> REASON_LABELS{
    "Hunger", "Thirst", "Tiredness", "Toilet", "Closing"};

// A label and its count, the counts lined up at the column, so the panel's width is known from its
// first frame and its pivot at the right holds.
void drawCount(const char *label, uint64_t count, float column) {
  ImGui::TextUnformatted(label);
  ImGui::SameLine(column);
  ImGui::Text("%6llu", static_cast<unsigned long long>(count));
}

} // namespace

void drawDeparturesPanel(const DepartureTally &tally) {
  // At the top right, below the Debug panel.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 12.0f, 240.0f),
                          ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
  if (ImGui::Begin("Departures", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    const float column = ImGui::GetCursorPosX() + ImGui::CalcTextSize("Tiredness  ").x;
    uint64_t total = 0;
    for (size_t i = 0; i < REASON_LABELS.size(); ++i) {
      drawCount(REASON_LABELS[i], tally.ByReason[i], column);
      total += tally.ByReason[i];
    }
    drawCount("Total", total, column);
  }
  ImGui::End();
}

} // namespace tpj
