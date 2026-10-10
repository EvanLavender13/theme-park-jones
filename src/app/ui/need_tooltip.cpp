#include "app/ui/need_tooltip.h"

#include "legible/availability.h"
#include "legible/needs.h"
#include "render/paving_overlay.h"

#include <imgui.h>

#include <string_view>

namespace tpj {

void drawNeedTooltip(const World &world, GuestNeed need, const OverlayShape &shape,
                     std::optional<ParkPoint> point) {
  if (!point) {
    return;
  }
  const std::optional<Place> place = overlayPlace(shape, GroundPoint{point->X, point->Z});
  if (!place || !ImGui::BeginTooltip()) {
    return;
  }
  const Availability availability = availabilityAt(world, needAvailability(need), *place);
  const std::string_view name = needAvailabilityName(need);
  ImGui::Text("%.*s %.3f", static_cast<int>(name.size()), name.data(), availability.Value);
  if (availability.Contributions.empty()) {
    ImGui::TextUnformatted("No supplied facility reachable");
  } else if (ImGui::BeginTable("need", 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    for (const char *heading : {"Facility", "Relief", "Route", "Wait", "Time", "Term"}) {
      ImGui::TableSetupColumn(heading);
    }
    ImGui::TableHeadersRow();
    for (const AvailabilityContribution &contribution : availability.Contributions) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(contribution.Source));
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", contribution.Relief);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f m", contribution.Distance);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f min", contribution.Wait);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f min", contribution.Time);
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", contribution.Term);
    }
    ImGui::EndTable();
  }
  ImGui::EndTooltip();
}

} // namespace tpj
