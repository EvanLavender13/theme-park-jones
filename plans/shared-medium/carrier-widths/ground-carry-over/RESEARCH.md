# Research: ground-carry-over

## How close must a carrier pass to a place's old ground to count as the same ground?

Detour's crowd re-places an agent when the polygon under it becomes invalid after a navmesh change. It searches for the nearest polygon inside a fixed box around the agent's position, sized from the largest agent radius. If no polygon lies inside the box, the agent becomes invalid and its owner deals with it. The search box is a tolerance chosen for agents that may stand anywhere on a wide surface.

A carrier is a center line, not a surface, so the question here is narrower. When an edit splits a straight run at a new point, each half's polyline holds the same ground, but a place's ground position on a half is interpolated between different points than before. The two positions then differ by rounding, around 1e-13 m at park scale. Exact equality would fail on that rounding, so carryOver needs a small tolerance. One millimeter is far above rounding and far below anything a player can see, so it counts a place as on the same ground only when it really is.

If a split falls inside a rounded corner, the halves' arcs may be sampled from different points, and the old ground can lie a few centimeters off the new polyline. Then the place is not on the same ground by this rule. It moves to the nearest place on its own carrier if that carrier remains, or its holder handles it as retired. Guests then move by at most those centimeters, which nobody sees.

Rejected: exact equality — rounding in the interpolated halves defeats it. A tolerance of half the carrier's width, like Detour's agent-sized box — a place on a deleted path would land on any path within half a width, so a deleted path's footfall would survive on a neighbor, and the rule "if the ground is gone, carryOver behaves as it does today" would not hold.

Sources: https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourCrowd.cpp — checkPathValidity re-places an agent with findNearestPoly inside m_agentPlacementHalfExtents, and marks it invalid when nothing is found.

## Which way should a carried guest walk on its new carrier?

A guest's walking direction is held relative to its carrier: forward means toward higher distances. If an edit carries the guest onto a carrier that runs the other way over the same ground, keeping the flag would turn the guest around. Comparing the direction of travel at the old place with the one at the new place tells whether the two carriers run the same way there. A negative dot product means they run opposite ways, and the flag flips. A zero dot product, where the carriers cross at right angles, can only happen at a node, where the guest chooses its next step anyway, so the flag is kept.

Only a guest that carryOver puts on another carrier needs this. If the guest stays on its own carrier, its flag already refers to that carrier. A guest whose ground is gone jumps to the nearest point of the network, and there is no way over the ground to keep, so it keeps today's behavior.

Rejected: storing the guest's heading as a ground direction instead of a flag — it changes the guest state, its saves, and every walking rule, for a case one comparison handles.
