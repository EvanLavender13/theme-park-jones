# Feature: Corners and Widths

## Summary

Paths can bend in rounded corners, and the player sets each corner's radius and each segment's width. Where exactly two segments meet at a point, their ground lines now round the bend with a circular arc of the point's radius. The radius shrinks where the segments are too short for it, and both lines meet at the arc's middle. The default radius is 0, a sharp corner, and every bend in the checked-in parks is set to 0, so no bend changes until the player rounds it. Guests walk the arcs of rounded bends. Two new commands, SetCornerRadius and SetSegmentWidth, set a point's radius and a segment's width. The Edit path tool shows a radius handle inside each corner. Dragging the handle sets the radius, from a sharp corner up to the largest that fits. Dragging near a segment's side sets its width, from 1.5 m to 16 m. Every edit's ghost now shows the segments as they will lie after the commit, with their rounded corners, so a path being drawn shows the bends it will have. The validity check tests each rounded line against the boxes. A deletion or insertion can change which corners round, so it is now refused when a corner it rounds would run into a box. The park world checks gain a chosen edit for each new command, and the scenarios' random edits draw them too.

## Acceptance criteria

1. Pick Guest path in the app's new park and click three points that make a right-angled bend. The bend is sharp, in the ghost and after the path commits. (play)
2. Pick Edit path. A radius handle shows inside the bend, on the line halfway between its two segments. Drag the handle away from the corner: the ghost's arc grows and follows the drag, and it stops growing once it touches half of the shorter segment. Release, and the bend commits with that radius. With the Graph checkbox on, the guest network's line follows the arc. Drag the handle toward the corner as far as it goes: the ghost shows a sharp corner. Click the handle without dragging: nothing changes. (play)
3. With Edit path, hover a side of a segment: both of its sides are marked. Drag outward: the ghost shows the segment wider, up to 16 m. Drag inward: it narrows, down to 1.5 m. Release, and the width commits. Click a side without dragging: nothing changes. (play)
4. Place a shop just inside a bend, clear of the path. With Edit path, drag the bend's radius handle away from the corner until the arc would reach the shop: the ghost turns to the invalid tint, and releasing there changes nothing. Do the same by widening a segment into a box. (play)
5. A capture of the winding-path stress park shows every bend sharp, as before this feature: `build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/winding-path.park --capture build/windows-debug/corners.bmp`. (capture)
6. The scenarios run their random edits, which now include SetCornerRadius and SetSegmentWidth, without a crash: `build/windows-debug/tpj_scenarios.exe --ticks 1000 2> /dev/null | grep -c "scenario park-edits"` prints `1001`. (check)
7. The park world checks pass with their two new chosen edits, and the whole suite passes: `ctest.exe --preset windows-debug` reports 0 tests failed. (listed tests)

## Tests

The feature adds two commands, so the milestone asks for a chosen edit of each in the park world checks. Add them to tests/sim/park_world_checks_test.cpp, next to the other chosen edits, and list them in chosenEdits after the existing ones. Each starts from the world warm.park reaches after one cycle, as the other chosen edits do. The existing checks then hold for each: its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name each for what it makes the capabilities do.

- Rounding a corner that guests walk. Setup: find the first corner parkCorners gives, in key order, whose point exactly two guest segments use, where a guest stands on at least one of the two (a guest record's place is on that segment's key). Require that one exists and that its MaxRadius is above 0. Change: a SetCornerRadius of that corner's point to half its MaxRadius. Require that the world accepts it. Expected: the existing checks hold. Both segments at the corner keep their keys and take a new shape, and the guests on them are carried onto it.
- Narrowing the segment a guest heading to the shop stands on, found by guestSegmentUnderGuestHeadingToShop, as insertingPointUnderGuests finds it. Change: a SetSegmentWidth of that segment to half the width parkSegments gives it. Require that the world accepts it. Expected: the existing checks hold. The segment keeps its key and its ground line, and only its width changes.

## Medium

The feature adds no fields or flows. It adds commands that change park intent, which decision 0025 places outside the medium. Rounding changes what parkSegments publishes: a segment's ground line now follows the arc at a rounded corner. routes reads it through parkSegments, so every guest carrier at a bend follows the arc, and route distances measure along it (principle 4). Guests on a segment whose corner changes are carried by carry-over (sim/medium/SPEC.md, Carry-over): where no carrier passes over a guest's old ground, the guest moves to the nearest place on its carrier's new shape. A segment's width is read by the validity check and the renderer. routes still gives every carrier its kind's path width until wide-routes.

No new intent type is registered. Every checked-in park already holds radii and widths, so no park file needs new state. The park world checks cover the new commands through the chosen edits in Tests, and cover rounded ground lines through every park whose points have two segments at a bend, such as warm.park.

A bend at radius 0 keeps exactly the ground line it had before this feature, and every bend in the checked-in parks and the scenarios' park builders is at radius 0. So every scenario but park-edits simulates unchanged, which `tpj_scenarios --compare` shows against main built with a default radius of 0. park-edits changes because it draws the new commands.

## Spec changes

### src/sim/park/SPEC.md

In the Boundary section, replace "addParkEdits registers the eight commands." with:

> addParkEdits registers the ten commands.

Replace the paragraph that starts "Other modules read intent only through" with:

> Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, parkCorners, and parkBoxes. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, ParkCorner, and ParkBox. Other modules change intent only through commands.

Replace the paragraph that starts "segmentLine, stopPosition, stopPositions," with:

> segmentLine, cornerRadiusFor, stopPosition, stopPositions, pathCrossings, editedSegments, footprintOf, and the sizes pathWidth, MIN_SEGMENT_LENGTH, ARC_TOLERANCE, DEFAULT_CORNER_RADIUS, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so that the renderer, the tools, and routes measure the same shapes this module validates.

In the Intent section, replace the first bullet with:

> - A path point has a ground position and a corner radius. The radius is for the corner the point makes when exactly two segments meet there.

Replace the whole Ground lines section, from its heading to the paragraph ending "has a width that is not finite and above zero.", with:

> ## Ground lines
>
> A segment's ground line runs from its From point to its To point, as CarrierPoints (sim/medium/SPEC.md), the first at distance 0 and each at its distance along the line. It is straight, except where it rounds a corner at either end. parkSegments gives each segment with its ground line. segmentLine gives the straight ground line between any two positions. The renderer, the validity check, and routes all use a segment's ground line, so what is drawn, what is refused, and what is walked are the same line.
>
> A segment's ground line is empty when segmentLine between its two points is empty, which it is when either position is not finite or lies outside the park, or the two lie closer together than MIN_SEGMENT_LENGTH. It is also empty when the segment names a key that holds no path point, names the same point twice, or has a width that is not finite and above zero.
>
> A point makes a corner when exactly two segments use it, neither ground line is empty, and their far ends are different points at least MIN_SEGMENT_LENGTH apart. The point must also lie farther than MIN_SEGMENT_LENGTH from the line through the two far ends. Where the segments run straight on, or fold straight back, the point makes no corner. parkCorners gives each corner, in its point's key order. A ParkCorner has the point's key and position, the unit direction along each of its two segments, the lower-keyed segment's first, and the unit direction into the corner, halfway between those two. It also has the radius the corner is rounded with, the largest radius that fits, and where the arc's middle lies.
>
> A corner is rounded with a circular arc that touches both of its segments. The arc's radius is the point's radius, made smaller if needed so that the arc touches each segment no farther from the point than half that segment's straight length. That smaller radius is the largest that fits. If the point's radius is not finite, or is below zero, the corner is sharp. Each of the two segments' ground lines runs straight from its far end to where the arc touches it, then along the arc to the arc's middle, where the two lines meet. Along the arc, the line is made of straight pieces, each within ARC_TOLERANCE of the arc. If the radius is 0, both lines end at the point. Where a point makes no corner, every segment at it ends at the point.
>
> cornerRadiusFor gives the radius whose arc's middle lies a given distance from a corner's point, into the corner. It gives 0 for a distance of 0 or less.

In the Physical validity section, after the paragraph that ends "isPhysicallyValid returns whether a world is valid.", add:

> A segment's ground line can change when an edit changes a corner at its end, even when the edit leaves the segment's own points in place. A deletion or an insertion can make a corner, remove one, or change how far an arc may reach. The check covers every ground line an edit changes.

In the Commands section, after the list of join cases that ends "If they meet within MIN_SEGMENT_LENGTH of an end of each, nothing splits." and its following sentences, add:

> Joins are found along the straight line between each segment's two points, not along its rounded ground line. Near a rounded corner, a path can cross or end on the arc without joining it.

In the Commands section, replace "Eight commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, and DeletePoint." with:

> Ten commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, DeletePoint, SetCornerRadius, and SetSegmentWidth.

After the paragraph that starts "MovePoint moves a path point to a target", add:

> SetCornerRadius names a path point and a radius. SetSegmentWidth names a segment and a width.

In the paragraph that starts "On a valid world, a command is accepted exactly when", after the sentence ending "and a segment it merges has a ground line, as does every part that segment splits into.", add:

> For SetCornerRadius, it means the key holds a path point and the radius is finite and at least 0. For SetSegmentWidth, it means the key holds a segment with a ground line and the width is finite and above zero. For every command, the world it describes is physically valid when every ground line the command adds or changes, including one whose corner it changes, is not empty, stays in the park, and meets no solid. So a DeleteSegment, DeletePoint, or InsertPoint is refused if a corner it changes would round into a solid.

In the list of what an accepted command leaves, after the DeletePoint bullet, add:

> - SetCornerRadius sets the point's radius, as given, and changes nothing else. A radius larger than fits is kept, and the corner is rounded with the largest that fits.
> - SetSegmentWidth sets the segment's width, as given, and changes nothing else.

Replace the paragraph that starts "movedSegments gives each segment at a MovePoint's point" with:

> editedSegments gives each segment an edit adds, or whose ground line or width it changes, as it would lie after the edit, in ascending key order. A new segment has the key the commit would give it. The parts of a segment the edit splits are among them, and so is a segment whose corner the edit changes. It gives them whether or not the world refuses the edit for meeting a solid. It gives none when the edit doesn't name what it acts on, when a segment the edit adds or reshapes has no ground line, and for edits of boxes.

Replace "A ParkEdit holds any one of the eight commands." with:

> A ParkEdit holds any one of the ten commands.

### src/tools/SPEC.md

In the Boundary section, replace "boxAt, segmentAt, pointAt, snapStop, and snapPoint are the picking queries the tools use." with:

> boxAt, segmentAt, pointAt, radiusHandleAt, sideAt, snapStop, and snapPoint are the picking queries the tools use. radiusHandles and hoveredHandle give the handles Edit path shows.

In the Picking section, after the paragraph that starts "boxAt gives the box whose footprint holds a ground point.", add:

> Each corner (sim/park/SPEC.md, Ground lines) has a radius handle. It lies on the line from the corner's point into the corner, at the arc's middle, but never nearer the point than RADIUS_HANDLE_DEPTH. radiusHandleAt gives the corner whose radius handle is nearest a ground point, within HANDLE_REACH, by its point's key. A segment's sides are the two lines half its width from its ground line. sideAt gives the segment whose side lies nearest a ground point, within SIDE_REACH. If two are equally near, each gives the one with the lower key.

At the end of the paragraph that starts "snapStop gives the stop a path tool's click makes.", add:

> Distances to a segment are measured to the straight line between its two points, not to its rounded ground line.

In the Tools section, replace the paragraph that starts "Edit path reshapes paths of both kinds." with:

> Edit path reshapes paths of both kinds. Under the pointer, a radius handle comes first, then a path point, then a segment's side, then the segment itself. radiusHandles gives where every corner's radius handle lies while Edit path is chosen and nothing is held, so the ghost can show them. hoveredHandle gives the radius handle or the segment's sides under the pointer, when a press would take hold of one. The tool marks the path point under the pointer, as pointAt gives it, when no radius handle is under the pointer.
>
> Dragging a point moves it, and where it lands snaps as snapPoint gives, so a point dropped on a point or segment of its kind joins it there. The ghost shows every segment the move reshapes, and marks a join with a point or segment, and each place where a moved segment crosses one.
>
> Dragging a radius handle sets the corner's radius. The arc's middle moves as far into the corner as the pointer moves along the corner's halfway line, so the press itself changes nothing. The radius stays between 0 and the largest that fits.
>
> Dragging a segment's side sets its width: the width at the press, plus twice how far the pointer has moved away from the ground line. The width stays between MIN_PATH_WIDTH and MAX_PATH_WIDTH.
>
> The ghost shows the reshaped segments. If the player clicks a point, a radius handle, or a side without dragging, nothing is committed. Away from all of these, the ghost marks the place nearest the pointer on the segment under it, and a click inserts a point there.

### src/render/SPEC.md

In the Boundary section, replace "- park_mesh.h for buildParkMesh, buildGhostMesh, appendEntity, meshBounds, and HIGHLIGHT_TINT" with:

> - park_mesh.h for buildParkMesh, buildGhostMesh, appendEntity, appendRadiusHandle, appendSides, meshBounds, HIGHLIGHT_TINT, and HANDLE_COLOR

In the Ghosts and highlights section, replace the first bullet of buildGhostMesh's list with:

> - every segment the edit adds, or whose ground line or width it changes, as editedSegments gives it, and an added box or a moved box, translucent in its kind's color, or in an invalid tint when the world refuses the edit. If the world refuses an added path so that editedSegments gives none, the ghost shows the straight segments between its stops instead.

Replace the third bullet with:

> - the entity an accepted delete would remove, or every segment at a point an accepted DeletePoint deletes, in a delete tint, under the segments the deletion reshapes

After the paragraph that starts "appendPathStart shows where a path would start", add:

> appendRadiusHandle adds a radius handle as a flat disc of RADIUS_HANDLE_SIZE, in a given color, above join marks. appendSides adds a strip along the inside of each of a segment's two sides, in a given color, above the segment's ghost. It adds nothing when the key holds no segment with a ground line.

### src/app/SPEC.md

In the Tools and the ghost section, replace "It shows the candidate park's walkways and starved marks, the box or path point the tool marks, and the Inspector's subject, highlighted." with:

> It shows the candidate park's walkways and starved marks, the box or path point the tool marks, and the Inspector's subject, highlighted. While Edit path is chosen and nothing is held, it shows every corner's radius handle in HANDLE_COLOR, and highlights the radius handle or the segment's sides under the pointer.

## Public interface

src/sim/park/intent.h, after ParkPathPoint:

```cpp
// The corner a path point makes where exactly two segments meet, as other modules read it: its
// point's key and position, the unit direction along each of its two segments, the lower-keyed
// segment's first, and the unit direction into the corner, halfway between them. Radius is the
// radius the corner is rounded with, at most MaxRadius, the largest that fits. Middle is the
// arc's middle, where the two segments' ground lines meet.
struct ParkCorner {
  EntityKey Point = NULL_KEY;
  ParkPoint At;
  std::array<ParkPoint, 2> Along;
  ParkPoint Toward;
  double Radius = 0.0;
  double MaxRadius = 0.0;
  ParkPoint Middle;

  bool operator==(const ParkCorner &) const = default;
};
```

and after parkSegments' declaration:

```cpp
// Every corner, in ascending key order of its point.
std::vector<ParkCorner> parkCorners(const World &world);
```

src/sim/park/geometry.h, after MIN_SEGMENT_LENGTH:

```cpp
// Each straight piece of a rounded corner's ground line lies within this many meters of its arc.
inline constexpr double ARC_TOLERANCE = 0.05;
```

and after footprintOf:

```cpp
// The radius whose arc's middle lies the depth from the corner's point, along Toward. 0 for a depth
// of 0 or less.
double cornerRadiusFor(const ParkCorner &corner, double depth);
```

src/sim/park/edits.h, after DeletePoint:

```cpp
// The path point's corner radius, held exactly as given.
struct SetCornerRadius {
  EntityKey Point = NULL_KEY;
  double Radius = 0.0;

  bool operator==(const SetCornerRadius &) const = default;
};

// The segment's width, held exactly as given.
struct SetSegmentWidth {
  EntityKey Segment = NULL_KEY;
  double Width = 0.0;

  bool operator==(const SetSegmentWidth &) const = default;
};
```

addParkEdits' comment and ParkEdit become:

```cpp
// Registers AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint,
// DeletePoint, SetCornerRadius, and SetSegmentWidth, in that order.
void addParkEdits(WorldSchema &schema);

// A tentative or committed edit to park intent: one of the ten commands.
using ParkEdit = std::variant<AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint,
                              InsertPoint, DeletePoint, SetCornerRadius, SetSegmentWidth>;
```

movedSegments and mergedSegment are removed. In their place:

```cpp
// Each segment the edit adds, or whose ground line or width it changes, as it would lie after the
// edit, in ascending key order, a new segment with the key the commit would give it. Given whether
// or not the world refuses the edit for meeting a solid. Empty when the edit names nothing it acts
// on, when a segment it adds or reshapes has no ground line, and for edits of boxes.
std::vector<ParkSegment> editedSegments(const World &world, const ParkEdit &edit);
```

isAccepted and applyCommand gain an overload for each of SetCornerRadius and SetSegmentWidth, after the DeletePoint ones.

src/tools/tools.h, after POINT_REACH:

```cpp
// Edit path keeps a corner's radius handle at least this far from its point, in meters.
inline constexpr double RADIUS_HANDLE_DEPTH = 3.0;
// A press or hover this near a radius handle takes it, in meters.
inline constexpr double HANDLE_REACH = 1.0;
// A press or hover this near a segment's side takes it, in meters.
inline constexpr double SIDE_REACH = 0.5;
// Edit path sets a segment's width between these, in meters.
inline constexpr double MIN_PATH_WIDTH = 1.5;
inline constexpr double MAX_PATH_WIDTH = 16.0;

// What an Edit path handle sets: a corner's radius, or a segment's width.
enum class HandleKind : uint8_t { Radius, Side };

// A handle Edit path takes hold of: a corner's radius handle, keyed by the corner's point, at At,
// or a segment's sides, keyed by the segment.
struct EditHandle {
  HandleKind Kind = HandleKind::Radius;
  EntityKey Key = NULL_KEY;
  ParkPoint At;

  bool operator==(const EditHandle &) const = default;
};
```

ToolState gains, after Inserting:

```cpp
  // Holding with EditPath on a radius handle or a segment's sides.
  std::optional<EditHandle> Grabbed;
  // For a radius handle, how much deeper the arc's middle lay than the pointer at the press, along
  // the corner's Toward. For a side, the width at the press less twice the pointer's distance from
  // the ground line.
  double GrabFrom = 0.0;
```

After snapPoint:

```cpp
// Where the corner's radius handle lies.
ParkPoint radiusHandleOf(const ParkCorner &corner);
// The point of the corner whose radius handle lies nearest the point within HANDLE_REACH. Ties go
// to the lower key.
std::optional<EntityKey> radiusHandleAt(const World &world, ParkPoint point);
// The segment with a ground line whose side lies nearest the point within SIDE_REACH. Ties go to
// the lower key.
std::optional<EntityKey> sideAt(const World &world, ParkPoint point);
// Where every corner's radius handle lies, in key order of its point, while EditPath is chosen and
// nothing is held. Empty otherwise.
std::vector<ParkPoint> radiusHandles(const ToolState &tool, const World &world);
// The radius handle or segment's sides under the pointer that a press with EditPath would take,
// while nothing is held. None otherwise.
std::optional<EditHandle> hoveredHandle(const ToolState &tool, const World &world);
```

src/render/park_mesh.h, after appendFinishMark:

```cpp
// Edit path's radius handles are flat discs of this radius, this high, in this color when not
// hovered.
inline constexpr double RADIUS_HANDLE_SIZE = 0.6;
inline constexpr float HANDLE_LIFT = 0.07f;
inline constexpr Rgba HANDLE_COLOR{0.95f, 0.95f, 0.95f, 0.8f};

// Adds a radius handle centered on the point.
void appendRadiusHandle(ParkMesh &mesh, ParkPoint at, Rgba color);

// A segment's side marks are strips this wide along the inside of each side.
inline constexpr double SIDE_MARK_WIDTH = 0.3;

// Adds a strip along the inside of each side of the segment the key holds, in the color, twice
// GHOST_LIFT above its kind's paths. Nothing when the key holds no segment with a ground line.
void appendSides(ParkMesh &mesh, const World &world, EntityKey segment, Rgba color);
```

## Files affected

- Create: `src/sim/park/internal/layout.h`
- Create: `src/sim/park/layout.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park/intent.h`
- Modify: `src/sim/park/intent.cpp`
- Modify: `src/sim/park/geometry.h`
- Modify: `src/sim/park/geometry.cpp`
- Modify: `src/sim/park/edits.h`
- Modify: `src/sim/park/edits.cpp`
- Modify: `src/sim/park/SPEC.md`
- Modify: `src/tools/tools.h`
- Modify: `src/tools/tools.cpp`
- Modify: `src/tools/SPEC.md`
- Modify: `src/render/park_mesh.h`
- Modify: `src/render/park_mesh.cpp`
- Modify: `src/render/SPEC.md`
- Modify: `src/app/scene/scene_sync.h`
- Modify: `src/app/scene/scene_uploads.cpp`
- Modify: `src/app/input/interaction.h`
- Modify: `src/app/input/interaction.cpp`
- Modify: `src/app/application.cpp`
- Modify: `src/app/SPEC.md`
- Modify: `src/scenarios/park_edits.cpp`
- Modify: `tests/sim/park_world_checks_test.cpp`

## Dependencies

- point-editing: the edit plan that every path command builds, the Edit path tool, and the ghosts of point edits. Merged.
- The default corner radius on every path point, from points-and-segments. Merged.
- sim/park's geometry runs in the simulation under decision 0022, so arcs and radii are built from square roots and products alone, with no trigonometry from the C runtime.

## Out of scope

- The paved surface. Ribbons still draw each segment on its own, so where a radius is less than half the width, the inner edge of the ribbon folds over itself, and junctions still show round joints. paved-surface draws both properly.
- Carriers as wide as their segments. routes keeps its kind's path width until wide-routes.
- Joining along a rounded corner. Snaps, inserts, and crossing splits still measure against the straight line between a segment's two points. At the default radius the arc stays within snap reach of that line, so joins still work there. On a wide curve, a path that ends on the arc doesn't join it. snaps-and-guides reworks snapping onto segments and takes this on.
- Snapping a radius or width to whole meters, or showing their values while dragging.
- Typing an exact radius or width. Added to the backlog.
- A handle at every path point. It is already in the milestone's deepening candidates.
- Shrinking a radius or width until it clears a box. The edit is refused instead, and the ghost shows why (principle 5).

## Open questions

None.
