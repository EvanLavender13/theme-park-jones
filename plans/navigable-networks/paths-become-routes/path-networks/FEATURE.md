# Feature: Path Networks

## Summary

Resolution derives the park's guest and backstage networks from its paths. A new module, src/sim/routes, registers the resolver path-networks in makeParkSchema. For each path kind it builds a Network whose carriers are that kind's paths, each keyed by its path and running along its ground line. Nodes sit at each path's two ends and wherever two lines of the kind meet within JUNCTION_TOLERANCE, 1 mm: at a snapped point, a crossing, a line passing through another's line point, along a collinear overlap, and where a path crosses itself. Nearby meetings on a carrier are grouped into one stop, and a union of the stops each meeting joins gives the nodes, numbered by a fixed walk. Other modules read a kind's network through parkNetwork. tests/parks/routes.park, a park with crossing and snapped guest paths, a backstage path, a shop, and a depot, is checked in, so the cross-build check runs the new derivation.

## Acceptance criteria

1. For a resolved world and each path kind, parkNetwork's carriers are exactly the paths of that kind whose ground line is not empty, each keyed by its path's key and with groundLine of the path's points as its points. For a world never resolved, parkNetwork is an empty network.
2. For every meeting that src/sim/routes/SPEC.md defines between two segments, each segment's carrier has a stop within JUNCTION_TOLERANCE of the meeting's distance on it, and those two stops are at the same node.
3. Every stop other than a carrier's first and last lies within JUNCTION_TOLERANCE of the distance on its carrier of some meeting. So a path whose line meets no line of its kind, nor itself, stops only at its two ends, at nodes no other stop has.
4. Consecutive stops of a carrier lie more than JUNCTION_TOLERANCE apart.
5. Nodes are numbered in order of first stop: walking carriers in key order and each carrier's stops in ascending distance, each stop's node is either the node of an earlier stop in the walk or one more than the highest node before it, starting from 0.
6. For any world a randomized sequence of park edits reaches, both networks equal those of the world loaded from its save and resolved, and for any edit, the networks of makeCandidate with the edit equal those of the world after a cycle applies it.
7. Every world a randomized sequence of park edits reaches, including ones with no paths, a lone path, or paths of one kind only, resolves without throwing.
8. tests/parks/routes.park is physically valid, loads with makeParkSchema and saves back to identical text, and once resolved every node of its guest network is reachable from every other along its edges.

## Medium

None consumed. The feature produces the park's guest and backstage networks in shared-medium's Network type, which box-connections extends with connectors and anchors, graph-view draws, and route-distance samples. It reads park intent through parkPaths and groundLine (decision 0025).

## Principle checks

- Principle 1: the networks are derived data. They equal those derived again after a save and load (criterion 6), and saveWorld never writes a derived type (src/sim/SPEC.md).
- Principle 2: every world an edit sequence reaches resolves (criterion 7), whatever paths it holds.
- Principle 4: every carrier's points are its path's ground line (criterion 1), so every edge's length is a stretch of a drawn line.
- Principle 6: the module reads intent only through parkPaths and groundLine, which the private header check enforces, and publishes only the Network type.
- Principle 8: a candidate's networks equal the committed world's (criterion 6).
- Principle 10: stops and node numbers follow fixed rules (criteria 4 and 5), and the cross-build check runs routes.park on both builds.

## Spec changes

Create src/sim/routes/SPEC.md:

    # routes

    Navigable networks (plans/navigable-networks): the park's guest and backstage networks, derived from park intent. It is part of tpj_sim and follows its contract (src/sim/SPEC.md). It reads intent only through parkPaths and groundLine (sim/park/SPEC.md, decision 0025), and publishes networks in the medium's Network type (sim/medium/SPEC.md), so other modules read them through parkNetwork and the Network type's queries, never through how they were derived (principle 6).

    ## Networks

    addRoutes registers the resolver path-networks, and makeParkSchema calls it after addParkEdits. In each resolution, path-networks derives one network for each PathKind and puts it on the entity keyed networkKey(kind), deriveKey(NULL_KEY, hashName("network"), the kind's value), which it creates with createDerivedEntity, replacing any network there. parkNetwork gives a kind's network, or an empty network when the world holds none, as before its first resolution. Networks are derived from intent alone, so saves never hold them (principle 1).

    A kind's network has one carrier for each path of that kind whose ground line is not empty, keyed by the path's key, with the ground line as its points. So every edge is a stretch of a drawn line, measured along it (principle 4). A path of the other kind is never a carrier, so lines of different kinds never share a node, even where they cross.

    ## Meetings

    A segment runs between consecutive points of a carrier, from a to b. Two segments can meet when they are on different carriers of the network, or on one carrier and not neighbors along it, their first points' indices differing by more than one. The side of a point p from the segment from a to b is (b.X - a.X) * (p.Z - a.Z) - (b.Z - a.Z) * (p.X - a.X).

    Two segments, from a to b and from c to d, cross when the sides of c and d from the first are nonzero with opposite signs, and so are the sides of a and b from the second. Crossing segments meet at their crossing, at the fraction s_a / (s_a - s_b) along the first and s_c / (s_c - s_d) along the second, where s_a and s_b are the sides of a and b from the second and s_c and s_d those of c and d from the first. A segment's distance at the fraction u is a.Distance + u * (b.Distance - a.Distance), clamped to between a.Distance and b.Distance.

    Whether or not they cross, two segments also meet at each of their four ends that lies within JUNCTION_TOLERANCE, 1 mm, of the other segment. The meeting's distances are the end's own distance, and on the other segment the distance of the end's projection onto it. The projection of p onto the segment from a to b is at the fraction t = ((p.X - a.X) * dx + (p.Z - a.Z) * dz) / (dx * dx + dz * dz), with dx = b.X - a.X and dz = b.Z - a.Z, clamped to between 0 and 1, and its point is a.X + t * dx, a.Z + t * dz, or exactly a or b when t is 0 or 1. The end lies within the tolerance when its offset from that point has dx * dx + dz * dz at most JUNCTION_TOLERANCE squared. Two segments that come within the tolerance without crossing are nearest at an end of one of them, so the meetings cover every place two lines come that close: a snapped point, a crossing, a line through another's line point, and the line points along a collinear overlap, including its two ends, even where rounding leaves nearly collinear segments crossing each other at a shallow angle.

    ## Stops and nodes

    A carrier's stops come from 0, its length, and its distance in each meeting. They are sorted ascending and grouped from the lowest: a group starts at the lowest distance not yet grouped and takes every distance at most JUNCTION_TOLERANCE above that one. Each group is one stop, at its first distance, except the group holding the length, which stops at the length. So every meeting lies within the tolerance of a stop, and consecutive stops lie more than the tolerance apart. A meeting joins the stop holding its distance on one carrier and the stop holding its distance on the other into one node, and joining is transitive. A meeting of a carrier with itself whose two distances fall in one stop joins nothing, and one whose distances fall in two stops gives the carrier the same node twice. Nodes are numbered in order of first stop: walking carriers in key order and each carrier's stops in ascending distance, a stop takes the number of its node when an earlier stop has one, and the next number from 0 otherwise. The networks have no anchors.

In src/sim/SPEC.md, the sentence "It registers the medium's types first (sim/medium/SPEC.md), then park intent and its commands (sim/park/SPEC.md)." becomes:

    It registers the medium's types first (sim/medium/SPEC.md), then park intent and its commands (sim/park/SPEC.md), then the routes resolver that derives the park's networks (sim/routes/SPEC.md).

## Files affected

- Create: `src/sim/routes/SPEC.md`
- Create: `src/sim/routes/networks.h`
- Create: `src/sim/routes/networks.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park_schema.cpp`
- Modify: `src/sim/SPEC.md`
- Create: `tests/parks/routes.park`
- Tests from the test pass, under `tests/sim/routes/`, with `tests/sim/CMakeLists.txt`.

## Dependencies

- park-intent and park-edits (sketch-a-park): parkPaths, groundLine, and the edit commands.
- first-field-and-flow: Network, its constructor's rules, and addNetworkComponent, already registered first in makeParkSchema.
- world-as-value: resolvers, createDerivedEntity, makeCandidate, saves, and the cross-build check.

## Out of scope

- Connectors, anchors, and boxes of any kind, which box-connections adds.
- Drawing the networks, which graph-view adds.
- Joining lines whose ribbons overlap but whose center lines stay farther apart than the tolerance, a milestone deepening candidate.
- A sweep-line search for meetings. The pairwise test with a bounding-box reject serves the slice's parks.
- A bound on how far apart two stops of one node may lie when many meetings chain within a millimeter of each other. Nodes form only through meetings, which criteria 2 and 3 cover.

## Open questions

None.
