# Implementation Plan: Tooling UI

## Goal

Move the Debug panel's park numbers into a park summary in legible, the graph's drawing into a graph view, and the frame's panels and shown views into a tooling UI component, so main.cpp builds no UI.

## Approach

summarizePark runs the loops drawPanels runs today and returns a ParkSummary, which DebugStats holds in place of its five park fields, so drawDebugPanel prints the same text. graph_view.cpp takes main.cpp's drawGraph and imColor whole. ToolingUi holds ShownViews and builds the frame's UI with main.cpp's buildUi and drawPanels, and runLoop holds one ToolingUi in place of its ShownViews local.

## Placement

Decision 0027 places each behavior this feature adds:

- The Debug panel's park numbers, meaning the shop lines, the guest count, mean hunger, waiting guests, and meals eaten: legible, new component legible/park_summary.h. They are queries of what the park publishes, legible's role, and need no window, so tpj_legible_tests checks them. ShopLine moves here from app/debug_panel.h, since the summary produces it.
- Drawing the networks' graph over the scene: app, new component app/graph_view.h, in tpj_app. It is ImGui draw-list calls over render's buildGraphOverlay, a platform edge with no rule of its own.
- Owning which views the Debug panel's checkboxes show, and building the frame's Debug, Tools, and Inspector panels, the graph, and the tooltips: app, new component app/tooling_ui.h, ToolingUi, in tpj_app. The checkboxes are the shown views' one writer, so the component that draws them owns them, and it needs ImGui, so it is in the executable.
- main.cpp: composition only. runLoop holds a ToolingUi, calls its build where it called buildUi, and reads its shown views for the overlay's look. It loses imColor, drawGraph, ShownViews, drawPanels, and buildUi.

## Tasks

### Task 1: Describe the park summary in legible's spec

Files:
- Modify: `src/legible/SPEC.md` (end of file)

Step 1: Add FEATURE.md's "## Park summary" section after the last paragraph of "## Inspectors", which ends the file, with its exact text.

### Task 2: Describe the tooling UI in the app spec

Files:
- Modify: `src/app/SPEC.md:45`

Step 1: In "## Tooling UI", add FEATURE.md's new paragraph after the paragraph beginning "A Debug panel shows the frame rate", with its exact text.

### Task 3: Declare the park summary

Files:
- Create: `src/legible/park_summary.h`
- Create: `src/legible/park_summary.cpp`
- Modify: `src/legible/CMakeLists.txt:3-7`

Step 1: Create src/legible/park_summary.h:

```cpp
#ifndef TPJ_LEGIBLE_PARK_SUMMARY_H
#define TPJ_LEGIBLE_PARK_SUMMARY_H

#include "sim/entity_key.h"
#include "sim/operations/operations.h"
#include "sim/world.h"

#include <stdint.h>
#include <vector>

namespace tpj {

// A shop box's key and its inspection record.
struct ShopLine {
  EntityKey Shop = NULL_KEY;
  ShopRecord Record;

  bool operator==(const ShopLine &) const = default;
};

// What the Debug panel shows about a park: its shops' records, its guests' count, mean hunger, and
// how many wait, and the meals eaten.
struct ParkSummary {
  std::vector<ShopLine> Shops;
  uint64_t Guests = 0;
  double MeanHunger = 0.0;
  uint64_t Waiting = 0;
  int64_t MealsEaten = 0;

  bool operator==(const ParkSummary &) const = default;
};

// A ShopLine for each box of parkBoxes that has a shopRecord, in parkBoxes' order; the number of
// parkGuests that have a guestRecord, the mean of their Hunger, 0 when there are none, and how
// many are Waiting; and the meals units consumed with the cause eaten. Changes nothing.
ParkSummary summarizePark(const World &world);

} // namespace tpj

#endif
```

Step 2: Create src/legible/park_summary.cpp, including "legible/park_summary.h", with a stub inside namespace tpj: summarizePark returns `{}`. Mark the unused parameter with a `/*world*/` comment.

Step 3: In src/legible/CMakeLists.txt, add park_summary.cpp to tpj_legible's sources after path_place.cpp.

### Task 4: Have the Debug panel take the summary

Files:
- Modify: `src/app/debug_panel.h`
- Modify: `src/app/debug_panel.cpp`
- Modify: `src/app/main.cpp:150-178`

Step 1: In src/app/debug_panel.h, delete ShopLine with its comment, replace `#include "sim/entity_key.h"` and `#include "sim/operations/operations.h"` with `#include "legible/park_summary.h"`, remove `#include <vector>`, and replace DebugStats's fields from `std::vector<ShopLine> Shops;` through `int64_t MealsEaten = 0;` with `ParkSummary Park;`. In drawDebugPanel's comment, "a line for each shop's record, the guest count and mean hunger, and the guests waiting and meals eaten" becomes "and the park summary's lines".

Step 2: In src/app/debug_panel.cpp, `stats.Shops` becomes `stats.Park.Shops`, `stats.Guests` becomes `stats.Park.Guests` (both places), `stats.MeanHunger` becomes `stats.Park.MeanHunger`, `stats.Waiting` becomes `stats.Park.Waiting`, and `stats.MealsEaten` becomes `stats.Park.MealsEaten`.

Step 3: So main.cpp builds against the new DebugStats until Task 9 removes drawPanels from it, replace drawPanels's lines from the `for (const tpj::ParkBox &box` loop through `stats.MealsEaten = ...;` with `stats.Park = tpj::summarizePark(world);`, and add `#include "legible/park_summary.h"` to main.cpp in sorted order. Task 9 removes that include again with drawPanels.

### Task 5: Declare the graph view and the tooling UI

Files:
- Create: `src/app/graph_view.h`, `src/app/graph_view.cpp`
- Create: `src/app/tooling_ui.h`, `src/app/tooling_ui.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Create src/app/graph_view.h:

```cpp
#ifndef TPJ_APP_GRAPH_VIEW_H
#define TPJ_APP_GRAPH_VIEW_H

#include "render/renderer.h"
#include "sim/world.h"

namespace tpj {

// Draws the networks over the scene and behind every panel, on ImGui's background draw list:
// buildGraphOverlay of the world, the view, and ImGui's display size, lines in their kind's graph
// color, or the connector color, then nodes, anchored ones larger in the anchor color. Call
// between ImGui::NewFrame and ImGui::Render.
void drawGraphView(const World &world, const CameraView &view);

} // namespace tpj

#endif
```

Step 2: Create src/app/tooling_ui.h:

```cpp
#ifndef TPJ_APP_TOOLING_UI_H
#define TPJ_APP_TOOLING_UI_H

#include "app/interaction.h"
#include "app/orbit_camera.h"
#include "app/park_dialogs.h"
#include "legible/preview.h"
#include "sim/world.h"

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

  // Builds the frame's ImGui draw data: the Debug and Tools panels, which set the shown views from
  // their checkboxes, select the tool the player chose, and start the park action they pressed;
  // the Inspector while the interaction has a subject, forgetting it when closed; the graph over
  // the scene while it is shown; the food tooltip at the cursor while the overlay is, on the
  // preview's candidate when it has one; and a shop ghost's context.
  void build(SDL_Window *window, ParkDialogs &dialogs, const World &world,
             const OrbitCamera &camera, Interaction &interaction, const Preview &preview);

private:
  ShownViews Shown;
};

} // namespace tpj

#endif
```

Step 3: Create src/app/graph_view.cpp and src/app/tooling_ui.cpp, each including its header, with stubs inside namespace tpj that do nothing. Mark unused parameters with `/*name*/` comments.

Step 4: In src/app/CMakeLists.txt, add graph_view.cpp after food_tooltip.cpp and tooling_ui.cpp after tool_panel.cpp in tpj_app's sources.

Step 5: Configure and build.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_app tpj_legible_tests`
Expected: the build succeeds with no warnings. The Debug panel's park lines are empty until Task 7 implements the summary, which no test checks.

### Task 6: Test pass

Step 1: Dispatch the test-writer agent with FEATURE.md, src/legible/SPEC.md, src/sim/operations/SPEC.md, and src/sim/guests/SPEC.md, and the public header src/legible/park_summary.h. It creates tests/legible/park_summary_test.cpp and adds it to tpj_legible_tests in tests/legible/CMakeLists.txt.

Step 2: Build and run it.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible_tests && build/windows-debug/tpj_legible_tests.exe -# "[#park_summary_test]"`
Expected: the build succeeds, and the tests that need the stub's behavior fail.

### Task 7: Implement the park summary

Files:
- Modify: `src/legible/park_summary.cpp`

Step 1: Replace the stub with this definition, and add `#include "sim/guests/guests.h"`, `#include "sim/medium/flow.h"`, and `#include "sim/park/intent.h"` after `#include "legible/park_summary.h"`, and `#include <optional>` below them:

```cpp
ParkSummary summarizePark(const World &world) {
  ParkSummary summary;
  for (const ParkBox &box : parkBoxes(world)) {
    if (const std::optional<ShopRecord> record = shopRecord(world, box.Key)) {
      summary.Shops.push_back({box.Key, *record});
    }
  }
  double hunger = 0.0;
  for (const EntityKey guest : parkGuests(world)) {
    if (const std::optional<GuestRecord> record = guestRecord(world, guest)) {
      ++summary.Guests;
      hunger += record->Hunger;
      if (record->Activity == GuestActivity::Waiting) {
        ++summary.Waiting;
      }
    }
  }
  summary.MeanHunger = summary.Guests == 0 ? 0.0 : hunger / static_cast<double>(summary.Guests);
  summary.MealsEaten = unitsConsumed<Meals>(world, EATEN_CAUSE);
  return summary;
}
```

Step 2: Run its tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible_tests && build/windows-debug/tpj_legible_tests.exe -# "[#park_summary_test]"`
Expected: every test passes.

### Task 8: Implement the graph view and the tooling UI

Files:
- Modify: `src/app/graph_view.cpp`
- Modify: `src/app/tooling_ui.cpp`

Step 1: Replace src/app/graph_view.cpp with main.cpp's imColor, in an anonymous namespace inside namespace tpj, and main.cpp's drawGraph body as drawGraphView, without the `tpj::` qualifiers. Its includes are "app/graph_view.h", "render/graph_overlay.h", and `<imgui.h>`.

Step 2: Replace src/app/tooling_ui.cpp with:

```cpp
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
                      const OrbitCamera &camera, Interaction &interaction,
                      const Preview &preview) {
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
```

Step 3: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 9: Compose the tooling UI in main.cpp

Files:
- Modify: `src/app/main.cpp`

Step 1: Delete from main.cpp, with their comments: imColor, drawGraph, ShownViews, drawPanels, and buildUi.

Step 2: Add `#include "app/tooling_ui.h"` in sorted order, and remove the includes of app/debug_panel.h, app/food_tooltip.h, app/inspector_window.h, app/shop_context_tooltip.h, app/tool_panel.h, legible/inspect.h, legible/park_summary.h, render/graph_overlay.h, sim/field_text.h, sim/guests/guests.h, sim/medium/flow.h, sim/operations/operations.h, and sim/park/intent.h.

Step 3: In runLoop, replace `ShownViews shown{options.ShowGraph, options.ShowFoodOverlay};` with `tpj::ToolingUi ui({options.ShowGraph, options.ShowFoodOverlay});`, replace the buildUi call with `ui.build(renderer.Window, dialogs, session.world(), camera, interaction, scene.preview());`, and in the PreviewLook, `shown.FoodOverlay` becomes `ui.shown().FoodOverlay`.

Step 4: Build the app and check main.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && grep -n "ImGui::NewFrame\|drawDebugPanel\|parkGuests\|AddLine" src/app/main.cpp`
Expected: the build succeeds with no warnings, and grep prints nothing.

### Task 10: Verify on both builds

Step 1: Format the changed sources.

Run: `git ls-files -m -o --exclude-standard -- 'src/app/*.h' 'src/app/*.cpp' 'src/legible/*.h' 'src/legible/*.cpp' 'tests/legible/*.cpp' | xargs clang-format -i`
Expected: no output.

Step 2: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: the build succeeds, and every test passes.

Run: `cmake --preset linux-debug && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `scripts/tidy.sh`
Expected: clean.

Step 3: Check the scene.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --ticks 200 --graph --overlay food --capture build/tooling-ui.bmp`
Expected: it exits with status 0, and build/tooling-ui.bmp shows the park with its walkways, guests, the graph, and the food band, and the Debug panel with its shop line and its guest and meal lines, as before the change.

Step 4: Check the panels by hand. Run `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park`, then:
- uncheck and check Graph and Food overlay;
- with Look, click a guest, then close the Inspector;
- choose Place shop and hover the ground;
- press New park.

Expected: the graph and the food band hide and show with their checkboxes; the Inspector shows the guest and its mark goes when it is closed; the shop ghost's context tooltip shows; New park empties the Debug panel's shop lines, since the new park has no shop.

### Task 11: Commit

Step 1: Commit the feature once through the commit-hygiene skill, naming its paths, with the subject `App: Give the panels, graph, and park summary their own homes`.
