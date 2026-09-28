# Feature: Box Connections

## Summary

Resolution connects the entrance and every box to the networks their faces serve. The entrance's front door, the midpoint of its front face, serves the guest network; a shop's front door serves the guest network and its back door the backstage network; and a depot's front door serves the backstage network. A door whose nearest point on a path of that kind lies within CONNECTION_REACH, 4 m, gets a connector: a straight two-point carrier from the door to that point, keyed from the entity and the face, whose far end joins the path at a node and whose door end is a node anchored to the entity. A door beyond reach gets nothing, and nothing else changes. The path-networks resolver derives connectors with the paths, so they follow every edit, candidate, and load exactly as paths do, and carryOver keeps places on paths and connectors across edits. The graph view marks connectors and anchored nodes in their own colors.

## Acceptance criteria

1. For a resolved world, each kind's network has the path carriers src/sim/routes/SPEC.md gives, and besides them exactly one carrier for each door serving that kind that has a connector as src/sim/routes/SPEC.md's Connectors section gives, its reach at most CONNECTION_REACH and more than JUNCTION_TOLERANCE, keyed connectorKey of the door's entity and face, with two points: the door at distance 0, and the connection point at distance equal to the reach.
2. A connector's first stop is at a node no other stop of the network has, anchored to its door's entity, and its last stop is at the same node as the stop of its path carrier within JUNCTION_TOLERANCE of the connection place's distance. A network's anchors are exactly its connectors' door nodes.
3. Every stop of a path carrier other than its first and last lies within JUNCTION_TOLERANCE of the distance on it of a meeting or of a connection.
4. path-networks' criteria 6 and 7 hold with connectors: for any world a randomized sequence of park edits reaches, the networks, anchors included, equal those of its save loaded and resolved and those of a candidate made with the edit, and resolving never throws. Such sequences reach worlds where some door has a connector and some door serving a kind has none.
5. Across any cycle a randomized edit sequence runs, carryOver from a kind's network before to its network after keeps a place on a path carrier present in both unchanged, carries a place on a connector present in both to a place on that same connector, and carries a place on a carrier the network after lacks to none.
6. buildGraphOverlay's lines mark as connectors exactly those whose carrier is not the key of a path parkPaths gives, and each of its nodes names nodeAnchor of its node. GRAPH_CONNECTOR_COLOR and GRAPH_ANCHOR_COLOR are opaque, and differ in red, green, or blue from each other, from both kinds' graphColor, from GRAPH_NODE_COLOR, and from both kinds' pathColor.
7. Once resolved, every door of tests/parks/routes.park, its entrance's front, its shop's front and back, and its depot's front, has a connector, and each of its two networks has every node reachable from every other along its edges. The new park's entrance has a connector to its guest path.
8. In the running app with the Graph checkbox on, connectors are drawn in GRAPH_CONNECTOR_COLOR and anchored nodes as larger dots in GRAPH_ANCHOR_COLOR. A --capture of tests/parks/routes.park with --graph shows all four connectors and their anchored doors. This is checked by looking at the capture, not by a test.

## Medium

Networks (produced here, extending path-networks): each kind's network gains connector carriers and anchors. It reads intent through parkPaths, parkEntrances, parkBoxes, groundLine, and footprintOf (decision 0025), and uses the medium's nearestPlace and groundPoint to find connection points. route-distance will read the anchors as its sources, and plausible-operations and believable-guests will find their boxes' nodes through anchoredNodes. The graph overlay reads the networks through parkNetwork and Network's public queries, and parkPaths to tell connectors from paths.

## Principle checks

- Principle 1: connectors and anchors are derived. They equal those derived again after a save and load, and from a candidate (criterion 4), and saves never hold them.
- Principle 2: every world an edit sequence reaches resolves (criterion 4), including a box with no door in reach, a door on a line in a hand-written save, and an entity with no footprint. A place whose connector is gone is retired, not an error (criterion 5).
- Principle 4: a connector's length is the straight distance from its door to its connection point, its own ground line, and connections never change a path's points (criteria 1 and 3), so every distance is still measured along a line.
- Principle 6: boxes are found by the network only through its public anchors (criterion 2), and the overlay reads connectors through public queries (criterion 6).
- Principle 8: a candidate's networks equal the committed world's (criterion 4).
- Principle 10: connection places follow nearestPlace's fixed ties, connectors sort by key, and the cross-build check runs routes.park, whose four doors connect (criterion 7).

## Spec changes

In src/sim/routes/SPEC.md:

The introduction's "It reads intent only through parkPaths and groundLine (sim/park/SPEC.md, decision 0025)" becomes "It reads intent only through parkPaths, parkEntrances, parkBoxes, groundLine, and footprintOf (sim/park/SPEC.md, decision 0025)".

In Networks, the paragraph starting "A kind's network has one carrier for each path" becomes:

    A kind's network has one carrier for each path of that kind whose ground line is not empty, keyed by the path's key, with the ground line as its points, and one for each of its connectors (Connectors). So every edge is a stretch of a drawn line or a connector, measured along it (principle 4). A path of the other kind is never a carrier, so lines of different kinds never share a node, even where they cross.

In Meetings, the first sentence's "Two segments can meet when they are on different carriers of the network" becomes "Two segments of path carriers can meet when they are on different carriers". Add, after the section's last paragraph:

    Connectors meet no segment. A connector's only meeting is its connection (Connectors).

Add a section after Meetings:

    ## Connectors

    A door is the midpoint of a face of an entrance's footprint, from footprintOf with ENTRANCE_SIZE, or of a box's, with its kind's boxSize. The front door is at the midpoint of the footprint's first two corners, and the back door of its last two, each coordinate (first + second) / 2. An entity with no footprint has no doors. An entrance's front door serves the guest network. A shop's front door serves the guest network and its back door the backstage network. A depot's front door serves the backstage network. No other door serves a network.

    For a door serving a kind, the connection place is nearestPlace of the door in a network of that kind's path carriers alone, so it never lies on a connector, and ties go as nearestPlace breaks them. The connection point is the place's groundPoint, and the reach is the straight distance from the door to it, sqrt(dx * dx + dz * dz). When the kind has no path carriers, the door has no connection place. When the reach is at most CONNECTION_REACH, 4 m, and more than JUNCTION_TOLERANCE, and no path carrier of the kind is keyed connectorKey(entity, face), the door has a connector: a carrier keyed connectorKey(entity, face), deriveKey(entity, hashName("connector"), the face's value), with two points, the door at distance 0 and the connection point at the reach. Otherwise the door has no connector, and nothing else about its entity changes (principle 2). A door that close to a line lies in no physically valid park, since lines stay their half width from every box, and a path keyed like a connector comes only from a hand-written save.

    A connector's connection is a meeting between it at the reach and its connection place's carrier at the place's distance, so the connection's stop on the path is grouped and joined as any meeting's is: a connection within the tolerance of a junction or a path's end lands on that node, and connectors reaching one point share a node. Nodes are numbered by walking all carriers, paths and connectors together, in key order.

In Stops and nodes, "A carrier's stops come from 0, its length, and its distance in each meeting." stays, since a connection is a meeting, and "The networks have no anchors." becomes:

    Each connector's first stop, at its door, is a node no other stop has, since nothing else meets it there, and that node is anchored to the door's entity. These are the networks' only anchors, so an entity finds where it meets a network through anchoredNodes.

In src/render/SPEC.md's Graph overlay section, after the sentence ending "has one GraphNode at that position's windowPoint.", add:

    A GraphLine is marked Connector exactly when its carrier is not the key of a path parkPaths gives, and a GraphNode holds nodeAnchor of its node as its Anchor.

and its last paragraph becomes:

    The app draws lines in graphColor(kind), or GRAPH_CONNECTOR_COLOR for a connector, GRAPH_LINE_THICKNESS thick, and nodes as filled circles of GRAPH_NODE_RADIUS in GRAPH_NODE_COLOR, or of GRAPH_ANCHOR_RADIUS in GRAPH_ANCHOR_COLOR for an anchored node, all in window units. The graph colors are opaque, and differ from each other and from both kinds' pathColor, so the graph stands out over the ribbons it follows.

In src/app/SPEC.md's Tooling UI section, "each line in graphColor of its kind, GRAPH_LINE_THICKNESS thick, and then each node as a filled circle of GRAPH_NODE_RADIUS in GRAPH_NODE_COLOR (render/SPEC.md)" becomes "each line in its color, GRAPH_LINE_THICKNESS thick, and then each node as a filled circle of its radius and color (render/SPEC.md)".

## Files affected

- Modify: `src/sim/routes/SPEC.md`
- Modify: `src/sim/routes/networks.h`
- Modify: `src/sim/routes/networks.cpp`
- Modify: `src/render/SPEC.md`
- Modify: `src/render/graph_overlay.h`
- Modify: `src/render/graph_overlay.cpp`
- Modify: `src/app/SPEC.md`
- Modify: `src/app/main.cpp`
- Tests from the test pass, under `tests/sim/routes/` and `tests/render/`.

## Dependencies

- path-networks: the resolver, its meetings, stop grouping, and numbering, and tests/parks/routes.park.
- graph-view: the overlay and the app's drawing of it.
- first-field-and-flow: nearestPlace, groundPoint, anchors, anchoredNodes, and carryOver.
- sketch-a-park: parkEntrances, parkBoxes, footprintOf, and the box commands.

## Out of scope

- A connector from the nearest point of a face rather than its door, and several connectors per face, which are milestone deepening candidates.
- Connectors meeting the lines they cross. A connector meets only its own path, at its connection (RESEARCH.md).
- Connectors from faces other than those listed, such as a depot's back or an entrance's sides.
- Keeping the previous networks available for holders' carry-over, which believable-guests' hungry-guests designs (milestone Hand-off). carryOver is checked here on networks the tests hold.
- Route distances, which route-distance adds.

## Open questions

None.
