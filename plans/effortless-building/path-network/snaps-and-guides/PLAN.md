# Implementation Plan: Snaps and Guides

## Goal

Make the path tools and point dragging snap to points, paving, doors, guide crossings, direction guides, and whole-meter lengths, with pixel reach, a wider release, a visible guide for every snap, a length readout, and Ctrl to turn it all off.

## Approach

A new tools component, snapping, finds the snap for a pointer position. It works through four groups of targets in priority order, then a length refinement, and returns the stop, its guides, and what it holds to. The tool finds the snap once each time the pointer moves or a press or release changes the tool, and keeps it in ToolState. Every other query reads the kept snap instead of snapping again. The app gives the tool the pointer's meters per pixel and the Ctrl state, and draws the guides and length labels in screen space on ImGui's background draw list, as the graph view draws.

## Placement

Decision 0027 places each behavior this feature adds:

- Where doors are and which kind each serves: sim/routes, networks.h and networks.cpp. Routes already derives doors for connectors. Publishing them as parkDoors keeps one owner of the rule.
- Finding a snap: its targets, priorities, guides, holds, and length refinement: tools, a new component in src/tools/internal/snapping.h and src/tools/snapping.cpp. It is private to tools. It is one concern, and moving it out keeps tools.cpp to the tool state machine.
- Keeping the snap in effect and acting on it: tools, tools.h and tools.cpp, which own ToolState and every query the app reads.
- The meters a pixel spans at the cursor: app/input, cursor.h and cursor.cpp, which already turn the cursor into a ground point.
- Ctrl as the suppress key: app/input, input_map.h and input_map.cpp, which map held keys, read by platform_input.cpp, which gathers them.
- Passing the pointer sample to the tool: app/input, interaction.h and interaction.cpp, and app, application.cpp. That is composition only: the frame loop already calls movePointer there.
- Drawing guides and length labels: app/ui, a new component in snap_guides.h and snap_guides.cpp, beside graph_view, which draws over the scene the same way. tooling_ui.cpp calls it. That is composition only.

## Tasks

### Task 1: Save the scenarios baseline

Before any change, with the windows-debug build of main at 804a27d or later in build/windows-debug.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null > build/snaps-base.txt; wc -l < build/snaps-base.txt`
Expected: `15336`

### Task 2: Update the specs

Files:
- Modify: `src/sim/routes/SPEC.md`, `src/tools/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

### Task 3: Publish doors from routes

Files:
- Modify: `src/sim/routes/networks.h`, `src/sim/routes/networks.cpp`

Step 1: In networks.h, after connectorKey, declare:

```cpp
// A door: its entity, the face it is on, the middle of that face, and the unit direction out of
// the face.
struct ParkDoor {
  EntityKey Entity = NULL_KEY;
  Face Side = Face::Front;
  ParkPoint At;
  ParkPoint Out;

  bool operator==(const ParkDoor &) const = default;
};

// The doors serving the kind: the entrances' first, then the boxes', each in key order of its
// entity. An entity with no footprint has no door.
std::vector<ParkDoor> parkDoors(const World &world, PathKind kind);
```

Include `<vector>`. The forward declaration of World already exists.

Step 2: In networks.cpp, delete the private struct Door. addDoor builds ParkDoor values. Out is the footprint's Forward for the front face and its negation for the back face. doorsServing returns `std::vector<ParkDoor>`. parkDoors calls it with parkEntrances(world) and parkBoxes(world). findConnections and deriveNetwork take `std::vector<ParkDoor>`. Where they use door.At as a GroundPoint, they convert it with `GroundPoint{door.At.X, door.At.Z}`. Nothing else changes.

Step 2 changes no behavior. Task 9 shows it with the scenarios comparison.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: builds with no warnings.

### Task 4: Declare the tools interface

Files:
- Modify: `src/tools/tools.h`
- Create: `src/tools/internal/snapping.h`

Step 1: In tools.h, delete SNAP_REACH, snapStop, and snapPoint, with their comments. Add these constants after FINISH_REACH:

```cpp
// A snap catches the pointer this near it, and holds it until it moves this far, in screen pixels.
inline constexpr double SNAP_CATCH_PIXELS = 10.0;
inline constexpr double SNAP_RELEASE_PIXELS = 14.0;
// A path that snaps to a door ends this far clear of the door's face, in meters.
inline constexpr double DOOR_CLEARANCE = 0.25;
```

Step 2: Add these types before ToolState:

```cpp
// What a snap guide shows: a point snapped to, a door's line to where the path ends, a segment
// snapped onto, a direction from an anchor, or whole meters from an anchor.
enum class GuideKind : uint8_t { Point, Door, Segment, Direction, Length };

// A guide to draw. Point: the point at To. Door: from the door at From to the target at To.
// Segment: the straight part of a segment from From to To. Direction: from its anchor at From
// through To, running on past it. Length: from its anchor at From to To, with a tick at each whole
// meter.
struct SnapGuide {
  GuideKind Kind = GuideKind::Point;
  ParkPoint From;
  ParkPoint To;

  bool operator==(const SnapGuide &) const = default;
};

// One thing a snap holds to. Key names a path point, a door's entity, a segment, or a rounded
// corner's point. Index names a drawn stop, a drawn segment, a door's face, a guide's place in the
// snap's list of guides, or a whole number of meters.
enum class HoldKind : uint8_t { Point, DrawnStop, Door, Segment, Bend, DrawnSegment, Guide, Length };

struct SnapHold {
  HoldKind Kind = HoldKind::Point;
  EntityKey Key = NULL_KEY;
  uint64_t Index = 0;

  bool operator==(const SnapHold &) const = default;
};

// The snap in effect: where the stop or point lands, the stop it makes, the guides it shows, and
// what it holds to. While snapping is off, it lands at the pointer as a new point, with no guides
// and no holds.
struct PointerSnap {
  ParkPoint At;
  PathStop Stop;
  std::vector<SnapGuide> Guides;
  std::vector<SnapHold> Holds;

  bool operator==(const PointerSnap &) const = default;
};

// Where the pointer meets the ground, if it does; how many meters of ground a screen pixel spans
// there; and whether the player is holding the key that turns snapping off.
struct PointerSample {
  std::optional<ParkPoint> Ground;
  double MetersPerPixel = 0.0;
  bool SnapsOff = false;
};

// A segment whose length the ghost shows, from one end to the other.
struct MeasuredLength {
  ParkPoint From;
  ParkPoint To;

  bool operator==(const MeasuredLength &) const = default;
};
```

Step 3: In ToolState, after Pointer, add:

```cpp
  // The pointer's last sample's meters per pixel and whether it turned snapping off.
  double MetersPerPixel = 0.0;
  bool SnapsOff = false;
  // The snap in effect while a path tool is chosen with the pointer on the ground, or while Edit
  // path drags a point. None otherwise.
  std::optional<PointerSnap> Snapped;
```

Step 4: Change movePointer to `void movePointer(ToolState &tool, const World &world, const PointerSample &pointer);` with the comment "Records the pointer's sample, then finds the snap in effect." After hoveredHandle, add:

```cpp
// The guides of the snap in effect, or none.
std::vector<SnapGuide> snapGuides(const ToolState &tool);
// From the last stop of the path being drawn to where a press would add the next, or from the
// other end of each segment at a dragged point to where it would land. Empty otherwise.
std::vector<MeasuredLength> measuredLengths(const ToolState &tool, const World &world);
```

Step 5: Create src/tools/internal/snapping.h:

```cpp
// What to snap: the kind, the path being drawn so far, or the point being dragged, the pointer,
// the reach to catch a snap and to keep one in meters, and what the last snap held.
struct SnapQuery {
  PathKind Kind = PathKind::Guest;
  std::span<const PathStop> Drawn;
  EntityKey Dragged = NULL_KEY;
  ParkPoint Pointer;
  double CatchReach = 0.0;
  double ReleaseReach = 0.0;
  std::span<const SnapHold> Held;
};

// The snap for the query, as tools/SPEC.md's Snapping section states.
PointerSnap findSnap(const World &world, const SnapQuery &query);
```

Use the usual include guard, TPJ_TOOLS_INTERNAL_SNAPPING_H, and include tools/tools.h, `<span>`.

### Task 5: Write the snapping component

Files:
- Create: `src/tools/snapping.cpp`
- Modify: `src/tools/tools.cpp`, `src/tools/CMakeLists.txt`

Step 1: Add snapping.cpp to tpj_tools in CMakeLists.txt.

Step 2: Move the snap helpers from tools.cpp's anonymous namespace into snapping.cpp's: Snap, isKindsPoint, nearestPointSnap, nearestDrawnSnap, pointOfDrawn, nearestDrawnSegmentSnap, straightPlace, nearestSegmentSnap, bendPlaceNear, nearestBendSnap, nearerSnap, and the geometry helpers they use. Keep in tools.cpp the helpers it still uses, such as roundedAt, straightPlace, and bendPlaceNear for insertAt. If both files need one, declare it in internal/snapping.h instead of copying it. Give each helper a reach argument in place of SNAP_REACH. Give Snap a `std::vector<SnapHold> Holds` and a `std::vector<SnapGuide> Guides`.

Step 3: findSnap does the following, in order. Build every list once per call, and reuse the buffers within it.

1. Positions. While drawing, take the drawn stops' positions from stopPositions. The half width for door targets is `pathWidth(query.Kind) / 2` while drawing, and half the widest segment at the dragged point while dragging.
2. Reach. A candidate uses ReleaseReach when every one of its holds is in query.Held, and CatchReach otherwise.
3. Points. Take the nearest of these within reach: path points of the kind, the drawn stops' points other than the last (RepeatStop), and door targets. A door target is `door.At + door.Out * (halfWidth + DOOR_CLEARANCE)` for each door parkDoors gives for the kind. Measure its distance to the line from door.At to it. Leave out a door whose target lies within half a segment's width of the ground line of any segment of the kind, measured as segmentAt measures it, because a path already reaches that door. An existing point wins a tie. Guides and holds: an existing point or stop gives a Point guide at it and a Point or DrawnStop hold. A door gives a Door guide from door.At to the target, with Door hold `{Door, entity, face}`. The stop is OnPoint, RepeatStop, or a NewPoint at the target. If a point catches, return it.
4. Anchors. While drawing with at least one stop, the anchors are the last stop and, once two stops are drawn, the first stop. While dragging, the anchors are the other end of each segment at the dragged point, in key order of the segment. Each anchor has arrival directions, the unit direction in which each segment reaches it:
    - each existing segment of the kind using the anchor's point, from its other end;
    - for a stop on a segment, both ways along it;
    - for a stop on an arc, both ways along the arc's tangent there, perpendicular to the line from the arc's center;
    - each drawn segment at the stop, from its other stop.
    Leave out the segments at the dragged point. If an anchor has no arrival direction, use +X and +Z. Drop a direction within 1e-9 of one already listed.
5. Guides. From each anchor and each arrival direction d, add rays in the directions d rotated by 0, +45, −45, +90, −90, +135, and −135 degrees. Rotate with cosine and sine of √0.5 for 45° steps. Drop a ray whose direction lies within 1e-9 of an earlier one from the same anchor. A guide's index is its place in this list.
6. Excluded segments. While drawing, leave out every existing or drawn segment whose ground line passes within MIN_SEGMENT_LENGTH of the last stop. While dragging, leave out every segment at the dragged point and every corner with a segment at it, as snapPoint did.
7. Crossings. For each pair of guides from different anchors that aren't parallel, where `|cross| > 1e-12`, find where they cross. Keep the crossing if it lies more than MIN_SEGMENT_LENGTH ahead of both anchors. For each guide and each segment of the kind not left out, find where the guide's ray crosses the segment's straight part. Keep it if it lies more than MIN_SEGMENT_LENGTH ahead of the anchor and more than MIN_SEGMENT_LENGTH from either end of the straight part. Take the nearest crossing within reach of the pointer.
    - A guide pair gives a NewPoint, two Direction guides from their anchors to it, and two Guide holds.
    - A guide and a segment gives an OnSegment at the crossing's fraction along the whole segment, a Direction guide, a Segment guide along the straight part, and holds Guide and Segment.
    If one catches, return it.
8. Segments. Use the existing segment, arc, and drawn segment searches, but catch within `max(reach, half width)`. That is the segment's own width for an existing segment, the narrower of a corner's two segments for an arc, and pathWidth(kind) for a drawn segment. Existing beats drawn on a tie, and a straight part beats an arc, as now. Guides: a Segment guide along the straight part, or along the drawn segment, or a Point guide at the place on an arc. Holds: Segment, Bend, or DrawnSegment. If one catches, return it.
9. Guides. Take the guide whose ray passes nearest the pointer within reach, where the pointer's projection lies more than MIN_SEGMENT_LENGTH ahead of its anchor. The stop is a NewPoint at the projection, with a Direction guide from the anchor and a Guide hold.
10. Length. If step 9 caught, let t be the projection's distance from the anchor and n be t rounded to the nearest whole number. If n is at least 1 and |t − n| lies within reach, move the stop to the anchor plus n along the guide. Use ReleaseReach when the Guide and `{Length, NULL_KEY, n}` holds are both held. Add that hold and a Length guide from the anchor to the stop.
11. Otherwise the stop is a NewPoint at the pointer, with no guides and no holds.

tools.cpp still calls snapStop and snapPoint until Task 6, so Task 6's build checks this task too.

### Task 6: Keep the snap in ToolState and act on it

Files:
- Modify: `src/tools/tools.cpp`

Step 1: Add an internal `void resnap(ToolState &tool, const World &world, bool keepHolds)`. It sets tool.Snapped like this:

- With a path tool and a Pointer, to findSnap for the tool's kind, its Drawn, and the Pointer.
- With Edit path holding a point that has been dragged, to findSnap with Dragged set to the point and the pointer at PointTo, for the kind of the segments at the point. A point that no segment uses gets a NewPoint at PointTo.
- To none otherwise.

The reaches are SNAP_CATCH_PIXELS and SNAP_RELEASE_PIXELS times tool.MetersPerPixel. Held is the old snap's Holds when keepHolds is set and a snap was in effect, and empty otherwise. While tool.SnapsOff is set, Snapped is `PointerSnap{at, NewPoint{at}, {}, {}}` at the pointer, or at PointTo while dragging.

Step 2: movePointer takes the world and the sample. It sets Pointer from the sample's Ground, and MetersPerPixel and SnapsOff from the sample, keeping today's Dragged and PointTo logic. Then it calls resnap with keepHolds set. pressPointer calls resnap without holds after it changes Drawn or takes hold, and releasePointer does the same after it lets go. selectTool resets Snapped.

Step 3: Replace every snapStop and snapPoint call with tool.Snapped:

- nextStop takes `tool.Snapped->Stop`, or none without a snap. The FINISH_REACH rule stays.
- pathStart reads tool.Snapped. Joins is false for a NewPoint.
- editPathEdit's MovePoint target turns tool.Snapped's stop into a PointTarget: NewPoint, OnPoint, OnSegment, or OnBend. Any other stop becomes a NewPoint at its At.

Step 4: Write snapGuides, which gives `tool.Snapped->Guides` or an empty list. Write measuredLengths:

- With a path tool, at least one stop drawn, and nextStop giving a stop, it gives one length from the last stop's position to the snap's At.
- With Edit path dragging a point that has a snap, it gives one length for each segment at the point, from that segment's other end to the snap's At, in key order of the segment.
- Otherwise it gives none.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 7: Give the tool the pointer's scale and the suppress key

Files:
- Modify: `src/app/input/cursor.h`, `src/app/input/cursor.cpp`, `src/app/input/input_map.h`, `src/app/input/input_map.cpp`, `src/app/input/platform_input.cpp`, `src/app/input/interaction.h`, `src/app/input/interaction.cpp`, `src/app/application.cpp`

Step 1: CursorNdc gains `float PixelX = 0.0f;` and `float PixelY = 0.0f;`, the span of one pixel in normalized device coordinates. Their comment is "the span of one pixel across and up". cursorNdc sets them to `2 / width` and `2 / height`. Add to cursor.h:

```cpp
// How many meters of ground one window pixel spans at the cursor: the mean of the distances from
// the ground under it to the ground one pixel to its right and one pixel above it, or the one of
// those that exists. None without a cursor, or when there is no ground under it or beside it.
std::optional<double> groundPerPixel(const std::optional<CursorNdc> &cursor,
                                     const CameraView &view);
```

Step 2: FrameInput gains `bool SnapsOff = false;` with the comment "Whether the player holds the key that turns snapping off." Add to input_map.h:

```cpp
// Whether the keys held, indexed by scancode, turn snapping off: either Ctrl key held, while ImGui
// doesn't want the keyboard.
bool snapsOff(std::span<const bool> keys, bool keyboardWanted);
```

gatherInput sets `input.SnapsOff` from it.

Step 3: Interaction::movePointer becomes `void movePointer(const World &world, const PointerSample &pointer);`. Add `std::vector<SnapGuide> snapGuides() const;` and `std::vector<MeasuredLength> measuredLengths(const World &world) const;`, each passing through to the tools.

Step 4: In application.cpp, read the cursor once, then call `Interactions.movePointer(Session.world(), PointerSample{groundUnderCursor(cursor, view), groundPerPixel(cursor, view).value_or(0.0), input.SnapsOff});`.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: builds with no warnings.

### Task 8: Draw guides and lengths

Files:
- Create: `src/app/ui/snap_guides.h`, `src/app/ui/snap_guides.cpp`
- Modify: `src/app/ui/tooling_ui.cpp`, `src/app/CMakeLists.txt`

Step 1: Declare in snap_guides.h:

```cpp
// Draws the guides over the scene and behind every panel, on ImGui's background draw list, each
// kind in its color, and each measured length at its segment's middle, in meters to two decimals.
// Call between ImGui::NewFrame and ImGui::Render.
void drawSnapGuides(const std::vector<SnapGuide> &guides,
                    const std::vector<MeasuredLength> &lengths, const CameraView &view);
```

Step 2: In snap_guides.cpp, project ground positions with windowPoint from render/graph_overlay.h, at ImGui's display size, and skip anything that doesn't project. Use these, all in window pixels:

- A Point guide is a ring of radius 9 and thickness 2, in cyan {0.15, 0.85, 0.95, 1}.
- A Door guide is a line of thickness 2 from From to To and a ring at To, in the same cyan.
- A Segment guide is a line of thickness 3, in green {0.30, 0.95, 0.40, 1}.
- A Direction guide is a dashed line of thickness 2 from From through To and 80 pixels beyond, with dashes of 8 and gaps of 6, in magenta {1.0, 0.35, 0.85, 1}.
- A Length guide is a tick 8 long across the line at each whole meter from From to To, in yellow {1.0, 0.85, 0.20, 1}.
- Each length's label is `%.2f m` of the distance between its ends. It sits 18 pixels above the projected middle, on a black rectangle at alpha 0.6.

Add snap_guides.cpp to tpj_app's sources in src/app/CMakeLists.txt.

Step 3: In ToolingUi::build, after the graph view, call `drawSnapGuides(interaction.snapGuides(), interaction.measuredLengths(world), view);`.

Run: `cmake.exe --build --preset windows-debug`
Expected: builds with no warnings.

### Task 9: Confirm the criteria

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -3`
Expected: `0`, then `100% tests passed, 0 tests failed out of 107`, then `tidy: clean.`

Step 2: Compare the simulation with the baseline, for criterion 8.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null > build/snaps-branch.txt; build/windows-debug/tpj_scenarios.exe --compare build/snaps-base.txt build/snaps-branch.txt; echo $?`
Expected: `0`

Step 3: Play criteria 1 to 7 in build/windows-debug/ThemeParkJones.exe, drawing freehand as a player would, not by typing exact positions.

Step 4: Build windows-release here and on main, for criterion 9.

Run: `git worktree add ../TPJ-baseline main && cmake.exe --preset windows-release && cmake.exe --build --preset windows-release && (cd ../TPJ-baseline && cmake.exe --preset windows-release && cmake.exe --build --preset windows-release)`
Expected: both builds finish with no warnings.

In each tree, run `build/windows-release/ThemeParkJones.exe --park tests/parks/stress/full.park`, pick Guest path, click one stop, and move the pointer around for about ten seconds. Note the Debug panel's frame time in each.

Step 5: Leave this tree's release build for the user to play. Remove the worktree, so main is free to merge into.

Run: `git worktree remove ../TPJ-baseline`
Expected: no output.

### Task 10: Commit

Stage only this feature's paths, never `git add -A`. Leave out image1.png, image2.png, and parks/*.park. Commit once with the commit-hygiene skill, subject "Tools: Snap paths to guides, doors, and paving with visible guides".
