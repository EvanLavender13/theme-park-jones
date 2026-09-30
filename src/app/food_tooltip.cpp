#include "app/food_tooltip.h"

#include "legible/food.h"
#include "render/food_overlay.h"

#include <imgui.h>

namespace tpj {

void drawFoodTooltip(const World &world, std::optional<ParkPoint> ground) {
  if (!ground) {
    return;
  }
  const std::optional<FoodAvailability> available =
      foodNear(world, GroundPoint{ground->X, ground->Z}, OVERLAY_BAND);
  if (!available || !ImGui::BeginTooltip()) {
    return;
  }
  const FoodAvailability &food = *available;
  ImGui::Text("Food %.3f", food.Value);
  if (food.Contributions.empty()) {
    ImGui::TextUnformatted("No supplied shop reachable");
  } else if (ImGui::BeginTable("food", 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    for (const char *heading : {"Shop", "Relief", "Route", "Wait", "Time", "Term"}) {
      ImGui::TableSetupColumn(heading);
    }
    ImGui::TableHeadersRow();
    for (const FoodContribution &contribution : food.Contributions) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(contribution.Shop));
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", contribution.Relief);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f m", contribution.Distance);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f s", contribution.Wait);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f s", contribution.Time);
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", contribution.Term);
    }
    ImGui::EndTable();
  }
  ImGui::EndTooltip();
}

} // namespace tpj
