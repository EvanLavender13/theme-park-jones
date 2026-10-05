# Feature: Crossing Splits

## Summary

When the player draws a path across a path of its kind, or across itself, the two now join where they cross. Both split there and share a new point, so deleting one arm of a crossing removes only that arm. The same happens where a drawn segment's end lies on another segment, or passes through another segment's end, without a click there. AddPath finds these places itself, so every caller of AddPath gets them: the tools, the scenarios' builders and random edits, and the tests. The ghost marks each crossing before commit, as it marks each snapped stop. Paths of different kinds still never join. The full stress park is made again, so its grid's crossings share points.

## Acceptance criteria

1. In the app's new park, pick Guest path. Click about 6 m to one side of the middle of the template path, then 6 m to the other side, and click there again to finish. Before the finishing click, the ghost marks the crossing. After the commit, pick Delete and hover each of the four arms in turn: the ghost tints only the arm under the pointer. (play)
2. Pick Guest path and click four new points, away from other paths: A, then B about 10 m from A, then C about 5 m to one side of the middle of AB, then D about 5 m to the other side, opposite C. Click D again to finish. Before the finishing click, the ghost marks where CD crosses AB. After the commit, pick Delete and hover AB near A: the ghost tints only the part from A to the crossing. (play)
3. Pick Backstage path and draw a path across a guest path. The ghost shows no mark at the crossing. After the commit, Delete hovering the guest path tints the whole guest segment, unsplit. (play)
4. Open tests/parks/warm.park with `--park`. Pick Guest path and draw a path straight across a segment guests are walking along, away from its ends. No guest on that segment jumps or turns around when the path commits. (play, the milestone's criterion 4)
5. The remade tests/parks/stress/full.park holds its grid as split segments that share a point at every crossing: 222 guest segments, 11 backstage segments, and 154 path points. (check: PLAN.md Task 9)
6. `ctest.exe --preset windows-debug` passes. This includes the park world checks over the chosen edit in Tests, a guest path crossing a segment guests stand on. (check: PLAN.md Task 10)

## Tests

The feature adds no intent type, so the park world checks cover it once a chosen edit makes a crossing. The milestone asks for that edit:

- In tests/sim/park_world_checks_test.cpp, add a chosen edit of warm.park. Setup: the world warm.park reaches after one cycle, as the other chosen edits use. Find the guest segment that a guest heading to the shop stands on, as addingPathEndingOnGuestSegment does. Change: an AddPath of guest kind with two NewPoint stops, each 6 m from the middle of that segment, square to it, one on each side. Require that the world accepts it. Expected: the existing checks hold for it. Its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name it for what it makes the capabilities do: a guest path crossing the middle of a segment guests stand on, which splits the segment under them.

## Medium

The feature adds no fields or flows. It changes what AddPath does to park intent, which decision 0025 places outside the medium. routes reads the split segments through parkSegments, as it reads any segments. Guests on a split segment are carried onto its parts by carry-over, as for an OnSegment split.

## Spec changes

src/sim/park/SPEC.md:

- In Boundary, the list of public names gains pathCrossings.
- In Intent, "Segments share a point where the player joined them. Routes also joins segments of one kind where they cross (sim/routes/SPEC.md)." becomes "Segments share a point where the player joined them, and where a path was drawn across them. Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md)."
- In Commands, after the list of what accepted commands leave, a new paragraph and list:

  > Where a segment AddPath adds meets another segment of its kind, the two join. The other segment can be one already in the park or another one the path adds.
  >
  > - If each end of each segment lies within MIN_SEGMENT_LENGTH of the line through the other, the segments lie along one line. They don't meet, and neither splits.
  > - Otherwise, if an end of one segment lies within MIN_SEGMENT_LENGTH of the other segment, and farther than MIN_SEGMENT_LENGTH from both of the other's ends, the other segment splits at that end's point.
  > - Otherwise, if the segments cross, AddPath adds a point at the crossing, with DEFAULT_CORNER_RADIUS, and both split there.
  >
  > If they meet within MIN_SEGMENT_LENGTH of an end of each, nothing splits. A segment that meets several others splits at each of their points, in order along it, together with any OnSegment or OnDrawnSegment stops on it, as described above. pathCrossings gives the ground position of each point where AddPath splits a segment this way, other than at an OnSegment or OnDrawnSegment stop. It gives none if the world refuses the path.

src/tools/SPEC.md, in Tools, the Guest path and Backstage path paragraph:

- After "a segment it joins splits there when the path commits." add "Where the path crosses a segment of its kind, or itself, both split and share a point there when it commits."
- "and marks each stop that joins a point or segment" becomes "and marks each stop that joins a point or segment, and each place where the path crosses one."

src/render/SPEC.md, in Ghosts and highlights, the second item of buildGhostMesh's list becomes "a mark at each stop where an added path joins a point or segment, and at each place pathCrossings gives, in JOIN_MARK_COLOR".

Public interface, in src/sim/park/edits.h:

```cpp
// The ground position of each point where the path splits a segment of its kind, or one of its
// own segments, because the two meet: a new point where they cross, or an end of one lying on the
// other. Leaves out the points of OnSegment and OnDrawnSegment stops. In the order the meetings
// are found. Empty when the world refuses the path.
std::vector<ParkPoint> pathCrossings(const World &world, const AddPath &path);
```

## Files affected

- Modify: src/sim/park/SPEC.md
- Modify: src/sim/park/edits.h
- Modify: src/sim/park/edits.cpp
- Create: src/sim/park/internal/meetings.h
- Create: src/sim/park/meetings.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: src/tools/SPEC.md
- Modify: src/render/SPEC.md
- Modify: src/render/park_mesh.h
- Modify: src/render/park_mesh.cpp
- Modify: tests/parks/stress/full.park
- Modify: tests/sim/park_world_checks_test.cpp (test pass)

## Dependencies

- path-network/points-and-segments: AddPath, its stops, and splitting at OnSegment and OnDrawnSegment stops. Merged.
- shared-medium/carrier-widths: carry-over keeps guests on a split segment where they stand. Complete.

## Out of scope

- Moving a point across a segment. point-editing reuses these rules for MovePoint.
- Crossings already in a park's intent. A hand-written save can still hold segments of one kind that cross without sharing a point. routes still joins them by their geometry until wide-routes. Once full.park is made again, the only checked-in park holding one is degenerate-intent.park, whose intent is broken on purpose.
- Joining two segments whose ends lie within MIN_SEGMENT_LENGTH of each other without sharing a point. The tools snap such ends onto one point already.
- Merging crossings closer together than MIN_SEGMENT_LENGTH on one segment. If three segments cross at one spot without a shared point, the parts between the crossings are too short, and the world refuses the path.

## Open questions

None.
