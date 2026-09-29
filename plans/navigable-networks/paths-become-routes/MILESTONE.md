# Milestone: Paths Become Routes

Slice: boxes-and-tubes

## Summary

paths-become-routes turns the park's intent into the park's networks. Resolution derives a guest network and a backstage network from the drawn paths, joined wherever same-kind lines meet. It connects the entrance, shops, and depots to nearby paths by the face that serves each network. It publishes the route distance field, which gives every place its distance along the paths to each anchored box and the next step toward it. A tooling view draws the networks over the park. It is fourth in the slice because the medium's network type and fields (first-field-and-flow) and the park's intent (sketch-a-park) are in place. The slice's shop needs a supply route, and its guests need a way to walk and a distance to choose by, so operations and guests both wait on this milestone.

## Acceptance criteria

1. Resolving a park derives one network per path kind, each on its own derived entity, which other modules read through the module's public queries. Every path whose ground line is not empty is a carrier of its kind's network, keyed by its entity, with its ground line as its points. Nodes sit at each path's two ends and wherever two lines of the same kind, or two stretches of one line that are not neighbors along it, meet within JUNCTION_TOLERANCE, 1 mm: at a snapped point, a crossing, or a line passing through another's end, so a path that crosses itself has a junction there. A collinear overlap gets a node at each end of the shared stretch. Lines of different kinds never share a node, even where they cross. An edge's length is the difference of two ground-line distances, so every distance is measured along the drawn line (principle 4).
2. The entrance's front connects to the guest network, a shop's front to the guest network and its back to the backstage network, and a depot's front to the backstage network. Each connector is a straight carrier from the face's midpoint, the door, to the nearest point of a line of that kind, when that point lies within CONNECTION_REACH, 4 m, of the door. It is keyed from its box and face, splits the path's edge where it meets it, and its door node is anchored to the box's entity. A door beyond reach has no connector, and nothing else about the box changes (principle 2). The new-park template's entrance connects to its path.
3. Every anchored entity is a route distance source on each network it anchors. Sampling the route distance field at a place gives, for each source reachable from it, the distance along the network and the next step: a carrier and a direction along it. A place with no route to a source has no entry for it. On random synthetic networks the distances equal a reference shortest-path computation, and from any place with an entry, repeatedly following next steps reaches the source's anchor, having walked exactly the sampled distance. Ties between equal routes break by a fixed rule.
4. The networks and the field are derived from intent alone: for any world a randomized sequence of edits reaches, the world loaded from its save and resolved equals it, and a candidate world made with an edit equals the world that commits the same edit (decision 0025). Every world such a sequence reaches is legitimate, with no crash for an unconnected box, a lone path, or an unreachable source (principle 2).
5. carryOver on derived networks keeps a place on a path unchanged when a new connector or junction splits its edge. It moves a place on a connector to the nearest point of the same connector when its box moves, and retires a place whose path or connector is deleted.
6. In the running app, a Graph checkbox draws both networks over the park: edges by kind, nodes, connectors, and anchored nodes marked. --graph turns it on at start, so a --capture of tests/parks/routes.park shows the networks.
7. tests/parks/routes.park holds crossing and snapped guest paths, a backstage path, and a shop and a depot connected to both kinds as their faces allow, and the cross-build check runs it. src/sim/routes/SPEC.md describes the module as built, and src/sim/SPEC.md and src/app/SPEC.md their changes. linux-debug builds without warnings and its tests pass, windows-debug builds, and the cross-build check passes.

## Medium

- Networks (produced by path-networks, extended by box-connections): the guest and backstage networks in shared-medium's Network type, with carriers, stops, and anchors. route-distance reads them to compute distances, and graph-view reads them through the Network type's public queries to draw them. Later in the slice, plausible-operations finds its shops' and depots' anchored nodes, believable-guests moves guests along the guest network, and every field is sampled at their places.
- Route distance (route-distance): two entry fields, guest-route-distance and backstage-route-distance, one per network kind, each with one resolved entry per source per node, with sampleEdge giving the entry at a place between nodes. It is produced here, and later sampled by plausible-operations for a supply route's length and delay, by believable-guests to choose and move, and by legible-simulation for the overlay and preview context. Nothing in this milestone consumes it except its tests.
- Park intent (decision 0025): path-networks and box-connections read it through parkPaths, parkBoxes, parkEntrances, groundLine, and footprintOf, committed or in a candidate. Connections are derived, never saved.

graph-view reads only the networks' public queries and draws nothing into the simulation. route-distance reads the networks it samples, never how they were derived.

## Dependencies

- deterministic-simulation's world-as-value: resolvers with declared dependencies, derived entities, makeCandidate, and the cross-build check. Met.
- shared-medium's first-field-and-flow: Network, Place, nearestPlace, carryOver, entry fields with sampleEdge. Met.
- effortless-building's sketch-a-park: park intent, ground lines, footprints, the path tool's snapping, the Tools panel, and --capture. Met.

## Core feature

path-networks is the core. It creates the module and derives the two networks from the paths alone, with every junction and crossing, so a drawn park has a real network whose edges and lengths tests can check against the lines. graph-view, box-connections, and route-distance all build on it.

## Features

1. `path-networks`: the module src/sim/routes registered in makeParkSchema, a resolver deriving the guest and backstage networks from path intent with junctions, crossings, and collinear overlaps, the public query for a kind's network, and tests/parks/routes.park. Depends on: none.
2. `graph-view`: the Graph checkbox and --graph option, drawing both networks' edges by kind and their nodes over the park on ImGui's background draw list, with points behind the camera clipped. Depends on: feature 1.
3. `box-connections`: connectors from each box's and the entrance's doors to the nearest same-kind line within CONNECTION_REACH, keyed by box and face and anchored to the box, the graph view marking connectors and anchored nodes, and carryOver checked on derived networks. Depends on: features 1 and 2.
4. `route-distance`: the route distance field, its per-source Dijkstra runs with fixed tie-breaking, its sampleEdge, and the synthetic reference and walk tests. Depends on: feature 3.

## Tuning values

The capability leaves these to this milestone. Playing adjusts them.

- Junction tolerance: 1 mm between two same-kind lines, since the path tool snaps clicked points exactly onto lines.
- Connection reach: 4 m from a face's midpoint to a line of its kind.

## Hand-off

Keeping the previous networks available until holders carry their places over is designed with the first holder, believable-guests' hungry-guests. A resolver derives only from intent, and a load must resolve back to the world saved, so the mechanism depends on how a holder's finisher runs, which that milestone decides.

## Deepening candidates

- Route distances in the graph view: hovering a node lists each source's distance and next step.
- Joining paths whose ribbons overlap but whose lines do not meet, with a short joint carrier whose length counts.
- A connector from the nearest point of a face rather than its door, so a path passing a corner still connects.
- Several connectors per face, one per nearby path.
- A sweep-line search for meetings, if parks grow large enough for the pairwise test to cost.
- The graph view marking a line as a connector when its carrier's first stop is anchored, as walkways find connectors, rather than when no path of either kind holds its key.

## Open questions

None.

## Research notes

- Edge lengths are ground-line distances, not quadrature arc lengths, since the ground line is what the medium interpolates along.
- Same-kind lines, and a line with itself, meet where two segments come within the junction tolerance, found pairwise with a bounding-box reject. Collinear overlaps get a node at each end.
- Connectors reuse the medium's nearestPlace, the only straight-line measure.
- A route distance entry's next step is a carrier and two of its stop distances, which survives edges being split and names which stop to leave from where a carrier meets a node twice. Ties break by carrier key, then direction, then the stop left from.
- The graph view draws on ImGui's background draw list, clipping segments behind the camera.

Depth is in RESEARCH.md.
