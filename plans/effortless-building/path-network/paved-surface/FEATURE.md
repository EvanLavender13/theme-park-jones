# Feature: Paved Surface

## Summary

Paths draw as one paved surface instead of separate ribbons with round ends. Where three or more segments meet, the inside corners are rounded, more for wider paths. A thin edge line runs along the surface's outline and never across a join. Each free end is cut square at its point. A bend's outer edge stays round, and its inner edge goes sharp where the bend is tighter than half the width. sim/park derives the surface from the segments and points, the validity check refuses boxes against it, and the renderer draws it, so what is drawn and what is refused are one shape. The ghost of an edit draws the same way.

## Acceptance criteria

1. A capture of tests/parks/fed.park and one of tests/parks/stress/full.park show the paths as one surface. Junctions have rounded inside corners, an edge line runs along the outline only, and free ends are cut square. (capture)
2. Draw a guest path through a sharp bend. The outer edge is round and the inner edge is sharp. Drag the bend's radius handle past half the width, and the inner edge becomes a curve too. (play)
3. Draw a guest path along an existing guest path so the two overlap without joining. Each one's edge line shows across the other. (play)
4. Try to place a shop so it overlaps a junction's rounded inside corner. The ghost is red and the click places nothing. Move it just clear of the drawn paving, and it places. (play)
5. In a new park, widen the entrance path to 10 m. The edit is accepted, because the square end no longer reaches the entrance. Before this feature, the round end ran into it. (play)
6. While moving a point at a junction, the ghost shows the paving as it will be drawn, with rounded corners and edge lines, and the commit draws the same shape. (play)
7. The park world checks pass over every park, so every checked-in park is still valid. (ctest) Every scenario except park-edits simulates unchanged against main. (`tpj_scenarios --compare`)

## Tests

None. The park world checks already cover validity over every park, and the surface adds no intent.

## Medium

No fields or flows. The surface is derived from park intent and isn't saved. sim/park publishes it through parkSurface and editedSurface. The validity check and the renderer read it. Routes still reads ground lines and is unchanged.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: add parkSurface to the public queries, and editedSurface to the public functions.
- Physical validity: replace "A segment occupies its ground line widened by half its width, with round ends." with "Paths occupy the paved surface. A footprint meets a path when it overlaps one of the surface's pieces by more than CONTACT_TOLERANCE. A segment leaves the park when its ground line comes nearer the park's edge than half its width."
- Commands: replace "is not empty, stays in the park, and meets no solid" with "is not empty and stays in the park, and no piece of the surface the command adds or changes meets a solid".
- After editedSegments: "editedSurface gives the pieces and edge lines of each segment and point whose part of the surface an edit changes, as they would lie after the edit, in ascending key order of the segment or point they belong to. It gives none when editedSegments gives none."
- A new section after Ground lines:

> ## Paved surface
>
> parkSurface gives the paved surface the park's segments make. It is a list of pieces and a list of edge lines, each belonging to a segment or a point, in ascending key order of what it belongs to. Each piece is a convex polygon on the ground, with its corners counter-clockwise seen from above. Each edge line runs along the surface's outline, with the paving on its left seen from above. A segment with an empty ground line adds nothing.
>
> - A segment's body is its width wide and runs along the straight part of its ground line. At each end it stops where the surface at that point begins.
> - At a point that only one segment uses, the body ends square at the point.
> - A corner's rounding belongs half to each of its two segments, split at the arc's middle. Around the arc, the paving takes the narrower of the two half widths, and the wider segment's end steps in to it. Its outer edge follows the arc that far outside it. If the radius is at least that half width, its inner edge follows the arc that far inside it. Otherwise the corner is sharp on the inside: the two inner edges run straight on until they meet. If one segment's inner edge would first meet the other's square end at the point, it runs to that end instead. If neither fits within the reach of each segment, the inner edge steps straight across at the point, from one segment's inner edge to the other's.
> - Any other point that segments use is a junction, which belongs to the point. Between each segment and the next one around the point, the inside corner is rounded with an arc that touches both segments' facing edges, with a radius of half the narrower width. Where the two segments are less than a quarter turn apart, the radius is made smaller, so that the arc reaches no farther past where the edges meet than its radius. Each segment stops where the farther of its two arcs touches it, within its reach. If an arc doesn't fit, it is made smaller. If no arc fits, the outline steps straight across from one edge to the other at the point. If the next segment lies half a turn or more around, the outline runs around the point from one edge to the other instead, at the narrower half width, and the wider segment's end steps in to it.
> - A segment's reach at one end is its whole straight length if no other segment uses its other end, and half of it otherwise.
> - Along curves, the outline is made of straight pieces, each within ARC_TOLERANCE of its curve.
>
> Edge lines run along each side of each segment, across each square end, along a corner's inner and outer edges, and around a junction from one segment to the next. They belong to the point they run around. No edge line runs where a body meets a corner's half or a junction. Segments are joined when a chain of shared points links them. Where an edge line lies inside the paving of segments joined to its own, that part of it is hidden. Where segments that aren't joined overlap, each one's edge lines run across the other.

src/render/SPEC.md:

- The park: replace "Path ends are round, so paths and walkways meeting at a node join with no notch at any angle." with "Paths draw as the paved surface parkSurface gives: each piece filled in its kind's color, and a strip EDGE_LINE_WIDTH wide along each edge line, on the paving's side, in a darker shade of that color, above the fills. Where an edge line turns sharply, its strip has no gap. A walkway ends round where it meets its path."
- Ghosts: the first bullet draws every piece and edge line editedSurface gives, the same way, instead of the segments editedSegments gives. appendEntity draws a segment as its own pieces of the surface: its body and its halves of the corners at its ends.

## Files affected

- Create: src/sim/park/surface.h, src/sim/park/surface.cpp
- Modify: src/sim/CMakeLists.txt, src/sim/park/edits.h, src/sim/park/edits.cpp, src/sim/park/SPEC.md
- Modify: src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/SPEC.md

## Dependencies

corners-and-widths, for corners and per-segment widths. Merged.

## Out of scope

- Separate inner and outer edge radii, and a radius for each inside corner of a junction (edge-radii, the next feature). Until then, junction corners take the default rounding.
- At very sharp angles, where pieces can't stop within a segment's reach, the pieces overlap and an edge line shows across the overlap.
- Walkways keep their round ends and aren't part of the surface.
- Hover highlights of a junction. A segment's highlight covers its body and corner halves, not the junctions at its ends.

## Open questions

None.
