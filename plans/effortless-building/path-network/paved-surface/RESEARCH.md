# Research: paved-surface

The milestone's RESEARCH.md covers how the edge line marks outline edges, and how a corner's arc is built. This file covers how a junction's rounded inside corners are built.

## How do road builders round the inside corners where three or more roads meet?

Road generators sort the roads leaving a node by angle. Between each road and the next, they fit a fillet: a circular arc that touches the facing edge of each road. Each road is cut back to where its fillet touches it, and the hub between the cut ends is filled. The Godot road generator trims each arm by the full junction radius so the arms' ends meet the fillets. It builds the hub from rectangles and quarter discs. Street design guides call the same arc the curb return. They keep its radius as small as practical, so a wider road gets a wider arc only where traffic needs it. The milestone already sets the radius at half the narrower width of the two roads.

A fillet's center lies at its radius plus each road's half width from each road's center line. That gives a linear solve with no trigonometry. Where two roads leave a node nearly in line, the fillet's touching points run far down the roads. Generators then cap the trim, and the arc shrinks to fit. If the roads leave the node more than half a turn apart, no fillet fits between them, and the outline goes around the node.

These findings suggest the following:

- Sort the segments at a point by direction. Fit a fillet between each segment's edge and the facing edge of the next one.
- Cut each segment back to the farther of its two fillets' touching points. That distance is at most half the segment's length. Shrink a fillet that doesn't fit, down to a sharp corner.
- Where the gap to the next segment is half a turn or more, run the outline around the point instead.
- Fill the hub as a fan of triangles from the point.

Rejected: a union of every segment's shape — the slice rejected it, and it joins separate paths that only overlap. Rounding with an offset in and back out, as Clipper2 does — it also rounds where separate paths only come near each other. Arcs at a fixed radius whatever the widths — the milestone asks for rounding that grows with width.

Sources: https://github.com/TheDuckCow/godot-road-generator/issues/121 — trimming arms by the junction radius and filling the hub; https://github.com/EVEngine/EVEngine/pull/424 — fillets at crossings in a procedural road system; https://sdg.minneapolismn.gov/design-guidance/intersections/curb-return-radii — curb-return radius kept as small as practical; https://www.redblobgames.com/articles/curved-paths/ — edges as offsets along the path's normals.
