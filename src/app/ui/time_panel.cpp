#include "app/ui/time_panel.h"

#include <imgui.h>

namespace tpj {

void drawTimePanel(const ParkTime &time) {
  // At the top center, clear of the Tools panel at the left and the Debug panel at the right.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 12.0f),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));
  if (ImGui::Begin("Time", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Day %llu  %02llu:%02llu", static_cast<unsigned long long>(time.Day),
                static_cast<unsigned long long>(time.Second / 3600),
                static_cast<unsigned long long>(time.Second % 3600 / 60));
  }
  ImGui::End();
}

} // namespace tpj
