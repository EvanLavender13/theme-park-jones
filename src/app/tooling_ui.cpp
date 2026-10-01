#include "app/tooling_ui.h"
#include "app/cursor.h"
#include "app/debug_panel.h"
#include "app/food_tooltip.h"
#include "app/graph_view.h"
#include "app/inspector_window.h"
#include "app/platform_input.h"
#include "app/shop_context_tooltip.h"
#include "app/tool_panel.h"
#include "legible/inspect.h"
#include "legible/park_summary.h"
#include "render/renderer.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <optional>

namespace tpj {
namespace {

// Builds the Debug and Tools panels, setting the shown views from their checkboxes, selecting the
// tool the player chose and starting the park action they pressed.
void drawPanels(ParkDialogs &dialogs, const World &world, const OrbitCamera &camera,
                ShownViews &shown, Interaction &interaction) {
  DebugStats stats;
  stats.SimTick = world.Tick;
  stats.Focus = camera.Focus;
  stats.Distance = camera.Distance;
  stats.Park = summarizePark(world);
  drawDebugPanel(stats, shown.Graph, shown.FoodOverlay);
  const ToolPanelChoice choice = drawToolPanel(
      interaction.tool().Kind, !interaction.tool().Drawn.empty(), dialogs.dialogShowing());
  if (choice.Tool) {
    interaction.selectTool(*choice.Tool);
  }
  dialogs.press(choice.Park);
}

} // namespace

void ToolingUi::build(SDL_Window *window, ParkDialogs &dialogs, const World &world,
                      const OrbitCamera &camera, Interaction &interaction, const Preview &preview) {
  const CameraView view = orbitCameraView(camera);
  beginUiFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawPanels(dialogs, world, camera, Shown, interaction);
  if (const std::optional<InspectorSubject> &subject = interaction.subject();
      subject && !drawInspector(inspectSubject(world, *subject))) {
    interaction.forgetSubject();
  }
  if (Shown.Graph) {
    drawGraphView(world, view);
  }
  if (Shown.FoodOverlay) {
    drawFoodTooltip(previewedWorld(world, preview), groundUnderCursor(readCursor(window), view));
  }
  if (preview.Shop) {
    drawShopContextTooltip(*preview.Shop);
  }
  ImGui::Render();
}

} // namespace tpj
