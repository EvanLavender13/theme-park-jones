#include "app/ui/tooling_ui.h"
#include "app/input/cursor.h"
#include "app/input/platform_input.h"
#include "app/ui/debug_panel.h"
#include "app/ui/food_tooltip.h"
#include "app/ui/graph_view.h"
#include "app/ui/inspector_window.h"
#include "app/ui/shop_context_tooltip.h"
#include "app/ui/snap_guides.h"
#include "app/ui/time_panel.h"
#include "app/ui/tool_panel.h"
#include "legible/inspect.h"
#include "legible/park_summary.h"
#include "render/renderer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <optional>

namespace tpj {
namespace {

// Builds the Debug, Time, and Tools panels, setting the shown views from their checkboxes,
// selecting the tool the player chose, starting the park action they pressed, and undoing or
// redoing as the panel or the keys ask, unless the tool drops a hold or a drawn path instead.
void drawPanels(ParkDialogs &dialogs, ParkSession &session, const OrbitCamera &camera,
                ShownViews &shown, Interaction &interaction, HistoryAction keys) {
  const World &world = session.world();
  DebugStats stats;
  stats.SimTick = world.Tick;
  stats.Focus = camera.Focus;
  stats.Distance = camera.Distance;
  stats.Park = summarizePark(world);
  drawDebugPanel(stats, shown.Graph, shown.FoodOverlay);
  drawTimePanel(parkTime(world.Tick));
  const ToolPanelChoice choice =
      drawToolPanel(interaction.tool().Kind, !interaction.tool().Drawn.empty(), session.canUndo(),
                    session.canRedo(), dialogs.dialogShowing());
  if (choice.Tool) {
    interaction.selectTool(*choice.Tool);
  }
  dialogs.press(choice.Park);
  const HistoryAction asked = choice.History != HistoryAction::None ? choice.History : keys;
  if (asked == HistoryAction::None || interaction.dropHeld()) {
    return;
  }
  if (asked == HistoryAction::Undo) {
    session.undo();
  } else {
    session.redo();
  }
}

} // namespace

ImDrawData *ToolingUi::build(SDL_Window *window, ParkDialogs &dialogs, ParkSession &session,
                             const OrbitCamera &camera, Interaction &interaction,
                             const Preview &preview, const OverlayShape &overlayShape,
                             HistoryAction keys) {
  const CameraView view = orbitCameraView(camera);
  beginUiFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawPanels(dialogs, session, camera, Shown, interaction, keys);
  const World &world = session.world();
  if (const std::optional<InspectorSubject> &subject = interaction.subject();
      subject && !drawInspector(inspectSubject(world, *subject))) {
    interaction.forgetSubject();
  }
  if (Shown.Graph) {
    drawGraphView(world, view);
  }
  drawSnapGuides(interaction.snapGuides(), interaction.measuredLengths(world), view);
  if (Shown.FoodOverlay) {
    drawFoodTooltip(previewedWorld(world, preview), overlayShape,
                    pointUnderCursor(readCursor(window), view, OVERLAY_HEIGHT));
  }
  if (preview.Shop) {
    drawShopContextTooltip(*preview.Shop);
  }
  ImGui::Render();
  return ImGui::GetDrawData();
}

} // namespace tpj
