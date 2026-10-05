# Feature: Point Editing

## Summary

The player reshapes paths after drawing them. A new Edit path tool drags a path point, and every segment at it follows. If the dragged point is dropped on a point or segment of its kind, it joins there: it merges into the point, or the segment splits at it. Where a moved segment crosses a segment of its kind, both split and share a point, as crossing-splits does for drawn paths. Clicking a segment with Edit path inserts a point there. The Delete tool now deletes the path point under the pointer before the segment. Deleting a point with two segments merges them into one segment with the wider width. Deleting a point with one segment, or with three or more, deletes them all. Three new commands carry these edits: MovePoint, InsertPoint, and DeletePoint. Each previews as a ghost before commit. The park world checks gain a chosen edit for each, and the scenarios' random edits draw them too.

## Acceptance criteria

1. In the app's new park, pick Edit path and hover the template path's inner end, at (0, 103). A highlight disc marks the point. Drag it about 10 m to the side. While dragging, the ghost shows the segment running from the entrance end to the pointer. On release, the path moves there. Click the point again without dragging: nothing changes. (play)
2. Pick Guest path and draw a second path that starts about 10 m beside the template path and ends 8 m further away. Pick Edit path and drag the second path's near end onto the middle of the template path. While dragging, the ghost marks the join. After the release, pick Delete and hover the template path near the entrance: the ghost tints only the part from the entrance to the joined point. (play)
3. Pick Edit path and drag a path's free end onto another guest point. The ghost marks the join. After the release, pick Delete and hover that point: the ghost tints every segment at it, including the moved one. (play)
4. Pick Edit path and drag the end of a guest path so that its segment crosses another guest path, away from that path's ends. The ghost marks the crossing. After the release, pick Delete and hover one arm of the crossed path: the ghost tints only that arm. (play)
5. Pick Edit path and hover the middle of a segment: the ghost marks the place nearest the pointer. Click there, then pick Delete and hover the new point. The ghost tints both of its segments in the delete tint and shows the one segment that would replace them. Click: the path is one segment again. Hover a point with three segments: the ghost tints all three, and clicking deletes them all. (play)
6. Open tests/parks/warm.park with `--park`. Pick Guest path and draw two segments in a straight line toward a point they share, so they run opposite ways from their far ends. Click A on a guest path that guests walk along, click B about 15 m away, and click B again to finish. Then click C about 15 m beyond B, in line with A and B, click B, and click B again. Wait for guests to walk along both. Pick Delete and click B to merge them. No guest jumps or turns around. (play, the milestone's criterion 4)
7. `ctest.exe --preset windows-debug` passes. This includes the park world checks over the three chosen edits in Tests. (check: PLAN.md Task 14)
8. AddPath behaves exactly as before: `tpj_scenarios --compare` shows the output of the windows-debug tpj_scenarios unchanged by the rework of AddPath's plan, before the new commands enter the random edits. (check: PLAN.md Task 6)

## Tests

The feature adds three commands, so the milestone asks for a chosen edit of each in the park world checks. Add them to tests/sim/park_world_checks_test.cpp, next to the other chosen edits, and list them in chosenEdits. Each starts from the world warm.park reaches after one cycle, as the other chosen edits do. The existing checks then hold for each: its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name each for what it makes the capabilities do.

- Moving a point at the end of a segment guests stand on. Setup: find the first path point, in key order, that exactly two segments use, both of guest kind, where a guest stands on at least one of the two (a guest record's place is on that segment's key). Require that one exists. Change: a MovePoint of that point to a NewPoint 3 m from it along +x, or else along -x, +z, or -z, whichever the world accepts first. Require that one is accepted. Expected: the existing checks hold. The guests on the reshaped segment are carried onto its new shape.
- Inserting a point in the middle of the segment a guest heading to the shop stands on, found as addingPathEndingOnGuestSegment finds it. Change: an InsertPoint of that segment at 0.5. Require that the world accepts it. Expected: the existing checks hold. The segment splits under the guests.
- Deleting the point found for the first edit, which merges its two segments under the guests. Change: a DeletePoint of that point. Require that the world accepts it, and that mergedSegment gives a segment for it, so the edit merges rather than deletes. Expected: the existing checks hold. The guests on the segment that goes are carried onto the merged one.

## Medium

The feature adds no fields or flows. It adds commands that change park intent, which decision 0025 places outside the medium. routes reads the moved, split, and merged segments through parkSegments, as it reads any segments. Guests on a reshaped, split, or merged segment are carried by carry-over (sim/guests/SPEC.md, Carrying across an edit). A guest carried onto a merged segment that runs the other way turns, so it keeps walking the same way over the ground.

No new intent type is registered, so no park file needs to hold new state. The park world checks cover the new commands through the chosen edits in Tests.

## Spec changes

src/sim/park/SPEC.md:

- In Boundary, "addParkEdits registers the five commands." becomes "addParkEdits registers the eight commands." The list of public names becomes "segmentLine, stopPosition, stopPositions, pathCrossings, movedSegments, mergedSegment, footprintOf, and the sizes ...".
- In Intent, "Segments share a point where the player joined them, and where a path was drawn across them." becomes "Segments share a point where the player joined them, and where a path was drawn or moved across them."
- In Commands, "Five commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, and DeleteBox." becomes "Eight commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, and DeletePoint."
- In Commands, after the paragraph that ends "stopPosition gives that of a NewPoint, an OnPoint, or an OnSegment.", a new paragraph:

  > MovePoint moves a path point to a target: a new position (NewPoint), another path point (OnPoint), or a place on a segment (OnSegment), given as for AddPath's stops. InsertPoint names a segment and a fraction of the way from its From point to its To point. DeletePoint names a path point. A segment's far end from a point is its other end.

- In Commands, after the sentence that ends "and every segment it adds or splits has a ground line.", add:

  > For MovePoint, it means at least one segment uses the point and all of them have one kind, an OnPoint target names another path point of that kind, an OnSegment target names a segment of that kind that doesn't end at the point, at a fraction strictly between 0 and 1, and every segment it moves, splits, or adds has a ground line. For InsertPoint, it means the segment has a ground line, the fraction lies strictly between 0 and 1, and both parts have ground lines. For DeletePoint, it means the key holds a path point, and a segment it merges has a ground line, as does every part that segment splits into.

- In Commands, after the list item "- DeleteBox destroys the box.", add four items:

  > - MovePoint to a NewPoint or OnSegment target moves the point there and keeps its key and radius. Every segment at it follows and keeps its key, kind, and width. An OnSegment target's segment splits at the point, as for an OnSegment stop.
  > - MovePoint to an OnPoint target destroys the moved point. Every segment at it ends at the target point instead and keeps its key, kind, and width. A segment between the two points is destroyed. If no segment then uses the target point, it is destroyed too.
  > - InsertPoint adds a point at the place, with DEFAULT_CORNER_RADIUS, and the segment splits there, as for an OnSegment stop.
  > - DeletePoint destroys the point. If exactly two segments use it, and their far ends are different points, it merges them. The one with the lower key keeps its key, kind, and far end, and takes the wider of the two widths. Its end at the point moves to the other segment's far end, and the other segment is destroyed. Otherwise, DeletePoint destroys every segment at the point, and each of their far ends that no other segment uses.

- In Commands, the paragraph and list on joins become:

  > Where a segment that AddPath adds, MovePoint moves, or DeletePoint merges meets another segment of its kind, the two join. The other segment can be one the command leaves in place, or another one the command adds or moves.
  >
  > - If each end of each segment lies within MIN_SEGMENT_LENGTH of the line through the other, the segments lie along one line. They don't meet, and neither splits.
  > - Otherwise, if an end of one segment lies within MIN_SEGMENT_LENGTH of the other segment, and farther than MIN_SEGMENT_LENGTH from both of the other's ends, the other segment splits at that end's point.
  > - Otherwise, if the segments cross, the command adds a point at the crossing, with DEFAULT_CORNER_RADIUS, and both split there. If the crossing lies within MIN_SEGMENT_LENGTH of a point the command already places, as where a path doubles back across a segment, both split at that point instead.
  >
  > If they meet within MIN_SEGMENT_LENGTH of an end of each, nothing splits. A segment that meets several others splits at each of their points, in order along it, together with any OnSegment or OnDrawnSegment stops or OnSegment target on it, as described above. pathCrossings gives the ground position of each point where AddPath, MovePoint, or DeletePoint splits a segment this way, other than at an OnSegment or OnDrawnSegment stop or a MovePoint's OnSegment target. It gives none if the world refuses the command.
  >
  > movedSegments gives each segment at a MovePoint's point as it would lie after the move, before any split, leaving out a segment the move destroys. mergedSegment gives the segment a DeletePoint would merge the point's two segments into, before any split, or none when it merges nothing. Both give their segments whether or not the world accepts the command.

- In Commands, "A ParkEdit holds any one of the five commands." becomes "A ParkEdit holds any one of the eight commands."

src/tools/SPEC.md:

- In Boundary, "MoveBox, or Delete" becomes "MoveBox, EditPath, or Delete", and "boxAt, segmentAt, and snapStop are the picking queries the tools use." becomes "boxAt, segmentAt, pointAt, snapStop, and snapPoint are the picking queries the tools use."
- In Picking, after the boxAt and segmentAt sentences, add: "pointAt gives the path point nearest a ground point, within POINT_REACH. If two are equally near, it gives the one with the lower key."
- At the end of Picking, add: "snapPoint gives the target a dragged path point moves to. It follows snapStop's rules for a path of the point's kind with no stops drawn, but it leaves out the dragged point and every segment at it. A point's kind is the kind of the segments at it. If no segment uses the point, the target is a new point at the place."
- In Tools, after the Move box paragraph, a new paragraph: "Edit path reshapes paths of both kinds. It marks the path point under the pointer, as pointAt gives it. Dragging a point moves it, and where it lands snaps as snapPoint gives, so a point dropped on a point or segment of its kind joins it there. The ghost shows the point's segments where they would lie, and marks a join with a point or segment, and each place where a moved segment crosses one. If the player clicks a point without dragging, nothing is committed. Away from any point, the ghost marks the place nearest the pointer on the segment under it, and a click inserts a point there."
- The Delete paragraph becomes: "Delete removes the box under the pointer. If there is no box, it deletes the path point under the pointer, as pointAt gives it. Otherwise it removes the segment under the pointer, and any point no other segment uses. The ghost shows what it removes, and the segment that a point's deletion would merge its two segments into."

src/render/SPEC.md, in Ghosts and highlights:

- buildGhostMesh's list becomes:

  > - an added or moved box, an added path's segments, a moved point's segments, or the segment a point's deletion merges two into, translucent in its kind's color, or in an invalid tint when the world refuses the edit
  > - a mark at each stop where an added path joins a point or segment, at a moved point's target when it is a point or segment, at the place where an inserted point goes, and at each place pathCrossings gives, in JOIN_MARK_COLOR
  > - the entity an accepted delete would remove, or every segment at a point an accepted DeletePoint deletes, in a delete tint

- "appendEntity adds the box or segment a key holds, shaped as the park draws it, in a given color such as HIGHLIGHT_TINT. It adds nothing when the key holds neither." becomes "appendEntity adds the box, segment, or path point a key holds, in a given color such as HIGHLIGHT_TINT. A box or segment is shaped as the park draws it, and a path point is a flat disc of JOIN_MARK_RADIUS. It adds nothing when the key holds none of these."

src/app/SPEC.md, in Tools and the ghost:

- "The tools are Look, Guest path, Backstage path, Place shop, Place depot, Move box, and Delete" becomes "The tools are Look, Guest path, Backstage path, Place shop, Place depot, Move box, Edit path, and Delete".
- "the box the tool marks" becomes "the box or path point the tool marks".

Public interface, in src/sim/park/edits.h:

```cpp
// Where MovePoint moves a point: a new position, another path point, or a place on a segment.
using PointTarget = std::variant<NewPoint, OnPoint, OnSegment>;

// The path point moves to the target, and every segment at it follows.
struct MovePoint {
  EntityKey Point = NULL_KEY;
  PointTarget To;

  bool operator==(const MovePoint &) const = default;
};

// A new point on the segment, Along of the way from its From point to its To point, splitting it
// there.
struct InsertPoint {
  EntityKey Segment = NULL_KEY;
  double Along = 0.0;

  bool operator==(const InsertPoint &) const = default;
};

// The path point goes, and its two segments merge when it has exactly two.
struct DeletePoint {
  EntityKey Point = NULL_KEY;

  bool operator==(const DeletePoint &) const = default;
};

// Registers AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint, InsertPoint, and
// DeletePoint, in that order.
void addParkEdits(WorldSchema &schema);

// A tentative or committed edit to park intent: one of the eight commands.
using ParkEdit = std::variant<AddPath, AddBox, MoveBox, DeleteSegment, DeleteBox, MovePoint,
                              InsertPoint, DeletePoint>;

// The ground position of each point where the move or the merge splits a segment of its kind
// because the two meet, leaving out a MovePoint's OnSegment target. In the order the meetings are
// found. Empty when the world refuses the command.
std::vector<ParkPoint> pathCrossings(const World &world, const MovePoint &move);
std::vector<ParkPoint> pathCrossings(const World &world, const DeletePoint &del);

// Each segment at the moved point, in ascending key order, as it would lie after the move and
// before any split: its end at the point is at the target, and an OnPoint target's key replaces
// the point's. Leaves out a segment between the point and an OnPoint target. Empty when the move
// names no path point or its target has no position. Given whether or not the world accepts the
// move.
std::vector<ParkSegment> movedSegments(const World &world, const MovePoint &move);
// The segment the deletion would merge the point's two segments into, before any split: the
// lower-keyed one, its end at the point moved to the other's far end, with the wider width. None
// when the point doesn't have exactly two segments with different far ends. Given whether or not
// the world accepts the deletion.
std::optional<ParkSegment> mergedSegment(const World &world, const DeletePoint &del);

bool isAccepted(const World &world, const MovePoint &command);
bool isAccepted(const World &world, const InsertPoint &command);
bool isAccepted(const World &world, const DeletePoint &command);

void applyCommand(World &world, const MovePoint &command);
void applyCommand(World &world, const InsertPoint &command);
void applyCommand(World &world, const DeletePoint &command);
```

Public interface, in src/tools/tools.h:

```cpp
enum class ToolKind : uint8_t {
  None,
  GuestPath,
  BackstagePath,
  PlaceShop,
  PlaceDepot,
  MoveBox,
  EditPath,
  Delete
};

// A press or hover this near a path point picks it, in meters.
inline constexpr double POINT_REACH = 2.0;

// In ToolState:
  // Whether the pointer has met the ground anywhere new since the press took hold.
  bool Dragged = false;
  // Holding with EditPath on a point: Held is the point, and PointTo where the drag would put it
  // before snapping. GrabOffset is the offset of its position from the pointer at the press.
  ParkPoint PointTo;
  // Holding with EditPath on a segment: the point the release inserts.
  std::optional<InsertPoint> Inserting;

// The path point nearest the point within POINT_REACH, of either kind. Ties go to the lower key.
std::optional<EntityKey> pointAt(const World &world, ParkPoint point);
// Where a dragged path point at the place moves to: snapStop's target for a path of the point's
// kind with no stops drawn, leaving out the point and every segment at it, or a new point at the
// place when no segment uses the point.
PointTarget snapPoint(const World &world, EntityKey point, ParkPoint at);
```

## Files affected

- Modify: src/sim/park/SPEC.md
- Modify: src/sim/park/edits.h
- Modify: src/sim/park/edits.cpp
- Modify: src/tools/SPEC.md
- Modify: src/tools/tools.h
- Modify: src/tools/tools.cpp
- Modify: src/render/SPEC.md
- Modify: src/render/park_mesh.h
- Modify: src/render/park_mesh.cpp
- Modify: src/app/SPEC.md
- Modify: src/app/ui/tool_panel.cpp
- Modify: src/scenarios/park_edits.cpp
- Modify: tests/sim/park_world_checks_test.cpp (test pass)

## Dependencies

- path-network/crossing-splits: meetingOf and AddPath's plan of points, splits, and crossings. Merged.
- shared-medium/carrier-widths and believable-guests' carried guests: carry-over keeps guests on a reshaped, split, or merged segment where they stand, and turns a guest carried onto a segment that runs the other way. Complete.

## Out of scope

- Width and radius. A merge takes the wider width, but every segment has its kind's default width until corners-and-widths adds SetSegmentWidth, so the play checks can't see it.
- Handles drawn at every path point while Edit path is chosen. The hover highlight shows a point when the pointer reaches it. This is a deepening candidate in MILESTONE.md.
- Dragging from a segment to insert a point and move it in one gesture. A click inserts, and a second drag moves. This is a deepening candidate in MILESTONE.md.
- Joining segments that come to join the same two points. AddPath can already make such a pair, and routes reads them as two carriers. If a move makes one, both stay. This is a deepening candidate in MILESTONE.md.
- Deleting a segment shorter than about 2.6 m on its own. Delete picks a point within POINT_REACH before a segment, so every place over such a segment picks one of its points instead. It can still be removed by deleting a point, or by dragging a point to lengthen it first.
- Snaps to directions, lengths, and doors, and the suppress key. snaps-and-guides adds them to point dragging.
- Undo. undo-redo adds it.

## Open questions

None.
