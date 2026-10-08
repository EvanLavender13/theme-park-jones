# Research: spread-guests

The good-paths slice's RESEARCH.md covers why guests walk on the paving itself, how string-pulling along a network route works, and why guests sense one another through a ground field. This file covers what the milestone adds to that.

## What makes guests on a wide path look natural rather than formulaic?

Real walkers differ in speed. Weidmann's review gives a mean free walking speed of 1.34 m/s, with most people between about 0.97 and 1.65 m/s, and later field data fit a normal distribution with a standard deviation of 0.26 to 0.37 m/s. When every guest walks at one speed, guests that arrive together keep their spacing for their whole stay and nobody overtakes, which reads as marching. Varying speed per guest is the cheapest fix with the largest effect.

In two-way flow, walkers sort into lanes by direction, and in right-hand-traffic countries the lanes form on the right. A spread that ignores direction puts both directions everywhere and looks random. A spread biased to one side by the way each guest walks shows two streams on a wide street.

Walkers keep clear of walls and edges, so a path's outer strip is used less than its middle. A per-guest share of the width taken from a distribution that thins out toward the edges, such as the mean of two uniform draws, looks more natural than a flat one.

Rejected: one fixed sideways offset per guest beside the center line — it jumps where the line bends or the width changes, which the slice's research records. Shares spread evenly across the width — no streams by direction, and guests hug the edges as often as the middle. Speed varied only in drawing — the drawn guest would leave its simulated position behind.

Sources: https://www.researchgate.net/publication/303264378_Free_speed_distributions_for_pedestrian_traffic — free speed distributions; https://www.researchgate.net/figure/The-walking-speed-of-pedestrians-is-distributed-normally-with-an-estimated-mean-of-134_fig4_286071735 — mean 1.34 m/s, normal; https://www.sciencedirect.com/science/article/abs/pii/S0378437119302596 — lane formation and overtaking in two-way flow; https://arxiv.org/pdf/1606.03434 — two-way lanes form on the right.

## How does a guest walk a straight line on the paving and stay on it?

Detour answers this with three queries over a mesh of convex polygons. findNearestPoly finds the polygon under a point. raycast walks a straight line from polygon to polygon through shared edges, and stops at a wall. moveAlongSurface moves an agent toward a target and, if the target is off the mesh, stops it where it can reach, so it slides along walls. Each costs work in proportion to the polygons crossed.

The paving's pieces looked like such a mesh, but walkway-pieces' check over every park showed they aren't one. Pairing piece edges as Recast's buildMeshAdjacency does left edges unpaired in two parks. At a taper in main-street.park, a triangle with almost no area is dropped, so one long edge lies along two shorter ones. Where a segment's body stops right at its point, the junction's fan triangle across its end has no area either. Where a short segment lies inside a wider junction, as at points 38 and 39 of parks/busted-path.park, its pieces stack on the junction's instead of fitting beside them. None of this shows in play, because edge lines are hidden inside joined paving, but a mesh walk would find walls in the middle of the paving.

The visible outline doesn't have these faults. sim/park already hides each edge line where it lies inside joined paving, so what is left is the outline of each group of joined paving, with the paving on its left. Each walkway's end at its door has no edge line, because it meets its box. The same check showed that these lines, with the door ends added, close in every park: every line ends on another line, end to end or partway along it. So a guest can walk inside the outline instead of across pieces. A straight line from a guest stays on its paving when it starts there and crosses none of the lines that bound that paving. Lines of paving the guest's paving isn't joined to are ignored, so where unjoined paths overlap, a guest stays on its own path. A lookup by cell keeps each test to the lines near the guest. The pieces still tell a guest which segment, point, or walkway it stands on.

Rejected: walking the pieces as a navigation mesh — stacked pieces, dropped slivers, and seams at zero-length ends break edge pairing, and fixing them all means cutting joined paving into regions that don't overlap, a large change to sim/park that nothing visible needs yet. Building the union of each group's pieces just for guests — the outline already is that union's boundary.

## How does a guest's box, not just its centre, stay on the paving?

Navigation meshes keep an agent's body inside by eroding the walkable area by the agent's radius when the mesh is built, or by narrowing each portal by the radius. Walking inside the outline makes this a distance test instead: a guest's straight line, and each step it takes, must keep at least its clearance from every line that bounds its paving. That holds at concave corners too, where eroding pieces one by one would miss a neighbour's wall. A clearance of half a guest box's diagonal keeps the whole drawn box on the paving at any angle. A guest arriving at a door, or appearing at the entrance's door, has to stand at the door, so a walkway's door end is the one place where a guest's box may meet the paving's edge.

Rejected: keeping only the centre on the paving — at a sharp inside corner, half the box overhangs the grass, which the user ruled out.

Sources: https://recastnav.com/classdtNavMeshQuery.html — findNearestPoly, raycast, and moveAlongSurface; https://github.com/recastnavigation/recastnavigation/blob/main/Recast/Source/RecastMesh.cpp — buildMeshAdjacency pairs polygon edges by their vertices; https://moonjump.com/game-dev-mechanics-navigation-meshes-navmesh-how-it-works/ — eroding the walkable area by the agent's radius.

## When does a guest decide its next turn?

Walking straight toward the farthest visible point of a route only works if the route runs past the next node. A guest heading to a shop or home already knows its whole route, because route distance's next steps chain from node to node. A wandering guest picks its next edge only at the node, so until then its route ends at the node, and it would aim at the node's centre every time it nears one. On a street with a connector node at every shop, that pulls wanderers back to the middle every few meters. People decide a turn before they reach it. Making the choice for each node about 10 m before reaching it, and the wanderer's pick with it, keeps the route running past the next node, so a guest only bends toward a node when it actually turns there.

A node with exactly two edges and no anchor is a shaping point. It exists only so the drawn path can bend, so choosing there would make the number of points the player draws change how guests behave. Guests walk on through it without choosing.

Rejected: choosing at the node, as guests do today — wanderers would converge on node centres. Storing a long planned route for wanderers — a pick for the next node is all the aim needs.

## How do guests step around one another?

Predictive models avoid collisions by extrapolating each neighbour's motion and reacting to the time until the nearest approach, not to the distance alone. Karamouzas and colleagues' model does this with a small sideways force for the soonest predicted collisions in a short horizon, and it reproduces lane formation and gives shorter, less curved paths than distance-based forces. Reynolds' unaligned collision avoidance is the same idea in its simplest form. Each guest reads where its neighbours stood and how they moved at the end of the last tick, from guest presence, so the result doesn't depend on the order guests step in. Sampling within a few meters covers what a walker reacts to in a horizon of a couple of seconds, and it keeps a sample within the ground field's cell order (4 m cells). The sidestep is folded into the constrained move, so it can never push a guest off the paving.

Rejected: reciprocal velocity obstacles (RVO, ORCA) — more than simple sidestepping needs, as the slice's research records. Avoidance by distance alone — guests swerve at walkers that are passing safely.

Sources: https://sites.google.com/site/ikaramouzas/pam — the predictive avoidance model; https://www.red3d.com/cwr/steer/Unaligned.html — unaligned collision avoidance.
