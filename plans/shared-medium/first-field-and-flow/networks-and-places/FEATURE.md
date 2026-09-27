# Feature: Networks and Places

## Summary

networks-and-places adds the medium's network type, in the new module src/sim/medium. A Network is a value built from carriers, a node count, and anchors. A carrier is a line with a stable key, such as a drawn path or a box's connector. Its ground line is a polyline of points, each with the arc length its producer measured, and its stops are the distances where it meets nodes. Its edges are the stretches between consecutive stops, so they tile it exactly. A place, which is a carrier key and a distance along it, resolves to a node or to a point inside an edge. A place also gives a ground position, and a ground position snaps to its nearest place. An entity finds the nodes anchored to it. The network is registered as derived data, and makeParkSchema registers it, so resolvers can put networks on entities and the walk copies and hashes them. Tests build random synthetic networks, where each edge is its own straight carrier.

## Acceptance criteria

1. The Network constructor throws std::invalid_argument for each malformed input that src/sim/medium/SPEC.md lists, and for nothing else. Networks built from the same carriers and anchors given in different orders are equal, with the same hash, when each is held in a world.
2. edges() lists, for each carrier in ascending key order, one edge per pair of consecutive stops, in order of distance. Each edge holds its carrier, the nodes of its two stops, and their distances. Its length is ToDistance minus FromDistance.
3. resolve gives a NodePosition for a place at exactly one of its carrier's stop distances. It gives an EdgePosition for a place strictly between two consecutive stops, naming that edge, with FromOffset the place's distance minus the edge's FromDistance and ToOffset the edge's ToDistance minus the place's distance, both positive. It gives no position for a carrier not in the network, or for a distance that is NaN, below 0, or above the carrier's length. On random synthetic networks every node resolves from the place of each stop at it, and every place strictly inside an edge resolves to that edge.
4. nodePlace gives a node's stop on the carrier with the lowest key, at the lowest distance there, and resolve of it gives that node. nodeAnchor gives a node's anchored entity, or NULL_KEY. anchoredNodes gives the nodes anchored to an entity in ascending order, and nothing for an entity with no anchor. nodePlace and nodeAnchor throw std::out_of_range for a node that is not below the node count.
5. groundPoint gives no position where resolve gives none. At exactly a carrier point's distance it gives that point's coordinates. Between two points it gives the linear interpolation by distance that the spec states.
6. nearestPlace gives no place for an empty network or for a ground position that is not finite. Otherwise it gives a place that resolves, and no point of any carrier is nearer the given position than that place's ground point by more than 1e-9. Among equally near places it gives the lowest carrier key, and then the lowest distance.
7. addNetworkComponent registers Network as the derived type network, and makeParkSchema registers it. A world holding networks copies equal and hashes equal, and changing any carrier point, stop, or anchor of a network in it changes its hash. Its save holds no network. A Place held in a registered state component saves and loads back equal.

## Medium

This feature introduces no fields or flows. It provides the network type every field is sampled on:

- navigable-networks will produce the park's guest and backstage networks in it, from path intent, with carriers keyed by path and by box and face, and connector nodes anchored to boxes.
- The fields features sample at places resolved against it. place-carry-over moves places between two networks.
- believable-guests will hold places, and plausible-operations will find its shops' and depot's anchored nodes.

## Principle checks

- Principle 4: criteria 2, 3, and 6. Edge lengths and a place's offsets are differences of distances along its carrier, which the producer measured along the route. nearestPlace is the medium's only straight-line measure, and it only snaps a ground position onto a carrier.
- Principle 10: criterion 1's order independence and criterion 6's tie rule. A network's contents, edge order, and node places depend only on its carriers and anchors, never on the order they were given in.
- Principle 1: criterion 7. A network is derived data and never appears in a save.
- Principle 2: criteria 3, 5, and 6. A place on a missing carrier, or off the end of one, resolves to nothing, and an empty network is legitimate. Neither is an error.
- Principle 6: Network keeps its members private, and holders reach it only through its const functions.

## Spec changes

Create src/sim/medium/SPEC.md:

"# medium

The shared medium (principles 3 and 6, decision 0005): the networks every field is sampled on. It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Networks

A Network is a value built from carriers, a node count, and anchors. A carrier is a line with a stable key, such as a drawn path or a box's connector, that edges are stretches of. Its ground line is a polyline of CarrierPoints. Each point is an x and z on the ground and its arc length from the carrier's start, which the producer supplies, so every distance is the producer's own. Its stops are the distances where it meets nodes, the first at 0 and the last at its length, so its edges, the stretches between consecutive stops, tile it exactly. A junction is a node where several carriers stop. A node may be anchored to one entity, and an entity may anchor several nodes, so an entity finds where it meets the network without reading how the network was derived.

The constructor sorts carriers by key and anchors by node, so the order they are given in makes no difference. It throws std::invalid_argument for any of these:

- a carrier key that is NULL_KEY or repeated;
- a carrier with fewer than two points, a coordinate or distance that is not finite, a first point whose distance is not 0, or point distances that do not strictly increase;
- a carrier with fewer than two stops, a stop distance that is not finite, a first stop not at 0, a last stop not at the last point's distance, stop distances that do not strictly increase, or a stop naming a node not below the node count;
- a node that no carrier stops at;
- an anchor naming a node not below the node count, naming NULL_KEY, or on a node already anchored.

The default network is empty.

edges() lists the edges: carriers in key order, and each carrier's edges in order of distance. An edge holds its carrier, From and To, the nodes at its two stops, and their distances. Its length is ToDistance minus FromDistance.

A Place is a carrier key and a distance along it. It names the same ground position however the carrier is cut into edges. resolve gives a place's position in the network. That is the node of the stop at exactly its distance, or the edge whose stops enclose it, with FromOffset its distance minus the edge's FromDistance and ToOffset the edge's ToDistance minus its distance. It gives no position when the carrier is not in the network, or when the distance is NaN, below 0, or above the carrier's length. That is a legitimate result, not an error (principle 2).

nodePlace gives a node's place: its stop on the carrier with the lowest key, at the lowest distance there. nodeAnchor gives a node's anchored entity, or NULL_KEY. anchoredNodes gives an entity's anchored nodes in ascending order. nodePlace and nodeAnchor throw std::out_of_range for a node not below the node count.

groundPoint gives a place's ground position, or none where resolve gives none. At exactly a point's distance it is that point's coordinates. Otherwise, with a and b the points before and after the distance d, it is a.X + t * (b.X - a.X) and a.Z + t * (b.Z - a.Z), where t = (d - a.Distance) / (b.Distance - a.Distance).

nearestPlace gives the place whose ground position is nearest a given one. It projects the position onto each segment of each carrier's polyline, clamped to the segment, and a projection at the fraction t of a segment from a to b is the ground point a.X + t * (b.X - a.X), a.Z + t * (b.Z - a.Z) at distance a.Distance + t * (b.Distance - a.Distance). When t is 0 or 1 it is exactly a or b, with its coordinates and distance, so a junction is equally near on every carrier that stops there. Ties go to the lower carrier key, then the lower distance. It is the medium's only straight-line measure, and it is used only to snap a ground position onto the network (principle 4). An empty network, or a position that is not finite, gives none.

addNetworkComponent registers Network as the derived component type network. Producers put networks on entities they derive in resolution. Place has a visitFields, so holders can keep places in their own components."

src/sim/SPEC.md: in the paragraph beginning "Component types are registered with a WorldSchema", replace "It registers nothing yet." with:

"It registers the medium's types first (sim/medium/SPEC.md), and so far nothing else."

## Files affected

- Create: src/sim/medium/SPEC.md
- Create: src/sim/medium/network.h
- Create: src/sim/medium/network.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/SPEC.md
- Modify: src/sim/park_schema.cpp
- Modify: plans/shared-medium/first-field-and-flow/MILESTONE.md (its open question on carrier geometry is resolved)
- Test pass: files under tests/, written by the test-writer agent. These are tests/sim/medium/network_test.cpp, a random synthetic network builder in tests/sim/support/, and the additions to tests/sim/CMakeLists.txt, whose tests join tpj_sim_tests.

## Dependencies

- deterministic-simulation's world-as-value: the walk, component registration, derived keys, and makeParkSchema. Merged.

## Out of scope

- Carrying a place across a re-derivation: place-carry-over.
- Fields sampled at places: resolved-fields and stepped-fields.
- Deriving networks from path intent, arc lengths by quadrature, and the ImGui graph view: navigable-networks.
- Heights: the park is flat, so ground positions are x and z.
- A spatial index for nearestPlace: it scans every segment, which suits the slice's few paths. Profiling can add one later.

## Open questions

None.
