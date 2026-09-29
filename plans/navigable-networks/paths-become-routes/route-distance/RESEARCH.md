# Research: route-distance

## One field or one per network?

A shop anchors a node on each network: its front door on the guest network and its back door on the backstage network. The medium lets a source publish into a field once per resolution, and sampleField finds a source's entries by resolving their places on the network it is given. One field over both networks would therefore hold each shop's guest and backstage entries in one list. That is sound only while no carrier key is in both networks, since an entry published for one network would otherwise resolve on the other and be sampled there. Derived networks never share a key, but a hand-written save can give a path the key of the other kind's connector, which the routes spec already allows. Every consumer samples on the network it walks: guests on the guest network, supplies on the backstage one. So a field per kind, one template instantiated twice, keeps the entries apart at no cost to consumers. Evan chose this.

Rejected: one field whose entries name their network kind — entries at a node are sampled without the field's rule, so the consumer would filter them, and the field's contract would depend on who reads it.

## What is the next step?

The milestone's research names the next step as a carrier and a direction, since an edge index changes whenever an edge splits. That is not quite enough. A carrier may stop at one node twice, where a path crosses itself, and a mover at that node cannot tell from the carrier and direction which of the two stops to leave from. A mover standing at a junction on another carrier has to change carriers, and needs the stop to join at. So a step is a carrier and two of its stop distances, From and To: walk the carrier from its stop at From to its stop at To. At a node, From is the stop there. Inside an edge, the step names the whole edge, walked toward the end it chose, and the mover walks from where it stands. The carrier and stop distances survive re-derivation as well as the direction did, and the field is rebuilt with every resolution anyway.

Rejected: a Place of the next node — it names where to arrive but not which carrier to walk, and a node may join two others by different carriers. An edge index — edges renumber whenever a connection or junction splits one.

## Are the distances exact?

Distances are sums of doubles, and floating-point addition is not associative, so "the shortest distance" needs a stated order of addition to be testable. Rounded addition is monotone: if a is at most b, a + l rounds to at most b + l. Dijkstra relies only on that and on a + l never falling below a for positive l, so its result equals the least, over routes from the source, of the route's lengths added in turn from the source's end, starting from 0.0. Bellman-Ford relaxation from the same sources converges to the same values, since it computes the same least fixed point. A reference test can compare exactly, as long as it adds from the source's end. A walk along next steps then reproduces the sampled distance exactly when its lengths are added from the anchored end, because each node's distance is its next node's distance plus the step's length, rounded once. Inside an edge the step's length is the place's offset, which resolve computes as a difference of the same two distances, so it matches too.

One case breaks the walk: an edge so short that adding it does not change the distance, leaving two nodes at equal distance that could each choose the other as its next step. In a derived network every edge is longer than JUNCTION_TOLERANCE, 1 mm, and a park's distances are a few kilometers at most, where a double resolves far below a millimeter, so it never arises. The spec states the walk for networks where adding an edge's length always increases a distance.

Rejected: choosing next steps from the search's predecessor records — correct, but the choice would then depend on the order the priority queue settles ties, not on the network alone.

## Ties and determinism

Picking a node's next step after the distances are final, from every step that achieves its distance, by a fixed order (carrier key, then toward lower distances, then the lower From), makes the result a function of the network alone (principle 10). The search's own order then matters only for speed. The distances use only addition and comparison under the simulation's flags, and the cross-build check covers them through tests/parks/routes.park.

Sources: none beyond the project's specs (src/sim/medium/SPEC.md, src/sim/routes/SPEC.md) and plans/navigable-networks/paths-become-routes/RESEARCH.md. The floating-point argument is standard: IEEE 754 rounding is monotone.
