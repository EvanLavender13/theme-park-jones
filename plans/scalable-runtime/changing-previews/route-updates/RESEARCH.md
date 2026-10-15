# Research: route-updates

## What route distance does on each changed frame

Route distance is 25% of a shop-drag frame and 21% of a path-drag frame on full.park (build/perf/layered-1.sleepy to layered-4.sleepy). Almost all of it is searchEntries. For each kind's network, publishRouteDistance runs one Dijkstra per source over the whole network and publishes every reached node's entry in node order. full.park has about 90 guest sources: every box with a door, and the entrance.

The network is rebuilt whole on every resolution. Nodes are numbered in order of first stop, carriers in key order (networks.cpp addStops), so any new or moved stop renumbers every later node. Entries are placed at each node's nodePlace, the node's stop on its lowest-keyed carrier, which renumbering doesn't change.

Edge lengths are double differences of stop distances. Moving a shop moves its connector's meeting stop along a path, cutting that path's edges differently. The halves' doubles can sum to a different last bit than the old edge, so every distance beyond the cut can change in its last bit for every source. With lengths in whole millimetres, the halves sum exactly to the old length (MILESTONE.md Decisions, 1).

The resolved layer is emptied and every source republished on every resolution (field.h emptyResolvedEntries, publishResolved). The settling finisher compares sources only where they have readable stepped entries. No system publishes stepped route distance, so for route distance it compares nothing.

## How to update only what changed

Within one shop-drag frame, for every source but the dragged shop, only a few nodes change: the moved meeting stop, the nodes on either side of it, and the shop's door. Their distances and next steps change. Every other node's entry stays the same.

LPA* without a heuristic, which is Ramalingam and Reps' DynamicSWSF-FP, keeps for each node its distance and a one-step lookahead, rhs, the least of its neighbours' distances plus the edge. A node is inconsistent when the two differ. It processes inconsistent nodes in order of the smaller of the two, and touches only nodes whose distance changes, plus their neighbours. When a moved stop splits an edge, the new stop's lookahead is smaller than the far node's old distance, so the new stop is settled first. The far node is then consistent again and nothing beyond it is visited.

Rejected:

- Marking every node whose next step passes through a changed edge as affected, then searching them again. A moved stop on a path is on the routes of everything beyond it, so this visits the whole subtree beyond the cut for every source, though no distance there changes.
- Deciding support from old distances only (Ramalingam and Reps' deletion phase run before insertions). The new stop has no old distance, so the far node has no support and the whole subtree beyond is marked again.
- Running a full search only for sources near the edit. Every source's routes reach the dragged shop's door, so every source changes.

## Keeping entries when nodes renumber

Entries are kept in each source's slot ordered by place, carrier key then distance. A place names the same ground position however nodes are numbered. A new node is matched to an old one when the old network has a node at the same nodePlace. Matching every node once per resolution is the renumbering the milestone allows to grow with the park.

Writing a source's changes moves the rest of its list after the first change. On a path drag on full.park that is about 90 sources of a few hundred 48-byte entries, about 1.7 MB per frame. It is a product of sources and nodes, a defect recorded in the milestone's whole-park work for a later feature.

Rejected:

- Keeping entries in node order. Renumbering would rewrite every source's list.
- A per-network place index shared by all sources, with each source's entries in an array by stable place id. It avoids the shift but needs its own sampling, separate from the medium's field slots, which every reader of route distance uses.

Sources: src/sim/routes/route_distance.cpp; src/sim/routes/networks.cpp; src/sim/medium/field.h; src/sim/medium/network.h; https://idm-lab.org/bib/abstracts/Koen02e.html (LPA* and D* Lite); https://research.cs.wisc.edu/wpis/abstracts/jalg96.abs.html (DynamicSWSF-FP).
