# Feature: Frontage Snap

## Summary

While the player places a shop or a depot, the box fronts a nearby segment of the kind its front door serves. A shop fronts guest segments, and a depot fronts backstage segments. The box turns to face the segment, square to its ground line at the nearest place on it, and the middle of its front face sits FRONTAGE_SETBACK, 1 m, beyond the segment's paving. A segment is near when the pointer lies over the box that fronts it, or between that box and the segment. The box keeps the pointer's place along the segment, and the pointer's side of it picks the side. Rounded corners count, because their arcs are part of the ground line, and the box follows the curve past the arc's middle, where the corner's two segments meet. A box pressed down while it fronts a segment keeps that pose through the drag. Holding Ctrl turns frontage off, as it turns off every snap. Frontage is tool state only: nothing is saved, and the simulation doesn't change.

## Acceptance criteria

1. In a new park, pick Place shop and move the pointer by hand toward the entrance path from the side. Once the pointer comes within about the shop's depth beyond where the shop would stand, the ghost turns to face the path, with its front face 1 m clear of the paving and a walkway from its front door to the path. Moving the pointer along the path slides the shop along it, still square to it. Crossing to the other side turns the shop to face the path from there. Moving the pointer onto the paving keeps the shop beside it, on the pointer's side. Moving the pointer well away, the shop follows the pointer and faces the way the last placed box faced. Clicking while it fronts the path places it as the ghost showed, and the placed shop has its walkway. (play)
2. Draw a backstage path. Place depot fronts it as the shop fronts the guest path, and its front door gets a walkway. Place depot doesn't front the guest path, and Place shop doesn't front the backstage path. (play)
3. Draw a guest path with a bend, and round the bend with Edit path's radius handle. Move Place shop by hand along the outside of the curve, from one straight part to the other. The shop faces the curve all the way round, square to it where the pointer lies along it, and it never drops back to the pointer at the arc's middle. On the inside of a gentle curve, it faces the curve the same way. On the inside of a curve tighter than the shop is wide, it faces the curve too, but the ghost shows invalid where its front corners reach the paving. Widen a straight segment to about 12 m with its side handle. A shop placed beside it sits with its front face 1 m from the wide paving's edge. (play)
4. Press while the ghost fronts a path, and drag in any direction before releasing. The shop doesn't turn, and it lands where the ghost showed before the press. Hold Ctrl while moving near the path. The shop follows the pointer and faces the way the last placed box faced, and pressing and dragging turns it to face the drag, as before this feature. Release Ctrl, and the shop fronts the path again without moving the mouse. (play)
5. The simulation is unchanged: `tpj_scenarios --compare` of this feature's output against main's exits with status 0. (scenarios comparison)
6. In the windows-release build, open tests/parks/stress/full.park, pick Place shop, and move the pointer beside a guest path. The Debug panel's frame time matches main's with the same steps. (play)

## Tests

None. The feature adds no state and fixes no bug. Frontage is tool state, which docs/testing.md leaves to play. The slice's integration test of frontage on the widest segment can pose its shop with frontagePose.

## Medium

The feature adds no fields or flows, and it adds no intent.

- Published segments: the tools read each segment's kind, width, and ground line through sim/park's parkSegments, as the milestone's Medium says: center lines and widths, not the drawn surface.
- Box sizes and doors: the tools read a box's footprint size from sim/park's boxSize. Which kind each box's front door serves follows routes' rule for doors, which src/sim/routes/SPEC.md states: a shop's front serves guests, and a depot's front serves backstage.
- Frontage state: the pose of the fronting box, kept in the tools' ToolState. It is never saved (principle 1).

## Spec changes

src/tools/SPEC.md:

- Boundary: after "boxAt, segmentAt, pointAt, handleAt, and sideAt are the picking queries the tools use.", add "frontagePose gives the pose of a box that fronts a segment."
- Tools: replace the paragraph "Place shop and Place depot place a box at the pointer. Pressing puts the box down, and dragging before release turns it to face the drag. The next box faces the same way as the last." with these two paragraphs:

  Place shop and Place depot place a box at the pointer. Pressing puts the box down, and dragging before release turns it to face the drag. The next box faces the same way as the last.

  Near a segment of the kind its front door serves, the box fronts that segment instead, in the pose frontagePose gives. A shop's front door serves guest segments, and a depot's serves backstage segments. A fronting box faces the segment's ground line, square to it at the place on it nearest the pointer, from the side the pointer is on. The middle of its front face lies FRONTAGE_SETBACK beyond the segment's paving, which reaches half the segment's width from its ground line. On the inside of a tight curve, the ends of the front face can reach the paving, and the world then refuses the box. A segment is near when the pointer lies within half its width, plus FRONTAGE_SETBACK, plus the box's depth of its ground line, and the nearest place on the ground line is not one of its ends, unless that end is the middle of a rounded corner's arc. If several segments are near, the box fronts the one whose paving lies nearest the pointer, or the one with the lower key on a tie. If the box fronts a segment when the button goes down, dragging doesn't turn it. While snapping is off, no box fronts a segment.

## Files affected

- Create: `src/tools/frontage.cpp`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/CMakeLists.txt`, `src/tools/SPEC.md`

## Dependencies

corners-and-widths, merged: segment widths and the arcs in ground lines. snaps-and-guides, merged: the pointer sample's SnapsOff, which Ctrl sets.

## Out of scope

- Frontage when moving a box. It is already a milestone candidate.
- A setback the player chooses, and placing rows of shops lined up with each other. plans/BACKLOG.md already holds both, in its Platter item.
- A guide for frontage. The turned ghost and its walkway show the snap.
- Frontage around a path's free end, the outside of a sharp corner, or a junction of three or more segments. The box would swing around the end point.
- Pushing a box on the inside of a tight curve out until its whole front face clears the paving. It goes to the milestone's candidates.
- A connector for a box fronting a segment wider than 6 m. Its door then lies beyond CONNECTION_REACH of the center line. wide-routes measures reach from the paving's edge, as the milestone says.

## Open questions

None.
