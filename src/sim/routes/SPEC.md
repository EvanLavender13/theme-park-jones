# routes

This module serves the navigable-networks capability (plans/navigable-networks). It derives the park's guest and backstage networks from park intent. It also computes route distance over them, which tells any mover the shortest way to any connected entity (principles 4 and 6). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

routes reads intent only through parkPaths, parkEntrances, parkBoxes, groundLine, and footprintOf (sim/park/SPEC.md, decision 0025). It publishes networks in the medium's Network type and route distance in the medium's fields (sim/medium/SPEC.md). Other modules read both through parkNetwork, previousNetwork, the Network type's queries, sampleField, and routeEntryAt, not through how they were derived.

addRoutes registers the derived type PreviousNetwork, the resolver path-networks, and route distance's two fields and its resolver. makeParkSchema calls it after addParkEdits. addDropPreviousNetworks registers the finisher that removes the previous networks. addPark calls it after every finisher that reads them.

## Networks

Each resolution derives one network for each PathKind from intent alone, so saves never hold networks (principle 1). parkNetwork returns a kind's network. Before the world's first resolution, it returns an empty network.

A kind's network has a carrier for each path of that kind, keyed by the path's key and following its ground line. It also has a carrier for each connector. Every edge is a stretch of a drawn line or a connector, measured along it (principle 4). Paths of different kinds never share a node, even where they cross.

Lines of one kind join at a node where they cross or come within JUNCTION_TOLERANCE of each other. These all give junctions:

- a crossing;
- a point snapped onto another line;
- a line through another line's point;
- a collinear overlap.

A line also meets itself where it crosses or doubles back. Nodes closer together along a carrier than JUNCTION_TOLERANCE are one node.

## Connectors

Doors sit at the middle of an entity's front and back faces.

- An entrance's front door serves the guest network.
- A shop's front door serves the guest network, and its back door serves the backstage network.
- A depot's front door serves the backstage network.

A door within CONNECTION_REACH of a path of the kind it serves gets a connector. A connector is a short straight carrier keyed connectorKey(entity, face). It runs from the door to the nearest point on that kind's paths, and joins the path's network there like any other meeting. A door farther away, or with no path of its kind, has no connector. Nothing else about its entity changes (principle 2). A door flush against a line of its kind still gets a connector.

Each connector's door end is a node of its own, anchored to the door's entity. These are the networks' only anchors. So an entity finds where it meets a network through anchoredNodes, and an entity with no connector on a network is not on it.

## Previous networks

previousNetwork returns a kind's network as the previous resolution derived it. Finishers use it to carry places held on the old network to the new one with carryOver (sim/medium/SPEC.md). In a world's first resolution, it returns none. No resolution ends holding a previous network. So a world between cycles, its copies and saves, and every candidate hold none.

## Route distance

Route distance covers each entity anchored on a network and each node that can reach it. For each such pair, it gives the least length of a route along edges from the node to the entity, and the next step to take. The result is a RouteEntry of a Distance and a Next RouteStep. A RouteStep is an edge of a carrier, walked from one stop toward another. At the entity's own nodes, Next has carrier NULL_KEY, which means the route has arrived. A node that can't reach the entity has no entry for it. Distances and steps depend only on the network, not on the order a search visits nodes in (principle 10).

Route distance is published as two entry fields, one per network:

- RouteDistance<PathKind::Guest>, named guest-route-distance;
- RouteDistance<PathKind::Backstage>, named backstage-route-distance.

This keeps an entity's entries on its two networks apart. Every anchored entity is a source on each network it anchors. Only resolved entries are published, so a resolved park's save holds the fields empty.

At a place inside an edge, a source's entry is the better of the edge's two ends. Its distance adds the distance along the carrier to that end, and its Next steps toward that end (principle 4).

routeEntryAt(world, kind, network, place, source) returns exactly the entry sampleField gives that source at the place, or none. It reads only that source's entries. It allocates nothing except the slot's order by place, on its first read. So a mover following one source pays for that source alone.

Suppose a mover samples at a place, walks Next along Next's carrier to Next.To, and samples again there. Doing this repeatedly, it arrives at the source's node. The lengths it walks add up exactly to the distance it first sampled.
