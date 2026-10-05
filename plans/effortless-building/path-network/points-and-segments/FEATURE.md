# Feature: Points and Segments

## Summary

Park intent stops holding paths as curves through clicked points. It holds path points and the straight segments between them, so a path is a run of segments that share points, and two paths join where the player made them share one. Until crossing-splits, routes also joins segments of one kind where they cross. Each point has a corner radius, which nothing uses until corners-and-widths. Each segment has a kind, its two points, and a width, which starts at its kind's default. The AddPath command draws a path through a list of stops. Each stop is a new point, an existing point, or a place on an existing segment, which splits there. DeleteSegment replaces DeletePath. It removes one segment and any point left with no segment. sim/park publishes points and segments with each segment's straight ground line, and every reader of paths moves to them without changing what it does. Routes still builds one carrier per segment with its kind's fixed width and still finds junctions from geometry. The path tools click stops that snap onto points and segments of their kind, and the ghost marks each join. The delete tool deletes one segment. Every checked-in park, the new-park template, and the scenarios' park builders are rewritten. Old saves don't load. The simulation's output changes, because route lengths follow straight segments instead of curves.

## Acceptance criteria

1. Every park in tests/parks and tests/parks/stress holds path points and path segments, and no paths. `ctest.exe --preset windows-debug` passes. This includes the park world checks over every park and over their edits of warm.park, which now include deleting a segment that guests stand on and drawing a path that ends on the middle of a segment guests stand on. It also includes food_loop_test on the regenerated warm.park. (check: Task 19)
2. In the app's new park, pick Guest path. Click about a meter from the end of the template path, then click 15 m away, then click there again to finish. The first click snaps onto the end point, and the ghost marks it before the finishing click. Draw a second guest path that starts a meter beside the middle of the template path. It snaps onto the segment, and the ghost marks the join. Turn on the Graph checkbox: after both commits, a node sits at each join, and the template path has a node where the second path split it. Pick Backstage path and click a meter beside a guest path: the click doesn't snap, and no mark shows. (play)
3. Pick Delete and hover the template path between the two joins. The ghost tints only that segment. Click: only that segment goes, and the paths at its ends stay. Delete the far segment of the first drawn path, whose free end no other segment uses. Then pick Guest path and click a meter from where that free end was: the click doesn't snap, because the point went with its last segment. (play)
4. Open tests/parks/warm.park with `--park`. Pick Guest path and draw a path that ends on the middle of a segment guests are walking along. No guest on that segment jumps along it when the path commits. (play)
5. `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm.park --graph --capture build/windows-debug/points-and-segments.bmp` writes a capture. Each segment shows as a ribbon at its width with round ends, the shop and depot have walkways to their paths, and a graph node sits at every point where segments meet. (capture)

## Tests

The rewritten parks hold the new intent types, so the park world checks cover them with no new test. The feature adds one world for those checks to cover, as the milestone requires:

- In tests/sim/park_world_checks_test.cpp, add a chosen edit of warm.park. Setup: the world warm.park reaches after one cycle, as the other chosen edits use. Find a guest segment that a guest heading to the shop stands on. Change: an AddPath of guest kind from a new point 6 m from the middle of that segment, square to it, to an OnSegment stop at the middle of that segment, 0.5. Try one side and then the other, and use the first the world accepts. Expected: the existing checks hold for it. Its candidate equals the world that commits it, both copy and save equal, and two runs with it end equal. Name it for what it makes the capabilities do: a guest path ending on the middle of a segment guests stand on, which splits the segment under them.

## Medium

The feature adds no fields or flows. It changes park intent, which decision 0025 places outside the medium, and every reader of it.

- Park intent registers two types in place of path: path-point and path-segment. Every park file in tests/parks holds both, so the park world checks cover them.
- sim/park publishes parkPathPoints and parkSegments in place of parkPaths, and segmentLine in place of groundLine. Readers: routes (navigable-networks), legible's path places and preview, the renderer's park mesh, band overlay, and graph overlay, the app's scene sync, the tools, the scenarios, and the tests. Each reads segments where it read paths, and does what it did.
- Routes gives each segment's carrier its kind's fixed width, pathWidth, as today, not the segment's width. navigable-networks' wide-routes changes that.
- Carry-over: AddPath splits a segment under guests and gives the split-off part a new key. ground-carry-over keeps each guest and footfall entry there on its ground.

## Spec changes

src/sim/park/SPEC.md, replace the Boundary section's second and third paragraphs with:

    Other modules read intent only through parkEntrances, parkPathPoints, parkSegments, and parkBoxes. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPathPoint, ParkSegment, and ParkBox. Other modules change intent only through commands.

    segmentLine, stopPosition, footprintOf, and the sizes pathWidth, MIN_SEGMENT_LENGTH, DEFAULT_CORNER_RADIUS, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so that the renderer, the tools, and routes measure the same shapes this module validates.

and in its first paragraph, replace "the intent types entrance, path, and box" with "the intent types entrance, path-point, path-segment, and box".

src/sim/park/SPEC.md, replace the Intent section's list and save lines with:

    - A path point has a ground position and a corner radius. The radius is for the corner the point makes when exactly two segments meet there. Nothing uses it yet.
    - A path segment has a kind, guest or backstage, its two path points, From and To, and a width. Segments share a point where the player joined them. Routes also joins segments of one kind where they cross (sim/routes/SPEC.md).
    - A box has a kind, shop or depot, and a pose.
    - An entrance has a pose.

    A Pose is a position and a facing on the ground, held as given. A default Pose is at the origin, facing -z.

    The save lines hold:

    - an entrance's x, z, facing-x, and facing-z;
    - a path point's x, z, and radius;
    - a path segment's kind, from, to, and width, from and to being its points' keys;
    - a box's kind, x, z, facing-x, and facing-z.

src/sim/park/SPEC.md, replace the Ground lines section's two paragraphs with:

    A segment's ground line is the straight line from its From point to its To point, as two CarrierPoints (sim/medium/SPEC.md), the first at distance 0 and the second at the segment's length. parkSegments gives each segment with its ground line, and segmentLine gives the ground line between any two positions. The renderer, the validity check, and routes all use it, so what is drawn, what is refused, and what is walked are the same line.

    A ground line is empty when either position is not finite or lies outside the park, or the two lie closer together than MIN_SEGMENT_LENGTH. A segment's ground line is also empty when it names a key that holds no path point, names the same point twice, or has a width that is not finite and above zero.

src/sim/park/SPEC.md, in Physical validity, replace "no path meets a solid" with "no segment meets a solid", replace "A path occupies its ground line widened by half its pathWidth." with "A segment occupies its ground line widened by half its width, with round ends.", replace "Paths never conflict with each other: where they cross, routes derives a junction." with "Segments never conflict with each other. Where they cross, routes derives a junction.", and replace "If an object has no footprint or has an empty ground line, the world is invalid." with "If an entrance or box has no footprint, or a segment has an empty ground line, the world is invalid. A path point that no segment uses doesn't affect validity."

src/sim/park/SPEC.md, replace the Commands section's paragraphs up to "A pose or path is kept exactly as given." with:

    Five commands change intent: AddPath, AddBox, MoveBox, DeleteSegment, and DeleteBox. No command moves or deletes the entrance. Tools submit commands to the command queue, so they apply between ticks.

    AddPath draws a path of one kind through a list of stops, in order. A stop is a new point at a ground position (NewPoint), an existing path point (OnPoint), or a place on an existing segment (OnSegment), given as the fraction of the way from the segment's From point to its To point. stopPosition gives a stop's ground position.

    On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. For AddPath, that means it has at least two stops, each OnPoint names a path point that at least one segment uses and every segment at it has the path's kind, each OnSegment names a segment of the path's kind at a fraction strictly between 0 and 1, and every segment it adds or splits has a ground line. isAccepted returns whether a command would be accepted. If the command is not accepted, applyCommand changes nothing. A ghost shown as valid always matches what its commit does.

    If a command is accepted, it leaves exactly the intent it describes and changes nothing else:

    - AddPath adds a point at each NewPoint and each OnSegment stop, with DEFAULT_CORNER_RADIUS. It adds a segment from each stop's point to the next, of the path's kind and pathWidth. Each segment that OnSegment stops name splits at their points, in order of fraction. The segment keeps its key, kind, and width, and now ends at its first split point. A new segment of the same kind and width runs on from each split point to the next, and the last ends at the segment's old To point.
    - AddBox adds one box.
    - MoveBox replaces a box's pose and keeps its key and kind.
    - DeleteSegment destroys the segment, and each of its points that no other segment uses.
    - DeleteBox destroys the box.

    Everything a command adds takes a key from the key counter. A pose or position is kept exactly as given.

src/sim/park/SPEC.md, in The new park, replace "a short guest path in front of it" with "a short guest path in front of it: two path points and the segment between them".

src/sim/routes/SPEC.md, in Boundary, replace "routes reads intent only through parkPaths, parkEntrances, parkBoxes, groundLine, and footprintOf" with "routes reads intent only through parkSegments, parkEntrances, parkBoxes, and footprintOf". In Networks, replace "A kind's network has a carrier for each path of that kind, keyed by the path's key and following its ground line." with "A kind's network has a carrier for each segment of that kind whose ground line is not empty, keyed by the segment's key and following its ground line.", replace "Every carrier, a path's or a connector's," with "Every carrier, a segment's or a connector's,", replace "Every edge is a stretch of a drawn line or a connector" with "Every edge is a stretch of a segment or a connector", and delete "A line also meets itself where it crosses or doubles back."

src/tools/SPEC.md, replace "boxAt, pathAt, and snapToPath are the picking queries the tools use." with "boxAt, segmentAt, and snapStop are the picking queries the tools use.", and replace the Picking section's paragraph with:

    boxAt gives the box whose footprint holds a ground point. segmentAt gives the segment whose width covers the point. If several do, it gives the first in key order. Neither gives the entrance.

    snapStop gives the stop a path tool's click makes. If a path point of the tool's kind lies within SNAP_REACH, the stop is the nearest such point. Otherwise, if a segment of the kind passes within SNAP_REACH, the stop is the place on the nearest such segment nearest the click, or the segment's end point when that place lies within MIN_SEGMENT_LENGTH of it. Otherwise the stop is a new point at the click. A point is a kind's when at least one segment uses it and every segment at it has that kind, so a path never snaps onto a point or segment of the other kind.

src/tools/SPEC.md, in Tools, replace the Guest path paragraph with:

    Guest path and Backstage path draw a path of their kind. Each click adds the stop snapStop gives. A click near a point or segment of the same kind joins the path there, and a segment it joins splits there when the path commits. Clicking the last stop again finishes and commits the path. The ghost shows the path through the stops and the pointer, and marks each stop that joins a point or segment. If the drawn path has fewer than two stops, finishing it commits nothing.

and replace "If there is no box, it removes the path under the pointer." with "If there is no box, it removes the segment under the pointer, and any point no other segment uses."

src/render/SPEC.md, in Ghosts and highlights, replace the two bullets with:

    - an added or moved box, or an added path's segments, translucent in its kind's color, or in an invalid tint when the world refuses the edit
    - a mark at each stop where an added path joins a point or segment, in JOIN_MARK_COLOR
    - the entity an accepted delete would remove, in a delete tint

replace "When the delete tool hovers a backstage path," with "When the delete tool hovers a backstage segment,", and replace "appendEntity adds the box or path a key holds" with "appendEntity adds the box or segment a key holds".

src/legible/SPEC.md, in Path places, replace "Ties between paths go to the lower key." with "Ties between segments go to the lower key."

plans/effortless-building/CAPABILITY.md is the capability's brief, not a spec. Its Foundation criteria and Medium describe paths as curves. Task 3 of PLAN.md updates them.

## Files affected

- Modify: src/sim/park/SPEC.md
- Modify: src/sim/park/internal/components.h
- Modify: src/sim/park/intent.h
- Modify: src/sim/park/intent.cpp
- Modify: src/sim/park/geometry.h
- Modify: src/sim/park/geometry.cpp
- Modify: src/sim/park/edits.h
- Modify: src/sim/park/edits.cpp
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.cpp
- Modify: src/legible/SPEC.md
- Modify: src/legible/path_place.cpp
- Modify: src/legible/preview.cpp
- Modify: src/tools/SPEC.md
- Modify: src/tools/tools.h
- Modify: src/tools/tools.cpp
- Modify: src/render/SPEC.md
- Modify: src/render/park_mesh.h
- Modify: src/render/park_mesh.cpp
- Modify: src/render/band_overlay.cpp
- Modify: src/render/graph_overlay.cpp
- Modify: src/app/scene/scene_sync.h
- Modify: src/app/scene/scene_sync.cpp
- Modify: src/scenarios/food_shop.cpp
- Modify: src/scenarios/full_park.cpp
- Modify: src/scenarios/park_edits.cpp
- Modify: src/scenarios/slice_parks.cpp
- Modify: src/scenarios/slice_parks.h
- Modify: tests/integration/food_loop_test.cpp
- Modify: tests/sim/park_world_checks_test.cpp
- Modify: tests/parks/new.park, fed.park, routes.park, sketch.park, supply.park, degenerate-intent.park, warm.park, cut.park
- Modify: tests/parks/stress/winding-path.park, full.park
- Modify: plans/effortless-building/CAPABILITY.md

## Dependencies

- shared-medium/carrier-widths, merged: ground-carry-over keeps guests on their ground when AddPath splits a segment under them.

## Out of scope

- Splitting where segments of one kind cross. Until crossing-splits, crossing segments share no point, and routes still joins them from geometry.
- Moving, inserting, and deleting points (point-editing).
- Corner arcs, and setting a segment's width or a corner's radius (corners-and-widths). Every point stores the default radius, and nothing reads it yet.
- The paved surface, square ends, and edge lines (paved-surface). Segments draw as today's ribbons with round ends.
- Snap reach in screen pixels, guides, and the suppress key (snaps-and-guides). The tools snap within today's reach in meters.
- Random joins in the park-edits scenario. Its paths stay runs of new points.

## Open questions

None.
