# Feature: Snaps and Guides

## Summary

While the player draws a path or drags a path point, the pointer snaps, and every snap shows why it moved. It snaps to:

- path points and segments of the path's kind;
- the doors that kind serves;
- crossings of guides, and where a guide crosses a segment;
- direction guides straight on, at 45°, and at 90° from the segments at the point the new segment starts from;
- whole-meter lengths along a direction guide.

A press anywhere on a segment's paving joins that segment, not only near its center line. A door snap ends the path just clear of the box, straight out from the door, so the door connects to it. Reach is measured in screen pixels, so a snap catches from the same distance on screen at any zoom: 10 pixels to catch and 14 to let go. Guides draw over the scene in a color for each kind, and the length of the segment being drawn or reshaped shows at its middle to two decimals. Holding Ctrl turns every snap off. Snaps are tool state only: nothing is saved, and the simulation doesn't change.

## Acceptance criteria

1. In a new park, pick Guest path and click a stop on the ground away from the entrance path. Move the pointer around it by hand. The ghost's next stop jumps onto rays straight along the world's axes and their diagonals, and a dashed guide shows the ray it is on. Click to add the stop. Now the guides run from the new stop straight on, at 45°, and at 90° from the segment just drawn. Moving along a guide, the stop steps to whole meters, the guide shows a tick at each meter, and the readout at the segment's middle shows the length to two decimals, such as 12.00 m. Away from every guide, the stop follows the pointer, and the readout shows lengths such as 12.37 m. (play)
2. Draw three sides of a rectangle by hand, using the 90° guides. Move the pointer near where the fourth corner belongs. The stop snaps to where the 90° guide from the last stop crosses the 90° guide from the first stop, and both guides show. Clicking there and then on the first stop closes a rectangle with square corners. (play)
3. Start a path on a point of the entrance path, then move toward another guest path. Where a guide crosses that path, the stop snaps to the crossing, and both the guide and that path's center line show. Clicking there joins the path, and the join mark shows before commit. Widen a segment to about 12 m and click anywhere on its paving, well away from its center line. The path joins that segment at the nearest place on its center line. (play)
4. Place a shop and draw a guest path toward its front door. Near the door, the stop snaps straight out from the door, a door guide shows from the door to the stop, and the ghost shows the path ending just clear of the shop with a walkway from the door to it. Committing is accepted. Backstage path snaps to a shop's back door and a depot's front door, and Guest path snaps to neither of those. Place a shop with its front door close beside a guest path, and click that path's paving in front of the door with Guest path. The stop joins the path, and the shop keeps its walkway to it. (play)
5. With Edit path, drag a path point. Guides run from the other end of each segment at it, crossings snap, and each moved segment's length shows. Dropped on another path's paving, it joins that path, as it did near the center line before. Dragging the free end of a path along a guide from its other end snaps it to whole-meter lengths. (play)
6. Zoom far out and close in. Each time, a snap catches when the pointer comes within about the same distance on screen, and once caught, it holds until the pointer moves a little farther. (play)
7. Hold Ctrl while drawing or dragging. Nothing snaps, not even to points or segments, and no guide shows. The stop or point lands exactly at the pointer, and the length readout still shows. Release Ctrl, and snapping resumes without moving the mouse. (play)
8. The simulation is unchanged: `tpj_scenarios --compare` of this feature's output against main's exits with status 0. (scenarios comparison)
9. In the windows-release build, open tests/parks/stress/full.park, pick Guest path, click one stop, and move the pointer around. The Debug panel's frame time matches main's with the same steps. (play)

## Tests

None. The feature adds no state and fixes no bug. Snaps are tool state, which docs/testing.md leaves to play.

## Medium

The feature adds no fields or flows, and it adds no intent.

- Snap state: the snap in effect and what it holds to, kept in the tools' ToolState. It is never saved (principle 1).
- Doors: routes publishes the doors serving each kind through a new public query, parkDoors, which the connectors already derive from intent. The tools read it to snap to doors, so the tools and routes agree on where doors are and which kind each one serves.
- Published points, segments, and corners: the tools read them through sim/park's public queries, as before.
- The pointer's scale: the app measures how many meters of ground a screen pixel spans at the pointer and gives it to the tools with the pointer's position. The tools stay windowless (principle 10).

## Spec changes

src/sim/routes/SPEC.md:

- Boundary: "Other modules read both through parkNetwork, previousNetwork, the Network type's queries, sampleField, and routeEntryAt", becomes "Other modules read both through parkNetwork, previousNetwork, the Network type's queries, sampleField, and routeEntryAt, and the doors connectors start from through parkDoors".
- Connectors, after the list of doors: "parkDoors gives the doors serving a kind: the entrances' first, then the boxes', each in key order of its entity. A ParkDoor has its entity, its face, its position, and the unit direction out of its face."

src/tools/SPEC.md:

- Boundary: "The app drives it with four calls" stays. "boxAt, segmentAt, pointAt, handleAt, sideAt, snapStop, and snapPoint are the picking queries the tools use" becomes "boxAt, segmentAt, pointAt, handleAt, and sideAt are the picking queries the tools use". Add: "snapGuides and measuredLengths give what the app draws for snapping."
- Input: "movePointer records where the pointer meets the ground, or that it meets none, as when it is over a panel" becomes "movePointer records where the pointer meets the ground, or that it meets none, as when it is over a panel, how many meters of ground a screen pixel spans there, and whether snapping is off. It then finds the snap in effect (Snapping)."
- Picking: delete the two paragraphs that start "snapStop gives the stop" and "snapPoint gives the target".
- Add a section "Snapping" after Picking:

  While a path tool is chosen, the pointer snaps to give the next stop, or the first. While Edit path drags a point, it snaps to give where the point lands. The tool finds the snap whenever the pointer moves and whenever a press or release changes what the tool holds. snapGuides gives the guides of the snap in effect, and measuredLengths gives the segments whose lengths the ghost shows.

  Reach is in screen pixels. A snap catches the pointer within SNAP_CATCH_PIXELS of it. Once it has caught the pointer, it holds until the pointer moves more than SNAP_RELEASE_PIXELS from it. A press or a release ends every hold. While snapping is off, nothing snaps: the stop or point lands at the pointer, and no guide shows.

  The targets come in four groups. A target in an earlier group beats every target in a later one, and within a group the nearest wins.

  1. Points: a path point of the kind, a point an earlier stop of the path being drawn makes, other than the last stop, and a door the kind serves (parkDoors). A door's target lies straight out from the door, half the path's width plus DOOR_CLEARANCE away, so the path ends just clear of the box. A dragged point uses the width of its widest segment. The pointer's distance to a door's target is measured to the line from the door to it. A door has no target when its target lies on the paving of a segment of the kind, within half the segment's width of its ground line, because a path already reaches that door. The segments that move with a dragged point don't count.
  2. Crossings: where two guides from different anchors cross, and where a guide crosses the straight part of a segment of the kind.
  3. Segments: the straight part of a segment of the kind, the arc of a rounded corner of the kind, and a segment of the path being drawn. Each catches the pointer anywhere within half its width or within reach, whichever is farther, so a press anywhere on its paving joins it. The stop is the nearest place on its ground line, or its end point when that place lies within MIN_SEGMENT_LENGTH of it.
  4. Guides: the place on a guide nearest the pointer.

  Guides are rays from anchors. While a path is drawn, its last stop is an anchor, and so is its first stop once two stops are drawn. While a point is dragged, the other end of each segment at it is an anchor. From each anchor, guides run straight on, at 45°, at 90°, and at 135° from the direction each segment arrives at it. That includes the segments the anchor ends, the segment or arc it lies on, both ways, and the drawn segments at it, but not the segments that move with a dragged point. If no segment arrives at an anchor, its guides run along the world's axes and their diagonals.

  When the stop lands on a guide, it can also snap to a length. If its distance from the guide's anchor lies within reach of a whole number of meters, at least 1, it moves along the guide to that whole length.

  Some targets are left out. A path never snaps onto a point or segment of the other kind. A path's next stop never snaps onto a segment that its last stop lies on or ends at, and the path's last stop is left out because a press there finishes the path. A dragged point never snaps onto itself, a segment at it, or a corner with a segment at it.

  Each snap in effect shows a guide. A point snap marks the point, and a door snap shows the line from the door to the target. A segment snap shows the segment's straight part, or marks the place on an arc. A guide snap shows the ray from its anchor through the stop, and a crossing shows both of its lines. A length snap shows a tick at each whole meter from its anchor to the stop.

  measuredLengths gives a segment from the last stop of the path being drawn to where its next stop would land, while a press would not finish the path. While a point is dragged, it gives a segment from the other end of each segment at the point to where the point would land. Otherwise it gives none.

- Tools: "Each click adds the stop snapStop gives" becomes "Each click adds the stop the snap gives (Snapping)". "A click near a point or segment of the same kind joins the path there" becomes "A click that snaps to a point or segment of the same kind joins the path there".
- Edit path: "Dragging a point moves it, and where it lands snaps as snapPoint gives, so a point dropped on a point or segment of its kind joins it there" becomes "Dragging a point moves it, and where it lands snaps (Snapping), so a point dropped on a point or segment of its kind, or on a segment's paving, joins it there".

src/app/SPEC.md:

- Tools and the ghost, after the paragraph on the ghost: "While the tool snaps, the guides of the snap in effect draw over the scene and behind the panels, each kind in its own color: a ring on a point, a line from a door, a solid line along a segment, a dashed ray from an anchor that runs on past the stop, and ticks at whole meters. The length of each segment measuredLengths gives shows at its middle, in meters to two decimals. A frame gives the tool the meters of ground a screen pixel spans at the pointer, averaged across and up the screen. Holding Ctrl turns snapping off while it is held, unless a text field has the keyboard."

## Files affected

- Modify: `src/sim/routes/networks.h`, `src/sim/routes/networks.cpp`, `src/sim/routes/SPEC.md`
- Create: `src/tools/internal/snapping.h`, `src/tools/snapping.cpp`
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`, `src/tools/CMakeLists.txt`, `src/tools/SPEC.md`
- Modify: `src/app/input/cursor.h`, `src/app/input/cursor.cpp`, `src/app/input/input_map.h`, `src/app/input/input_map.cpp`, `src/app/input/platform_input.cpp`
- Modify: `src/app/input/interaction.h`, `src/app/input/interaction.cpp`, `src/app/application.cpp`
- Create: `src/app/ui/snap_guides.h`, `src/app/ui/snap_guides.cpp`
- Modify: `src/app/ui/tooling_ui.cpp`, `src/app/CMakeLists.txt`, `src/app/SPEC.md`

## Dependencies

point-editing, merged. Doors come from routes' connectors, which exist.

## Out of scope

Each of these is in the milestone's deepening candidates or goes to them:

- Guides from the ends of nearby paths, as Cities: Skylines 2 and Precision Engineering draw them. Players turn them off because they pull roads out of line.
- Locking a direction with a held key, and typing an exact length. Both are already candidates.
- Whole-meter lengths away from every guide. The slice research treats a length snap as a refinement once the direction is fixed. At the default zoom, the reach is wider than half a meter, so a length snap off the guides would round every free stop.
- Snapping a handle drag, such as a width or a radius, to whole numbers.
- Reach in pixels for picking points and handles in Edit path, and for finishing a path. They stay in meters.

## Open questions

None.
