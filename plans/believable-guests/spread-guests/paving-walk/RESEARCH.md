# Research: paving-walk

The milestone's RESEARCH.md covers why guests walk inside the paving's outline and keep a clearance from it. This file covers how a guest finds its aim cheaply, how its place follows it along the route, and where a stranded guest goes.

## How does a guest find the farthest point of its route it can reach, at a cost that suits 2,000 guests every tick?

Detour's crowd keeps each agent's path in a corridor and never searches it again from scratch. Each frame, movePosition moves the agent and trims the corridor's start, so the path only ever shrinks from the front. optimizePathVisibility then casts one ray toward a point further along the path, clamped to a range (pathOptimizationRange), and if the ray is clear it shortcuts the corridor to that point. Topology optimization, the expensive search, is limited to one agent a frame and runs only after half a second without a replan.

The same idea fits a guest. Its aim is a point on its route, and the aim only moves forward. Each tick the guest tests the route's points beyond its aim, in order, with a straight line from where it stands, and moves the aim to each one that is clear. At the first point that isn't clear, it halves the stretch between its aim and that point a few times to find how far along it the aim can go. A stretch of route that runs straight costs one test, and the halving happens only where the route bends out of sight. Since the guest walks along a line it has already found clear, it never needs to test the line again while it walks it. Looking at most a fixed length of route ahead, as Detour's range does, bounds the work of each test and keeps it independent of the park's size (decision 0031).

Rejected: testing every point of the route from the guest's place each tick — on a bending route it repeats the same blocked tests each tick. A full funnel (string-pulling) over the pieces — the pieces don't form a mesh (the milestone's RESEARCH.md), and the outline needs only line tests. Re-aiming only every few ticks — it saves little once the aim only moves forward, and it adds state for when to re-aim.

Sources: https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourPathCorridor.cpp — movePosition trims the corridor as the agent advances, and optimizePathVisibility casts one ray within a range; https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourCrowd.cpp — visibility is optimized each frame and topology at most one agent a frame, after 0.5 s.

## How does a guest's place on the network follow it?

A guest walking across a wide street is never on the center line, so its place has to be worked out from where it stands. The nearest place on the carrier of the piece under it changes carrier abruptly. Cutting the inside of a corner, the guest crosses from one segment's half of the corner to the other's, and its place jumps past the node without ever being at it. Where a short segment's pieces stack on a wider street's, as at parks/busted-path.park's points 38 and 39, a guest walking along the street would stand on the side path's pieces and take a place on a path it isn't walking.

Detour's corridor answers the same question by where the agent has got to along its path. paving-walk does the same. A guest's place is the point of its route, between its place and its aim, nearest where it stands. It moves only forward along the route, it stays on carriers the route uses, and it passes each node of the route in order, so the guest chooses at nodes as it does today. It is the place decision 0038 describes: a guest walking on the paving along its route travels that route. Only after an edit, when the route may be gone, does a guest take its place from the piece it stands on.

A route's end is often out of the guest's reach. A shop's door and a dead end lie on the outline, and the box can't come closer to them than its clearance. So a guest that has walked as far along the last stretch of its route as its box allows counts as having reached the end of its route. That is no fixed distance from the end: where a connector meets its door at a slant, the door's end lies along the door's face, and the nearest point of the route the box can reach may be well over the clearance from the door. Where it can reach the end, as at a junction's middle, it walks onto it.

Rejected: the nearest place on the carrier of the piece under the guest, while it walks — places jump past nodes at corners, and stacked pieces give places on paths the guest isn't walking. The nearest place on the whole network — the same faults, and a guest near two paths could take the other's place.

Sources: https://github.com/recastnavigation/recastnavigation/blob/main/DetourCrowd/Source/DetourPathCorridor.cpp — the corridor's position advances along the path, never back.

## How far must a line keep from the outline, and what about a guest that is already closer?

A line keeps the box on the paving when it crosses no wall and keeps at least the clearance from each one. Two cases start closer than that. A guest arriving at the entrance stands at its door, on the door's wall. A guest carried across an edit may stand nearer a wall than its clearance. If every line had to keep the full clearance, neither could move at all. So a wall nearer than the clearance to where a line starts only has to stay as far away as it already is. The guest can walk along or away from it, never closer, and never across it. Every other wall needs the full clearance. Once the guest is clear of a wall, the full clearance applies to it again.

Rejected: no clearance for door walls — guests passing a door would walk with their boxes inside the shop. A guest that can't move until it is moved clear — a just-arrived guest would wait at the door for ever.

## Where does a stranded guest go?

Where the box fits is the paving shrunk by the clearance, its Minkowski erosion by a disc. Its edge is made of two kinds of curve: each wall moved inward by the clearance, and circles of the clearance's radius around the ends of walls. The nearest point of the shrunk paving to a point outside it lies on one of those. So the nearest fitting point is the nearest of the candidates on the walls near the guest: for each wall, the nearest point of its inward copy, and the point the clearance's distance from each of its ends toward the guest. Each candidate counts only if it lies on its own paving and the box fits there. Searching outward from the guest, ring of cells by ring of cells, stops once the nearest candidate found is nearer than any unsearched cell.

Rejected: the nearest place on the network, then moved toward where the guest stood — it is not the nearest fitting point, and it can still land too close to an edge. Walking the guest off deleted paving — a deepening candidate in the capability's brief.

Sources: https://en.wikipedia.org/wiki/Minkowski_addition — erosion as Minkowski difference with a disc.
