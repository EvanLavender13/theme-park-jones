# Implementation Plan: Food Overlay

## Goal

Add tpj_legible with food availability and its exact attribution, and draw it as a banded overlay the Debug panel and --overlay food turn on.

## Approach

tpj_legible is a static library linking tpj_sim alone, holding foodAvailability, which samples guest route distance at a place and each reachable source's offer the way guests find it, and sums the discounted reliefs in source order. The renderer builds the overlay on the CPU as a tent-shaped band along each guest path carrier: rows of three vertices at sample distances, joined by up-facing triangles, with a cone beyond each end, colored through a five-stop viridis ramp. The builder takes the value to shade by as a function, so render never depends on tpj_legible. The renderer draws the overlay through the park pipeline right after the terrain, and the app, which composes the two, rebuilds it every frame while the Debug panel's Food overlay checkbox is checked, keeping no cache.

## Placement

Decision 0027 places each behavior this feature adds:

- Food availability, its contributions, REFERENCE_SPEED, and the discount curve: the new module src/legible, library tpj_legible, component legible/food.h. tpj_legible depends on sim and core alone, a peer of tools, render, and scenarios, depending on none of them and depended on only by app. sound-architecture's layered-dependencies declares its layer with the others.
- The ramp, the band's geometry, and the overlay's constants: render, component render/food_overlay.h, built from any world and any value function, so it needs no legible header.
- Uploading and drawing the overlay mesh: render, the Renderer in render/renderer.h, beside the park, guest, and ghost meshes.
- The Food overlay checkbox: app, the Debug panel in app/debug_panel.h.
- --overlay food: app, main.cpp's parseOptions, which already owns every option.
- Building the overlay each frame: one call in main.cpp's frame sequence, composing foodAvailability with buildFoodOverlay and setOverlayMesh. It holds no state and no cache, so replacing the world needs no reset, and it adds no concern to main.cpp beyond the step, as drawing the graph view is one. composed-app moves it into the scene sync with the other steps.

All commands run from WSL at the repository root unless a step says otherwise.

## Tasks

### Task 1: Write the legible module's spec

Files:
- Create: `src/legible/SPEC.md`

Step 1: Write the file with exactly this content.

```markdown
# legible

Legible simulation (plans/legible-simulation): explanations of the simulation for display, computed from what the park publishes. The library tpj_legible links tpj_sim alone, so its values are computed and tested without a window (principle 10). It reads a world only through the medium's fields and networks, parkNetwork, and park intent (decision 0025), takes it by const reference, and changes nothing, so nothing it computes enters a world's state, hash, or save (principle 1).

## Food availability

Food availability at a place is how well fed a guest standing there could be: the sum, over the shops it can reach whose offers say meals are available, of each shop's relief discounted by the time it would take to get a meal there (decision 0020). Each shop's term is its contribution, so the value is attributed to its sources exactly (principle 8).

foodAvailability(world, place) gives a FoodAvailability for a place on parkNetwork(world, PathKind::Guest), N. Let R be the entries sampleField of guest route distance on N gives at the place. For each source of R, in ascending key order, with E its first entry in R, the source's offer is the first of its own entries that sampleField of food-offer on N gives at the nodePlace of the source's lowest anchored node on N, or none when it anchors no node of N or has no entry there, as guests find offers (sim/guests/SPEC.md, Choice). A source contributes when its offer exists and has Supplied true. Its FoodContribution holds Shop, the source; Relief, the offer's Relief; Distance, E's Distance; Wait, the offer's Wait in seconds, the double of Wait times SIM_TICK_SECONDS; Time, the effective time, Distance / REFERENCE_SPEED + Wait, computed in that order; and Term, Relief times foodDiscount(Time). Contributions holds them in ascending source order, and Value is 0.0 with each Term added in that order. So the listed terms reconstruct the value exactly, bit for bit, and a place with no contributing source, including one that does not resolve on N, has no contributions and the value 0.0.

Distance is measured along the paths, so a shop across a fence is as far as its route (principle 4). Every reachable supplied shop contributes, however far, and its term only falls as its effective time grows (principle 5).

REFERENCE_SPEED is WALK_SPEED, 1.3 m/s, so an effective time is what a guest would feel. foodDiscount(t) is the authored piecewise-linear curve through the points of FOOD_DISCOUNT_CURVE, times in seconds: (0, 1), (60, 0.5), (120, 0.2), and (240, 0). A time below 0 counts as 0, and one above 240 as 240. At a point's X it is that point's Y, and strictly between consecutive points a and b it is a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X), computed in that order. A NaN gives 0.
```

### Task 2: Add the food overlay to the render spec

Files:
- Modify: `src/render/SPEC.md:15` (after the setGuestMesh paragraph) and before `## Graph overlay`

Step 1: After the paragraph that starts `setGuestMesh uploads the guests' mesh`, insert this paragraph, with a blank line before it.

```markdown
setOverlayMesh uploads the food overlay's mesh, replacing the one drawn before; an empty mesh draws nothing. drawFrame draws it after the terrain and before the park mesh, through the park mesh's pipeline, so it is lit, opaque, and depth tested, and paths, boxes, and guests draw over it where they lie above it.
```

Step 2: Before the line `## Graph overlay`, insert this section, followed by a blank line.

```markdown
## Food overlay

food_overlay.h builds the food-availability overlay on the CPU, so it can be tested without a GPU. It shades by a value function it is given, which the app makes from foodAvailability's Value (legible/SPEC.md), so render depends on no module but sim and core. It reads intent through parkPaths and the guest network through parkNetwork and the Network type's public queries, and changes nothing.

foodColor(value) gives OVERLAY_ZERO_COLOR, a gray of 0.55 in red, green, and blue, for a value not above 0, including a NaN. Otherwise, with s = min(value / OVERLAY_FULL, 1) * 4, i the lesser of floor(s) and 3, and f = s - i converted to float, each of red, green, and blue is a + (b - a) * f in float arithmetic, where a and b are that channel of OVERLAY_RAMP's stops i and i + 1, and alpha is 1. OVERLAY_RAMP is viridis at quarters of its range, #440154, #3B528B, #21908C, #5DC863, and #FDE725, each channel its byte divided by 255 and rounded to three places, so the ramp rises from dark purple to yellow, perceptually uniform and colorblind-safe. OVERLAY_FULL, 0.5, is a meal's relief with no walk and no wait, so a color means the same availability in every park, and more saturates at yellow. The ramp gives red 0.55 only between its green and yellow stops, where green exceeds 0.8, so no positive value's color is OVERLAY_ZERO_COLOR, and no food reads apart from a little.

A band's line is a carrier of parkNetwork(world, PathKind::Guest) whose key is the key of a guest path of parkPaths. Its sample distances are the distances of its points, of its stops, and of each positive multiple of OVERLAY_SPACING, 1 m, below its length, ascending, each distance once. A sample's direction t is that of the first segment at the first point, that of the last segment at the last point, at a point strictly between them the normalized sum of the unit directions of the segments before and after it, or the one before when that sum has zero length, as a ribbon's tangent is, and elsewhere that of the segment whose points' distances enclose it. A sample's row is three vertices, with p the groundPoint of its place, {the carrier, its distance}, and r = (-t.z, t.x): its left edge at p - r * OVERLAY_BAND and the height OVERLAY_EDGE_LIFT, its center at p and OVERLAY_TOP_LIFT, and its right edge at p + r * OVERLAY_BAND and OVERLAY_EDGE_LIFT. Each has the normal (0, 1, 0) and the color foodColor of the value function's value at the sample's place. Consecutive rows are joined by four triangles, wound counter-clockwise seen from above, covering the quad between their left edges and centers and the quad between their centers and right edges. They face up wherever neither row is at a point strictly between the line's ends. At a bend, the inner side's rows cross, and triangles there can face down and be culled, as a ribbon's do. An end cone lies beyond each end of the line, for the end's sample and the unit direction t pointing away from the line, its first segment's direction reversed at the first end and its last segment's direction at the last: a center vertex at the end's ground point and OVERLAY_TOP_LIFT, then JOINT_SEGMENTS + 1 rim vertices, the k-th at p + OVERLAY_BAND * (cos(πk/16) * r + sin(πk/16) * t) and OVERLAY_EDGE_LIFT, and a triangle facing up from the center to each rim vertex and the next, all with the normal (0, 1, 0) and the end sample's color. buildFoodOverlay(world, value) gives, for each band line in ascending key order, its rows' vertices and their triangles, then its first end's cone, then its last end's. So the band covers the ground within OVERLAY_BAND of each guest path's line, except on the outer side of its bends, where rows at the averaged direction leave it a little narrower. Connectors and backstage paths have none.

OVERLAY_BAND is 6 m, OVERLAY_TOP_LIFT 1.5 cm, and OVERLAY_EDGE_LIFT 0.5 cm, so the band lies over the terrain and under every path. Its surface falls with distance from its own line, as a tent. Between two consecutive rows of one segment, neither at a point strictly between the line's ends, both rows are perpendicular to the segment, and the surface over a ground point at distance d from the segment lies at OVERLAY_TOP_LIFT - (OVERLAY_TOP_LIFT - OVERLAY_EDGE_LIFT) * d / OVERLAY_BAND. So where the bands of two lines overlap, the nearer line's lies higher, and the depth test shows the value at the nearest guest path place. Within a line's own bend, where the rows at its point fold over one another, the far side of the bend can show.
```

### Task 3: Add the overlay to the app spec

Files:
- Modify: `src/app/SPEC.md:37`, `src/app/SPEC.md:43`, `src/app/SPEC.md:47`

Step 1: In the Tooling UI paragraph, replace

```
each node as a filled circle of its radius and color (render/SPEC.md). Below the checkbox, each frame,
```

with

```
each node as a filled circle of its radius and color (render/SPEC.md). Below it, a Food overlay checkbox is off at start unless --overlay food is given. Each frame, after the panels are built, the app gives the renderer setOverlayMesh of buildFoodOverlay for the world, shaded by foodAvailability's Value at each place, while the checkbox is checked, and an empty mesh while it is not (render/SPEC.md, legible/SPEC.md). It keeps no copy of the overlay, so the band follows every tick's offers and every world the app replaces. Below the checkboxes, each frame,
```

Step 2: After the line `--graph checks the Debug panel's Graph checkbox at start, so a capture shows the networks.`, insert a blank line and this paragraph.

```markdown
--overlay food checks the Debug panel's Food overlay checkbox at start, so a capture shows food availability. food is the only overlay.
```

Step 3: In the paragraph that starts `An unknown option`, replace

```
a --ticks value that is not a decimal count, or --hash given with --frames, --capture, or --graph, whatever their values,
```

with

```
a --ticks value that is not a decimal count, an --overlay value other than food, or --hash given with --frames, --capture, --graph, or --overlay, whatever their values,
```

### Task 4: Create tpj_legible with its interface

Files:
- Create: `src/legible/CMakeLists.txt`
- Create: `src/legible/food.h`
- Create: `src/legible/food.cpp`
- Modify: `CMakeLists.txt:45`

Step 1: Write `src/legible/CMakeLists.txt`.

```cmake
# Explanations of the simulation for display, computed from its fields, networks, and intent. They
# link the simulation alone, so tests compute them without a window (principle 10).
add_library(tpj_legible STATIC
    food.cpp)
target_include_directories(tpj_legible PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(tpj_legible PUBLIC tpj_sim)
tpj_configure_target(tpj_legible)
```

Step 2: Write `src/legible/food.h`.

```cpp
#ifndef TPJ_LEGIBLE_FOOD_H
#define TPJ_LEGIBLE_FOOD_H

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"

#include <array>
#include <vector>

namespace tpj {

class World;

// The walking speed, in meters per second, that turns a route distance into time: the guests'
// own, so an effective time is what a guest would feel.
inline constexpr double REFERENCE_SPEED = WALK_SPEED;

// How much of a shop's relief counts after an effective time in seconds (decision 0020): all of it
// at once, half after a minute, and none after four.
inline constexpr std::array<CurvePoint, 4> FOOD_DISCOUNT_CURVE{
    {{0.0, 1.0}, {60.0, 0.5}, {120.0, 0.2}, {240.0, 0.0}}};

// One shop's part of the food availability at a place: its offer's relief, its route distance in
// meters, its offer's wait and the effective time in seconds, and its term, the relief discounted
// by that time.
struct FoodContribution {
  EntityKey Shop = NULL_KEY;
  double Relief = 0.0;
  double Distance = 0.0;
  double Wait = 0.0;
  double Time = 0.0;
  double Term = 0.0;

  bool operator==(const FoodContribution &) const = default;
};

// The food availability at a place and the contributions it is the sum of, in ascending shop key.
struct FoodAvailability {
  std::vector<FoodContribution> Contributions;
  double Value = 0.0;

  bool operator==(const FoodAvailability &) const = default;
};

// The discount curve at a time in seconds: 1 at or below its first point, 0 at or beyond its last,
// and 0 for a NaN.
double foodDiscount(double seconds);

// The food availability at a place on the world's guest network: each reachable shop whose offer
// says meals are supplied contributes its relief discounted by its effective time, and the value
// is their terms added in order. Changes nothing.
FoodAvailability foodAvailability(const World &world, const Place &place);

} // namespace tpj

#endif
```

Step 3: Write `src/legible/food.cpp` with stubs.

```cpp
#include "legible/food.h"

namespace tpj {

double foodDiscount(double /*seconds*/) { return 0.0; }

FoodAvailability foodAvailability(const World & /*world*/, const Place & /*place*/) { return {}; }

} // namespace tpj
```

Step 4: In `CMakeLists.txt`, after the line `add_subdirectory(src/tools)`, add the line `add_subdirectory(src/legible)`.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: the build succeeds with no warnings.

### Task 5: Create the overlay's interface in the renderer

Files:
- Create: `src/render/food_overlay.h`
- Create: `src/render/food_overlay.cpp`
- Modify: `src/render/CMakeLists.txt:3-10`

Step 1: Write `src/render/food_overlay.h`.

```cpp
#ifndef TPJ_RENDER_FOOD_OVERLAY_H
#define TPJ_RENDER_FOOD_OVERLAY_H

#include "render/park_mesh.h"
#include "sim/medium/network.h"
#include "sim/world.h"

#include <array>
#include <functional>

namespace tpj {

// The overlay shades the ground this far, in meters, either side of each guest path's line.
inline constexpr double OVERLAY_BAND = 6.0;
// The most distance, in meters, between the places the overlay samples along a line, besides its
// points and stops.
inline constexpr double OVERLAY_SPACING = 1.0;
// The band is a low tent, highest on its line and lowest at its edges, so where bands overlap the
// nearest line's lies on top. It lies over the terrain and under every path.
inline constexpr float OVERLAY_TOP_LIFT = 0.015f;
inline constexpr float OVERLAY_EDGE_LIFT = 0.005f;
// The availability at the ramp's top: a meal's full relief with no walk and no wait.
inline constexpr double OVERLAY_FULL = 0.5;
// Viridis at quarters of its range, from dark purple to yellow: perceptually uniform and
// colorblind-safe.
inline constexpr std::array<Rgba, 5> OVERLAY_RAMP{{{0.267f, 0.004f, 0.329f, 1.0f},
                                                   {0.231f, 0.322f, 0.545f, 1.0f},
                                                   {0.129f, 0.565f, 0.549f, 1.0f},
                                                   {0.365f, 0.784f, 0.388f, 1.0f},
                                                   {0.992f, 0.906f, 0.145f, 1.0f}}};
// No food at all, in a gray the ramp never gives.
inline constexpr Rgba OVERLAY_ZERO_COLOR{0.55f, 0.55f, 0.55f, 1.0f};

// The ramp's color for an availability, saturating at OVERLAY_FULL, or OVERLAY_ZERO_COLOR for a
// value not above 0.
Rgba foodColor(double value);

// The value the overlay shades a place by, such as the food availability there.
using OverlayValue = std::function<double(const Place &)>;

// The band along each guest path's line, in key order, shaded by the value at the line's places,
// with a cone beyond each end.
ParkMesh buildFoodOverlay(const World &world, const OverlayValue &value);

} // namespace tpj

#endif
```

Step 2: Write `src/render/food_overlay.cpp` with stubs.

```cpp
#include "render/food_overlay.h"

namespace tpj {

Rgba foodColor(double /*value*/) { return OVERLAY_ZERO_COLOR; }

ParkMesh buildFoodOverlay(const World & /*world*/, const OverlayValue & /*value*/) { return {}; }

} // namespace tpj
```

Step 3: In `src/render/CMakeLists.txt`, add `food_overlay.cpp` to the `add_library(tpj_render STATIC` source list before `graph_overlay.cpp`. tpj_render links nothing new.

Step 4: In `src/app/CMakeLists.txt`, replace

```cmake
target_link_libraries(tpj_app PRIVATE tpj_park_files tpj_render tpj_sim tpj_tools)
```

with

```cmake
target_link_libraries(tpj_app PRIVATE tpj_legible tpj_park_files tpj_render tpj_sim tpj_tools)
```

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build succeeds with no warnings.

### Task 6: Run the test pass

Dispatch the test-writer agent with only these paths: `plans/legible-simulation/explained-food/food-overlay/FEATURE.md`, `src/legible/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`, `src/sim/SPEC.md`, `src/sim/medium/SPEC.md`, `src/sim/routes/SPEC.md`, `src/sim/operations/SPEC.md`, `src/sim/guests/SPEC.md`, `src/legible/food.h`, `src/render/food_overlay.h`, `docs/principles.md`, `docs/conventions.md`, `plans/slices/boxes-and-tubes/SLICE.md`.

Expected: the agent writes tests under tests/legible/ (a new directory with its CMakeLists.txt building tpj_legible_tests), tests/render/, tests/app/, and tests/integration/, wires them into tests/CMakeLists.txt and each directory's CMakeLists.txt, and reports the files it wrote. Its tests compile against the stubs, and those of criteria 1 to 6 and 8 fail until the tasks below. Criterion 7's refusals already hold, since the app refuses any option it does not know. Record the file names it reports; the filters below use them.

### Task 7: Implement foodDiscount and foodAvailability

Files:
- Modify: `src/legible/food.cpp`

Step 1: Replace the file with this content.

```cpp
#include "legible/food.h"

#include "sim/medium/field.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <algorithm>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

// The source's offer when it says meals are supplied, found as guests find it: its first food-offer
// entry at the place of its lowest anchored node, or none.
std::optional<OfferEntry> suppliedOffer(const World &world, const Network &network,
                                        EntityKey source) {
  const std::vector<uint32_t> anchored = network.anchoredNodes(source);
  if (anchored.empty()) {
    return std::nullopt;
  }
  for (const SampledEntry<OfferEntry> &entry :
       sampleField<FoodOffer>(world, network, network.nodePlace(anchored.front()))) {
    if (entry.Source == source) {
      if (!entry.Value.Supplied) {
        return std::nullopt;
      }
      return entry.Value;
    }
  }
  return std::nullopt;
}

} // namespace

double foodDiscount(double seconds) {
  const double t =
      std::clamp(seconds, FOOD_DISCOUNT_CURVE.front().X, FOOD_DISCOUNT_CURVE.back().X);
  for (size_t i = 1; i < FOOD_DISCOUNT_CURVE.size(); ++i) {
    const CurvePoint &a = FOOD_DISCOUNT_CURVE[i - 1];
    const CurvePoint &b = FOOD_DISCOUNT_CURVE[i];
    if (t == a.X) {
      return a.Y;
    }
    if (t < b.X) {
      return a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X);
    }
  }
  // Only the last point and a NaN, which no comparison holds for, reach here.
  return FOOD_DISCOUNT_CURVE.back().Y;
}

FoodAvailability foodAvailability(const World &world, const Place &place) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  FoodAvailability availability;
  EntityKey previous = NULL_KEY;
  for (const SampledEntry<RouteEntry> &route :
       sampleField<RouteDistance<PathKind::Guest>>(world, network, place)) {
    // Sources come in ascending order, and only a source's first entry counts.
    if (route.Source == previous) {
      continue;
    }
    previous = route.Source;
    const std::optional<OfferEntry> offer = suppliedOffer(world, network, route.Source);
    if (!offer) {
      continue;
    }
    FoodContribution contribution;
    contribution.Shop = route.Source;
    contribution.Relief = offer->Relief;
    contribution.Distance = route.Value.Distance;
    contribution.Wait = static_cast<double>(offer->Wait) * SIM_TICK_SECONDS;
    contribution.Time = contribution.Distance / REFERENCE_SPEED + contribution.Wait;
    contribution.Term = contribution.Relief * foodDiscount(contribution.Time);
    availability.Value += contribution.Term;
    availability.Contributions.push_back(contribution);
  }
  return availability;
}

} // namespace tpj
```

Step 2: Build and run the legible tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible_tests && build/windows-debug/tpj_legible_tests.exe 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`, covering criteria 1, 2, and 3.

Step 3: Build and run the integration tests of criterion 8.

Run: `cmake.exe --build --preset windows-debug --target tpj_integration_tests && build/windows-debug/tpj_integration_tests.exe -# "[#<the test pass's food availability file, without .cpp>]" --durations yes 2>&1 | tr -d '\r' | tail -8`
Expected: `All tests passed`, each test well under 1 s.

### Task 8: Implement foodColor

Files:
- Modify: `src/render/food_overlay.cpp`

Step 1: Replace the file with this content, whose buildFoodOverlay stays a stub until Task 9.

```cpp
#include "render/food_overlay.h"

#include <algorithm>
#include <math.h>
#include <stddef.h>

namespace tpj {

Rgba foodColor(double value) {
  if (isnan(value) || value <= 0.0) {
    return OVERLAY_ZERO_COLOR;
  }
  const double scaled =
      std::min(value / OVERLAY_FULL, 1.0) * static_cast<double>(OVERLAY_RAMP.size() - 1);
  const size_t index = std::min(static_cast<size_t>(scaled), OVERLAY_RAMP.size() - 2);
  const auto fraction = static_cast<float>(scaled - static_cast<double>(index));
  const Rgba &low = OVERLAY_RAMP[index];
  const Rgba &high = OVERLAY_RAMP[index + 1];
  return {low.R + (high.R - low.R) * fraction, low.G + (high.G - low.G) * fraction,
          low.B + (high.B - low.B) * fraction, 1.0f};
}

ParkMesh buildFoodOverlay(const World & /*world*/, const OverlayValue & /*value*/) { return {}; }

} // namespace tpj
```

Step 2: Build and run the render tests of the overlay.

Run: `cmake.exe --build --preset windows-debug --target tpj_render_tests && build/windows-debug/tpj_render_tests.exe -# "[#<the test pass's food overlay file, without .cpp>]" 2>&1 | tr -d '\r' | tail -5`
Expected: the tests of criterion 4 pass; those of criteria 5 and 6 still fail.

### Task 9: Implement buildFoodOverlay

Files:
- Modify: `src/render/food_overlay.cpp`

Step 1: Add these includes to the file's project and system include groups, keeping each group sorted.

```cpp
#include "sim/park/intent.h"
#include "sim/routes/networks.h"

#include <numbers>
#include <optional>
#include <stdint.h>
#include <vector>
```

`<math.h>` is already there from Task 8.

Step 2: After `namespace tpj {`, insert this anonymous namespace.

```cpp
namespace {

// A sample of a band's line: where it stands, the line's direction there, and its color.
struct BandRow {
  GroundPoint At;
  ParkPoint Direction;
  Rgba Color;
};

void addVertex(ParkMesh &mesh, double x, float y, double z, Rgba color) {
  ParkVertex vertex;
  vertex.Position[0] = static_cast<float>(x);
  vertex.Position[1] = y;
  vertex.Position[2] = static_cast<float>(z);
  vertex.Normal[1] = 1.0f;
  vertex.Color = color;
  mesh.Vertices.push_back(vertex);
}

// The unit direction from one line point to the next. A carrier's points are distinct.
ParkPoint unitStep(const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double length = sqrt(dx * dx + dz * dz);
  return {dx / length, dz / length};
}

// The distances the band samples along a carrier: its points', its stops', and each positive
// multiple of OVERLAY_SPACING below its length, ascending, each once.
std::vector<double> sampleDistances(const Carrier &carrier) {
  const double length = carrier.Points.back().Distance;
  std::vector<double> distances;
  distances.reserve(carrier.Points.size() + carrier.Stops.size() +
                    static_cast<size_t>(length / OVERLAY_SPACING));
  for (const CarrierPoint &point : carrier.Points) {
    distances.push_back(point.Distance);
  }
  for (const CarrierStop &stop : carrier.Stops) {
    distances.push_back(stop.Distance);
  }
  for (uint64_t k = 1; static_cast<double>(k) * OVERLAY_SPACING < length; ++k) {
    distances.push_back(static_cast<double>(k) * OVERLAY_SPACING);
  }
  std::ranges::sort(distances);
  const auto [first, last] = std::ranges::unique(distances);
  distances.erase(first, last);
  return distances;
}

// The line's direction at a distance along it: its first segment's at the first point, its last
// segment's at the last, the two segments' averaged at a point between, as a ribbon's tangent is,
// and elsewhere the direction of the segment holding it.
ParkPoint directionAt(const std::vector<CarrierPoint> &points, double distance) {
  const auto after = std::ranges::lower_bound(points, distance, {}, &CarrierPoint::Distance);
  const auto i = static_cast<size_t>(after - points.begin());
  if (i == 0) {
    return unitStep(points[0], points[1]);
  }
  const ParkPoint before = unitStep(points[i - 1], points[i]);
  if (points[i].Distance != distance || i + 1 == points.size()) {
    return before;
  }
  const ParkPoint next = unitStep(points[i], points[i + 1]);
  const ParkPoint sum{before.X + next.X, before.Z + next.Z};
  const double length = sqrt(sum.X * sum.X + sum.Z * sum.Z);
  if (length > 0.0) {
    return {sum.X / length, sum.Z / length};
  }
  return before;
}

// Adds a row of three vertices for each sample, left edge, center, and right edge, and joins each
// row to the next with four triangles facing up.
void appendBand(ParkMesh &mesh, const std::vector<BandRow> &rows) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (const BandRow &row : rows) {
    const ParkPoint right{-row.Direction.Z, row.Direction.X};
    addVertex(mesh, row.At.X - right.X * OVERLAY_BAND, OVERLAY_EDGE_LIFT,
              row.At.Z - right.Z * OVERLAY_BAND, row.Color);
    addVertex(mesh, row.At.X, OVERLAY_TOP_LIFT, row.At.Z, row.Color);
    addVertex(mesh, row.At.X + right.X * OVERLAY_BAND, OVERLAY_EDGE_LIFT,
              row.At.Z + right.Z * OVERLAY_BAND, row.Color);
  }
  for (uint32_t i = 0; i + 1 < rows.size(); ++i) {
    const uint32_t left0 = first + 3 * i;
    const uint32_t center0 = left0 + 1;
    const uint32_t right0 = left0 + 2;
    const uint32_t left1 = left0 + 3;
    const uint32_t center1 = left0 + 4;
    const uint32_t right1 = left0 + 5;
    mesh.Indices.insert(mesh.Indices.end(), {left0, center0, left1, left1, center0, center1,
                                             center0, right0, center1, center1, right0, right1});
  }
}

// Adds the cone beyond a line's end, for the unit direction pointing away from the line: its
// center on the line at the top lift, and a half circle of the band's width at the edge lift.
void appendCone(ParkMesh &mesh, const BandRow &end, ParkPoint direction) {
  const ParkPoint right{-direction.Z, direction.X};
  const auto center = static_cast<uint32_t>(mesh.Vertices.size());
  addVertex(mesh, end.At.X, OVERLAY_TOP_LIFT, end.At.Z, end.Color);
  for (uint32_t k = 0; k <= JOINT_SEGMENTS; ++k) {
    const double angle = std::numbers::pi * static_cast<double>(k) / JOINT_SEGMENTS;
    const double across = OVERLAY_BAND * cos(angle);
    const double along = OVERLAY_BAND * sin(angle);
    addVertex(mesh, end.At.X + across * right.X + along * direction.X, OVERLAY_EDGE_LIFT,
              end.At.Z + across * right.Z + along * direction.Z, end.Color);
  }
  for (uint32_t k = 0; k < JOINT_SEGMENTS; ++k) {
    mesh.Indices.insert(mesh.Indices.end(), {center, center + 1 + k, center + 2 + k});
  }
}

} // namespace
```

Step 3: Replace the stub `ParkMesh buildFoodOverlay(const World & /*world*/, const OverlayValue & /*value*/) { return {}; }` with this definition.

```cpp
ParkMesh buildFoodOverlay(const World &world, const OverlayValue &value) {
  std::vector<EntityKey> guestPaths;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Guest) {
      guestPaths.push_back(path.Key);
    }
  }
  const Network &network = parkNetwork(world, PathKind::Guest);
  ParkMesh mesh;
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
      rows.push_back({*at, directionAt(carrier.Points, distance),
                      foodColor(value(place))});
    }
    if (rows.empty()) {
      continue;
    }
    const std::vector<CarrierPoint> &points = carrier.Points;
    appendBand(mesh, rows);
    appendCone(mesh, rows.front(), unitStep(points[1], points[0]));
    appendCone(mesh, rows.back(), unitStep(points[points.size() - 2], points.back()));
  }
  return mesh;
}
```

Step 4: Build and run the render tests of the overlay.

Run: `cmake.exe --build --preset windows-debug --target tpj_render_tests && build/windows-debug/tpj_render_tests.exe -# "[#<the test pass's food overlay file, without .cpp>]" 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`, covering criteria 4, 5, and 6.

### Task 10: Give the renderer an overlay mesh

Files:
- Modify: `src/render/renderer.h`
- Modify: `src/render/renderer.cpp`

Step 1: In `src/render/renderer.h`, in `struct Renderer`, after `uint32_t TerrainIndexCount = 0;`, add

```cpp
  SDL_GPUBuffer *OverlayVertices = nullptr;
  SDL_GPUBuffer *OverlayIndices = nullptr;
  uint32_t OverlayIndexCount = 0;
```

and after the declaration of `setParkMesh`, add

```cpp

// Uploads the food overlay's mesh drawn from now on, replacing the one before, drawn opaque over
// the terrain and under the park. An empty mesh draws nothing. Returns false and logs through SDL
// on failure.
bool setOverlayMesh(Renderer &renderer, const ParkMesh &mesh);
```

Step 2: In `src/render/renderer.cpp`, in `destroyRenderer`, after `SDL_ReleaseGPUGraphicsPipeline(renderer.Device, renderer.TerrainPipeline);`, add

```cpp
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.OverlayIndices);
```

Step 3: After the definition of `setParkMesh`, add

```cpp

bool setOverlayMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.OverlayVertices, renderer.OverlayIndices,
                     renderer.OverlayIndexCount);
}
```

Step 4: In `drawScene`, replace

```cpp
  // The camera uniforms pushed before the pass serve this pipeline too.
  if (renderer.ParkIndexCount > 0) {
```

with

```cpp
  // The camera uniforms pushed before the pass serve this pipeline too. The overlay lies just over
  // the terrain, and the park's paths, boxes, and guests over it.
  if (renderer.OverlayIndexCount > 0) {
    SDL_BindGPUGraphicsPipeline(pass, renderer.ParkPipeline);
    const SDL_GPUBufferBinding overlayVertexBinding = {renderer.OverlayVertices, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &overlayVertexBinding, 1);
    const SDL_GPUBufferBinding overlayIndexBinding = {renderer.OverlayIndices, 0};
    SDL_BindGPUIndexBuffer(pass, &overlayIndexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, renderer.OverlayIndexCount, 1, 0, 0, 0);
  }
  if (renderer.ParkIndexCount > 0) {
```

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build succeeds with no warnings.

### Task 11: Parse --overlay food

Files:
- Modify: `src/app/main.cpp:45-102`

Step 1: In `struct Options`, after `bool ShowGraph = false;`, add `bool ShowFoodOverlay = false;`.

Step 2: Replace the comment above `parseOptions`

```cpp
// --park PATH starts from a park file, --ticks N steps it N ticks before the first frame, and
// --hash prints the state hash after them and exits. --frames N exits after N frames. --graph
// starts with the graph view on.
```

with

```cpp
// --park PATH starts from a park file, --ticks N steps it N ticks before the first frame, and
// --hash prints the state hash after them and exits. --frames N exits after N frames. --graph
// starts with the graph view on, and --overlay food with the food overlay on.
```

Step 3: In `parseOptions`, after the `--graph` branch, add

```cpp
    } else if (strcmp(argv[i], "--overlay") == 0 && hasValue) {
      // food is the only overlay.
      valid = strcmp(argv[++i], "food") == 0;
      options.ShowFoodOverlay = valid;
```

Step 4: Replace

```cpp
      (options.FramesGiven || options.CapturePath != nullptr || options.ShowGraph)) {
```

with

```cpp
      (options.FramesGiven || options.CapturePath != nullptr || options.ShowGraph ||
       options.ShowFoodOverlay)) {
```

and replace

```cpp
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH] [--graph]",
            argv[0]);
```

with

```cpp
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH] [--graph] "
            "[--overlay food]",
            argv[0]);
```

Step 5: Build and run the command line tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_tests && build/windows-debug/tpj_app_tests.exe -# "[#command_line_test]" 2>&1 | tr -d '\r' | tail -3`
Expected: `All tests passed`, covering criterion 7.

### Task 12: Add the Food overlay checkbox

Files:
- Modify: `src/app/debug_panel.h`
- Modify: `src/app/debug_panel.cpp`

Step 1: In `src/app/debug_panel.h`, replace

```cpp
// Draws the tooling panel with frame rate, simulation tick, camera state, the Graph checkbox,
// which sets showGraph, a line for each shop's record, the guest count and mean hunger, and the
// guests waiting and meals eaten. Call between ImGui::NewFrame and ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph);
```

with

```cpp
// Draws the tooling panel with frame rate, simulation tick, camera state, the Graph checkbox,
// which sets showGraph, the Food overlay checkbox, which sets showFoodOverlay, a line for each
// shop's record, the guest count and mean hunger, and the guests waiting and meals eaten. Call
// between ImGui::NewFrame and ImGui::Render.
void drawDebugPanel(const DebugStats &stats, bool &showGraph, bool &showFoodOverlay);
```

Step 2: In `src/app/debug_panel.cpp`, change the definition's signature to `void drawDebugPanel(const DebugStats &stats, bool &showGraph, bool &showFoodOverlay) {`, and after `ImGui::Checkbox("Graph", &showGraph);` add `ImGui::Checkbox("Food overlay", &showFoodOverlay);`.

### Task 13: Draw the overlay in the app

Files:
- Modify: `src/app/main.cpp`

Step 1: Add `#include "legible/food.h"` and `#include "render/food_overlay.h"` to the project include group, keeping it sorted.

Step 2: After the definition of `updateGuestMesh`, add

```cpp

// The food overlay while it is shown, shaded by the food availability at each place, and an empty
// mesh while it is not. It is built afresh each frame, so it keeps nothing to reset.
tpj::ParkMesh foodOverlayMesh(const tpj::World &world, bool show) {
  if (!show) {
    return {};
  }
  return tpj::buildFoodOverlay(world, [&world](const tpj::Place &place) {
    return tpj::foodAvailability(world, place).Value;
  });
}
```

Step 3: Give `drawPanels` and `buildUi` a `bool &showFoodOverlay` parameter after `bool &showGraph`, pass it from `buildUi` to `drawPanels`, and in `drawPanels` replace `tpj::drawDebugPanel(stats, showGraph);` with `tpj::drawDebugPanel(stats, showGraph, showFoodOverlay);`. Update both functions' comments to say they also set showFoodOverlay from the Food overlay checkbox.

Step 4: In `runLoop`, after `bool showGraph = options.ShowGraph;`, add

```cpp
  bool showFoodOverlay = options.ShowFoodOverlay;
```

and replace

```cpp
    buildUi(renderer.Window, world, camera, view, showGraph, tool);
```

with

```cpp
    buildUi(renderer.Window, world, camera, view, showGraph, showFoodOverlay, tool);
    // After the panels, so the checkbox's change shows in this frame.
    if (!tpj::setOverlayMesh(renderer, foodOverlayMesh(world, showFoodOverlay))) {
      return false;
    }
```

Step 5: Build the app.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 14: Capture warm.park and cut.park with the overlay

Step 1: Capture both parks after 3000 ticks with the overlay on.

Run: `cd build/windows-debug && timeout 120 ./ThemeParkJones.exe --park ../../tests/parks/warm.park --ticks 3000 --overlay food --capture warm-food.bmp && timeout 120 ./ThemeParkJones.exe --park ../../tests/parks/cut.park --ticks 3000 --overlay food --capture cut-food.bmp; cd ../..`
Expected: both exit with status 0 and write their BMPs.

Step 2: Convert them to PNG in the scratchpad with PIL, view both, then delete the BMPs.

Expected (criterion 9): in warm-food, the guest paths and backstage path lie as flat ribbons over a band of ramp colors either side of each guest path, brighter toward the shop, the shop and depot are boxes, and guests stand on the paths; the Debug panel's Food overlay box is checked. In cut-food, the shop has its violet starved mark, and the band is the zero gray throughout. If the band's colors show no gradient in warm-food, or it hides a path, stop and report: that is a deviation.

Step 3: Read the frame time. With the overlay shown, the Debug panel's `Frame ... ms` line in warm-food is recorded in the report, beside the same line from a capture without --overlay.

Run: `cd build/windows-debug && timeout 120 ./ThemeParkJones.exe --park ../../tests/parks/warm.park --ticks 3000 --capture warm-plain.bmp; cd ../..`
Expected: the capture is written. Convert it, read its frame time, and delete the BMP. The overlay is rebuilt and uploaded every frame, so a frame time with the overlay more than 2 ms above the one without is a deviation to report.

### Task 15: Verify the feature

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
Expected: it finishes with no warnings.

Step 5: The cross-build check is unaffected, since nothing under src/sim, src/core, src/scenarios, or tests/parks changes. Confirm with `git status --short` that no file under those paths is modified.

### Task 16: Commit

Step 1: Dispatch the reviewer on the diff, with FEATURE.md, src/legible/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, and docs/principles.md as context, and give its findings to Evan verbatim.

Step 2: Stage exactly the feature's paths: `git add plans/legible-simulation/explained-food/food-overlay src/legible src/render src/app CMakeLists.txt tests/legible tests/render tests/app tests/integration tests/CMakeLists.txt`, then check `git status --short` shows nothing else staged and the untracked parks/routes.park and parks/sketch.park unstaged.

Step 3: Commit through commit-hygiene with the subject `Legible: Add the food-availability overlay` and a body of at most 72-character lines saying that tpj_legible computes food availability with exact per-shop terms, that the renderer draws it as a tent-shaped band along guest paths through a viridis ramp, and that the Debug panel and --overlay food turn it on, ending with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
