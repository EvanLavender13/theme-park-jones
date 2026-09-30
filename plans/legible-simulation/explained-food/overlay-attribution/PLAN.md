# Implementation Plan: Overlay Attribution

## Goal

Show, while the food overlay is on, a tooltip attributing the food availability at the cursor's nearest guest path place to its shops.

## Approach

tpj_legible gets a path_place component whose nearestGuestPathPlace takes nearestPlaceOn on each guest path's carrier and keeps the nearest, with its straight distance, and food.h gains foodNear, the availability at that place within a reach. A new app component, food_tooltip, holds only the ImGui edge: it draws foodNear's result for the ground under the cursor and the reach OVERLAY_BAND as a tooltip with a table. main.cpp's buildUi calls it while the overlay is shown.

All commands run from WSL at the repository root.

## Placement

Decision 0027 places each behavior this feature adds:

- Finding the nearest guest path place of a ground point: legible, new component legible/path_place.h. It is a query over the guest network and intent, needed by any explanation that starts from where the player points, and it has no window.
- The food at a pointed place within a reach: legible, legible/food.h's foodNear, headless and tested.
- Drawing the tooltip: app, new component app/food_tooltip.h, the platform edge. It only renders foodNear's result, passing render's OVERLAY_BAND as the reach.
- Showing it: one call in main.cpp's buildUi, beside the graph view's, while the overlay is shown. It adds no state to main.cpp.

## Tasks

### Task 1: Specify nearestGuestPathPlace

Files:
- Modify: `src/legible/SPEC.md` (append at the end)

Step 1: Append this section, with a blank line before it.

```markdown
## Path places

An explanation that starts from where the player points needs the place on the paths the point stands beside. A PathPlace is a place and its Distance, in meters, from a ground point. nearestGuestPathPlace(world, point) takes, for each guest path of parkPaths in ascending key order whose key is a carrier of parkNetwork(world, PathKind::Guest), N, the place nearestPlaceOn gives on that carrier for the point, and gives the one whose groundPoint is nearest the point, by sqrt(dx * dx + dz * dz), a later path's replacing an earlier one's only when strictly nearer, so ties go to the lower key. Its Distance is that straight distance. It gives none when the point is not finite or no guest path is a carrier of N. So it never gives a place on a connector. The straight distance only locates the point on the paths, as nearestPlace does for snapping; what is explained there, such as food availability, is measured along routes (principle 4).

foodNear(world, point, reach) gives foodAvailability at nearestGuestPathPlace's place for the point when that gives one whose Distance is at most the reach, and none otherwise. So a point within the reach of a guest path is explained by the food at the path place it stands beside.
```

### Task 2: Specify the food tooltip

Files:
- Modify: `src/app/SPEC.md:37`

Step 1: In the Tooling UI paragraph, replace

```
It keeps no copy of the overlay, so the band follows every tick's offers and every world the app replaces. Below the checkboxes, each frame,
```

with

```
It keeps no copy of the overlay, so the band follows every tick's offers and every world the app replaces. While the overlay is shown, each frame, the app takes the ground under the cursor, as the tools do, none while ImGui wants the mouse. When it has one, and foodNear with the reach OVERLAY_BAND gives an availability for it, the app shows a tooltip at the cursor with `Food <v>`, v being the availability's Value to three decimals (legible/SPEC.md). Below it is `No supplied shop reachable` when there are no contributions, and otherwise a table with a row for each contribution, in order: the shop's key in decimal, its relief to two decimals, its route distance in meters and its wait and effective time in seconds to one decimal each, and its term to three decimals. So the rows are exactly the terms the value sums. Below the checkboxes, each frame,
```

### Task 3: Create the path_place and foodNear interfaces

Files:
- Create: `src/legible/path_place.h`
- Create: `src/legible/path_place.cpp`
- Modify: `src/legible/CMakeLists.txt`
- Modify: `src/legible/food.h`
- Modify: `src/legible/food.cpp`

Step 1: Write `src/legible/path_place.h`.

```cpp
#ifndef TPJ_LEGIBLE_PATH_PLACE_H
#define TPJ_LEGIBLE_PATH_PLACE_H

#include "sim/medium/network.h"

#include <optional>

namespace tpj {

class World;

// A place on the paths and its straight distance, in meters, from a ground point.
struct PathPlace {
  Place At;
  double Distance = 0.0;

  bool operator==(const PathPlace &) const = default;
};

// The place on a guest path nearest the point, ties to the lower path key, or none when the point
// is not finite or no guest path is a carrier of the guest network. The straight distance only
// locates the point; what is explained there is measured along routes.
std::optional<PathPlace> nearestGuestPathPlace(const World &world, GroundPoint point);

} // namespace tpj

#endif
```

Step 2: Write `src/legible/path_place.cpp` with a stub.

```cpp
#include "legible/path_place.h"

namespace tpj {

std::optional<PathPlace> nearestGuestPathPlace(const World & /*world*/, GroundPoint /*point*/) {
  return std::nullopt;
}

} // namespace tpj
```

Step 3: In `src/legible/CMakeLists.txt`, replace

```cmake
add_library(tpj_legible STATIC
    food.cpp)
```

with

```cmake
add_library(tpj_legible STATIC
    food.cpp
    path_place.cpp)
```

Step 4: In `src/legible/food.h`, add `#include "legible/path_place.h"` as the first project include and `#include <optional>` to the system includes, keeping each group sorted, and after the declaration of `foodAvailability` add

```cpp

// The food availability at the guest path place nearest the point, when the point lies within the
// reach of it, and none otherwise.
std::optional<FoodAvailability> foodNear(const World &world, GroundPoint point, double reach);
```

Step 5: In `src/legible/food.cpp`, before the closing `} // namespace tpj`, add the stub

```cpp

std::optional<FoodAvailability> foodNear(const World & /*world*/, GroundPoint /*point*/,
                                         double /*reach*/) {
  return std::nullopt;
}
```

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build succeeds with no warnings.

### Task 4: Run the test pass

Dispatch the test-writer agent with only these paths: `plans/legible-simulation/explained-food/overlay-attribution/FEATURE.md`, `src/legible/SPEC.md`, `src/app/SPEC.md`, `src/sim/medium/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/park/SPEC.md`, `src/legible/path_place.h`, `src/legible/food.h`, `docs/principles.md`, `docs/conventions.md`.

Expected: the agent adds tests to tests/legible/ for criteria 1 to 4 and reports the files. They build against the stubs, and those of criteria 1 and 3 fail until Task 5. Those of criteria 2 and 4 may pass against the stubs, which give none and change nothing. Criterion 5 is checked by hand in Task 8.

### Task 5: Implement nearestGuestPathPlace and foodNear

Files:
- Modify: `src/legible/path_place.cpp`

Step 1: Replace the file with this content.

```cpp
#include "legible/path_place.h"

#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <math.h>

namespace tpj {

std::optional<PathPlace> nearestGuestPathPlace(const World &world, GroundPoint point) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::optional<PathPlace> nearest;
  // Paths come in ascending key order, and only a strictly nearer one replaces the nearest.
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind != PathKind::Guest) {
      continue;
    }
    const std::optional<Place> place = network.nearestPlaceOn(path.Key, point);
    if (!place) {
      continue;
    }
    const std::optional<GroundPoint> at = network.groundPoint(*place);
    if (!at) {
      continue;
    }
    const double dx = at->X - point.X;
    const double dz = at->Z - point.Z;
    const double distance = sqrt(dx * dx + dz * dz);
    if (!nearest || distance < nearest->Distance) {
      nearest = PathPlace{*place, distance};
    }
  }
  return nearest;
}

} // namespace tpj
```

Step 2: In `src/legible/food.cpp`, replace the foodNear stub with

```cpp

std::optional<FoodAvailability> foodNear(const World &world, GroundPoint point, double reach) {
  const std::optional<PathPlace> nearest = nearestGuestPathPlace(world, point);
  if (!nearest || nearest->Distance > reach) {
    return std::nullopt;
  }
  return foodAvailability(world, nearest->At);
}
```

Step 3: Build and run the legible tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible_tests && build/windows-debug/tpj_legible_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`.

### Task 6: Create the food tooltip component

Files:
- Create: `src/app/food_tooltip.h`
- Create: `src/app/food_tooltip.cpp`
- Modify: `src/app/CMakeLists.txt`

Step 1: Write `src/app/food_tooltip.h`.

```cpp
#ifndef TPJ_APP_FOOD_TOOLTIP_H
#define TPJ_APP_FOOD_TOOLTIP_H

#include "sim/park/intent.h"
#include "sim/world.h"

#include <optional>

namespace tpj {

// Draws a tooltip at the cursor attributing foodNear's availability for the ground under it, with
// the reach OVERLAY_BAND, to its shops, and nothing when there is no ground or no availability.
// Call between ImGui::NewFrame and ImGui::Render.
void drawFoodTooltip(const World &world, std::optional<ParkPoint> ground);

} // namespace tpj

#endif
```

Step 2: Write `src/app/food_tooltip.cpp`.

```cpp
#include "app/food_tooltip.h"

#include "legible/food.h"
#include "render/food_overlay.h"

#include <imgui.h>

namespace tpj {

void drawFoodTooltip(const World &world, std::optional<ParkPoint> ground) {
  if (!ground) {
    return;
  }
  const std::optional<FoodAvailability> available =
      foodNear(world, GroundPoint{ground->X, ground->Z}, OVERLAY_BAND);
  if (!available || !ImGui::BeginTooltip()) {
    return;
  }
  const FoodAvailability &food = *available;
  ImGui::Text("Food %.3f", food.Value);
  if (food.Contributions.empty()) {
    ImGui::TextUnformatted("No supplied shop reachable");
  } else if (ImGui::BeginTable("food", 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
    for (const char *heading : {"Shop", "Relief", "Route", "Wait", "Time", "Term"}) {
      ImGui::TableSetupColumn(heading);
    }
    ImGui::TableHeadersRow();
    for (const FoodContribution &contribution : food.Contributions) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(contribution.Shop));
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", contribution.Relief);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f m", contribution.Distance);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f s", contribution.Wait);
      ImGui::TableNextColumn();
      ImGui::Text("%.1f s", contribution.Time);
      ImGui::TableNextColumn();
      ImGui::Text("%.3f", contribution.Term);
    }
    ImGui::EndTable();
  }
  ImGui::EndTooltip();
}

} // namespace tpj
```

Step 3: In `src/app/CMakeLists.txt`, add `food_tooltip.cpp` to the `add_executable(tpj_app` source list after `debug_panel.cpp`.

### Task 7: Show the tooltip while the overlay is shown

Files:
- Modify: `src/app/main.cpp:519-530` (buildUi)

Step 1: Add `#include "app/food_tooltip.h"` to the project include group after `#include "app/debug_panel.h"`.

Step 2: In `buildUi`, replace

```cpp
  if (shown.Graph) {
    drawGraph(world, view);
  }
```

with

```cpp
  if (shown.Graph) {
    drawGraph(world, view);
  }
  if (shown.FoodOverlay) {
    tpj::drawFoodTooltip(world, groundUnderCursor(window, view));
  }
```

and replace its comment

```cpp
// Builds the frame's ImGui draw data: the panels, which set the shown views from their
// checkboxes, and the graph over the scene while it is shown.
```

with

```cpp
// Builds the frame's ImGui draw data: the panels, which set the shown views from their
// checkboxes, the graph over the scene while it is shown, and the food tooltip at the cursor while
// the overlay is.
```

Step 3: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 8: Check the tooltip by hand

Step 1: Ask Evan to run, from `build/windows-debug`, `./ThemeParkJones.exe --park ../../tests/parks/warm.park --overlay food`, hover the band near the shop and far from it, beyond the band, and over a panel, and uncheck Food overlay.

Expected (criterion 4): over the band, a tooltip with `Food` and a value, and one row for shop 7 whose term matches the value to the digit shown; beyond the band, over a panel, and with the overlay unchecked, no tooltip. A tooltip that lists a term not matching the value, or shows beyond the band, is a deviation to report.

### Task 9: Verify the feature

Step 1: Format.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
Expected: no output.

Step 2: Build and test windows-debug.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -E "warning|error" ; ctest.exe --preset windows-debug 2>&1 | tr -d '\r' | tail -4`
Expected: no warnings or errors, and `100% tests passed`.

Step 3: Build and test linux-debug.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug 2>&1 | tail -4`
Expected: no warnings or errors, and `100% tests passed`.

Step 4: Tidy.

Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

### Task 10: Commit

Step 1: Stage exactly the feature's paths: `git add plans/legible-simulation/explained-food/overlay-attribution src/legible src/app tests/legible`, and check `git status --short` shows nothing else staged.

Step 2: Dispatch the reviewer on the staged diff, `git diff --cached`, with FEATURE.md, src/legible/SPEC.md, src/app/SPEC.md, docs/principles.md, and docs/decisions/0027-code-architecture.md as context, and give its findings to Evan verbatim.

Step 3: Commit through commit-hygiene with the subject `Legible: Attribute the food overlay on hover` and a body of at most 72-character lines saying that nearestGuestPathPlace finds the guest path place a ground point stands beside, and that while the overlay is shown a tooltip lists each shop's relief, route, wait, time, and term at the cursor's place, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
