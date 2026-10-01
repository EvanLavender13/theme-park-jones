#include "app/debug_panel.h"

#include <imgui.h>

#include <string_view>

namespace tpj {

void drawDebugPanel(const DebugStats &stats, bool &showGraph, bool &showFoodOverlay) {
  // At the top right, so the shop lines never run under the Tools panel at the left.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 12.0f, 12.0f),
                          ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
  if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    const ImGuiIO &io = ImGui::GetIO();
    ImGui::Text("Frame %.2f ms (%.0f fps)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Sim tick %llu", static_cast<unsigned long long>(stats.SimTick));
    ImGui::Text("Focus %.1f, %.1f", stats.Focus.X, stats.Focus.Z);
    ImGui::Text("Distance %.1f m", stats.Distance);
    ImGui::Checkbox("Graph", &showGraph);
    ImGui::Checkbox("Food overlay", &showFoodOverlay);
    if (!stats.Park.Shops.empty()) {
      ImGui::Separator();
    }
    for (const ShopLine &line : stats.Park.Shops) {
      const std::string_view limit = limitingFactorName(line.Record.Limit);
      ImGui::Text(
          "Shop %llu: stock %lld, queue %lld, on order %lld, %.*s",
          static_cast<unsigned long long>(line.Shop), static_cast<long long>(line.Record.Stock),
          static_cast<long long>(line.Record.Queue), static_cast<long long>(line.Record.OnOrder),
          static_cast<int>(limit.size()), limit.data());
    }
    ImGui::Separator();
    if (stats.Park.Guests == 0) {
      ImGui::Text("Guests 0");
    } else {
      ImGui::Text("Guests %llu, mean hunger %.2f",
                  static_cast<unsigned long long>(stats.Park.Guests), stats.Park.MeanHunger);
    }
    ImGui::Text("Waiting %llu, meals eaten %lld",
                static_cast<unsigned long long>(stats.Park.Waiting),
                static_cast<long long>(stats.Park.MealsEaten));
  }
  ImGui::End();
}

} // namespace tpj
