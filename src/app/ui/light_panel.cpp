#include "app/ui/light_panel.h"

#include "sim/clock.h"

#include <imgui.h>

#include <array>
#include <stdio.h>

namespace tpj {

void drawLightPanel(uint64_t clockSecond, LightPreview &preview) {
  // At the bottom right, clear of the Inspector at the bottom left.
  const ImVec2 display = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(display.x - 12.0f, display.y - 12.0f), ImGuiCond_FirstUseEver,
                          ImVec2(1.0f, 1.0f));
  if (ImGui::Begin("Light", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    // Following the clock while off, so turning the preview on starts from the light on screen.
    if (!preview.On) {
      preview.Second = clockSecond;
    }
    ImGui::Checkbox("Preview", &preview.On);
    auto minute = static_cast<int>(preview.Second / 60);
    std::array<char, 8> text{};
    (void)snprintf(text.data(), text.size(), "%02d:%02d", minute / 60, minute % 60);
    // ImGui shows a format with no conversion as it is. NoInput keeps Ctrl+click from parsing it.
    if (ImGui::SliderInt("Hour", &minute, 0, static_cast<int>(DAY_SECONDS / 60) - 1, text.data(),
                         ImGuiSliderFlags_NoInput)) {
      preview.On = true;
      preview.Second = static_cast<uint64_t>(minute) * 60;
    }
  }
  ImGui::End();
}

} // namespace tpj
