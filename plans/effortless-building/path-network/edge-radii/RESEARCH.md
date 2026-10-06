# Research: edge-radii

The milestone's RESEARCH.md covers arcs and tangent lengths. paved-surface's RESEARCH.md covers fillets at junctions. This file covers giving each edge its own radius, and how to bound a sharp outside corner.

## How do tools let the player round each corner of a shape on its own?

Vector editors give each corner of a shape its own radius, set by dragging a handle at that corner or by typing a value. Figma and Illustrator both keep one radius per corner, and a corner with no radius of its own takes the shape's. City builders rarely do this. Cities: Skylines rounds junction corners from a per-road setting, and mods add a per-junction offset, not a per-corner radius.

A fillet between two straight edges has a closed form. Its center lies at its radius from each edge. Where the edges are parallel to the two segments, each touching point's distance along its segment is linear in the radius, for an inside corner and for an outside one. The arc's middle lies on the line that halves the angle between the edges. It moves along that line a fixed distance for each meter of radius, 1/sin(g/2) − 1, where g is the angle between the two segments. So a handle at the arc's middle maps a drag to a radius with no trigonometry.

These findings suggest the following:

- Give each turn of the outline at a point its own radius, with a handle at the curve's middle.
- Treat a corner's inner and outer edges and a junction's inside corners as the same thing: a fillet between two segments' facing edges, inside or outside.
- Start every edge sharp, and let the corner's radius handle keep curving the inside edge and the line guests walk.

Rejected: a radius on the point that every edge there shares — the user asked to control each corner. Arcs concentric with the ground line only — an edge then can't be rounded apart from the corner's radius. Edges that follow the corner's radius until set — the player can't keep the outside sharp while curving the inside.

## How do stroke renderers bound a sharp outside corner?

Where two thick lines meet at a sharp angle, the outside edges meet far from the joint. SVG limits this with stroke-miterlimit, the ratio of the miter's length to the stroke's width, 4 by default. Past the limit, it cuts the corner off with a bevel. A limit of 4 bevels angles tighter than about 29°.

For paving, a bevel adds a new shape. Growing the radius until the corner reaches no farther than the limit keeps the outline made of fillets only. The limit then works like the reach limits paved-surface already has.

Rejected: no limit — the corner runs to infinity as a bend folds back. A bevel — it adds a third kind of edge for a rare case.

Sources: https://developer.mozilla.org/en-US/docs/Web/SVG/Reference/Attribute/stroke-miterlimit — miter limit as a ratio, default 4, bevel past it; https://css-tricks.com/mastering-svgs-stroke-miterlimit-attribute/ — the angles each limit bevels; https://cslmodding.info/asset/network/ — Cities: Skylines rounds junction corners from per-road settings.
