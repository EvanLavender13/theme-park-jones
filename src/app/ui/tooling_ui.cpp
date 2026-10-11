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
                ShownViews &shown, Interaction &interaction, GameSpeed &speed,
                KeptArrivalDay &arrivals, HistoryAction keys) {
  const World &world = session.world();
  DebugStats stats;
  stats.SimTick = world.Tick;
  stats.Guests = parkGuests(world).size();
  stats.Focus = camera.Focus;
  stats.Distance = camera.Distance;
  drawDebugPanel(stats, shown.Graph, shown.Overlay);
  drawTimePanel(parkTime(world.Tick), speed);
  interaction.settleHours(world);
  const OpeningHours hours = parkHours(world);
  const OpeningHours shownHours = interaction.hoursChoice().value_or(hours);
  keepArrivalDay(arrivals, world, shownHours);
  const HoursUse hoursUse =
      drawHoursPanel(HoursControls{hours, shownHours}, arrivals.Day, parkTime(world.Tick).Second);
  if (hoursUse.Chosen) {
    interaction.chooseHours(*hoursUse.Chosen);
  }
  if (hoursUse.Apply) {
    interaction.applyHours(session.commands());
  }
  drawDeparturesPanel(departureTally(world));
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
  drawPanels(dialogs, session, camera, Shown, interaction, Speed, Arrivals, keys);
  drawLightPanel(parkTime(session.world().Tick).Second, Light);
  const World &world = session.world();
  interaction.settleStock(world);
  if (const std::optional<InspectorSubject> subject = interaction.subject()) {
    std::optional<StockControls> stock;
    if (subject->Kind == SubjectKind::Shop) {
      for (const ParkBox &box : parkBoxes(world)) {
        if (box.Key == subject->Key) {
          stock = StockControls{box.Stock, interaction.stockChoice().value_or(box.Stock)};
        }
      }
    }
    const InspectorUse use = drawInspector(inspectSubject(world, *subject), stock);
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
    drawGraphView(world, view);
  }
  drawSnapGuides(interaction.snapGuides(), interaction.measuredLengths(world), view);
  if (Shown.Overlay) {
    drawNeedTooltip(previewedWorld(world, preview), *Shown.Overlay, overlayShape,
                    pointUnderCursor(readCursor(window), view, OVERLAY_HEIGHT));
  }
  if (preview.Shop) {
    drawShopContextTooltip(*preview.Shop);
  }
  ImGui::Render();
  return ImGui::GetDrawData();
}

} // namespace tpj
