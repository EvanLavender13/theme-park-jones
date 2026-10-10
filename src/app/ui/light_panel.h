#ifndef TPJ_APP_UI_LIGHT_PANEL_H
#define TPJ_APP_UI_LIGHT_PANEL_H

#include <stdint.h>

namespace tpj {

// The Light panel's preview: whether it lights the scene, and the second of the day it shows.
struct LightPreview {
  bool On = false;
  uint64_t Second = 0;
};

// Draws the Light panel: a Preview checkbox, which sets preview.On, and an Hour slider showing
// preview.Second's time of day as HH:MM, in whole minutes. While the preview is off, the slider
// follows clockSecond. Dragging the slider turns the preview on. Call between ImGui::NewFrame and
// ImGui::Render.
void drawLightPanel(uint64_t clockSecond, LightPreview &preview);

} // namespace tpj

#endif
