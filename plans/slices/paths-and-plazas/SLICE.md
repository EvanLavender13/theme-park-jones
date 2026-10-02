# Slice: Paths and Plazas

Status: planned

## Summary

The player lays out a park's walkable ground the way an art director sketches: paths of any width drawn with visible, suppressible snaps, plazas drawn as outlines or stamped down as squares, paths continued, reshaped, split, and joined, boxes that snap to face a path or plaza at a setback, and undo for all of it. Wherever paths and plazas meet or overlap, the ground is one clean surface with rounded junctions, derived from the strokes every time, so no stroke is ever rewritten and each stays editable. Guests cut straight across plazas, around their corners, and spread across wide paths instead of walking single file. It is the right next goal because building is this game's reason to exist (docs/vision.md), the boxes-and-tubes tools were the minimum to draw a park, and Planet Coaster 2 shows how much a clumsy path system costs. Its paths fuse behind the player's back, leave seams, and lose their identity because it saves the merged result instead of the player's strokes, which principle 1 already forbids. Its snaps act even when switched off because some are not covered by the toggle. Its guests stall on plazas because a plaza gives them no structure. Here intent stays the strokes, every snap is visible and suppressible, and plazas join the route graph through edges guests walk.

## End-to-end scenario

The player selects the new park's entrance path and sets its width to wide. Then they pick the guest path tool and start on the path's end, so the new stroke continues it as one path with no junction, at the path's own width. As they draw, guides appear: the next segment snaps straight on from the path it leaves, and to 90 and 45 degrees from it, with its length read out in meters and snapped to whole meters. Holding a key suppresses every snap at once. Each snap shows as a guide line in the ghost, and none acts without one.

At the end of the promenade they switch to the plaza tool and drag out a rectangle, then click corners for an irregular forecourt beside it, snapping to the rectangle's edges and corners. The two overlap. Then they stamp three squares of paving along one side, each turned and sized as it is set down. Every outline is saved as its own plaza, and on screen the forecourt, rectangle, stamps, and promenade read as one paved surface, with no seams where they overlap and rounded fillets where paths meet plazas and each other. Afterwards each one can still be selected, reshaped, or deleted on its own.

A narrow side path drawn beside the promenade picks up a parallel guide at a pleasant spacing. It branches from the middle of the promenade, snapping to its midpoint. The player drags one of its points to reshape it, inserts a point, splits it in two, and joins the two halves back. A bend they dislike goes with one undo, and comes back with redo.

They place a shop near the promenade. Its ghost turns to face the path, slides to a setback from the path's edge, and follows the curve's direction where it lands. A second shop snaps flush beside the first. A third snaps to face the forecourt from its edge. A depot goes behind the shops with a backstage path drawn to all three back doors.

Guests arrive. On the promenade they spread across its width. Crossing the plaza they walk straight from one path's mouth to another, and bend around the forecourt's inner corner where it is in the way, to reach the shop on its edge. The graph view shows the plaza's crossing edges between the places paths and doors meet it. The player drags a corner of the forecourt with guests on it. The crossings re-derive around its new shape, guests already on the plaza are carried to where they stand, and nobody is lost or stuck.

## Acceptance criteria

One park file is checked in: tests/parks/plaza.park, with the entrance path continued as a wide promenade, an overlapping rectangle, polygon, and stamped plazas, a narrow side path, two shops snapped along the promenade, a third snapped to the forecourt's edge, a depot, and a backstage path to all three back doors, with guests in the park after a warmup. tpj_scenarios --slice-parks regenerates it alongside the other slices' parks.

1. A path drawn on from another path's end is one path: the network has one carrier through the joint and no node there, and the route distance across it equals the path's length along its ground line. (integration test)
2. In plaza.park, route distance between any two places where paths or doors meet a plaza equals the shortest walk between them inside the paved area: the straight line when it leaves the area nowhere, and otherwise the reference geodesic around the area outline's reflex corners. (integration test)
3. Running plaza.park serves meals at the shop on the forecourt's edge to guests who reached it across the plaza. (integration test)
4. A box placed by the frontage snap beside a path of any width, or a plaza's edge, connects to it by its front door. (integration test)
5. Overlapping and touching plazas of one kind, and paths that meet them, form one walkable area: a guest can be routed between any two of them without leaving the paved surface, and no route crosses a gap between plazas that do not touch. (integration test)
6. Randomized sequences of every new edit (adding, reshaping, splitting, joining, and deleting paths and plazas, changing widths, and moving boxes against and away from plaza edges) applied to plaza.park with guests in it leave a physically valid or refused-unchanged world at every step, with every guest on the network or gone through the existing rules, and flows conserved. (integration test)
7. Undoing an edit, which commits without a preview (decision 0029), restores the park's intent and its derived networks to what they were before the edit, and redoing it restores what the edit produced. (integration test)
8. Every new edit's preview, undo and redo aside, equals the resolved park immediately after the edit is committed. (integration test)
9. Loading a saved plaza.park and saving it again gives an identical file, two runs give identical state hashes, and the cross-build check runs plaza.park on both builds with the same hash at every tick (decision 0022). (integration test, plus the cross-build check)
10. A capture of plaza.park shows the promenade, plazas, stamps, and side path as one paved surface with no seams or overlapping ribbons and rounded junctions, the shops facing the promenade in a row and the forecourt from its edge, and guests spread across the promenade's width rather than on its centerline. (scripted capture: effortless-building, believable-guests)
11. In the running app, the width setting, every snap with its guide and the suppress key, the plaza outline and rectangle modes, the square stamp, continuing, reshaping, inserting, splitting, and joining paths, the frontage snap, and undo and redo all work as the scenario describes, and the graph view shows the plaza's crossing edges. (manual)

## Medium

The slice adds no fields or flows. What crosses capabilities is the network type, which every field and flow lives on, and park intent. Abbreviations are as in the boxes-and-tubes slice.

- Networks: the guest and backstage graphs, now with a walkable width at every place of every carrier. A drawn path's carrier has the path's width throughout, and a connector its walkway's. A plaza adds a carrier for each pair of its portals, the places where a path or a door's connector meets its paved area, along the shortest walk between them inside the area, around the outline's reflex corners; overlapping and touching plazas of one kind, with the paths meeting them, count as one area. A crossing's width at a place is the clear width of paving around it, which falls to zero at a corner it bends around, so guests spread across it never stand off the paving. A door's reach is measured to the edge of a path or plaza, not its centerline, and a door near a plaza's edge connects by a connector to the nearest point of that edge. The width, and queries for the width and direction at a place, are added to SM's network type. NN produces the networks from intent. Consumed by BG (movement, and the spread guests are drawn with), by PO (anchors, unchanged), by every field sampled on network places, and by EB (walkways, and the graph view).
- Route distance: unchanged, measured along the new carriers, so crossing a plaza costs its shortest walk (principle 4). Produced by NN.

Dependencies that are not fields or flows, ruled on by decision 0025:

- Park intent: paths with one width each, plazas by kind as outlines (a polygon of corners, a rectangle, or a stamped square, each saved as the shape the player made), and boxes as before. Authored by EB through new edit commands: set width, add, reshape, and delete plazas, move, insert, and remove a path's points, split and join paths, and continue a path from its end at that path's width. Every tool mode saves this same intent. Nothing is ever merged or rewritten in intent except by the edit the player made. NN derives networks from it, and EB derives the paved surface, its fillets, and the merged look of overlaps from it, every time.
- Candidate resolution: every new edit previews on a candidate world as placements do today.
- Inspection records: unchanged. BG's guest mesh reads a guest's place from its record and the walkable width and direction there from the network's public queries.

Undo history is the tools' own session state, a list of edits and their inverses. It is not intent and is never saved (principle 1). Undo and redo commit at once, without a preview (decision 0029). Undoing a deletion recreates the deleted entity under its original key, a key the counter issued and no entity now holds, so a restored path is the same carrier and the derived networks return to what they were. Keys are still never handed to a different entity.

## Members

Ordered for building. Milestone slugs are provisional until each capability plans them.

Prerequisite, not a member: sound-architecture's `composed-app`, since the slice adds tool modes, keys, and undo to the app, which decision 0027 keeps out of src/app/main.cpp until the restructure.

1. `deterministic-simulation/restored-keys`: a command may recreate an entity under a key the counter issued and no entity holds, so undoing a deletion restores the same key, with saves, copies, and hashes unchanged in meaning. Depends on: none.
2. `shared-medium/carrier-widths`: a walkable width at every place of every carrier of the network type, which may vary along a carrier, carried through construction, copies, and saves, with queries for the width and direction at a place. Depends on: none.
3. `effortless-building/paved-ground`: path widths, plazas as outlines, rectangles, and stamped squares, continuing, reshaping, inserting, splitting, and joining paths, visible suppressible snaps (straight-on and angle, parallel guides, round lengths, plaza edges and corners, box faces, path midpoints), the frontage snap for boxes, undo and redo, physical validity with widths and plazas, a box overlapping a plaza being refused like one overlapping another box, and the paved surface rendered as one derived mesh with rounded junctions and seamless overlaps. Depends on: members 1 and 2, and member 4's plaza carriers for its walkways and graph-view work, its last feature.
4. `navigable-networks/plazas-become-routes`: carrier widths from intent, door reach to path and plaza edges, plaza portals and crossing carriers around the outline's corners with their clear widths, overlapping plazas as one area, and doors connected to plaza edges. Depends on: members 2 and 3.
5. `believable-guests/spread-guests`: guests drawn spread across the walkable width at their place by an offset derived from their key, varying smoothly along their walk, with nothing in the simulation changed, and tpj_scenarios --slice-parks writing plaza.park. Depends on: members 2 and 4.

## Out of scope

Stamps beyond a square, saved or user-made stamps, and stamps that stay grouped. Frontage saved as an attachment, so boxes follow a reshaped path. A tool that lays rows of boxes. Terrain, slopes, stairs, bridges, and heights. Queue paths and one-way paths. Path materials and styles. Crowding, capacity from width, and crowd avoidance in the simulation. Freehand strokes and brush-painted plazas. Boxes standing inside plazas, such as kiosks, and supplying boxes from inside guest areas, which waits on how supplies and staff reach guest areas (docs/open-questions.md). Moving the entrance. Player-facing UI: ImGui stands in, and the question stays open (docs/open-questions.md).

## Open questions

- Whether portal-pair crossings stay cheap in a large plaza with many paths and doors. Resolved by measuring plaza.park and a large synthetic plaza in plazas-become-routes; bounding the portals a plaza connects, as OpenTripPlanner caps area visibility, is the fallback.
- Snap reach, guide spacing, setback, width presets, fillet radius, and the stamp's default size are tuning values, set while planning paved-ground and adjusted by drawing parks.

## Research notes

- Platter gives each building its own frame snapped to the road edge with a setback, which is what makes curved streets work; the frontage snap follows it.
- Planet Coaster 2's fusing, seams, and lost strokes come from saving merged results, its hidden snaps from snaps the toggle misses, and its stalled guests from plazas with no structure; intent here stays the strokes and outlines, merges are derived, and every snap is visible.
- Width belongs on the path, never as parallel paths (Parkitect's distance inflation); plazas are drawn areas, never tiled path pieces.
- Plazas join the graph through portals and shortest-walk edges between them, so route distance stays exact; navmeshes, flow fields, and medial axes would make distance an artifact of the derivation.

Depth is in RESEARCH.md.
