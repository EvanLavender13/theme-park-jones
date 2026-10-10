#include "app/ui/debug_panel.h"

#include <imgui.h>

namespace tpj {

void drawDebugPanel(const DebugStats &stats, bool &showGraph, bool &showFoodOverlay) {
  // At the top right, clear of the Tools panel at the left.
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
  }
  ImGui::End();
}

} // namespace tpj
