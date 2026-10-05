# Implementation Plan: Corners and Widths

## Goal

Rounded corners in every segment's ground line, the commands SetCornerRadius and SetSegmentWidth, Edit path's radius and side handles, and ghosts that show each edit's segments as they will lie.

## Approach

A new private component in sim/park holds a park's path points and segments apart from the rest of the world, as a PathLayout. It derives every segment's ground line with its rounded corners, so parkSegments, parkCorners, and the edit plans all read the same lines. Every path command builds an edit plan as today. Instead of checking each new part as a straight line, the plan is applied to a copy of the layout, and every segment whose ground line or width changes must have a line and fit the solids. The same comparison gives editedSegments, which the ghost draws. The tools add two handle kinds to Edit path, and the app draws them through two new park_mesh functions.

## Placement

Decision 0027 places each behavior this feature adds:

- A park's path layout, its rounded ground lines, and its corners: sim/park, a new private component, src/sim/park/internal/layout.h with src/sim/park/layout.cpp. parkSegments, parkCorners, and the edit plans all need the lines, and the plans need them for a layout no world holds yet. Keeping the geometry apart keeps intent.cpp about registering and reading intent, and edits.cpp about commands.
- parkCorners and ParkCorner: sim/park, intent (src/sim/park/intent.h). It is where other modules read every kind of intent.
- cornerRadiusFor and ARC_TOLERANCE: sim/park, geometry (src/sim/park/geometry.h). It holds the shared measures the tools and renderer use, such as segmentLine and footprintOf.
- SetCornerRadius, SetSegmentWidth, the check of every changed ground line, DeleteSegment's plan, and editedSegments: sim/park, edits (src/sim/park/edits.h). Every command's plan lives there, and one plan per command serves isAccepted, applyCommand, pathCrossings, and editedSegments, so the ghost and the commit agree. edits.cpp grows by about 150 lines and loses about 110. It stays one component about commands.
- Radius and side handles, their picking, and Edit path's drags: tools (src/tools/tools.h). It owns picking and what each tool's pointer does.
- Drawing every edit's reshaped segments, the radius handles, and a segment's side marks: render, park_mesh (src/render/park_mesh.h). It draws every ghost and highlight.
- Passing the handles from the tool to the ghost: app, interaction (src/app/input/interaction.h), scene_sync's PreviewLook (src/app/scene/scene_sync.h), and scene_uploads (src/app/scene/scene_uploads.cpp). They already pass pathStart and pathFinish the same way. application.cpp gains two arguments in PreviewLook's construction, which is composition only.
- Random SetCornerRadius and SetSegmentWidth edits: scenarios, park_edits (src/scenarios/park_edits.cpp). It draws the random edits of every command.

## Tasks

### Task 1: Update the park spec

Files:
- Modify: `src/sim/park/SPEC.md`

Step 1: Make each src/sim/park/SPEC.md change in FEATURE.md's Spec changes, in order. Copy the quoted text without the `>` quote marks, keeping blank lines between paragraphs and lists.

### Task 2: Update the tools, render, and app specs

Files:
- Modify: `src/tools/SPEC.md`
- Modify: `src/render/SPEC.md`
- Modify: `src/app/SPEC.md`

Step 1: Make each change in FEATURE.md's Spec changes to these three files, in order.

### Task 3: Declare the new types, commands, and queries

Files:
- Modify: `src/sim/park/intent.h`
- Modify: `src/sim/park/intent.cpp`
- Modify: `src/sim/park/geometry.h`
- Modify: `src/sim/park/geometry.cpp`
- Modify: `src/sim/park/edits.h`
- Modify: `src/sim/park/edits.cpp`
- Modify: `src/tools/tools.h`
- Modify: `src/tools/tools.cpp`
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`
- Modify: `tests/sim/park_world_checks_test.cpp`

Step 1: In intent.h, add ParkCorner after ParkPathPoint and parkCorners after parkSegments, as FEATURE.md's Public interface gives them. In intent.cpp, add a stub after parkSegments:

```cpp
std::vector<ParkCorner> parkCorners(const World & /*world*/) { return {}; }
```

Step 2: In geometry.h, add ARC_TOLERANCE after MIN_SEGMENT_LENGTH and cornerRadiusFor after footprintOf, as FEATURE.md gives them. In geometry.cpp, add a stub at the end of the namespace:

```cpp
double cornerRadiusFor(const ParkCorner & /*corner*/, double /*depth*/) { return 0.0; }
```

Step 3: In edits.h, add SetCornerRadius and SetSegmentWidth after DeletePoint, and replace addParkEdits' comment and the ParkEdit alias, as FEATURE.md gives them. Delete movedSegments and mergedSegment with their comments, and declare editedSegments in their place. After the DeletePoint isAccepted declaration, add:

```cpp
bool isAccepted(const World &world, const SetCornerRadius &command);
bool isAccepted(const World &world, const SetSegmentWidth &command);
```

After the DeletePoint applyCommand declaration, add:

```cpp
void applyCommand(World &world, const SetCornerRadius &command);
void applyCommand(World &world, const SetSegmentWidth &command);
```

Step 4: In edits.cpp, in addParkEdits, register the two after DeletePoint:

```cpp
  schema.addCommand<SetCornerRadius>();
  schema.addCommand<SetSegmentWidth>();
```

Delete the definitions of movedSegments and mergedSegment. Add stubs after the DeletePoint definitions of the same names, leaving out the names of unused parameters: the two isAccepted return `false`, the two applyCommand do nothing, and editedSegments returns `{}`. Task 6 replaces them.

Step 5: In src/render/park_mesh.cpp, two places call the deleted functions. In appendGhostMove, replace `const std::vector<ParkSegment> moved = movedSegments(world, move);` with `const std::vector<ParkSegment> moved = editedSegments(world, edit);`. In appendGhostDeletePoint, replace the `if (const std::optional<ParkSegment> merged = mergedSegment(world, del)) { ... }` block with:

```cpp
  for (const ParkSegment &segment : editedSegments(world, edit)) {
    appendGhostSegment(mesh, segment.Kind, segment.Line, segment.Width,
                       ghostColor(world, edit, pathColor(segment.Kind)));
  }
```

Task 8 rewrites these ghosts.

Step 6: In park_mesh.h, add the radius handle and side mark declarations after appendFinishMark, as FEATURE.md gives them. In park_mesh.cpp, add stubs after appendFinishMark that do nothing, leaving out the names of unused parameters. Task 10 replaces them.

Step 7: In tools.h, add the five constants after POINT_REACH, then HandleKind and EditHandle, the two ToolState fields after Inserting, and the five functions after snapPoint, as FEATURE.md gives them. In tools.cpp, add stubs after snapPoint, leaving out the names of unused parameters: radiusHandleOf returns `corner.At`, radiusHandleAt and sideAt return `std::nullopt`, radiusHandles returns `{}`, and hoveredHandle returns `std::nullopt`. Task 9 replaces them.

Step 8: tests/sim/park_world_checks_test.cpp calls mergedSegment in deletingPointUnderGuests, to require that the deletion merges. Replace the comment and REQUIRE

```cpp
  // Without a merge, this edit would only delete segments, as deletingGuestSegment already does.
  REQUIRE(mergedSegment(before, del).has_value());
```

with a check of the same thing through parkSegments:

```cpp
  // Without a merge, this edit would only delete segments, as deletingGuestSegment already does.
  // The point merges its two segments when their far ends differ.
  std::vector<EntityKey> farEnds;
  for (const ParkSegment &segment : parkSegments(before)) {
    if (segment.From == pointKey || segment.To == pointKey) {
      farEnds.push_back(segment.From == pointKey ? segment.To : segment.From);
    }
  }
  REQUIRE(farEnds.size() == 2);
  REQUIRE(farEnds[0] != farEnds[1]);
```

This changes only how the test finds what it already required, so the build keeps passing.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings and no errors.

### Task 4: Run the test pass

Step 1: Run the test pass that implementing-features describes, for the two chosen edits in FEATURE.md's Tests section. While the stubs stand, parkCorners gives no corner, so the radius edit fails at its REQUIRE that a corner exists, and the width edit fails at its REQUIRE that the world accepts it. Those are the expected failures.

### Task 5: Round corners in ground lines

Files:
- Create: `src/sim/park/internal/layout.h`
- Create: `src/sim/park/layout.cpp`
- Modify: `src/sim/CMakeLists.txt:13-16`
- Modify: `src/sim/park/intent.cpp`
- Modify: `src/sim/park/geometry.cpp`

Step 1: Create src/sim/park/internal/layout.h:

```cpp
#ifndef TPJ_SIM_PARK_INTERNAL_LAYOUT_H
#define TPJ_SIM_PARK_INTERNAL_LAYOUT_H

#include "sim/entity_key.h"
#include "sim/park/intent.h"
#include "sim/park/internal/components.h"
#include "sim/world.h"

#include <vector>

namespace tpj {

struct LayoutPoint {
  EntityKey Key = NULL_KEY;
  PathPointIntent Intent;
};

struct LayoutSegment {
  EntityKey Key = NULL_KEY;
  PathSegmentIntent Intent;
};

// A park's path points and segments, each in ascending key order, apart from the rest of its
// world, so that an edit can be tried on a copy.
struct PathLayout {
  std::vector<LayoutPoint> Points;
  std::vector<LayoutSegment> Segments;
};

PathLayout layoutOf(const World &world);
// Each segment with its ground line, rounded at its corners, in ascending key order.
std::vector<ParkSegment> segmentsOf(const PathLayout &layout);
// Each corner, in ascending key order of its point.
std::vector<ParkCorner> cornersOf(const PathLayout &layout);

} // namespace tpj

#endif
```

Step 2: Create src/sim/park/layout.cpp. The arc's tangent length is the radius times cross / (1 - dot) of the unit directions along the two segments, which needs no trigonometry (decision 0022). An unrounded segment's line is built exactly as segmentLine builds it, so a park without corners keeps its lines bit for bit. The code runs every frame through parkSegments, so it allocates no scratch list inside its loops: each half arc's piece count is found first and its points are written in place into one arc buffer for the whole layout, and each rounded line is built directly into the segment's own line.

```cpp
#include "sim/park/internal/layout.h"

#include "sim/park/geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// Ground line points nearer together than this, in meters, are one point.
constexpr double SAME_POINT = 1e-6;
// A half arc is halved into equal pieces at most this many times.
constexpr int MAX_ARC_LEVELS = 6;

double distanceBetween(ParkPoint a, ParkPoint b) {
  const double dx = b.X - a.X;
  const double dz = b.Z - a.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// The position the step takes from the position along the unit direction.
ParkPoint stepFrom(ParkPoint at, ParkPoint direction, double step) {
  return {at.X + direction.X * step, at.Z + direction.Z * step};
}

// The unit direction from one position to another at least MIN_SEGMENT_LENGTH from it.
ParkPoint unitToward(ParkPoint from, ParkPoint to) {
  const double length = distanceBetween(from, to);
  return {(to.X - from.X) / length, (to.Z - from.Z) / length};
}

// The distance from the point to the line through two positions at least MIN_SEGMENT_LENGTH
// apart.
double lineDistance(ParkPoint point, ParkPoint from, ParkPoint to) {
  const double cross = (to.X - from.X) * (point.Z - from.Z) - (to.Z - from.Z) * (point.X - from.X);
  return std::abs(cross) / distanceBetween(from, to);
}

// The point of the circle halfway along its arc between two of its points less than a half turn
// apart: their chord's middle, pushed out to the radius.
ParkPoint arcMiddle(ParkPoint center, double radius, ParkPoint a, ParkPoint b) {
  const ParkPoint out{(a.X + b.X) / 2.0 - center.X, (a.Z + b.Z) / 2.0 - center.Z};
  const double length = std::sqrt(out.X * out.X + out.Z * out.Z);
  return {center.X + out.X * radius / length, center.Z + out.Z * radius / length};
}

// How far an arc of the circle bulges from its chord of the length.
double sagittaOf(double radius, double chord) {
  const double half = chord / 2.0;
  return radius - std::sqrt(std::max(radius * radius - half * half, 0.0));
}

// Appends points along the circle's arc from one of its points to another, both included: the arc
// halved into equal pieces until each lies within ARC_TOLERANCE of it, at most MAX_ARC_LEVELS
// times. The piece count is found first, so the points are written in place.
void appendArc(std::vector<ParkPoint> &out, ParkPoint center, double radius, ParkPoint from,
               ParkPoint to) {
  size_t pieces = 1;
  double chord = distanceBetween(from, to);
  for (int level = 0; level < MAX_ARC_LEVELS && sagittaOf(radius, chord) > ARC_TOLERANCE;
       ++level) {
    // The chord of half the arc is the square root of twice the radius times the sagitta.
    chord = std::sqrt(2.0 * radius * sagittaOf(radius, chord));
    pieces *= 2;
  }
  const size_t first = out.size();
  out.resize(first + pieces + 1);
  out[first] = from;
  out[first + pieces] = to;
  for (size_t step = pieces; step > 1; step /= 2) {
    for (size_t i = first; i < first + pieces; i += step) {
      out[i + step / 2] = arcMiddle(center, radius, out[i], out[i + step]);
    }
  }
}

// Adds the point to the end of the ground line at its distance along it, unless it lies within
// SAME_POINT of the line's last point.
void extendLine(std::vector<CarrierPoint> &line, ParkPoint point) {
  if (line.empty()) {
    line.push_back(CarrierPoint{point.X, point.Z, 0.0});
    return;
  }
  const double step = distanceBetween(ParkPoint{line.back().X, line.back().Z}, point);
  if (step >= SAME_POINT) {
    const double distance = line.back().Distance + step;
    line.push_back(CarrierPoint{point.X, point.Z, distance});
  }
}

// Ends the ground line at the point, after leaving out each point but the first that lies within
// SAME_POINT of it.
void endLine(std::vector<CarrierPoint> &line, ParkPoint point) {
  while (line.size() > 1 &&
         distanceBetween(ParkPoint{line.back().X, line.back().Z}, point) < SAME_POINT) {
    line.pop_back();
  }
  const double distance =
      line.back().Distance + distanceBetween(ParkPoint{line.back().X, line.back().Z}, point);
  line.push_back(CarrierPoint{point.X, point.Z, distance});
}

// The path point the key holds, or null.
const PathPointIntent *findPoint(const PathLayout &layout, EntityKey key) {
  const auto found = std::ranges::lower_bound(layout.Points, key, {}, &LayoutPoint::Key);
  return found != layout.Points.end() && found->Key == key ? &found->Intent : nullptr;
}

// The straight ground line between the segment's points: empty unless they are two different path
// points and its width is finite and above zero.
std::vector<CarrierPoint> straightLine(const PathLayout &layout,
                                       const PathSegmentIntent &segment) {
  const PathPointIntent *from = findPoint(layout, segment.From);
  const PathPointIntent *to = findPoint(layout, segment.To);
  if (from == nullptr || to == nullptr || segment.From == segment.To ||
      !std::isfinite(segment.Width) || !(segment.Width > 0.0)) {
    return {};
  }
  return segmentLine(from->At, to->At);
}

// A corner, and its two segments as indices into the layout's segments. For each segment, the
// points of its ground line from the arc's middle to where the arc touches it lie in a shared arc
// buffer, from HalfBegin up to HalfEnd.
struct Rounding {
  ParkCorner Corner;
  std::array<size_t, 2> Segments{};
  std::array<size_t, 2> HalfBegin{};
  std::array<size_t, 2> HalfEnd{};
};

// The corner the point makes with its two segments, the lower-keyed first, each with a straight
// ground line, with its halves appended to the arc buffer. None where their far ends are one point
// or too close, or the segments run straight on or fold straight back.
std::optional<Rounding> roundingOf(const PathLayout &layout, EntityKey key,
                                   const PathPointIntent &point, std::array<size_t, 2> segments,
                                   std::vector<ParkPoint> &arcs) {
  std::array<EntityKey, 2> farKeys{};
  std::array<ParkPoint, 2> far{};
  for (size_t k = 0; k < 2; ++k) {
    const PathSegmentIntent &segment = layout.Segments[segments[k]].Intent;
    farKeys[k] = segment.From == key ? segment.To : segment.From;
    far[k] = findPoint(layout, farKeys[k])->At;
  }
  const ParkPoint at = point.At;
  if (farKeys[0] == farKeys[1] || distanceBetween(far[0], far[1]) < MIN_SEGMENT_LENGTH ||
      lineDistance(at, far[0], far[1]) <= MIN_SEGMENT_LENGTH) {
    return std::nullopt;
  }
  const std::array<ParkPoint, 2> along{unitToward(at, far[0]), unitToward(at, far[1])};
  const double dot = along[0].X * along[1].X + along[0].Z * along[1].Z;
  const double cross = std::abs(along[0].X * along[1].Z - along[0].Z * along[1].X);
  // The arc's tangent length is the radius times cross / (1 - dot), so the largest radius that
  // fits touches the shorter segment halfway along it.
  const double shorter = std::min(distanceBetween(at, far[0]), distanceBetween(at, far[1]));
  const double maxRadius = 0.5 * shorter * (1.0 - dot) / cross;
  const double asked = std::isfinite(point.Radius) && point.Radius >= 0.0 ? point.Radius : 0.0;
  const double radius = std::min(asked, maxRadius);
  const double tangent = radius * cross / (1.0 - dot);
  const ParkPoint sum{along[0].X + along[1].X, along[0].Z + along[1].Z};
  const double sumLength = std::sqrt(sum.X * sum.X + sum.Z * sum.Z);
  const ParkPoint toward{sum.X / sumLength, sum.Z / sumLength};
  // The arc's center lies along toward, as far from the point as the hypotenuse of the tangent
  // length and the radius. The arc's middle lies the radius nearer the point.
  const double centerDepth = std::sqrt(tangent * tangent + radius * radius);
  const ParkPoint middle = stepFrom(at, toward, centerDepth - radius);
  Rounding rounding{ParkCorner{key, at, along, toward, radius, maxRadius, middle}, segments, {}, {}};
  for (size_t k = 0; k < 2; ++k) {
    rounding.HalfBegin[k] = arcs.size();
    if (radius > 0.0) {
      appendArc(arcs, stepFrom(at, toward, centerDepth), radius, middle,
                stepFrom(at, along[k], tangent));
    } else {
      arcs.push_back(at);
    }
    rounding.HalfEnd[k] = arcs.size();
  }
  return rounding;
}

// Each straight ground line, by the segment's index in the layout.
std::vector<std::vector<CarrierPoint>> straightLines(const PathLayout &layout) {
  std::vector<std::vector<CarrierPoint>> lines;
  for (const LayoutSegment &segment : layout.Segments) {
    lines.push_back(straightLine(layout, segment.Intent));
  }
  return lines;
}

// Each corner the layout's points make, in ascending key order of its point, with their halves in
// the arc buffer.
std::vector<Rounding> roundingsOf(const PathLayout &layout,
                                  const std::vector<std::vector<CarrierPoint>> &straight,
                                  std::vector<ParkPoint> &arcs) {
  // Each segment end, by its point's key, and then by the segment's index.
  std::vector<std::pair<EntityKey, size_t>> ends;
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    ends.emplace_back(layout.Segments[i].Intent.From, i);
    ends.emplace_back(layout.Segments[i].Intent.To, i);
  }
  std::ranges::sort(ends);
  std::vector<Rounding> roundings;
  for (size_t first = 0; first < ends.size();) {
    size_t last = first;
    while (last < ends.size() && ends[last].first == ends[first].first) {
      ++last;
    }
    const EntityKey key = ends[first].first;
    const PathPointIntent *point = findPoint(layout, key);
    if (last - first == 2 && point != nullptr && !straight[ends[first].second].empty() &&
        !straight[ends[first + 1].second].empty()) {
      if (std::optional<Rounding> rounding = roundingOf(
              layout, key, *point, {ends[first].second, ends[first + 1].second}, arcs)) {
        roundings.push_back(*rounding);
      }
    }
    first = last;
  }
  return roundings;
}

} // namespace

PathLayout layoutOf(const World &world) {
  PathLayout layout;
  for (const EntityKey key : world.keys()) {
    const entt::entity entity = world.findEntity(key);
    if (const auto *point = world.Registry.try_get<PathPointIntent>(entity)) {
      layout.Points.push_back(LayoutPoint{key, *point});
    }
    if (const auto *segment = world.Registry.try_get<PathSegmentIntent>(entity)) {
      layout.Segments.push_back(LayoutSegment{key, *segment});
    }
  }
  return layout;
}

std::vector<ParkSegment> segmentsOf(const PathLayout &layout) {
  std::vector<std::vector<CarrierPoint>> lines = straightLines(layout);
  std::vector<ParkPoint> arcs;
  const std::vector<Rounding> roundings = roundingsOf(layout, lines, arcs);
  // The rounded corner at the point, or null.
  const auto roundingAt = [&roundings](EntityKey point) -> const Rounding * {
    const auto found = std::ranges::lower_bound(
        roundings, point, {}, [](const Rounding &rounding) { return rounding.Corner.Point; });
    return found != roundings.end() && found->Corner.Point == point ? &*found : nullptr;
  };
  std::vector<ParkSegment> segments;
  for (size_t i = 0; i < layout.Segments.size(); ++i) {
    const auto &[key, intent] = layout.Segments[i];
    std::vector<CarrierPoint> line = std::move(lines[i]);
    const Rounding *start = line.empty() ? nullptr : roundingAt(intent.From);
    const Rounding *end = line.empty() ? nullptr : roundingAt(intent.To);
    if (start != nullptr || end != nullptr) {
      const ParkPoint from{line.front().X, line.front().Z};
      const ParkPoint to{line.back().X, line.back().Z};
      line.clear();
      if (start != nullptr) {
        // From the arc's middle to where it touches this segment.
        const size_t k = start->Segments[0] == i ? 0 : 1;
        for (size_t j = start->HalfBegin[k]; j < start->HalfEnd[k]; ++j) {
          extendLine(line, arcs[j]);
        }
      } else {
        extendLine(line, from);
      }
      if (end != nullptr) {
        // From where the arc touches this segment back to its middle.
        const size_t k = end->Segments[0] == i ? 0 : 1;
        for (size_t j = end->HalfEnd[k] - 1; j > end->HalfBegin[k]; --j) {
          extendLine(line, arcs[j]);
        }
        endLine(line, arcs[end->HalfBegin[k]]);
      } else {
        endLine(line, to);
      }
    }
    segments.push_back(
        ParkSegment{key, intent.Kind, intent.From, intent.To, intent.Width, std::move(line)});
  }
  return segments;
}

std::vector<ParkCorner> cornersOf(const PathLayout &layout) {
  std::vector<ParkPoint> arcs;
  std::vector<ParkCorner> corners;
  for (const Rounding &rounding : roundingsOf(layout, straightLines(layout), arcs)) {
    corners.push_back(rounding.Corner);
  }
  return corners;
}

} // namespace tpj
```

A segment whose two ends are both rounded has its first point at one arc's middle and its last at the other's. Those two always lie apart, because each arc's middle lies nearer its own point than where that arc touches the segment, and each arc touches no farther than halfway along it.

Step 3: In src/sim/CMakeLists.txt, add `park/layout.cpp` after `park/intent.cpp`.

Step 4: In intent.cpp, include `"sim/park/internal/layout.h"`. Replace parkSegments' body with `return segmentsOf(layoutOf(world));` and the parkCorners stub's body with `return cornersOf(layoutOf(world));`, naming the parameter again. Delete findPoint and lineOf, which nothing then uses, and every include that nothing then uses.

Step 5: In geometry.cpp, replace the cornerRadiusFor stub. A corner's arc's middle lies radius × (1 / sine - 1) from its point, where sine is the sine of half the angle between its two segments.

```cpp
double cornerRadiusFor(const ParkCorner &corner, double depth) {
  if (!(depth > 0.0)) {
    return 0.0;
  }
  const double dot = corner.Along[0].X * corner.Along[1].X + corner.Along[0].Z * corner.Along[1].Z;
  // The sine of half the angle between the two segments, below 1 because they don't run straight
  // on.
  const double sine = std::sqrt((1.0 - dot) / 2.0);
  return depth * sine / (1.0 - sine);
}
```

Step 6: Check that bends now draw rounded.

Run: `cmake.exe --build --preset windows-debug --target tpj_app && build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/winding-path.park --capture build/windows-debug/corners.bmp; echo $?`
Expected: the build finishes with no warnings, then `0`. Open build/windows-debug/corners.bmp: every bend of the winding path is rounded, where it was a point before. This is criterion 5.

### Task 6: Check every changed ground line, and the two new commands

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Include `"sim/park/internal/layout.h"`.

Step 2: In EditPlan, add two fields after MovedTo, and add a sentence to its comment, after "If Moved is not NULL_KEY, that existing point moves to MovedTo.": "If Rounded is not NULL_KEY, that existing point's radius becomes RoundedTo."

```cpp
  EntityKey Rounded = NULL_KEY;
  double RoundedTo = 0.0;
```

Step 3: A world segment's ground line no longer ends at its points when a corner is rounded, so meetings read the points' positions. Give meetWorldSegment and addMeetings the world as their first parameter. In meetWorldSegment, replace the two lines that read `segment.Line.front()` and `segment.Line.back()` with:

```cpp
  // The segment has a ground line, so both of its ends are path points.
  const ParkPoint b0 = findIntent<PathPointIntent>(world, segment.From)->At;
  const ParkPoint b1 = findIntent<PathPointIntent>(world, segment.To)->At;
```

Pass `world` to meetWorldSegment in addMeetings, and to addMeetings in planPath, planMove, and planDelete. Change addMeetings' comment to say it reads the world's points' positions.

Step 4: Delete runFits and partsFit. Change finishPlan to sort only:

```cpp
// The plan with its meetings sorted.
EditPlan finishPlan(EditPlan plan) {
  for (Split &split : plan.Splits) {
    std::ranges::sort(split.At, {}, &PlacedPoint::Along);
  }
  for (PlanLine &line : plan.Lines) {
    std::ranges::sort(line.Through, {}, &PlacedPoint::Along);
  }
  std::ranges::sort(plan.Splits, {}, &Split::Segment);
  return plan;
}
```

Change each call from `finishPlan(world, std::move(plan), <kept or segments>)` to `finishPlan(std::move(plan))`. Change the comments of planPath, planMove, planInsert, and planDelete from "The plan of a <command> the world accepts, or none when it refuses it." to "The plan of a <command> that names what it acts on, or none.", and planInsert's second sentence stays.

Step 5: After planDelete, add the plans of the other three path commands:

```cpp
// The plan of a DeleteSegment that names a segment: the segment, and each of its points no other
// segment uses.
std::optional<EditPlan> planDeleteSegment(const World &world, const DeleteSegment &command) {
  if (findIntent<PathSegmentIntent>(world, command.Segment) == nullptr) {
    return std::nullopt;
  }
  const std::vector<ParkSegment> segments = parkSegments(world);
  const ParkSegment &segment = *findSegment(segments, command.Segment);
  EditPlan plan;
  plan.Destroyed.push_back(segment.Key);
  for (const EntityKey point : {segment.From, segment.To}) {
    const bool used = std::ranges::any_of(segments, [&](const ParkSegment &other) {
      return other.Key != segment.Key && endsAt(other, point);
    });
    if (!used && findIntent<PathPointIntent>(world, point) != nullptr &&
        std::ranges::find(plan.Destroyed, point) == plan.Destroyed.end()) {
      plan.Destroyed.push_back(point);
    }
  }
  return plan;
}

// The plan of a SetCornerRadius that names a path point with a radius finite and at least 0.
std::optional<EditPlan> planRadius(const World &world, const SetCornerRadius &command) {
  if (findIntent<PathPointIntent>(world, command.Point) == nullptr ||
      !std::isfinite(command.Radius) || command.Radius < 0.0) {
    return std::nullopt;
  }
  EditPlan plan;
  plan.Rounded = command.Point;
  plan.RoundedTo = command.Radius;
  return plan;
}

// The plan of a SetSegmentWidth that names a segment with a ground line and a width finite and
// above zero: the segment reshaped between its own points, at the width. It meets nothing it
// didn't meet before.
std::optional<EditPlan> planWidth(const World &world, const SetSegmentWidth &command) {
  const std::vector<ParkSegment> segments = parkSegments(world);
  const ParkSegment *segment = findSegment(segments, command.Segment);
  if (segment == nullptr || segment->Line.empty() || !std::isfinite(command.Width) ||
      !(command.Width > 0.0)) {
    return std::nullopt;
  }
  EditPlan plan;
  // The segment has a ground line, so both of its ends are path points.
  const size_t from = pointEntry(plan, segment->From, *positionOf(world, segment->From));
  const size_t to = pointEntry(plan, segment->To, *positionOf(world, segment->To));
  plan.Lines.push_back(PlanLine{segment->Key, segment->Kind, command.Width, from, to, {}});
  return plan;
}
```

The DeleteSegment plan destroys what applyCommand destroys today. A point that both ends name is destroyed once.

Step 6: Replace applyPlan with a template that writes through a store, and add the two stores before it:

```cpp
// Where applyPlan makes a plan's changes: a world.
class WorldStore {
public:
  explicit WorldStore(World &world) : Target(&world) {}
  void destroy(EntityKey key) { Target->destroyEntity(key); }
  EntityKey addPoint(const PathPointIntent &point) {
    const EntityKey key = Target->createEntity();
    Target->Registry.emplace<PathPointIntent>(Target->findEntity(key), point);
    return key;
  }
  EntityKey addSegment(const PathSegmentIntent &segment) {
    const EntityKey key = Target->createEntity();
    Target->Registry.emplace<PathSegmentIntent>(Target->findEntity(key), segment);
    return key;
  }
  PathPointIntent &point(EntityKey key) {
    return Target->Registry.get<PathPointIntent>(Target->findEntity(key));
  }
  PathSegmentIntent &segment(EntityKey key) {
    return Target->Registry.get<PathSegmentIntent>(Target->findEntity(key));
  }

private:
  World *Target;
};

// Where applyPlan makes a plan's changes: a copy of a world's layout. New keys come from the
// world's counter in the order the world would give them.
class LayoutStore {
public:
  LayoutStore(PathLayout &layout, uint64_t nextKey) : Target(&layout), NextKey(nextKey) {}
  void destroy(EntityKey key) {
    std::erase_if(Target->Points, [key](const LayoutPoint &point) { return point.Key == key; });
    std::erase_if(Target->Segments,
                  [key](const LayoutSegment &segment) { return segment.Key == key; });
  }
  EntityKey addPoint(const PathPointIntent &point) {
    const EntityKey key{NextKey++};
    Target->Points.push_back(LayoutPoint{key, point});
    return key;
  }
  EntityKey addSegment(const PathSegmentIntent &segment) {
    const EntityKey key{NextKey++};
    Target->Segments.push_back(LayoutSegment{key, segment});
    return key;
  }
  PathPointIntent &point(EntityKey key) {
    return std::ranges::lower_bound(Target->Points, key, {}, &LayoutPoint::Key)->Intent;
  }
  PathSegmentIntent &segment(EntityKey key) {
    return std::ranges::lower_bound(Target->Segments, key, {}, &LayoutSegment::Key)->Intent;
  }

private:
  PathLayout *Target;
  uint64_t NextKey;
};

// Makes what the plan describes in the store. Destroyed keys go first. Each plan point's key is
// its existing point's, or a new point's, in the plan's order. A moved point moves, and a rounded
// point takes its radius. Each split segment ends at its first point, and new parts run on through
// the rest to its old end. Each line runs through its points: a reshaped line keeps its key for
// its first part, and every other part takes a new key.
template <typename Store> void applyPlan(Store &store, const EditPlan &plan) {
  for (const EntityKey key : plan.Destroyed) {
    store.destroy(key);
  }
  std::vector<EntityKey> keys;
  for (const PlanPoint &point : plan.Points) {
    keys.push_back(point.Existing != NULL_KEY ? point.Existing
                                              : store.addPoint(PathPointIntent{point.At}));
  }
  if (plan.Moved != NULL_KEY) {
    store.point(plan.Moved).At = plan.MovedTo;
  }
  if (plan.Rounded != NULL_KEY) {
    store.point(plan.Rounded).Radius = plan.RoundedTo;
  }
  for (const Split &split : plan.Splits) {
    const PathSegmentIntent was = store.segment(split.Segment);
    store.segment(split.Segment).To = keys[split.At.front().Point];
    for (size_t k = 0; k < split.At.size(); ++k) {
      const EntityKey to = k + 1 < split.At.size() ? keys[split.At[k + 1].Point] : was.To;
      store.addSegment(PathSegmentIntent{was.Kind, keys[split.At[k].Point], to, was.Width});
    }
  }
  for (const PlanLine &line : plan.Lines) {
    EntityKey from = keys[line.From];
    bool first = true;
    const auto addPart = [&](EntityKey to) {
      const PathSegmentIntent part{line.Kind, from, to, line.Width};
      if (first && line.Key != NULL_KEY) {
        store.segment(line.Key) = part;
      } else {
        store.addSegment(part);
      }
      first = false;
      from = to;
    };
    for (const PlacedPoint &placed : line.Through) {
      addPart(keys[placed.Point]);
    }
    addPart(keys[line.To]);
  }
}
```

The store's segment reference is taken again after each addSegment, because adding to a layout can move its segments in memory.

The layout's keys stay in ascending order, because every new key is above every key the world has given.

Step 7: After applyPlan, add the check of every changed ground line:

```cpp
// Each segment the plan adds, or whose ground line or width it changes, as it would lie after the
// plan, in ascending key order.
std::vector<ParkSegment> editedBy(const World &world, const EditPlan &plan) {
  PathLayout layout = layoutOf(world);
  const std::vector<ParkSegment> before = segmentsOf(layout);
  LayoutStore store(layout, world.nextKey());
  applyPlan(store, plan);
  std::vector<ParkSegment> edited;
  for (ParkSegment &segment : segmentsOf(layout)) {
    const ParkSegment *was = findSegment(before, segment.Key);
    if (was == nullptr || was->Line != segment.Line || was->Width != segment.Width) {
      edited.push_back(std::move(segment));
    }
  }
  return edited;
}

// The plan when the world accepts it: every segment it adds or changes has a ground line and fits
// the solids.
std::optional<EditPlan> acceptedPlan(const World &world, std::optional<EditPlan> plan) {
  if (!plan) {
    return std::nullopt;
  }
  const std::vector<Solid> solids = solidsOf(world);
  const bool fitting =
      std::ranges::all_of(editedBy(world, *plan), [&solids](const ParkSegment &segment) {
        const std::optional<Line> line = lineOf(segment);
        return line && fits(*line, solids);
      });
  return fitting ? std::move(plan) : std::nullopt;
}

// Each path command's plan, and none for box commands, which have no plan.
std::optional<EditPlan> planOf(const World &world, const AddPath &command) {
  return planPath(world, command);
}
std::optional<EditPlan> planOf(const World & /*world*/, const AddBox & /*command*/) {
  return std::nullopt;
}
std::optional<EditPlan> planOf(const World & /*world*/, const MoveBox & /*command*/) {
  return std::nullopt;
}
std::optional<EditPlan> planOf(const World &world, const DeleteSegment &command) {
  return planDeleteSegment(world, command);
}
std::optional<EditPlan> planOf(const World & /*world*/, const DeleteBox & /*command*/) {
  return std::nullopt;
}
std::optional<EditPlan> planOf(const World &world, const MovePoint &command) {
  return planMove(world, command);
}
std::optional<EditPlan> planOf(const World &world, const InsertPoint &command) {
  return planInsert(world, command);
}
std::optional<EditPlan> planOf(const World &world, const DeletePoint &command) {
  return planDelete(world, command);
}
std::optional<EditPlan> planOf(const World &world, const SetCornerRadius &command) {
  return planRadius(world, command);
}
std::optional<EditPlan> planOf(const World &world, const SetSegmentWidth &command) {
  return planWidth(world, command);
}
```

Move crossingsOf after these, unchanged.

Step 8: Change the public functions:

- The three pathCrossings return `crossingsOf(acceptedPlan(world, planOf(world, <argument>)))`.
- isAccepted for AddPath, DeleteSegment, MovePoint, InsertPoint, DeletePoint, SetCornerRadius, and SetSegmentWidth each return `acceptedPlan(world, planOf(world, command)).has_value();`.
- applyCommand for the same seven each become:

```cpp
  if (const std::optional<EditPlan> plan = acceptedPlan(world, planOf(world, command))) {
    WorldStore store(world);
    applyPlan(store, *plan);
  }
```

DeleteSegment's old body, which destroyed the segment and its unused points by hand, goes.

- editedSegments replaces its stub:

```cpp
std::vector<ParkSegment> editedSegments(const World &world, const ParkEdit &edit) {
  const std::optional<EditPlan> plan =
      std::visit([&world](const auto &command) { return planOf(world, command); }, edit);
  if (!plan) {
    return {};
  }
  std::vector<ParkSegment> edited = editedBy(world, *plan);
  // A segment the edit adds or reshapes with no ground line makes the edit name nothing it can
  // show.
  if (std::ranges::any_of(edited, [](const ParkSegment &segment) { return segment.Line.empty(); })) {
    return {};
  }
  return edited;
}
```

Step 9: Build the simulation and its tests, and run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: the build finishes with no warnings. Every test case passes. The two new chosen edits now find their corner and segment, and the world accepts both.

If a chosen edit of point-editing or crossing-splits is now refused, a corner it changes may round into a box in warm.park. Stop and report it as a deviation, with the edit's name.

### Task 7: Draw the new commands in the random edits

Files:
- Modify: `src/scenarios/park_edits.cpp:24-26,93-95`

Step 1: Replace the weights and their comment:

```cpp
// Weights of AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint,
// DeletePoint, SetCornerRadius, and SetSegmentWidth.
constexpr std::array<uint64_t, 10> EDIT_WEIGHTS{3, 4, 3, 1, 1, 2, 1, 1, 1, 1};
```

Step 2: Replace the last branch of queueEdit, `} else { commands.push(DeletePoint{drawGivenKey(world, index)}); }`, with:

```cpp
  } else if (kind == 7) {
    commands.push(DeletePoint{drawGivenKey(world, index)});
  } else if (kind == 8) {
    // Radii up to 12 m, so some don't fit and are made smaller.
    const EntityKey point = drawGivenKey(world, index);
    commands.push(SetCornerRadius{point, drawEdit(world, index) * 12.0});
  } else {
    // Widths across the Edit path tool's range, so some meet a box and are refused.
    const EntityKey segment = drawGivenKey(world, index);
    commands.push(SetSegmentWidth{segment, 1.5 + drawEdit(world, index) * 14.5});
  }
```

Each key is drawn before its value, in its own statement, so the draws are made in a fixed order.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null | grep -c "scenario park-edits"`
Expected: the build finishes with no warnings, then `1001`. This is criterion 6.

### Task 8: Draw each edit's reshaped segments in its ghost

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Replace ghostColor with a version that takes whether the world accepts the edit, so the ghost asks once:

```cpp
// An edit's ghost color for a kind's color: translucent when accepted, the invalid tint when not.
Rgba ghostColor(bool accepted, Rgba color) {
  return accepted ? Rgba{color.R, color.G, color.B, GHOST_ALPHA} : INVALID_TINT;
}
```

Step 2: Replace appendGhostPath, appendGhostMove, and appendGhostDeletePoint with these three:

```cpp
// Adds an added path's straight segments between its stops, for a path the world refuses so that
// editedSegments gives none. Nothing when a stop has no position.
void appendStraightPath(ParkMesh &mesh, const World &world, const AddPath &path, Rgba color) {
  const std::optional<std::vector<ParkPoint>> positions = stopPositions(world, path);
  if (!positions) {
    return;
  }
  for (size_t i = 0; i + 1 < positions->size(); ++i) {
    appendGhostSegment(mesh, path.Kind, segmentLine((*positions)[i], (*positions)[i + 1]),
                       pathWidth(path.Kind), color);
  }
}

// Adds, in the delete tint, the box or segment a deletion removes, or every segment at a point
// DeletePoint deletes.
void appendDeleted(ParkMesh &mesh, const World &world, const ParkEdit &edit) {
  if (const auto *del = std::get_if<DeletePoint>(&edit)) {
    for (const ParkSegment &segment : parkSegments(world)) {
      if (segment.From == del->Point || segment.To == del->Point) {
        appendEntity(mesh, world, segment.Key, DELETE_TINT);
      }
    }
  } else if (const auto *segment = std::get_if<DeleteSegment>(&edit)) {
    appendEntity(mesh, world, segment->Segment, DELETE_TINT);
  } else if (const auto *box = std::get_if<DeleteBox>(&edit)) {
    appendEntity(mesh, world, box->Box, DELETE_TINT);
  }
}

// Adds a join mark at each stop where an added path joins a point, a segment, or an earlier stop,
// at a moved point's target when it is a point or segment, at an inserted point's place, and at
// each place pathCrossings gives.
void appendJoinMarks(ParkMesh &mesh, const World &world, const ParkEdit &edit) {
  if (const auto *path = std::get_if<AddPath>(&edit)) {
    if (const std::optional<std::vector<ParkPoint>> positions = stopPositions(world, *path)) {
      for (size_t i = 0; i < positions->size(); ++i) {
        if (!std::holds_alternative<NewPoint>(path->Stops[i])) {
          appendJoinMark(mesh, (*positions)[i]);
        }
      }
    }
    for (const ParkPoint &at : pathCrossings(world, *path)) {
      appendJoinMark(mesh, at);
    }
  } else if (const auto *move = std::get_if<MovePoint>(&edit)) {
    if (!std::holds_alternative<NewPoint>(move->To)) {
      const PathStop target = std::visit([](const auto &stop) { return PathStop{stop}; }, move->To);
      if (const std::optional<ParkPoint> at = stopPosition(world, target)) {
        appendJoinMark(mesh, *at);
      }
    }
    for (const ParkPoint &at : pathCrossings(world, *move)) {
      appendJoinMark(mesh, at);
    }
  } else if (const auto *insert = std::get_if<InsertPoint>(&edit)) {
    if (const std::optional<ParkPoint> at =
            stopPosition(world, OnSegment{insert->Segment, insert->Along})) {
      appendJoinMark(mesh, *at);
    }
  } else if (const auto *del = std::get_if<DeletePoint>(&edit)) {
    for (const ParkPoint &at : pathCrossings(world, *del)) {
      appendJoinMark(mesh, at);
    }
  }
}
```

Step 3: Replace the body of buildGhostMesh(world, edit, candidate) up to `if (candidate) {` with:

```cpp
  ParkMesh mesh;
  const bool accepted = isAccepted(world, edit);
  if (const auto *add = std::get_if<AddBox>(&edit)) {
    appendBox(mesh, add->At, boxSize(add->Kind), boxHeight(add->Kind),
              ghostColor(accepted, boxColor(add->Kind)));
  } else if (const auto *move = std::get_if<MoveBox>(&edit)) {
    for (const ParkBox &box : parkBoxes(world)) {
      if (box.Key == move->Box) {
        appendBox(mesh, move->At, boxSize(box.Kind), boxHeight(box.Kind),
                  ghostColor(accepted, boxColor(box.Kind)));
      }
    }
  }
  if (accepted) {
    appendDeleted(mesh, world, edit);
  }
  const std::vector<ParkSegment> edited = editedSegments(world, edit);
  if (const auto *path = std::get_if<AddPath>(&edit); path != nullptr && edited.empty()) {
    appendStraightPath(mesh, world, *path, ghostColor(accepted, pathColor(path->Kind)));
  }
  for (const ParkSegment &segment : edited) {
    appendGhostSegment(mesh, segment.Kind, segment.Line, segment.Width,
                       ghostColor(accepted, pathColor(segment.Kind)));
  }
  appendJoinMarks(mesh, world, edit);
```

The deleted segments go under the reshaped ones, so a merged segment shows over the two it replaces.

Step 4: Update the comments in park_mesh.h above buildGhostMesh to match: "The ghost of an edit on a world: the box it adds or moves, and every segment it adds or reshapes, translucent in its kind's color when accepted and INVALID_TINT when not; what an accepted deletion removes, in DELETE_TINT; and its join marks; followed by the candidate's walkways and starved marks when a candidate is given."

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with no warnings and no errors.

### Task 9: Radius and side handles in Edit path

Files:
- Modify: `src/tools/tools.cpp`

Step 1: In the anonymous namespace, after segmentDistance, add:

```cpp
// The distance from the point to the ground line, which has at least two points.
double lineDistance(ParkPoint point, const std::vector<CarrierPoint> &line) {
  double nearest = segmentDistance(point, line[0], line[1]);
  for (size_t i = 1; i + 1 < line.size(); ++i) {
    nearest = std::min(nearest, segmentDistance(point, line[i], line[i + 1]));
  }
  return nearest;
}

// How far the point lies from the corner's point along its Toward.
double depthAlong(const ParkCorner &corner, ParkPoint point) {
  return (point.X - corner.At.X) * corner.Toward.X + (point.Z - corner.At.Z) * corner.Toward.Z;
}

// The corner the point makes, or none.
std::optional<ParkCorner> cornerOf(const World &world, EntityKey point) {
  for (const ParkCorner &corner : parkCorners(world)) {
    if (corner.Point == point) {
      return corner;
    }
  }
  return std::nullopt;
}

// The segment the key holds, or none.
std::optional<ParkSegment> segmentOf(const World &world, EntityKey key) {
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Key == key) {
      return segment;
    }
  }
  return std::nullopt;
}
```

Step 2: Before editPathEdit, add the edit a dragged handle makes:

```cpp
// The edit a dragged radius handle or side makes with the pointer where it is: the arc's middle
// moved as far into the corner as the pointer has moved, or the width grown by twice how far the
// pointer has moved away from the ground line. None when the corner or segment is gone.
std::optional<ParkEdit> handleEdit(const EditHandle &handle, double grabFrom, const World &world,
                                   ParkPoint pointer) {
  if (handle.Kind == HandleKind::Radius) {
    const std::optional<ParkCorner> corner = cornerOf(world, handle.Key);
    if (!corner) {
      return std::nullopt;
    }
    const double radius = cornerRadiusFor(*corner, depthAlong(*corner, pointer) + grabFrom);
    return SetCornerRadius{corner->Point, std::min(radius, corner->MaxRadius)};
  }
  const std::optional<ParkSegment> segment = segmentOf(world, handle.Key);
  if (!segment || segment->Line.size() < 2) {
    return std::nullopt;
  }
  const double width = std::clamp(2.0 * lineDistance(pointer, segment->Line) + grabFrom,
                                  MIN_PATH_WIDTH, MAX_PATH_WIDTH);
  return SetSegmentWidth{segment->Key, width};
}
```

Step 3: Replace editPathEdit and its comment:

```cpp
// Edit path's edit: the held handle's or point's once it has been dragged, the held insert, or,
// while not holding, the insert a click away from any handle, point, or side would make.
std::optional<ParkEdit> editPathEdit(const ToolState &tool, const World &world) {
  if (tool.Holding && tool.Grabbed) {
    if (!tool.Dragged || !tool.Pointer) {
      return std::nullopt;
    }
    return handleEdit(*tool.Grabbed, tool.GrabFrom, world, *tool.Pointer);
  }
  if (tool.Holding && tool.Held != NULL_KEY) {
    if (!tool.Dragged) {
      return std::nullopt;
    }
    return MovePoint{tool.Held, snapPoint(world, tool.Held, tool.PointTo)};
  }
  if (tool.Holding && tool.Inserting) {
    return *tool.Inserting;
  }
  if (tool.Holding || !tool.Pointer || radiusHandleAt(world, *tool.Pointer) ||
      pointAt(world, *tool.Pointer) || sideAt(world, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<InsertPoint> insert = insertAt(world, *tool.Pointer)) {
    return *insert;
  }
  return std::nullopt;
}
```

Step 4: Before `} // namespace` that closes the anonymous namespace, add Edit path's press:

```cpp
// Edit path's press takes, in order, the radius handle, the point, the segment's sides, or the
// insert under the pointer.
void pressEditPath(ToolState &tool, const World &world, ParkPoint pointer) {
  if (const std::optional<EntityKey> key = radiusHandleAt(world, pointer)) {
    if (const std::optional<ParkCorner> corner = cornerOf(world, *key)) {
      tool.Holding = true;
      tool.Grabbed = EditHandle{HandleKind::Radius, *key, radiusHandleOf(*corner)};
      tool.GrabFrom = distanceBetween(corner->At, corner->Middle) - depthAlong(*corner, pointer);
      return;
    }
  }
  const std::optional<EntityKey> point = pointAt(world, pointer);
  if (const std::optional<ParkPoint> at =
          point ? stopPosition(world, OnPoint{*point}) : std::nullopt) {
    tool.Holding = true;
    tool.Held = *point;
    tool.PointTo = *at;
    tool.GrabOffset = ParkPoint{at->X - pointer.X, at->Z - pointer.Z};
    return;
  }
  if (const std::optional<EntityKey> key = sideAt(world, pointer)) {
    if (const std::optional<ParkSegment> segment = segmentOf(world, *key)) {
      tool.Holding = true;
      tool.Grabbed = EditHandle{HandleKind::Side, *key, {}};
      tool.GrabFrom = segment->Width - 2.0 * lineDistance(pointer, segment->Line);
      return;
    }
  }
  if (const std::optional<InsertPoint> insert = insertAt(world, pointer)) {
    tool.Holding = true;
    tool.Inserting = *insert;
  }
}
```

In pressPointer, replace the whole EditPath branch with:

```cpp
  } else if (tool.Kind == ToolKind::EditPath && tool.Pointer) {
    pressEditPath(tool, world, *tool.Pointer);
  }
```

Step 5: In selectTool and in releasePointer, after `tool.Inserting.reset();`, add `tool.Grabbed.reset();`.

Step 6: In highlightedEntity, make Edit path mark a point only when no radius handle is under the pointer:

```cpp
  if (tool.Kind == ToolKind::EditPath && !tool.Holding && tool.Pointer &&
      !radiusHandleAt(world, *tool.Pointer)) {
    return pointAt(world, *tool.Pointer);
  }
```

Step 7: Replace the five stubs after snapPoint:

```cpp
ParkPoint radiusHandleOf(const ParkCorner &corner) {
  const double depth = std::max(distanceBetween(corner.At, corner.Middle), RADIUS_HANDLE_DEPTH);
  return {corner.At.X + corner.Toward.X * depth, corner.At.Z + corner.Toward.Z * depth};
}

std::optional<EntityKey> radiusHandleAt(const World &world, ParkPoint point) {
  std::optional<EntityKey> nearest;
  double nearestDistance = 0.0;
  for (const ParkCorner &corner : parkCorners(world)) {
    const double distance = distanceBetween(point, radiusHandleOf(corner));
    if (distance <= HANDLE_REACH && (!nearest || distance < nearestDistance)) {
      nearest = corner.Point;
      nearestDistance = distance;
    }
  }
  return nearest;
}

std::optional<EntityKey> sideAt(const World &world, ParkPoint point) {
  std::optional<EntityKey> nearest;
  double nearestOff = 0.0;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Line.size() < 2) {
      continue;
    }
    const double off = fabs(lineDistance(point, segment.Line) - 0.5 * segment.Width);
    if (off <= SIDE_REACH && (!nearest || off < nearestOff)) {
      nearest = segment.Key;
      nearestOff = off;
    }
  }
  return nearest;
}

std::vector<ParkPoint> radiusHandles(const ToolState &tool, const World &world) {
  std::vector<ParkPoint> handles;
  if (tool.Kind == ToolKind::EditPath && !tool.Holding) {
    for (const ParkCorner &corner : parkCorners(world)) {
      handles.push_back(radiusHandleOf(corner));
    }
  }
  return handles;
}

std::optional<EditHandle> hoveredHandle(const ToolState &tool, const World &world) {
  if (tool.Kind != ToolKind::EditPath || tool.Holding || !tool.Pointer) {
    return std::nullopt;
  }
  if (const std::optional<EntityKey> key = radiusHandleAt(world, *tool.Pointer)) {
    if (const std::optional<ParkCorner> corner = cornerOf(world, *key)) {
      return EditHandle{HandleKind::Radius, *key, radiusHandleOf(*corner)};
    }
  }
  if (pointAt(world, *tool.Pointer)) {
    return std::nullopt;
  }
  if (const std::optional<EntityKey> key = sideAt(world, *tool.Pointer)) {
    return EditHandle{HandleKind::Side, *key, {}};
  }
  return std::nullopt;
}
```

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: the build finishes with no warnings and no errors.

### Task 10: Draw radius handles and side marks

Files:
- Modify: `src/render/park_mesh.cpp`

Step 1: Replace appendRibbon with a band between two offsets across the line, and keep appendRibbon as the band across the whole width:

```cpp
// Adds a flat band along the line, from one offset across it to another, each measured to the
// right of the line's direction, the lift above the ground. The line has at least two points, each
// distinct from the next.
void appendBand(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double from, double to,
                float lift, Rgba color) {
  const auto first = static_cast<uint32_t>(mesh.Vertices.size());
  for (size_t i = 0; i < line.size(); ++i) {
    const ParkPoint tangent = tangentAt(line, i);
    const ParkPoint right{-tangent.Z, tangent.X};
    addVertex(mesh, line[i].X + right.X * from, lift, line[i].Z + right.Z * from, {}, 1.0f, color);
    addVertex(mesh, line[i].X + right.X * to, lift, line[i].Z + right.Z * to, {}, 1.0f, color);
  }
  for (uint32_t i = 0; i + 1 < line.size(); ++i) {
    const uint32_t left0 = first + 2 * i;
    const uint32_t right0 = left0 + 1;
    const uint32_t left1 = left0 + 2;
    const uint32_t right1 = left0 + 3;
    mesh.Indices.insert(mesh.Indices.end(), {left0, right0, left1, left1, right0, right1});
  }
}

// Adds the flat ribbon of the width along the line, the lift above the ground. The line has at
// least two points, each distinct from the next.
void appendRibbon(ParkMesh &mesh, const std::vector<CarrierPoint> &line, double width, float lift,
                  Rgba color) {
  appendBand(mesh, line, -0.5 * width, 0.5 * width, lift, color);
}
```

The ribbon's vertices are the same as before: -0.5 × width to the right is the old left vertex.

Step 2: Replace the two stubs:

```cpp
void appendRadiusHandle(ParkMesh &mesh, ParkPoint at, Rgba color) {
  appendDisc(mesh, at, RADIUS_HANDLE_SIZE, HANDLE_LIFT, color);
}

void appendSides(ParkMesh &mesh, const World &world, EntityKey segment, Rgba color) {
  for (const ParkSegment &candidate : parkSegments(world)) {
    if (candidate.Key != segment || candidate.Line.size() < 2) {
      continue;
    }
    const double half = 0.5 * candidate.Width;
    const double inset = std::min(SIDE_MARK_WIDTH, half);
    const float lift = pathLift(candidate.Kind) + 2.0f * GHOST_LIFT;
    appendBand(mesh, candidate.Line, -half, -half + inset, lift, color);
    appendBand(mesh, candidate.Line, half - inset, half, lift, color);
  }
}
```

appendDisc is in the anonymous namespace before appendRadiusHandle's place, so it is visible there.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with no warnings and no errors.

### Task 11: Show the handles in the app

Files:
- Modify: `src/app/scene/scene_sync.h`
- Modify: `src/app/scene/scene_uploads.cpp`
- Modify: `src/app/input/interaction.h`
- Modify: `src/app/input/interaction.cpp`
- Modify: `src/app/application.cpp:97-99`

Step 1: In scene_sync.h, add two fields to PreviewLook after Finish, and end its comment with ", and Edit path's radius handles and the handle under the pointer":

```cpp
  std::vector<ParkPoint> Handles;
  std::optional<EditHandle> Hovered;
```

Step 2: In interaction.h, after pathFinish, add:

```cpp
  // Where Edit path's radius handles lie, as radiusHandles gives them.
  [[nodiscard]] std::vector<ParkPoint> radiusHandles(const World &world) const;
  // The handle under the pointer, as hoveredHandle gives it.
  [[nodiscard]] std::optional<EditHandle> hoveredHandle(const World &world) const;
```

In interaction.cpp, define them as pathFinish is defined, returning `tpj::radiusHandles(Tool, world)` and `tpj::hoveredHandle(Tool, world)`. Add `#include <vector>` to interaction.h.

Step 3: In application.cpp, extend the PreviewLook construction with the two values, after `Interactions.pathFinish(Session.world())`:

```cpp
                           Interactions.pathFinish(Session.world()),
                           Interactions.radiusHandles(Session.world()),
                           Interactions.hoveredHandle(Session.world())};
```

Step 4: In scene_uploads.cpp's ghostMesh, after the highlight's `appendEntity` block and before the Inspector's, add:

```cpp
  for (const ParkPoint &at : look.Handles) {
    appendRadiusHandle(mesh, at, HANDLE_COLOR);
  }
  if (look.Hovered && look.Hovered->Kind == HandleKind::Radius) {
    appendRadiusHandle(mesh, look.Hovered->At, HIGHLIGHT_TINT);
  } else if (look.Hovered) {
    appendSides(mesh, world, look.Hovered->Key, HIGHLIGHT_TINT);
  }
```

Change ghostMesh's comment to say it adds Edit path's radius handles and the hovered handle after the highlight.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with no warnings and no errors.

### Task 12: Confirm the criteria

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug 2>&1 | tail -3 && scripts/tidy.sh`
Expected: the build finishes with no warnings, ctest reports `100% tests passed, 0 tests failed`, and tidy prints `tidy: clean.`. This is criterion 7.

Step 2: Run criterion 5's capture and criterion 6's check again on the final build, as Task 5 and Task 7 give them.

Step 3: Ask the user to play criteria 1 to 4 in `build/windows-debug/ThemeParkJones.exe`, and record what they report.

### Task 13: Review and commit

Step 1: Review and commit as implementing-features describes. Stage only this feature's paths, never parks/routes.park or parks/sketch.park. The commit's subject is `Park: Add rounded corners and segment widths`.
