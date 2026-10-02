#ifndef TPJ_APP_UI_TOOLING_UI_H
#define TPJ_APP_UI_TOOLING_UI_H

#include "app/input/interaction.h"
#include "app/input/orbit_camera.h"
#include "app/session/park_dialogs.h"
#include "legible/preview.h"
#include "sim/world.h"

struct ImDrawData;
struct SDL_Window;

namespace tpj {

// The views over the scene that the Debug panel's checkboxes show.
struct ShownViews {
  bool Graph = false;
  bool FoodOverlay = false;
};

// The frame's panels and tooltips, and which views over the scene the Debug panel's checkboxes
// show.
class ToolingUi {
public:
  explicit ToolingUi(ShownViews shown) : Shown(shown) {}

  [[nodiscard]] const ShownViews &shown() const { return Shown; }

  // Builds the frame's ImGui frame and gives its draw data: the Debug and Tools panels, which set
  // the shown views from their checkboxes, select the tool the player chose, and start the park
  // action they pressed; the Inspector while the interaction has a subject, forgetting it when
  // closed; the graph over the scene while it is shown; the food tooltip at the cursor while the
  // overlay is, on the preview's candidate when it has one; and a shop ghost's context.
  [[nodiscard]] ImDrawData *build(SDL_Window *window, ParkDialogs &dialogs, const World &world,
                                  const OrbitCamera &camera, Interaction &interaction,
                                  const Preview &preview);

private:
  ShownViews Shown;
};

} // namespace tpj

#endif
