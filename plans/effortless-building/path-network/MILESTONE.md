# Milestone: Path Network

Slice: good-paths

Status: planned

## Summary

path-network replaces drawn curves with a network of shared points joined by straight segments. Each segment has its own width, and each bend has its own corner radius. The player draws paths point by point. A path joins another only where it ends on a point or segment at a visible snap, or where it crosses one of its own kind, and the ghost marks every join before commit. The player reshapes any path after drawing it. They can drag, insert, and delete points, drag a corner's radius, and drag a segment's side to set its width. Every snap shows a guide, and one held key turns them all off. Joined paths draw as one paved surface with an edge line along its outline only, and square ends. The player curves each edge where paths meet, inside or outside, with its own handle. A box is refused exactly where it would overlap that surface as drawn. Shops and depots placed beside a path face it at a fixed setback. Undo and redo cover every edit. This is the slice's second member. It lands after carrier-widths, whose carry-over keeps guests where they stand when a segment splits or merges under them. It lands before wide-routes, wide-overlay, and spread-guests, which read the segments it publishes.

## Acceptance criteria

1. Every park in tests/parks and tests/parks/stress holds points and segments, and so do the new-park template and the scenarios' park builders. tests/parks/main-street.park holds the slice scenario's park with no guests: the entrance, a 10 m main street, side paths with angled bends and a wide rounded corner, a loop crossing a side path, rows of shops along both sides of the street, a depot, and a backstage path behind one row. The park world checks pass over every park, and their chosen edits include each new edit command. food_loop_test passes on the rewritten warm.park. (ctest)
2. The guest and backstage path tools draw a path point by point. A point that ends on an existing point or segment of the same kind joins it, and the segment splits there. Where a new segment crosses a segment of its kind, both split and share a point at the crossing. The ghost marks every join before commit. Paths of different kinds never join. (play)
3. The edit tool moves a point, and every segment at it follows. If the moved point ends on another point or segment of its kind at a snap, it joins there: it merges into the point, or splits the segment. Clicking a segment inserts a point there. The delete tool removes the segment or box under the pointer and nothing else. The remove point tool removes a point with two segments and merges them into one segment with the wider width. A point left with no segments is removed. Each of these edits previews as a ghost before commit. (play)
4. Draw two segments in a straight line toward the point they share, so they run opposite ways. Wait for guests to walk along both, then remove the shared point with the remove point tool to merge them. No guest jumps or turns around. Then draw a path that ends on the middle of a segment guests walk on. No guest jumps. (play, carried from ground-carry-over)
5. Dragging a corner's radius handle rounds the bend into an arc, and dragging a segment's side sets its width, each within the limits in Tuning. A new bend gets the default radius. (play)
6. A capture of main-street.park shows the paths as one paved surface. The inside corners at junctions are sharp, or curved where the player dragged their handles. A thin edge line runs along the surface's outline and never across a join. Every free end is cut square. Where two segments of one kind overlap without sharing a point, each one's edge line shows across the other. (capture)
7. Placing a box where it would overlap the paved surface is refused, including against a rounded corner or the rounding at a junction. Placing it just clear of the drawn surface is accepted. (play)
8. While drawing or dragging a point, the pointer snaps to existing points, existing segments, the doors its kind serves, crossings of guides, straight-on and 90° and 45° directions, and whole-meter lengths. A path that snaps to a door ends just clear of the door's box, and the door connects to it there. Every snap in effect shows its guide. The ghost shows the segment's exact length. While the suppress key is held, nothing snaps and no guide shows. (play)
9. A shop or depot ghost near a segment of the kind its front door serves turns to face the segment, and its front face sits at the frontage setback from the paving's edge. On a path of default width, its front door then gets a connector. (play)
10. Undo reverses the last edit and redo applies it again, for every edit command. Undo and redo commit at once, with no ghost (decision 0029). Anything they restore gets a fresh key. Opening or starting a park clears the history. (play)

On wide segments, a door at the frontage setback lies beyond today's connection reach from the center line. wide-routes measures reach from the segment's edge, and the slice's integration test then checks that such a shop connects.

## Tuning

These are starting values. Drawing main-street.park adjusts them, and each feature that uses one states its final value.

- Default widths: guest 3 m and backstage 2 m, as today. Allowed widths: 1.5 m to 16 m for both kinds.
- Default corner radius: 0, a sharp corner. The player rounds a bend by dragging its radius handle. A radius can be anything from 0 up. If the radius is less than half the width, the paving's inner edge is sharp where it would fold back. The outer edge follows its own edge radius (edge-radii). If the arc's tangent lengths don't fit, the radius shrinks until each fits in what the segment's other end leaves free, or half the segment if both ends ask for more (predictable-editing).
- Default rounding of every edge besides a corner's inside: 0, sharp. The player curves each one with its handle (edge-radii). A bend's radius handle curves its inside edge and the line guests walk.
- Default taper length: 0, which gives each curve its natural length, twice how far it moves out. The player tapers it with its handle (width-tapers).
- Snap reach: 10 screen pixels to catch a snap and 14 to release it.
- Frontage setback: 1 m from the paving's edge to the box's front face.
- Suppress key: Ctrl, held.

## Medium

The milestone adds no fields or flows. It changes park intent, which decision 0025 places outside the medium, and the tools that edit it.

- Park intent: points and segments, authored through commands. A point has a ground position and a taper length, and a point with exactly two segments has a corner radius. A segment has a kind, its two points, a width, and an edge radius at each end. Features 1 to 4, 6, and 8 add the commands: add a path, move a point, insert a point, remove a point, delete a segment, set a segment's width, set a corner's radius, set an edge's radius, and set a taper length.
- Published segments: sim/park publishes each segment with its key, kind, width, ground line, and the keys of its two points, and each point with its key, position, and radius. They replace parkPaths. Segments that share a point have ground lines that meet there, at the middle of the arc for a rounded corner. Readers: routes (navigable-networks), legible's path places and preview, the renderer's park mesh, band overlay, and graph overlay, the app's scene sync, the tools, the scenarios, and the tests. Each reader keeps doing what it does today. Until wide-routes, routes builds one carrier per segment, with its kind's fixed path width, and still derives junctions from geometry.
- Paved surface: sim/park derives the surface's pieces from the published segments and points. The physical-validity check tests boxes against them, and the renderer draws them, so what is drawn and what is refused are one shape (feature 5). Nothing saves them.
- Tentative intent: every new command previews on a candidate world, as placements do today.
- Tools' session state: snaps, guides, and the undo history live in tpj_tools. None of it is intent, and none of it is saved (principle 1). The history holds each committed edit with its inverse. If undo restores something deleted, the restored thing gets a fresh key from the counter, so keys are never reused (src/sim/SPEC.md).
- Frontage: the place tools read published segments' center lines and widths, not the drawn surface.

## Dependencies

- shared-medium/carrier-widths: carrier widths, and carry-over that keeps a place on ground that survives under a new key. Complete.
- deterministic-simulation: candidate worlds for ghosts, and the command queue. Met.
- sim/park's geometry runs in the simulation, under decision 0022. The C runtime's trigonometry is not allowed there, so arcs and their tangent lengths are built from square roots and vector products alone.

## Core feature

`points-and-segments` is the core. It changes the intent model, the save format, and every reader of paths in one change, because old saves don't load and the park world checks read every park. After it, the player draws paths as points and segments that join where they snap, and deletes single segments. Every later feature builds on its intent and published segments.

## Features

1. `points-and-segments`: intent holds points and segments. Each segment has the default width of its kind, and each point has the default corner radius, which matters once a point has exactly two segments. Every bend in the rewritten parks gets the default radius, so feature 4 changes no save. The commands are AddPath and DeleteSegment. AddPath names each of its points as a new position, an existing point, or a place on an existing segment, which it splits. DeleteSegment removes points left with no segments. sim/park publishes segments and points, and every reader of parkPaths and groundLine moves to them without changing what it does. The path tools click points and snap onto points and segments of their kind within today's reach in meters, and the delete tool deletes a segment. Paths draw as today's ribbons. It rewrites every checked-in park, the new-park template, the scenarios' park builders and random edits, and the chosen edits of the park world checks. Those gain an AddPath that ends on the middle of a segment guests stand on in warm.park, which splits it. winding-path.park is a played park whose guests stand on today's path keys, so it has to be remade, not only rewritten. It also updates this capability's brief, where it describes paths as curves. Depends on: none.
2. `crossing-splits`: when an edit makes segments of one kind cross, every crossed segment splits there and the segments share a new point. The ghost marks each crossing join before commit. The park world checks gain a chosen AddPath that crosses a segment guests stand on. Depends on: feature 1.
3. `point-editing`: the commands MovePoint, InsertPoint, and DeletePoint, with the merge and delete rules. A moved point that ends on a point or segment of its kind at a snap joins it, as AddPath's points do, and the ghost marks the join. An edit tool drags points and inserts them by clicking a segment. The delete tool deletes the point under the pointer before the segment. A moved point splits crossings as feature 2 does. The park world checks gain a chosen edit for each new command. Depends on: feature 2.
4. `corners-and-widths`: ground lines round each two-segment point's corner with the radius feature 1 stores, and the commands SetCornerRadius and SetSegmentWidth set radius and width. The edit tool gains a radius handle at each corner and a width handle on each segment's side. New bends get the default radius. The park world checks gain a chosen edit for each new command. Depends on: feature 3.
5. `paved-surface`: sim/park derives the paved surface: segment bodies, corner arcs, junctions with rounded inside corners, and square ends. A free end is cut square at its point, not beyond it, so widening a segment never lengthens it toward what lies past its end, such as the entrance. The validity check tests boxes against it, and the renderer draws it, with an edge line along each piece's outline edges only. The ghost draws the same way. Depends on: feature 4.
6. `edge-radii`: a corner's outside edge, and each turn of the outline at a junction, can be given their own radius, set like the corner's radius with a handle of their own. The corner's radius handle then curves only its inside edge and the line guests walk. Each segment holds the radius of the edge at each of its ends, so a radius stays with its segment through splits, merges, and moves. Until then, a corner's edges follow its center radius, and a junction's inside corners take a fixed rounding. Depends on: feature 5.
7. `predictable-editing`: every edit changes only what it names, and the player can see what a click will do. Joins and splits keep every curve the player set: a path that joins a rounded bend's point keeps the bend's curves as the junction's edges, and a path that ends on a curve, crosses it, or has a point inserted on it splits the bend there into two bends of the same radius. A curve may use as much of its segment as the other end leaves free, so a split beside it doesn't shrink it. Delete removes only the segment or box under the pointer, and its ghost tints only what goes. Merging two segments becomes its own tool, Remove point, so criteria 3 and 4 use it. While Edit path or Remove point is chosen, every path point shows a marker. Depends on: feature 6.
8. `width-tapers`: where two segments of different widths meet, the paving's outline never steps across at the shared point: each side that would step curves smoothly from the narrower segment's side into the wider one's, over a length the player sets with a handle and kept as intent on the point. Depends on: feature 5.
9. `snaps-and-guides`: the path tools and point dragging snap to points, segments, the doors their kind serves, guide crossings, straight-on and 90° and 45° directions, and whole-meter lengths, with reach in screen pixels and a wider release. A point placed or dropped anywhere on a segment's paving joins that segment, not only near its ground line. Each snap draws its guide, the ghost shows the exact length, and the held suppress key turns every snap off. Depends on: feature 3.
10. `frontage-snap`: the place tools turn a box to face a nearby segment of the kind its front door serves, and set its front face at the setback from the paving's edge. Depends on: feature 4, for widths.
11. `undo-redo`: the tools keep a history of committed edits and their inverses. Undo and redo commit at once. Restored things get fresh keys, and starting or opening a park clears the history. Depends on: features 6 to 8, so every edit command exists.
12. `main-street-park`: draw the slice scenario's park with the tools, check it in as tests/parks/main-street.park, and settle the tuning values while drawing it. Depends on: features 5 to 11.

Feature 1 and feature 3 each carry part of criterion 4: feature 1 the split under guests, feature 3 the merge. Feature 2 adds crossing splits to the same check.

None of these features adds a test of its own unless it fixes a bug (docs/testing.md). The park world checks cover the new intent and commands once the parks hold them. The slice adds integration tests for splits under guests, frontage on the widest segment, and undo of a deleted segment.

## Deepening candidates

- Frontage when moving a box, not only when placing one.
- A box fronting the inside of a tight curve moves out until its whole front face clears the paving by the setback, not only the face's middle.
- Typing an exact length while drawing, as SketchUp does.
- Locking the current direction with a held key, so the pointer can leave the line and still draw along it.
- Showing the ghost of an undo's inverse while the undo control is hovered (decision 0029).
- Dragging from a segment to insert a point there and move it, in one gesture.
- Joining two segments into one when an edit leaves them joining the same two points. AddPath and MovePoint can both make such a pair.
- Typing an exact corner radius or segment width while dragging its handle.
- Showing Edit path's handles only near the pointer, so a large park isn't covered in them.
- Inserting a point inside a taper's curve keeps the curve where it was, instead of ending it at the new point.
- A segment that an edit adds or moves meets others along its own arcs, not along the straight lines to its points, so a dragged rounded bend joins a path where its curve crosses it.
- Guides from the ends of nearby paths, straight on from where they stop, for lining a new path up with one it doesn't touch. Cities: Skylines 2 players turn such guides off when they pull roads out of line, so keep them to nearby ends.
- Snapping a handle drag, such as a segment's width or a corner's radius, to whole numbers.
- Reach in screen pixels for picking points and handles in Edit path, and for finishing a path, which still use meters.
- Undo while a path is half drawn removes only its last stop, as Backspace does in Inkscape's pen tool.
- RestoreIntent among tpj_scenarios' random edits, so the scenarios try it on worlds no undo would reach.

## Open questions

- The player-facing UI for the tools (docs/open-questions.md). ImGui buttons and one held key stand in.

## Research notes

- Undo records the intent each edit touched, before and after, rather than a hand-written inverse per command.
- Splits at crossings reuse an existing point within a small tolerance and order several crossings along each segment.
- The edge line comes from each surface piece marking which of its edges are outline, not from a union or from casings under fills.
- Arcs use tangent length R·tan(θ/2), computed from the cross and dot products of the two directions, with the radius clamped to fit.

Depth is in RESEARCH.md.
