#include "app/ui/time_panel.h"

#include <imgui.h>

#include <array>
#include <stdint.h>
#include <stdio.h>

namespace tpj {

void drawTimePanel(const ParkTime &time, GameSpeed &speed) {
  // At the top center, clear of the Tools panel at the left and the Debug panel at the right.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 12.0f),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));
  if (ImGui::Begin("Time", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Day %llu  %02llu:%02llu", static_cast<unsigned long long>(time.Day),
                static_cast<unsigned long long>(time.Second / 3600),
                static_cast<unsigned long long>(time.Second % 3600 / 60));
    if (ImGui::RadioButton("Pause", speed.paused())) {
      speed.pause();
    }
    for (const uint32_t run : std::array<uint32_t, 3>{1, 2, 4}) {
      std::array<char, 8> label{};
      (void)snprintf(label.data(), label.size(), "%ux", static_cast<unsigned>(run));
      ImGui::SameLine();
      if (ImGui::RadioButton(label.data(), !speed.paused() && speed.running() == run)) {
        speed.run(run);
      }
    }
  }
  ImGui::End();
}

} // namespace tpj
