#include "app/ui/tooling_ui.h"
#include "app/input/cursor.h"
#include "app/input/platform_input.h"
#include "app/ui/debug_panel.h"
#include "app/ui/departures_panel.h"
#include "app/ui/graph_view.h"
#include "app/ui/hours_panel.h"
#include "app/ui/inspector_window.h"
#include "app/ui/need_tooltip.h"
#include "app/ui/shop_context_tooltip.h"
#include "app/ui/snap_guides.h"
#include "app/ui/time_panel.h"
#include "app/ui/tool_panel.h"
#include "legible/inspect.h"
#include "render/renderer.h"
#include "sim/guests/guests.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <optional>

namespace tpj {
namespace {

// Builds the Debug, Time, Hours, Departures, and Tools panels, setting the speed from the Time
// panel's buttons, the interaction's chosen hours from the Hours panel and applying them, the
// arrival curve for the hours it shows, the Departures panel, the shown views from their
// checkboxes, selecting the tool the player chose, starting the park action they pressed, and
// undoing or redoing as the panel or the keys ask, unless the tool drops a hold or a drawn path
// instead.
void drawPanels(ParkDialogs &dialogs, ParkSession &session, const OrbitCamera &camera,
                ShownViews &shown, Interaction &interaction, GameSpeed &speed, PanelData &panels,
                HistoryAction keys) {
  const World &world = session.world();
  DebugStats stats;
  stats.SimTick = world.Tick;
  stats.Guests = panels.guestCount(world);
  stats.Focus = camera.Focus;
  stats.Distance = camera.Distance;
  drawDebugPanel(stats, shown.Graph, shown.Overlay);
  drawTimePanel(parkTime(world.Tick), speed);
  interaction.settleHours(world);
  const OpeningHours hours = parkHours(world);
  const OpeningHours shownHours = interaction.hoursChoice().value_or(hours);
  const HoursUse hoursUse =
      drawHoursPanel(HoursControls{hours, shownHours}, panels.arrivalDay(world, shownHours),
                     parkTime(world.Tick).Second);
  if (hoursUse.Chosen) {
    interaction.chooseHours(*hoursUse.Chosen);
  }
  if (hoursUse.Apply) {
    interaction.applyHours(session.commands());
  }
  drawDeparturesPanel(departureTally(world));
  const ToolPanelChoice choice =
      drawToolPanel(interaction.tool().Kind, !interaction.tool().Drawn.empty(), session.canUndo(),
                    session.canRedo(), dialogs.dialogShowing(), interaction.tool().Furnish);
  if (choice.Tool) {
    interaction.selectTool(*choice.Tool);
  }
  if (choice.Furnish) {
    interaction.chooseFurnish(*choice.Furnish);
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
  drawPanels(dialogs, session, camera, Shown, interaction, Speed, Panels, keys);
  drawLightPanel(parkTime(session.world().Tick).Second, Light);
  const World &world = session.world();
  interaction.settleStock();
  if (const std::optional<InspectorSubject> subject = interaction.subject()) {
    std::optional<StockControls> stock;
    if (subject->Kind == SubjectKind::Shop) {
      if (const std::optional<ShopStock> current = PanelData::shopStock(world, subject->Key)) {
        stock = StockControls{*current, interaction.stockChoice().value_or(*current)};
      }
    }
    const InspectorUse use = drawInspector(Panels.inspection(world, *subject), stock);
    if (!use.Open) {
      interaction.forgetSubject();
    } else {
      if (use.Chosen) {
        interaction.chooseStock(*use.Chosen);
      }
      if (use.Apply) {
        interaction.applyStock(session.commands());
      }
    }
  }
  if (Shown.Graph) {
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    drawGraphView(Panels.graph(world, view, size.x, size.y));
  }
  drawSnapGuides(interaction.snapGuides(), interaction.measuredLengths(world), view);
  if (Shown.Overlay) {
    if (const Availability *availability = Panels.tooltipAvailability(
            previewedWorld(world, preview), *Shown.Overlay, overlayShape,
            pointUnderCursor(readCursor(window), view, OVERLAY_HEIGHT))) {
      drawNeedTooltip(*Shown.Overlay, *availability);
    }
  }
  if (preview.Shop) {
    drawShopContextTooltip(*preview.Shop);
  }
  ImGui::Render();
  return ImGui::GetDrawData();
}

} // namespace tpj
