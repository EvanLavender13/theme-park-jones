# Research: walkway-pieces

## How can a check show that the paving guests walk on has no holes a walker can't cross?

Navigation meshes join polygons into a walkable graph by matching edges. Recast's buildMeshAdjacency keys every polygon edge by its two vertices and pairs each edge with the edge running the other way between the same two vertices in another polygon. An edge left without a partner is a wall. The same rule shows where a mesh is broken. If the outline is known, any unpaired edge that isn't on the outline is a hole or a crack. A T-junction is one long edge on one side and two shorter ones on the other. It also leaves edges unpaired, and a walker can't cross it, though nothing visible is missing.

The paving's pieces are convex and counter-clockwise, so two pieces that share an edge list it in opposite directions. The visible edge lines run along the outline of joined paving, with the paving on their left (src/sim/park/SPEC.md). So a check can pair every guest piece edge with a reversed edge of another guest piece, with ends equal within the surface's own SAME_POINT. It then requires each unpaired edge to lie along a visible guest edge line. An edge paired twice, or paired with an edge running the same way, means pieces overlap. The check reports each such edge with its owner, and reports each park as clean or not.

Walkways overlap the paving they join instead of sharing edges, so they are left out of this check.

Run over every park, the pairing check found unpaired edges in main-street.park and parks/busted-path.park. None of them showed in play: they came from a dropped sliver at a taper, a zero-area fan triangle where a body stops at its point, and a short segment whose pieces stack on a wider junction's. So guests walk inside the paving's outline instead (the milestone's RESEARCH.md), and the check that matters is that the outline closes. The visible guest edge lines, with each walkway's door end added, close when every line's start and end lies on another line. Lines meet partway along each other, not only end to end, where hiding cuts a line at the edge of joined paving. That check found no open end in any park.

Rejected: comparing the union's area with the sum of the pieces' areas — it shows overlaps but not cracks of zero width, and says nothing about where they are. Rasterizing the paving and looking for gaps — too coarse to see slivers under LEAST_AREA, and blind to T-junctions.

Sources: https://github.com/recastnavigation/recastnavigation/blob/main/Recast/Source/RecastMesh.cpp — buildMeshAdjacency pairs polygon edges by their vertices; src/sim/park/surface.cpp — how pieces and edge lines are built.
