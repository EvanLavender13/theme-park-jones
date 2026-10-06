# Research: frontage-snap

The slice's RESEARCH.md (plans/slices/good-paths/RESEARCH.md) names Platter and Anno 117 as the prior art, and advises deriving frontage from the segment's center line and width, not from the drawn outline. This file adds what Platter's source does.

## How does Platter snap a parcel to a road?

Platter is a Cities: Skylines 2 mod that places zoning parcels. Its road-side snap works like this:

- A road is near when the cursor lies within half the road's width, plus half the parcel's smaller side, plus 4 m, of the road's center curve. For a 3 m path and a 6 m deep shop, that is 8.5 m from the center line.
- Among the near roads, it takes the road edge curve nearest the cursor. Each road has a left and a right edge, and the curves around its nodes count too. The edge the cursor is nearest picks the side.
- The parcel faces the road, square to the edge's tangent at the nearest place on it. Its center sits on the line out from that place, half the parcel's depth plus the setback back from the edge. The parcel keeps the cursor's position along the road.
- The setback is 0 m by default. The player can change it from 0 m to 8 m in the tool's options.
- Other snaps compete by priority: sliding along a neighbouring parcel's side, and lining up with its front corners. The road-side snap is one level among them.

For this game, these findings suggest the following:

- Measure from the published center line and width. Platter measures from the road's drawn edge curves, which this game derives from the same two values, so the result is the same on a straight segment. On a rounded corner, the ground line already holds the arc, so the nearest place on the ground line gives the facing on curves too.
- Make a segment near when the pointer lies over the place where the fronting box would stand, or between that place and the segment. That is half the width, plus the setback, plus the box's depth. For a shop on a 3 m path, it is the same 8.5 m that Platter's rule gives, and it scales with each box's size.
- Pick the side from where the pointer lies, and keep the pointer's position along the segment.

Rejected:

- A setback the player changes while placing. The milestone fixes it at 1 m, and the backlog already holds Platter-style placing with a chosen setback and rows of shops.
- Lining a box up with its neighbours' sides and fronts. That belongs to placing rows of shops, which the same backlog item holds.
- Snapping around a path's free end or a sharp corner's outside, as Platter's node curves do. The box would swing around the end point, which reads as a fault. A box near the end of a path takes no frontage from it, and the player can place it freely.
- Reach in screen pixels. The box is drawn to scale with the path, so a reach set by the box's own depth catches at the same place on screen relative to both, at any zoom.

Planet Coaster has no snap that turns a shop to face a path. Its players align shops by hand with the advanced move tool, and they keep asking for an alignment snap.

Sources:

- https://github.com/lucarager/CS2-Platter — Platter/Systems/Tool/P_SnapSystem.cs and P_SnapSystem.AdhocParcelSnapJob.cs: snap modes and priorities, the reach from the center curve, the nearest edge curve picking side and facing, and the setback range.
- https://forums.planetcoaster.com/showthread.php/14269-Aligning-Building-to-path-instead-of-path-to-building — players asking to align a building to a path.
- https://steamcommunity.com/app/493340/discussions/0/1479856439039192246/ — no snap of objects to paths in Planet Coaster.
