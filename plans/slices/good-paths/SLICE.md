# Slice: Good Paths

Status: planned

## Summary

The player draws paths that look and behave the way they meant. A path is a network of shared points joined by straight segments. Each segment has its own width, and each corner has its own radius. The player reshapes any path after drawing it by dragging, inserting, and deleting points, by dragging a corner's radius, and by dragging a segment's side to set its width. Every snap shows a guide, and one held key suppresses them all. Joined paths draw as one paved surface with rounded inside corners and an edge line along its outline. Shops placed beside a path face it and line up along it. Guests use a path's whole width instead of walking single file. Undo and redo cover every edit. This is the right next goal because building is why this game exists (docs/vision.md), and Planet Coaster 2 shows what bad paths cost. Its paths fuse by nearness, its seams show, its delete removes too much, its snaps ignore the off switch, and its guests walk single file. Plazas and open paving wait for a later slice, which can build on paths that already work.

## End-to-end scenario

The player starts a new park and lays out a main street. They pick the guest path tool, click the end of the short path in front of the entrance, then click 60 m straight ahead. A dashed guide shows the run is straight on from the entrance path, and the length reads 60 m exactly because it snapped to whole meters. They drag the segment's side out until it is 10 m wide.

From the far end they draw a side path at 90° to the street, 3 m wide, and turn it 45° partway along. A guide marks each angle as it snaps. The bend gets a default corner radius, and they drag its radius handle until the bend is a wide, smooth arc. They draw a second side path from the middle of the street. Clicking on the street's side snaps onto it, a marker shows the join, and the street splits there at a new shared point. They draw a loop path across the first side path. The ghost marks the crossing as a new junction before they commit. Once it is committed, the two paths meet at a shared point.

Where paths meet, the paving reads as one surface. The inside corners are rounded, and the rounding grows with the paths' widths. A thin edge line runs along the outline of the whole surface and never across a join. Each path's free end is cut square.

Next they adjust the layout. They drag the shared point where three paths meet, and every segment there follows. They click a segment to insert a point and pull it out to bend the path. They delete a point where a path kinked, and its two segments merge into one straight segment. They delete one segment of the loop, and only that segment goes. To put a point where no snap will let it go, they hold the suppress key and drag freely, and every guide disappears while they hold it. One undo puts back the deleted segment, and redo removes it again.

They place shops along both sides of the main street. As each ghost nears the street, it turns to face it, slides to a fixed setback from the paving's edge, and connects by its front door. They draw a backstage path behind one row, past the shops' back doors, and place a depot beside it. The depot turns to face the backstage path and connects by its front door. Where the backstage path crosses a side path, the two paths don't join.

Guests arrive. On the 10 m main street they walk spread across its width, each keeping its own line with a slow drift, not filing down the middle. On the 3 m side paths they walk close together. When the player widens, splits, or merges a segment under walking guests, the guests keep walking from where they stand. Nobody jumps along the path.

## Acceptance criteria

Two park files are checked in under tests/parks:

- main-street.park holds the scenario's park with no guests: the entrance, the 10 m main street, the side paths with their angled bends and rounded corner, the loop and its crossing, shops in rows along both sides of the street, the depot, and the backstage path.
- warm-main-street.park is main-street.park warmed until guests fill the park. tpj_scenarios --slice-parks makes it from main-street.park, as it makes warm.park from fed.park.

Every other park in tests/parks and tests/parks/stress is rewritten as points and segments. The park world checks and the cross-build check read every park in tests/parks. Once these parks are there, those checks cover the new intent without a new test. The world checks' chosen edits include each new edit command, so each one's preview is checked against its commit (principle 8). food_loop_test must keep passing on the rewritten warm.park.

1. In warm-main-street.park, adding a guest path that crosses the main street leaves every guest on the street where it stood, although the crossing splits the street into two segments. (integration test)
2. If a shop is posed by the frontage snap beside a segment of the widest width the tools allow, placing it there gives its front door a connector to the guest network. (integration test)
3. In main-street.park, deleting a segment and then undoing the deletion gives back networks whose carriers have the same lines and widths as before, and route distance from every node to every shop is unchanged. (integration test)
4. A capture of warm-main-street.park shows the paths as one paved surface with rounded inside corners, an edge line only along the outline, and square free ends. The shops face the street in rows, and guests walk spread across the main street. (scripted capture: believable-guests' member)
5. A capture of main-street.park with --graph shows one line down the middle of each segment, a node at every point and at both ends of every connector, and no other nodes. Where the backstage path crosses a guest path, there is no node. (scripted capture: navigable-networks' member)
6. A capture of warm-main-street.park with --overlay food shows shading beside the 10 m main street as well as beside the 3 m side paths. (scripted capture: legible-simulation's member)
7. In the running app, every tool, snap, guide, edit, the suppress key, the frontage snap, and undo and redo work as the scenario describes. Paths join only at a visible snap or at a crossing the ghost marked. Where paths of one kind overlap without joining, an edge line shows they are separate. (manual)
8. In the running app, guests in warm-main-street.park walk spread across wide segments, and none jumps along a path when the player widens, splits, or merges a segment under it. (manual)

## Medium

The slice adds no fields or flows. It changes three things that cross capabilities: park intent, the network type that every field and flow lives on, and how places carry over when a network is derived again. Abbreviations: EB is effortless-building, NN is navigable-networks, SM is shared-medium, BG is believable-guests, LS is legible-simulation, and PO is plausible-operations.

- Networks: the guest and backstage networks, in SM's network type, produced by NN. Each segment is a carrier, keyed by the segment's key and following its ground line, as each path is today. Nodes sit only at points, shared or free, and at both ends of each connector: its door, and where it meets a segment. Lines that cross or come near each other without sharing a point don't join, so routes no longer derives junctions from geometry. Every carrier now has a walkable width. A segment's carrier has the segment's width, and a connector has its kind's walkway width, which is today's fixed path width. SM's network type answers the width and direction at a place. Consumers: BG walks guests through nodes and draws them spread across the width, LS sizes the overlay, and PO, every field, and NN's own graph view are unchanged.
- Shaping points: a shared point where exactly two segments meet, with no door's connector, is a node only so the line can bend. If guests chose there, the number of points the player drew would change how guests behave. BG consumes this: a guest walks straight through a node with exactly two edges and no anchor, without choosing there. It still chooses at every other node and at the triggers it chooses at today.
- Carry-over: SM's carryOver moves a holder's place to the new network. Today, if a carrier changes shape, a place moves to the nearest place on the same carrier. On a split, that would move a guest on the split-off half to the split point. Under this slice, if an edit leaves ground where it was under a different key, a place on that ground stays at the same ground position. That ground can be either half of a split segment, or two segments merged into one. Guests and every other holder of places use it, including hungry footfall, which already carries its entries over with carryOver.
- Connectors: produced by NN. A door connects if it is within reach of the edge of a segment of its kind, not of its center line. EB's frontage setback must stay inside that reach, so a box placed by frontage always connects.
- Route distance: unchanged in kind, produced by NN. It is measured along segment center lines and the arcs of their rounded corners (principle 4). Width doesn't change route distance or capacity.

Dependencies that are not fields or flows, ruled on by decision 0025:

- Park intent: points and segments, authored by EB. A point is a ground position. A point with exactly two segments has a corner radius. A segment has a kind, guest or backstage, its two points, and a width. Points are shared only by segments of one kind. EB adds edit commands to add a path, move a point, insert a point into a segment, delete a point, delete a segment, set a segment's width, and set a corner's radius. Two rules hold for every edit. If an edit ends a point on a point or segment of its kind at a visible snap, they join there. If an edit makes segments of one kind cross, they split at the crossing and share a point there. The ghost marks each such join before commit. Nothing else joins. Deleting a point with two segments merges them into one segment with the wider of their widths. Deleting a point with one segment, or with three or more, deletes them all. A point left with no segments is removed. sim/park publishes each segment with its key, kind, width, ground line, and the keys of its two points, so NN finds shared points from intent, not from geometry. Segments that share a point have ground lines that meet there. At a rounded corner, they meet at the middle of the arc.
- Candidate resolution: every new edit previews on a candidate world, as placements do today. Undo and redo commit without a preview (decision 0029).
- Inspection records: unchanged. BG draws a guest from the place in its record, using the width and direction the network gives there.

The paved surface, its rounded inside corners, and its edge line are EB's own rendering. No other capability reads them. Where segments of one kind overlap without sharing a point, each one's edge line is drawn across the other, so unjoined paving never looks joined. The ghost shows this before commit. Undo history is the tools' session state: a list of edits and their inverses. It is not intent and is never saved (principle 1). If undo restores something deleted, the restored thing gets a fresh key from the counter, so keys are still never reused (src/sim/SPEC.md).

## Members

These are in build order. Milestone slugs are provisional until each capability plans them.

1. `shared-medium/carrier-widths`: every carrier has a width that construction, copies, and loads keep, with queries for the width and direction at a place, and carryOver keeps a place at its ground position when its ground survives under a different key. Until member 3, routes gives each carrier its kind's current fixed width. Depends on: none.
2. `effortless-building/path-network`: paths as points and segments. This member covers every edit command in Medium, the draw tool and its snaps, guides, length readout, and suppress key, and editing handles for points, corners, and widths. It also covers undo and redo, the frontage snap, validity that tests boxes against the paved surface as it is drawn, rounded corners included, so what is drawn and what is refused stay the same shape, and the paved surface drawn as one surface. It rewrites every checked-in park, the new park template, and the scenarios' park builders, and adds main-street.park. The world checks' chosen edits include each new edit command. It updates every reader of parkPaths and groundLine to the published segments in the same change, without changing what those readers do. Depends on: member 1.
3. `navigable-networks/wide-routes`: nodes come only from points and connectors' ends, each carrier takes its segment's width, doors connect within reach of a segment's edge, and the graph view shows the new networks. Depends on: members 1 and 2.
4. `legible-simulation/wide-overlay`: the food overlay's band reaches past the edge of a segment of any width, sized by the network's width at each place, so wide paths never hide it. Depends on: member 3.
5. `believable-guests/spread-guests`: guests walk straight through shaping points without choosing, and still choose at every other node and at today's triggers. They are drawn spread across the width at their place, by a sideways offset from their key with a slow drift, blended where the width changes. Picking uses the same position. tpj_scenarios --slice-parks makes warm-main-street.park. Depends on: member 3.

Simulation output changes on purpose, because route lengths change from today's smooth curves to straight segments and arcs. The runtime report taken after the last member becomes the new baseline. Old saves don't load.

## Out of scope

- Plazas, areas, clearings, stamps, and open paving. These are the next slice.
- Curved segments other than rounded corners, and Bezier handles.
- Width that varies along one segment.
- Box-select of several points, and dragging a whole segment.
- Boxes that follow a path when it moves. If a moved path would meet a box, the ghost shows the edit refused.
- A tool that lays rows of boxes.
- Path materials, textures, and decoration.
- Terrain, slopes, stairs, bridges, and heights.
- Queue paths and one-way paths.
- Width as capacity, crowding, and crowd avoidance in the simulation.
- Restoring a deleted thing's old key on undo.
- Moving the entrance.
- Player-facing UI. ImGui stands in, and the question stays open (docs/open-questions.md).

## Open questions

- Tuning values: the snap reach in screen pixels and its release band, the default and allowed widths, the default corner radius, how inside rounding grows with width, the frontage setback, and door reach. Members 2 and 3 set them while planning and adjust them by drawing main-street.park.

## Research notes

- Planet Coaster 2's problems come from one merged surface: fusing by nearness, seams, delete overreach, and a snap the off switch misses. Here intent is points and segments, joins happen only at visible snaps and marked crossings, and the surface is derived.
- Snaps feel right when their reach is in screen pixels, the smallest correction wins, a snap releases a little farther out than it caught, every satisfied guide is drawn, and one key turns them all off (SketchUp, Figma, tldraw).
- Cities: Skylines splits a road where another crosses it and removes points left with nothing. Neither it nor its sequel can merge two segments at a point or undo without mods.
- Build the surface at each point the way road games build intersections. Each piece marks which of its edges are outline, and edge strips are drawn along those edges over the fills, so joins show no line and unjoined overlaps do (plans/effortless-building/path-network/RESEARCH.md). Clipper2's offset-and-back rounding was rejected because it also joins nearby separate paths.
- A fixed sideways offset per guest from a hash of its key, with a slow drift, spreads guests across a path without crowd simulation.

Depth is in RESEARCH.md.
