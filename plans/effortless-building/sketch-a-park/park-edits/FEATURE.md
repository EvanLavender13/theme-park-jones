# Feature: Park Edits

## Summary

park-edits lets intent change. It adds five commands to src/sim/park: add a path of a kind through points, add a box of a kind at a pose, move a box to a pose, and delete a path or a box. They are registered in makeParkSchema, so tools submit them to the command queue and they apply between ticks. The physical-validity check is their only refusal (principle 5): a command that describes no physical object, a footprint that overlaps another or leaves the park's square, and a ground line that comes within half its width of a footprint or leaves the square. isAccepted answers for a command on a world, and applying a command calls it first, so the answer a ghost shows is the commit's outcome by construction (principle 8). isPhysicallyValid checks a whole world the same way, which states the template's validity and gives the randomized command-sequence tests their invariant. Every conflict is judged with a 1 mm contact tolerance, so objects that only touch are accepted whatever rounding their facings bring. A new scenario, park-edits, queues drawn commands from the template, so the check's answers are compared across builds.

## Acceptance criteria

1. addParkEdits registers AddPath, AddBox, MoveBox, DeletePath, and DeleteBox as commands, and makeParkSchema calls it after addParkIntent. A command applied by applyCommand, queued for a cycle, or given to makeCandidate is applied exactly when isAccepted is true for it on the world at that moment. An applied command leaves intent exactly as src/sim/park/SPEC.md's Commands section describes, with nothing else changed, so a MoveBox to the box's own pose is applied and leaves the world equal. An added path holds keptPoints of the command's points, which are exactly the points groundLine keeps from them. A refused command applied by applyCommand leaves the world equal to what it was, the next key included.
2. A command that describes no physical object is refused, and isAccepted never throws for any command on any world. Such a command is one whose kind is not a value of its enum, an AddPath with a coordinate that is not finite or fewer than two kept points, an AddBox or MoveBox whose pose has a component that is not finite or a facing of zero length, and a MoveBox, DeletePath, or DeleteBox whose key holds no intent of the kind it names, as with a missing key, a deleted one, or the entrance's.
3. Conflicts are physical, at every facing. Two footprints overlap whenever some point of one lies at least 2 × CONTACT_TOLERANCE inside the other, and never when their interiors are disjoint. A ground line meets a footprint whenever some point on its segments lies nearer the footprint's rectangle than half the path's width less 2 × CONTACT_TOLERANCE, and never when every point on it is at least half the path's width away. A footprint leaves the park exactly when a corner footprintOf gives has an x or z of magnitude above PARK_SIZE / 2 + CONTACT_TOLERANCE, and a ground line exactly when a point groundLine gives has one above (PARK_SIZE / 2 - half the path's width) + CONTACT_TOLERANCE.
4. For a physically valid world and a command holding no NaN, isAccepted is true exactly when the command names intent of the kind it acts on, where it names any, and the world it describes is physically valid. Everything that passes is accepted, including a box far from any path and a path crossing another (principle 5).
5. makeNewPark's world is physically valid.
6. Randomized sequences of commands of every kind, including ones that name missing, deleted, or wrong-kind keys, ones that describe no physical object, and ones the check refuses, queued one or several to a cycle from makeNewPark, leave after every cycle a world that is physically valid, whose copy is equal to it, and whose save loads back equal and saves again to identical text (principle 2).
7. tpj_scenarios runs the park-edits scenario as src/scenarios/SPEC.md describes. Across its run it queues commands of every kind, some adds and moves are accepted and some refused, and its world is physically valid after every cycle.

## Medium

None. Park intent and its commands sit outside principle 3 (decision 0025). Tools, in box-tools and path-tool, build these commands and call isAccepted for their ghosts. legible-simulation passes the same commands to makeCandidate. navigable-networks, plausible-operations, and believable-guests derive from the intent they leave. This feature reads nothing from any of them.

## Principle checks

- Principle 1: a command stores only kind, kept points, and a pose exactly as given. Nothing it derives is saved: a save after any sequence loads back equal and saves identically (criterion 6).
- Principle 2: every command, whatever it names or describes, leaves a legitimate world, and from the template a physically valid one (criteria 2 and 6).
- Principle 5: refusals are exactly the physical conflicts, tested both ways: those refused, with the tolerance band as the only gray zone, and everything else accepted (criteria 3 and 4).
- Principle 8: isAccepted on the world a command is applied to is its outcome, directly, through a cycle, and in a candidate (criterion 1).
- Principle 10: commands change intent only between ticks, through the cycle. The check uses only basic operations, abs, and sqrt, so the symbol check keeps passing, and the park-edits scenario puts its answers under the cross-build check (criterion 7).

## Spec changes

src/sim/SPEC.md: in the paragraph on registration, replace "then park intent (sim/park/SPEC.md)." with "then park intent and its commands (sim/park/SPEC.md)."

src/scenarios/SPEC.md: in the Contract's second paragraph, after the sentence that ends "so tests can audit its world.", add:

```markdown
park-edits makes its world with makeParkSchema and fills it with makeNewPark's template. Before every tenth cycle it queues one park command, of a kind and with values drawn by keyed draws: paths of two to five points, some repeated, boxes and moves with coordinates reaching 8 m past the park's edge and facings of any direction, and moves and deletes naming any key the counter has given. So the physical-validity check's answers are compared across builds.
```

src/sim/park/SPEC.md: in Ground lines, after "With fewer than two kept points the line is empty.", add "keptPoints gives the points it keeps." In The new park, after "holding exactly two entities.", add "It is physically valid." Between Footprints and The new park, add:

```markdown
## Physical validity

Physical validity is the only refusal (principle 5). The check compares solids, the footprint of each entrance and each box, from its pose and ENTRANCE_SIZE or its kind's boxSize, and lines, the ground line of each path with half its kind's pathWidth. Every comparison allows CONTACT_TOLERANCE, 1 mm, so objects that only touch never conflict, whatever rounding their facings bring.

- A solid leaves the park when a corner has an x or z of magnitude above PARK_SIZE / 2 + CONTACT_TOLERANCE.
- A line leaves the park when one of its points has an x or z of magnitude above (PARK_SIZE / 2 - its half width) + CONTACT_TOLERANCE. The line is straight between its points and the square is convex, so this is the line widened by its half width leaving the square.
- Two solids overlap when, on each of four axes, the Forward and Right of each, the intervals their corners project onto share more than CONTACT_TOLERANCE: the lesser of the two maxima minus the greater of the two minima exceeds it. A corner projects onto an axis as x * axis.X + z * axis.Z.
- A line meets a solid when some segment between consecutive points of it lies nearer the solid's rectangle than its half width less CONTACT_TOLERANCE. The distance is measured in the rectangle's own frame, along Forward and Right from the pose's position: it is zero when the segment enters the rectangle, and otherwise the least of each end's distance to the rectangle and each corner's distance to the segment.

Paths never conflict with each other: where they cross, navigable-networks derives a junction. A world is physically valid when every entrance and box has a footprint and every path a non-empty ground line, no solid or line leaves the park, no two solids overlap, and no line meets a solid. isPhysicallyValid gives whether it is.

## Commands

Five commands change intent: AddPath, a kind and points; AddBox, a kind and a pose; MoveBox, a box's key and a pose; DeletePath, a path's key; and DeleteBox, a box's key. addParkEdits registers them in that order, and makeParkSchema calls it after addParkIntent. Tools submit them to the command queue, so they apply between ticks (sim/SPEC.md).

isAccepted gives whether a command would be applied to a world. applyCommand calls it on the world it is given and changes nothing when it is false, so what a ghost shows as valid is what its commit does, as long as nothing else changes intent first.

- AddPath is accepted when the ground line of its kept points is not empty, does not leave the park, and meets no solid. It creates an entity from the key counter holding a path of its kind through its kept points. A path with a point that is not finite or outside the square, or fewer than two kept points, is refused.
- AddBox is accepted when its pose has a footprint for its kind's size, and that footprint does not leave the park, overlaps no solid, and meets no line. It creates an entity from the key counter holding a box of its kind with its pose exactly as given.
- MoveBox is accepted when its key holds a box, and the pose, with that box's kind, would be accepted as an AddBox by the world without that box. It replaces the box's pose with the one given, exactly, keeping its key and kind.
- DeletePath is accepted when its key holds a path, and DeleteBox when its key holds a box. Each destroys that entity. No command deletes or moves the entrance.

An AddPath or AddBox whose kind is not a value of its enum describes nothing, and is refused. A box or entrance with no footprint, or a path with an empty ground line, which only a hand-written save can hold, is no solid or line, so it blocks nothing. For a physically valid world, a command is therefore accepted exactly when it names what it acts on and the world it describes is physically valid.
```

Public interface, src/sim/park/edits.h:

```cpp
// Conflicts no deeper than this are contact, not overlap.
inline constexpr double CONTACT_TOLERANCE = 0.001;

// A path of the kind through the points, which the path keeps as keptPoints gives them.
struct AddPath {
  PathKind Kind = PathKind::Guest;
  std::vector<ParkPoint> Points;
};

// A box of the kind at the pose, held exactly as given.
struct AddBox {
  BoxKind Kind = BoxKind::Shop;
  Pose At;
};

// The box's new pose, held exactly as given.
struct MoveBox {
  EntityKey Box = NULL_KEY;
  Pose At;
};

struct DeletePath {
  EntityKey Path = NULL_KEY;
};

struct DeleteBox {
  EntityKey Box = NULL_KEY;
};

// Registers AddPath, AddBox, MoveBox, DeletePath, and DeleteBox, in that order.
void addParkEdits(WorldSchema &schema);

// Whether the command would be applied to the world: it describes a physical object, names what it acts
// on, and conflicts with nothing. See sim/park/SPEC.md.
bool isAccepted(const World &world, const AddPath &command);
bool isAccepted(const World &world, const AddBox &command);
bool isAccepted(const World &world, const MoveBox &command);
bool isAccepted(const World &world, const DeletePath &command);
bool isAccepted(const World &world, const DeleteBox &command);

// Apply the command when isAccepted, and change nothing otherwise.
void applyCommand(World &world, const AddPath &command);
void applyCommand(World &world, const AddBox &command);
void applyCommand(World &world, const MoveBox &command);
void applyCommand(World &world, const DeletePath &command);
void applyCommand(World &world, const DeleteBox &command);

// Whether every solid and line has its geometry, stays in the park, and conflicts with no other.
bool isPhysicallyValid(const World &world);
```

src/sim/park/geometry.h gains:

```cpp
// The points in order, without any closer than MIN_POINT_SPACING to the last kept one.
std::vector<ParkPoint> keptPoints(const std::vector<ParkPoint> &points);
```

## Files affected

- Modify: src/sim/SPEC.md, src/sim/park/SPEC.md, src/scenarios/SPEC.md
- Create: src/sim/park/edits.h, src/sim/park/edits.cpp
- Modify: src/sim/park/geometry.h, src/sim/park/geometry.cpp
- Modify: src/sim/park_schema.cpp, src/sim/CMakeLists.txt
- Create: src/scenarios/park_edits.cpp
- Modify: src/scenarios/synthetic.h, src/scenarios/scenarios.cpp, src/scenarios/CMakeLists.txt
- Create, by the test pass: tests/sim/park/ and tests/scenarios/ test files
- Modify, by the test pass: tests/sim/CMakeLists.txt, tests/scenarios/CMakeLists.txt

## Dependencies

park-intent: the intent components, public queries, groundLine, footprintOf, sizes, and makeNewPark. deterministic-simulation's world-as-value: commands, the cycle, makeCandidate, saves, keyed draws, and the scenario runner. No other sibling feature.

## Out of scope

- The tools that build commands, their ghosts, and snapping: box-tools and path-tool.
- A refusal's reason, such as which box a ghost overlaps. Nothing reads it yet, and the ghost shows only valid or invalid.
- Moving or deleting the entrance, reshaping paths, and undo: capability deepening candidates.
- Caching ground lines between checks. Each check recomputes the lines it compares.

## Open questions

- Whether recomputing every path's ground line on each isAccepted call is fast enough for a ghost checked every frame. Resolved by box-tools' frame times in a park with many paths; the remedy is ground lines cached as derived data.
- Whether 1 mm of contact tolerance feels right. Resolved by placing boxes flush with box-tools.
