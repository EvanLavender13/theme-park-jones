#ifndef TPJ_APP_UI_TOOLING_UI_H
#define TPJ_APP_UI_TOOLING_UI_H

#include "app/input/input_map.h"
#include "app/input/interaction.h"
#include "app/input/orbit_camera.h"
#include "app/session/park_dialogs.h"
#include "app/session/park_session.h"
#include "legible/preview.h"
#include "render/paving_overlay.h"

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
  // action they pressed. It undoes or redoes as the Tools panel's buttons or the frame's keys ask,
  // unless the tool held or had drawn something, which it drops instead. It also builds the
  // Inspector while the interaction has a subject, forgetting it when
  // closed; the graph over the scene while it is shown; the snap's guides and the measured
  // lengths; the food tooltip at the cursor while the overlay is, on the preview's candidate when
  // it has one, over the overlay's shape; and a shop ghost's context.
  [[nodiscard]] ImDrawData *build(SDL_Window *window, ParkDialogs &dialogs, ParkSession &session,
                                  const OrbitCamera &camera, Interaction &interaction,
                                  const Preview &preview, const OverlayShape &overlayShape,
                                  HistoryAction keys);

private:
  ShownViews Shown;
};

} // namespace tpj

#endif
