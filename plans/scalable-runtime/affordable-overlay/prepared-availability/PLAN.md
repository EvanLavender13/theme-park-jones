# Implementation Plan: Prepared Availability

## Goal

Make the food overlay the first availability overlay: legible's availability defined by an offer, a curve, and a saturating value, its values for a whole build found in one call with each offer found once, render's band asking for them in one call, and the report timing the app's frames with the overlay shown before and after.

## Approach

The report script gains a second app launch per park with --overlay food, and the before report and capture are taken first, with no compiling while the report runs. legible/availability.h generalizes foodAvailability over an AvailabilityDefinition; availabilityValues memoizes each source's supplied offer for one call and computes each place's value through the same function availabilityAt uses, so the values match bit for bit. render's food overlay becomes band_overlay.h, which collects every row's place, calls its value function once, and colors rows by a ramp saturating at the full it is given; views' buildAvailabilityOverlay joins them, and the app and tpj_bench call it with FOOD_AVAILABILITY. The feature adds no tests (FEATURE.md), so headers and their implementations land together, and the old render and views files go once their callers have moved.

## Placement

Decision 0027 places each behavior this feature adds:

- The availability definition, availabilityAt, and availabilityValues: legible, legible/availability.h. legible owns explanations computed from the medium, and this is the general form of its food explanation, with no rendering in it.
- FOOD_AVAILABILITY and the food names as aliases: legible, legible/food.h. Food's instance stays beside foodNear, which the tooltip reads.
- The band asking for all its values in one call, its ramp saturating at a given full: render, render/band_overlay.h, renamed from food_overlay.h. render owns the overlay's mesh, and the band is no longer food's.
- buildAvailabilityOverlay: views, views/availability_overlay.h, renamed from food_overlay.h. views is the one layer that may join legible and render.
- The app's and tpj_bench's calls: app/scene/scene_uploads.cpp and bench/stages.cpp change the call they already make, and app/ui/food_tooltip.cpp the header it reads OVERLAY_BAND from, composition only.
- Timing the app's frames with the overlay shown: scripts/runtime-report.sh, which already launches the app.

## Tasks

### Task 1: Launch the app with the overlay in the report

Files:
- Modify: `scripts/runtime-report.sh:9-10`, `scripts/runtime-report.sh:104-111`

Step 1: Replace the app loop

Replace the block

```bash
    for park in "${parks[@]}"; do
        build/windows-release/ThemeParkJones.exe --park "$park" --frames "$app_frames" \
            --frame-times >"$app_output" || fail "the app failed on $park in round $round"
        echo "build windows-release-app" >>"$out/launches.txt"
        build/windows-release/tpj_bench_report.exe frames "$park" "$(wslpath -w "$app_output")" |
            tr -d '\r' >>"$out/launches.txt" ||
            fail "converting the app's frames on $park in round $round failed"
    done
```

with

```bash
    # The app without the food overlay, then with it, which rebuilds it at every tick.
    for overlay in none food; do
        options=()
        label=windows-release-app
        if [ "$overlay" = food ]; then
            options=(--overlay food)
            label=windows-release-app-overlay
        fi
        for park in "${parks[@]}"; do
            build/windows-release/ThemeParkJones.exe --park "$park" --frames "$app_frames" \
                --frame-times "${options[@]}" >"$app_output" ||
                fail "the app failed on $park with overlay $overlay in round $round"
            echo "build $label" >>"$out/launches.txt"
            build/windows-release/tpj_bench_report.exe frames "$park" \
                "$(wslpath -w "$app_output")" | tr -d '\r' >>"$out/launches.txt" ||
                fail "converting the app's frames on $park with overlay $overlay in round $round failed"
        done
    done
```

and in the header comment, where the sentence runs over lines 9 and 10, replace "Each round also launches the windows-release app on every park and keeps its frame times." with "Each round also launches the windows-release app on every park, without and then with the food overlay, and keeps its frame times.", rewrapping the comment within 100 columns.

Step 2: Check the script parses

Run: `bash -n scripts/runtime-report.sh && echo ok`
Expected: `ok`

### Task 2: Take the before report and capture

Files: none in the repository.

Step 1: Start the before report in the background

Run (in the background): `scripts/runtime-report.sh build/runtime-report/prepared-availability-before.txt`
Expected, when it ends: `runtime-report: wrote .../build/runtime-report/prepared-availability-before.txt in <m>m <ss>s`. It takes about 7 minutes. Do Tasks 3 to 6, which edit text only, while it runs. Compile nothing until it ends, since compiling would slow the launches it times.

Step 2: Confirm criterion 1 on it

Run: `grep -c '^park windows-release-app-overlay ' build/runtime-report/prepared-availability-before.txt; ls tests/parks/stress/*.park | wc -l`
Expected: the same number twice (2 today).

Step 3: Capture the overlay as it is, with the windows-release build the report just made

Run: `build/windows-release/ThemeParkJones.exe --park tests/parks/warm.park --ticks 3000 --overlay food --capture build/prepared-availability-before.bmp; echo $?`
Expected: `0`, with the file written.

### Task 3: Legible spec

Files:
- Modify: `src/legible/SPEC.md:9`, `src/legible/SPEC.md:11-16`

Step 1: In "## Boundary", replace "views calls foodAvailability." with "views calls availability.h for availabilityValues, and the app and tpj_bench read FOOD_AVAILABILITY from food.h."

Step 2: Replace the section "## Food availability" whole, from its heading to the line before "## Path places", with the text FEATURE.md's Spec changes gives for it, starting at "## Availability".

### Task 4: Render spec

Files:
- Modify: `src/render/SPEC.md:7`, `src/render/SPEC.md:9`, `src/render/SPEC.md:37-39`

Step 1: In "## Boundary", replace "The food overlay shades by a value function it is given, which views supplies." with "The band overlay shades by values it is given, which views supplies.", and "food_overlay.h for OVERLAY_BAND" with "band_overlay.h for OVERLAY_BAND".

Step 2: Replace the section "## Food overlay" whole, its heading and its one paragraph, with the text FEATURE.md gives, starting at "## Band overlay".

### Task 5: Views spec

Files:
- Modify: `src/views/SPEC.md:7-11`

Step 1: In "## Boundary", replace "The app and tpj_bench call food_overlay.h." with "The app and tpj_bench call availability_overlay.h."

Step 2: Replace the section "## Food overlay" whole with the text FEATURE.md gives, starting at "## Availability overlays".

### Task 6: Bench spec

Files:
- Modify: `src/bench/SPEC.md` (the paragraph under "## The script")

Step 1: Replace "and the windows-release app on every park." with "and the windows-release app on every park, without and then with the food overlay shown, so the report shows what rebuilding the overlay at every tick costs a player who shows it."

Step 2: Check no spec names the old overlay

Run: `grep -rn -E 'food_overlay|buildFoodOverlay|buildFoodAvailabilityOverlay|foodColor|## Food (availability|overlay)' src --include=SPEC.md | wc -l`
Expected: `0`

### Task 7: The availability and availabilityAt

Wait for Task 2's report to finish before this task.

Files:
- Create: `src/legible/availability.h`, `src/legible/availability.cpp`
- Modify: `src/legible/food.h`, `src/legible/food.cpp`, `src/legible/CMakeLists.txt`

Step 1: Create src/legible/availability.h

```cpp
#ifndef TPJ_LEGIBLE_AVAILABILITY_H
#define TPJ_LEGIBLE_AVAILABILITY_H

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"

#include <optional>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

class World;

// The walking speed, in meters per second, that turns a route distance into time: the guests'
// own, so an effective time is what a guest would feel.
inline constexpr double REFERENCE_SPEED = WALK_SPEED;

// A source's offer at a node of a network: its own first entry of one offer field at the node, or
// none.
using OfferAtNode = std::optional<OfferEntry> (*)(const World &world, const Network &network,
                                                  uint32_t node, EntityKey source);

// What an availability measures: the offer it reads, the curve that discounts a relief by the
// effective time in seconds, at least one point ascending in X, and the positive availability at
// which an overlay of it is fully saturated.
struct AvailabilityDefinition {
  OfferAtNode Offer = nullptr;
  std::span<const CurvePoint> Curve;
  double Full = 1.0;
};

// One source's part of an availability at a place: its offer's relief, its route distance in
// meters, its offer's wait and the effective time in seconds, and its term, the relief discounted
// by that time.
struct AvailabilityContribution {
  EntityKey Shop = NULL_KEY;
  double Relief = 0.0;
  double Distance = 0.0;
  double Wait = 0.0;
  double Time = 0.0;
  double Term = 0.0;

  bool operator==(const AvailabilityContribution &) const = default;
};

// An availability at a place and the contributions it is the sum of, in ascending source key.
struct Availability {
  std::vector<AvailabilityContribution> Contributions;
  double Value = 0.0;

  bool operator==(const Availability &) const = default;
};

// The availability at a place on the world's guest network: each reachable source whose offer says
// it is supplied contributes its relief discounted by its effective time, and the value is their
// terms added in order. Changes nothing.
Availability availabilityAt(const World &world, const AvailabilityDefinition &definition,
                            const Place &place);

// The value availabilityAt gives at each of the places, in order, finding each source's offer once
// in the call.
std::vector<double> availabilityValues(const World &world, const AvailabilityDefinition &definition,
                                       std::span<const Place> places);

} // namespace tpj

#endif
```

Step 2: Create src/legible/availability.cpp

```cpp
#include "legible/availability.h"

#include "sim/medium/field.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <algorithm>
#include <optional>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// The curve at a time in seconds: its first point's Y at or before its first X, its last point's
// at or beyond its last X, and its last point's for a NaN.
double discount(std::span<const CurvePoint> curve, double seconds) {
  const double t = std::clamp(seconds, curve.front().X, curve.back().X);
  for (size_t i = 1; i < curve.size(); ++i) {
    const CurvePoint &a = curve[i - 1];
    const CurvePoint &b = curve[i];
    if (t == a.X) {
      return a.Y;
    }
    if (t < b.X) {
      return a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X);
    }
  }
  // Only the last point and a NaN, which no comparison holds for, reach here.
  return curve.back().Y;
}

// The source's offer when it says it is supplied, found as guests find offers: the definition's
// offer at the source's lowest anchored node, or none.
std::optional<OfferEntry> suppliedOffer(const World &world, const AvailabilityDefinition &definition,
                                        const Network &network, EntityKey source) {
  const std::optional<uint32_t> anchor = network.firstAnchoredNode(source);
  if (!anchor) {
    return std::nullopt;
  }
  const std::optional<OfferEntry> offer = definition.Offer(world, network, *anchor, source);
  if (!offer || !offer->Supplied) {
    return std::nullopt;
  }
  return offer;
}

// The availability at the place, with each source's supplied offer from offerOf. Every value an
// availability gives is computed here, so a place's value is the same however it is asked for.
template <typename OfferOf>
Availability availabilityWith(const World &world, const AvailabilityDefinition &definition,
                              const Network &network, const Place &place, const OfferOf &offerOf) {
  Availability availability;
  EntityKey previous = NULL_KEY;
  for (const SampledEntry<RouteEntry> &route :
       sampleField<RouteDistance<PathKind::Guest>>(world, network, place)) {
    // Sources come in ascending order, and only a source's first entry counts.
    if (route.Source == previous) {
      continue;
    }
    previous = route.Source;
    const std::optional<OfferEntry> offer = offerOf(route.Source);
    if (!offer) {
      continue;
    }
    AvailabilityContribution contribution;
    contribution.Shop = route.Source;
    contribution.Relief = offer->Relief;
    contribution.Distance = route.Value.Distance;
    contribution.Wait = static_cast<double>(offer->Wait) * SIM_TICK_SECONDS;
    contribution.Time = contribution.Distance / REFERENCE_SPEED + contribution.Wait;
    contribution.Term = contribution.Relief * discount(definition.Curve, contribution.Time);
    availability.Value += contribution.Term;
    availability.Contributions.push_back(contribution);
  }
  return availability;
}

} // namespace

Availability availabilityAt(const World &world, const AvailabilityDefinition &definition,
                            const Place &place) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  return availabilityWith(world, definition, network, place, [&](EntityKey source) {
    return suppliedOffer(world, definition, network, source);
  });
}

std::vector<double> availabilityValues(const World & /*world*/,
                                       const AvailabilityDefinition & /*definition*/,
                                       std::span<const Place> /*places*/) {
  return {};
}

} // namespace tpj
```

availabilityValues is filled in by Task 8; nothing calls it before then.

Step 3: In src/legible/food.h

- delete REFERENCE_SPEED and its comment, which availability.h now holds;
- replace the comments and definitions of FoodContribution and FoodAvailability with

```cpp
// One shop's part of the food availability at a place, and the food availability at a place.
using FoodContribution = AvailabilityContribution;
using FoodAvailability = Availability;
```

- delete foodDiscount's declaration and its comment;
- after FOOD_DISCOUNT_CURVE, add

```cpp
// Food availability: shops' food offers, discounted by FOOD_DISCOUNT_CURVE, with an overlay of it
// fully saturated at a meal's relief with no walk and no wait.
extern const AvailabilityDefinition FOOD_AVAILABILITY;
```

- make its includes exactly `legible/availability.h`, `legible/path_place.h`, `sim/guests/guests.h`, `sim/medium/network.h`, `<array>`, and `<optional>`, in that order.

Step 4: In src/legible/food.cpp

- delete the anonymous namespace holding suppliedOffer, and foodDiscount's definition;
- after `namespace tpj {`, add

```cpp
const AvailabilityDefinition FOOD_AVAILABILITY{&sourceEntryAtNode<FoodOffer>, FOOD_DISCOUNT_CURVE,
                                               0.5};
```

- replace foodAvailability's body with `return availabilityAt(world, FOOD_AVAILABILITY, place);`;
- make its includes exactly `legible/food.h`, `sim/medium/field.h`, `sim/operations/operations.h`, and `<optional>`.

Step 5: In src/legible/CMakeLists.txt, add `availability.cpp` to tpj_legible's sources, before `food.cpp`.

Step 6: Build

Run: `cmake.exe --build --preset windows-debug --target tpj_legible 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

### Task 8: availabilityValues

Files:
- Modify: `src/legible/availability.cpp`

Step 1: Replace availabilityValues' placeholder with

```cpp
std::vector<double> availabilityValues(const World &world, const AvailabilityDefinition &definition,
                                       std::span<const Place> places) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  // Each source's supplied offer, found the first time a place reaches the source, in ascending
  // source order, and dropped when the call returns.
  using Found = std::pair<EntityKey, std::optional<OfferEntry>>;
  std::vector<Found> found;
  const auto offerOf = [&](EntityKey source) -> std::optional<OfferEntry> {
    const auto at = std::ranges::lower_bound(found, source, {}, &Found::first);
    if (at != found.end() && at->first == source) {
      return at->second;
    }
    return found.insert(at, Found{source, suppliedOffer(world, definition, network, source)})
        ->second;
  };
  std::vector<double> values;
  values.reserve(places.size());
  for (const Place &place : places) {
    values.push_back(availabilityWith(world, definition, network, place, offerOf).Value);
  }
  return values;
}
```

Step 2: Build

Run: `cmake.exe --build --preset windows-debug --target tpj_legible 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

### Task 9: The band overlay

Files:
- Create: `src/render/band_overlay.h`, `src/render/band_overlay.cpp`
- Modify: `src/render/CMakeLists.txt`

Step 1: Create src/render/band_overlay.h

Its include guard is TPJ_RENDER_BAND_OVERLAY_H. It includes `render/park_mesh.h`, `sim/medium/network.h`, `sim/world.h`, `<array>`, `<functional>`, `<span>`, and `<vector>`. In namespace tpj it holds, copied from src/render/food_overlay.h with their comments unchanged, OVERLAY_BAND, OVERLAY_SPACING, OVERLAY_TOP_LIFT, OVERLAY_EDGE_LIFT, OVERLAY_RAMP, and OVERLAY_ZERO_COLOR, except that OVERLAY_ZERO_COLOR's comment reads `// No value at all, in a gray the ramp never gives.` It does not hold OVERLAY_FULL or foodColor. Then:

```cpp
// The values an overlay shades its places by, one for each place and in their order, such as an
// availability's.
using OverlayValues = std::function<std::vector<double>(std::span<const Place> places)>;

// The band along each guest path's line, in key order, with a cone beyond each end, shaded by the
// values one call of values gives at the lines' places, the ramp reaching its top at full, which
// is positive, and the gray OVERLAY_ZERO_COLOR for a value not above 0.
ParkMesh buildBandOverlay(const World &world, const OverlayValues &values, double full);
```

Step 2: Create src/render/band_overlay.cpp from the contents of src/render/food_overlay.cpp, then:

- include `render/band_overlay.h` in place of `render/food_overlay.h`, and add `<span>` and `<utility>`;
- move foodColor into the anonymous namespace, after appendCone, renamed rampColor with a second parameter `double full`, its body's `value / OVERLAY_FULL` replaced with `value / full`, and its comment `// The ramp's color for a value, reaching its top at full, or OVERLAY_ZERO_COLOR for a value not above 0.`;
- replace buildFoodOverlay with

```cpp
ParkMesh buildBandOverlay(const World &world, const OverlayValues &values, double full) {
  std::vector<EntityKey> guestPaths;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Guest) {
      guestPaths.push_back(path.Key);
    }
  }
  const Network &network = parkNetwork(world, PathKind::Guest);
  // Each band line's rows, colored once every row's value is known.
  struct BandLine {
    const Carrier *Line = nullptr;
    std::vector<BandRow> Rows;
  };
  std::vector<BandLine> lines;
  std::vector<Place> places;
  // Carriers come in key order, and a connector's key is no path's.
  for (const Carrier &carrier : network.carriers()) {
    if (!std::ranges::binary_search(guestPaths, carrier.Key)) {
      continue;
    }
    const std::vector<double> distances = sampleDistances(carrier);
    std::vector<BandRow> rows;
    rows.reserve(distances.size());
    for (const double distance : distances) {
      const Place place{carrier.Key, distance};
      const std::optional<GroundPoint> at = network.groundPoint(place);
      if (!at) {
        continue;
      }
      rows.push_back({*at, directionAt(carrier.Points, distance), OVERLAY_ZERO_COLOR});
      places.push_back(place);
    }
    if (!rows.empty()) {
      lines.push_back({&carrier, std::move(rows)});
    }
  }
  const std::vector<double> shades = values(places);
  ParkMesh mesh;
  size_t next = 0;
  for (BandLine &line : lines) {
    for (BandRow &row : line.Rows) {
      row.Color = rampColor(shades.at(next), full);
      ++next;
    }
    const std::vector<CarrierPoint> &points = line.Line->Points;
    appendBand(mesh, line.Rows);
    appendCone(mesh, line.Rows.front(), unitStep(points[1], points[0]));
    appendCone(mesh, line.Rows.back(), unitStep(points[points.size() - 2], points.back()));
  }
  return mesh;
}
```

Step 3: In src/render/CMakeLists.txt, add `band_overlay.cpp` to tpj_render's sources, before `food_overlay.cpp`.

Step 4: Build

Run: `cmake.exe --build --preset windows-debug --target tpj_render 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error. food_overlay.h and band_overlay.h both define the OVERLAY_ constants until Task 11, but no file includes both, so they do not collide.

### Task 10: The availability overlay

Files:
- Create: `src/views/availability_overlay.h`, `src/views/availability_overlay.cpp`
- Modify: `src/views/CMakeLists.txt`

Step 1: Create src/views/availability_overlay.h

```cpp
#ifndef TPJ_VIEWS_AVAILABILITY_OVERLAY_H
#define TPJ_VIEWS_AVAILABILITY_OVERLAY_H

#include "legible/availability.h"
#include "render/park_mesh.h"
#include "sim/world.h"

namespace tpj {

// An availability's overlay as the app shows it: render's band along each guest path, shaded by
// the availability at each place and saturating at the definition's Full. Changes nothing in the
// world.
ParkMesh buildAvailabilityOverlay(const World &world, const AvailabilityDefinition &definition);

} // namespace tpj

#endif
```

Step 2: Create src/views/availability_overlay.cpp

```cpp
#include "views/availability_overlay.h"

#include "render/band_overlay.h"

#include <span>
#include <vector>

namespace tpj {

ParkMesh buildAvailabilityOverlay(const World &world, const AvailabilityDefinition &definition) {
  return buildBandOverlay(
      world,
      [&world, &definition](std::span<const Place> places) {
        return availabilityValues(world, definition, places);
      },
      definition.Full);
}

} // namespace tpj
```

Step 3: In src/views/CMakeLists.txt, add `availability_overlay.cpp` to tpj_views' sources, before `food_overlay.cpp`.

Step 4: Build

Run: `cmake.exe --build --preset windows-debug --target tpj_views 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

### Task 11: Move the callers and delete the food overlay files

Files:
- Modify: `src/app/scene/scene_uploads.cpp:5,37`, `src/app/ui/food_tooltip.cpp:4`, `src/bench/stages.cpp:8,106`, `src/render/CMakeLists.txt`, `src/views/CMakeLists.txt`
- Delete: `src/render/food_overlay.h`, `src/render/food_overlay.cpp`, `src/views/food_overlay.h`, `src/views/food_overlay.cpp`

Step 1: In src/app/scene/scene_uploads.cpp, replace `#include "views/food_overlay.h"` with `#include "legible/food.h"` and `#include "views/availability_overlay.h"`, and in foodOverlayMesh replace `return buildFoodAvailabilityOverlay(world);` with `return buildAvailabilityOverlay(world, FOOD_AVAILABILITY);`.

Step 2: In src/bench/stages.cpp, replace `#include "views/food_overlay.h"` with `#include "legible/food.h"` and `#include "views/availability_overlay.h"`, and replace `stages.push_back(meshStage("food-overlay", park, buildFoodAvailabilityOverlay));` with

```cpp
  stages.push_back(meshStage("food-overlay", park, [](const World &world) {
    return buildAvailabilityOverlay(world, FOOD_AVAILABILITY);
  }));
```

Step 3: In src/app/ui/food_tooltip.cpp, replace `#include "render/food_overlay.h"` with `#include "render/band_overlay.h"`, which holds the OVERLAY_BAND the tooltip reaches with.

Step 4: Delete the four files with `git rm src/render/food_overlay.h src/render/food_overlay.cpp src/views/food_overlay.h src/views/food_overlay.cpp`, and remove `food_overlay.cpp` from src/render/CMakeLists.txt and src/views/CMakeLists.txt.

Step 5: Check nothing names them

Run: `grep -rn -E 'food_overlay\.h|buildFoodOverlay|buildFoodAvailabilityOverlay|foodColor|foodDiscount|OVERLAY_FULL' src tests | wc -l`
Expected: `0`

### Task 12: Confirm the criteria

Step 1: Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i warning`
Expected: `0`, and the build ends without error.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: clean, exit status 0.

Step 4: Confirm criterion 3

Run: `grep -c -i food src/legible/availability.h src/legible/availability.cpp src/render/band_overlay.h src/render/band_overlay.cpp src/views/availability_overlay.h src/views/availability_overlay.cpp`
Expected: each of the six lines ends in `:0`.

### Task 13: The after report and capture

Step 1: Run (in the background): `scripts/runtime-report.sh build/runtime-report/prepared-availability-after.txt`
Expected: `runtime-report: wrote ...` after about 7 minutes. Compile nothing while it runs.

Step 2: Run: `scripts/runtime-report.sh --compare build/runtime-report/prepared-availability-before.txt build/runtime-report/prepared-availability-after.txt`
Expected: the line for windows-release tests/parks/stress/full.park food-overlay reads `faster` (criterion 4). Put the comparison in the feature's report, with the windows-release-app-overlay frames lines of both reports.

Step 3: Capture the overlay with the windows-release build the after report made

Run: `build/windows-release/ThemeParkJones.exe --park tests/parks/warm.park --ticks 3000 --overlay food --capture build/prepared-availability-after.bmp; echo $?`
Expected: `0`. Open it beside build/prepared-availability-before.bmp: the same bands in the same colors, apart from where guests stand (criterion 2). Put both captures in the feature's report, and ask the user to try criterion 2's hover by play.

### Task 14: Commit

Commit the feature once through the commit-hygiene skill, with `git commit -o` naming only this feature's files: the files Tasks 1 and 3 to 11 create, modify, or delete, and this feature's FEATURE.md, PLAN.md, and RESEARCH.md. Subject: `Legible: Build the food overlay as an availability overlay`.
