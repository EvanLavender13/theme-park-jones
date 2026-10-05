# Research: path-network

The good-paths slice's RESEARCH.md covers snapping, node-and-segment editors, building the paved surface, and spreading guests. This file covers the tactical questions the milestone adds.

## How should undo and redo record an edit that splits or merges records, when restored records get fresh keys?

Editors use three record shapes:

- Whole-document snapshots. iD keeps a stack of immutable graphs that share unchanged entities, so undo moves to the previous graph and no action needs an inverse. Blender's global undo reloads a deduplicated in-memory file.
- A memento of the touched records. Inkscape logs each XML change with its old and new values. JOSM builds a split from smaller add and change commands in a sequence.
- A hand-written inverse per command. Godot's UndoRedo pairs a do method with an undo method and keeps a deleted node alive in the history, so it comes back with the same identity. Figma keeps a deleted object's properties in the deleting client's undo buffer and restores it under its old ID.

Almost none of them give a restored record a new ID. The exceptions are the collaborative-editing libraries. Yjs creates new items on undo and links each old item to its replacement, following those links on later undos. Yorkie re-creates split elements under new IDs, records each old and new ID pair, and rewrites both stacks to use the new IDs. Undo It! for Cities: Skylines rebuilds roads from what it recorded and loses what was attached to them, such as bus stops.

For this milestone, the fit is a memento: each history entry holds the points and segments the edit touched, whole, as they were before and after. Undoing removes the after records and adds the before records. Redoing does the reverse. Records added again take fresh keys from the counter, as src/sim/SPEC.md requires. The history keeps a map from each replaced key to the key that replaced it, and resolves every key it stores through that map, following chains, before it applies an entry. Boxes fit the same shape: a box's before and after records are its pose and kind.

Rejected: whole-document snapshots — restoring one brings back old keys, which breaks the rule that keys are never reused, and remapping them on restore makes them no simpler than mementos. Hand-written inverses for splits and merges — they are easy to get wrong, and JOSM had a sequence that failed to roll back its earlier steps when a later one failed. Finding records again by position — near-duplicates make it ambiguous. Rewriting both stacks on every undo, as Yorkie does — the forwarding map gives the same result for less work.

Sources: https://github.com/WorldBank-Transport/ram-iD/blob/master/ARCHITECTURE.md — iD's stack of immutable graphs; https://developer.blender.org/T60695 — Blender's in-memory file undo; https://wiki.inkscape.org/wiki/XML_subsystem — Inkscape's event log with old and new values; https://josm.openstreetmap.de/doc/org/openstreetmap/josm/command/SplitWayCommand.html — JOSM's split as a sequence of commands; https://josm.openstreetmap.de/ticket/14526 — a sequence that failed to roll back; https://docs.godotengine.org/en/stable/classes/class_undoredo.html — do and undo methods, objects kept alive; https://www.figma.com/blog/how-figmas-multiplayer-technology-works/ — deleted properties in the undo buffer, redo rewritten on undo; https://github.com/yjs/yjs/issues/806 — Yjs links from old to new items; https://github.com/yorkie-team/yorkie-js-sdk/pull/1426 — remapping old IDs to new ones in both stacks; https://steamcommunity.com/sharedfiles/filedetails/?id=1890830956 — Undo It! losing attached stops.

## How should an edit split segments where a new segment crosses existing ones?

iD's crossing-ways fix adds a node at the crossing. If an existing end node lies within 0.75 m of the crossing, it reuses that node instead. It also refuses to make edges shorter than 0.55 m. Its intersection test returns nothing for parallel segments and for an end lying exactly on the other line, so touches at an end and collinear overlaps are skipped without notice. JOSM treats a determinant below a tiny fraction of the segments' size as parallel, and reuses a segment's end when the crossing lies within about a centimeter of it. It handles several crossings on one segment by inserting nodes into a working list and reading it again, which puts later crossings in order without sorting. CGAL's arrangements insert a curve by walking the faces it passes through and splitting each crossed edge, possibly more than once. With exact arithmetic, they report overlaps as shared parts.

For this milestone, these steps follow:

- Find every crossing of the new or moved segment with segments of its kind.
- Treat a crossing within a small tolerance of an existing point as meeting that point, and join there without splitting.
- Treat a crossing within the tolerance of either segment's end as meeting that end.
- Merge crossings closer together than the tolerance.
- Group the crossings by the segment they cross, sort each group along that segment, and split each segment once into all its parts. Then no edit needs to find a freshly keyed part again.
- Decide near-parallel cases by the distance from a point to a segment, which stays accurate when the line solve does not.

Collinear overlaps of one kind are not crossings. The slice keeps them as separate paving, and the surface shows each one's edge line across the other. The simulation's arithmetic is already identical on every build (decision 0022), so these decisions can use doubles. They don't need integers or a grid.

Rejected: connecting crossings later through a validation pass, as iD does — the edit must join when it commits, and the ghost must show it. Skipping parallel and end cases without a rule, as iD and JOSM do — each case needs a stated outcome. CGAL as a dependency — it is heavy and needs exact kernels, and its ideas carry over without it. Splitting one crossing at a time and finding the parts again by position — fresh keys make the parts hard to find.

Sources: https://raw.githubusercontent.com/openstreetmap/iD/develop/modules/validations/crossing_ways.ts — 0.75 m node reuse and 0.55 m shortest edge; https://raw.githubusercontent.com/openstreetmap/iD/develop/modules/geo/geom.ts — the cases its intersection test skips; https://josm.openstreetmap.de/export/HEAD/josm/trunk/src/org/openstreetmap/josm/tools/Geometry.java — JOSM's tolerances and ordering; https://doc.cgal.org/latest/Arrangement_on_surface_2/index.html — splitting each crossed edge, overlaps.

## How can the edge line run only along the surface's outline without a polygon union?

Map renderers draw every road's casing first and every fill on top, so fills hide the casings where roads join. OpenStreetMap Carto's history lists the costs: anything drawn between the two passes cuts the casing, caps of different styles leave marks where roads meet at an angle, each layer needs its own pair of passes, and roads that overlap without joining look joined.

Games that build paths from pieces mark each piece's edges instead. RollerCoaster Tycoon 2, as OpenRCT2 shows, stores edge and corner bits on each path tile and picks a sprite with borders only on unconnected sides. Cities: Skylines picks node meshes by flags such as end, junction, and bend.

For this milestone, each surface piece marks which of its edges are outline and which are joins. A segment body's sides are outline. A free end's square cap is outline. A corner arc's inner and outer edges are outline. A junction's rounded inside corners are outline, and its trim lines, where segments meet it, are joins. The renderer draws every fill, then an edge strip along each outline edge, inside the fill's boundary so it never spills onto a neighbor. Where two segments of one kind overlap without sharing a point, each one's outline crosses the other's fill, which is what the slice asks for. Three details need care:

- Each side edge must end exactly at its trim line, or the outline shows gaps or doubles.
- Where widths differ at a join, the wider end has a part the narrower piece doesn't cover. That part is outline.
- Strips and fills at one height fight in the depth buffer. They need draw order within one pass, a depth bias, or a small lift.

Rejected: casings under fills — they make unjoined overlaps look joined, which is the Planet Coaster 2 failure. A polygon union — the slice rejected it, and it is costly to rebuild on every edit. A screen-space outline pass — it can't tell a join from pieces that only touch.

Sources: https://github.com/openstreetmap-carto/openstreetmap-carto/issues/162 — layer and casing order problems; https://github.com/giggls/openstreetmap-carto-de/issues/46 — a casing cutting a neighbor's fill; https://imagico.de/blog/en/navigating-the-maze-part-1/ — cap marks and duplicated passes; https://raw.githubusercontent.com/OpenRCT2/OpenRCT2/develop/src/openrct2/paint/tile_element/Paint.Path.cpp — edge bits picking bordered sprites; https://cslmodding.info/asset/network/ — node meshes chosen by flags; https://learn.microsoft.com/en-us/windows/win32/direct3d9/depth-bias — depth bias for shapes at one height.

## How should a corner's arc be built, clamped, and split between its two segments?

If θ is the turn angle, the arc's tangent length is R·tan(θ/2). With unit directions in and out of the corner, tan(θ/2) is their cross product divided by one plus their dot product. That needs no trigonometry, which the simulation can't take from the C runtime (decision 0022). The arc's middle lies on the bisector of the corner. Each segment's ground line runs from its far end to its tangent point and then along half the arc to the middle, so the two lines meet there.

Tools clamp a radius that doesn't fit in two ways. Figma clamps each corner on its own to half the length of each edge beside it. CSS scales every radius of a box by one factor when any pair doesn't fit, so one short edge changes corners elsewhere. AutoCAD's fillet either fails with "radius too large" or skips the vertex. The Figma rule fits here: a corner's tangent length is at most half of each of its two segments, so each corner is clamped without looking at its neighbors.

The inner edge of the arc has radius R minus half the width. If R is less than half the width, it folds back on itself. Cities: Skylines refuses such curves as an invalid shape. Offset libraries such as CavalierContours replace a collapsing arc with the meeting point of the two offset lines. A game can either set a floor of half the width, where the inner edge closes to a point, or allow a smaller radius and miter the inner edge, as strokes do. The second keeps sharp corners possible, down to a radius of zero. If the two segments differ in width, the edge also has to change width across the arc.

Rejected: failing or skipping when the radius doesn't fit, as AutoCAD does — the player sees nothing happen. Scaling every radius together, as CSS does — one short segment changes distant corners. Bezier corners — their offsets are not Beziers and the inner edge pinches, while a circular arc's offsets stay circular.

Sources: https://developers.figma.com/docs/plugins/api/properties/nodes-cornerradius — clamping to half the edge; https://drafts.csswg.org/css-borders-4/ — CSS's scaling factor; https://cadmastercoach.com/commands/modify/fillet/radius-too-large — AutoCAD's fillet failures; https://github.com/jbuckmccready/CavalierContours — collapsing arcs in offsets; https://sandboxspirit.com/blog/art-of-roads-in-games/ — arcs offset cleanly where Beziers pinch.
