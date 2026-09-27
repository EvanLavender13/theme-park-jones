# medium

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

nearestPlace gives the place whose ground position is nearest a given one. It projects the position onto each segment of each carrier's polyline, clamped to the segment, and a projection at the fraction t of a segment from a to b is the ground point a.X + t * (b.X - a.X), a.Z + t * (b.Z - a.Z) at distance a.Distance + t * (b.Distance - a.Distance). When t is 0 or 1 it is exactly a or b, with its coordinates and distance, so a junction is equally near on every carrier that stops there. A segment whose two points share a ground position projects to a, with t 0. Ties go to the lower carrier key, then the lower distance. It is the medium's only straight-line measure, and it is used only to snap a ground position onto the network (principle 4). An empty network, or a position that is not finite, gives none.

addNetworkComponent registers Network as the derived component type network. Producers put networks on entities they derive in resolution. Place has a visitFields, so holders can keep places in their own components.
