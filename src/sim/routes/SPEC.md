# routes

Navigable networks (plans/navigable-networks): the park's guest and backstage networks, derived from park intent, and route distance over them, which tells any mover the shortest way to any connected entity (principles 4 and 6). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

routes reads intent only through parkPaths, parkEntrances, parkBoxes, groundLine, and footprintOf (sim/park/SPEC.md, decision 0025). It publishes networks in the medium's Network type and route distance in the medium's fields (sim/medium/SPEC.md), so other modules read them through parkNetwork, previousNetwork, the Network type's queries, sampleField, and routeEntryAt, never through how they were derived.

addRoutes registers the derived type PreviousNetwork, the resolver path-networks, and route distance's two fields and its resolver, and makeParkSchema calls it after addParkEdits. addDropPreviousNetworks registers the finisher that removes the previous networks, and addPark calls it after every finisher that reads them.

## Networks

Each resolution derives one network for each PathKind from intent alone, so saves never hold them (principle 1). parkNetwork gives a kind's network, or an empty network before the world's first resolution.

A kind's network has a carrier for each path of that kind, keyed by the path's key and following its ground line, and a carrier for each connector. Every edge is a stretch of a drawn line or a connector, measured along it (principle 4). Paths of different kinds never share a node, even where they cross.

Lines of one kind that cross, or come within JUNCTION_TOLERANCE of each other, join at a node there: a crossing, a point snapped onto another line, a line through another's point, and a collinear overlap all give junctions. A line also meets itself where it crosses or doubles back. Nodes closer together along a carrier than JUNCTION_TOLERANCE are one node.

## Connectors

Doors sit at the middle of an entity's front and back faces. An entrance's front door serves the guest network, a shop's front door the guest network and its back door the backstage network, and a depot's front door the backstage network.

A door within CONNECTION_REACH of a path of the kind it serves gets a connector: a short straight carrier keyed connectorKey(entity, face), from the door to the nearest point on that kind's paths, where it joins the path's network like any meeting. A door farther away, or with no path of its kind, has no connector, and nothing else about its entity changes (principle 2). A door flush against a line of its kind still gets a connector.

Each connector's door end is a node of its own, anchored to the door's entity. These are the networks' only anchors, so an entity finds where it meets a network through anchoredNodes, and an entity with no connector on a network is not on it.

## Previous networks

previousNetwork gives a kind's network as the resolution before the current one derived it, so finishers can carry places held on it to the new network with carryOver (sim/medium/SPEC.md), or none in a world's first resolution. No resolution ends holding a previous network, so a world between cycles, its copies and saves, and every candidate hold none.

## Route distance

Route distance gives, for each entity anchored on a network and each node that can reach it, the least length of a route along edges from the node to the entity, and the next step to take: a RouteEntry of a Distance and a Next RouteStep, an edge of a carrier walked from one stop toward another. At the entity's own nodes Next has carrier NULL_KEY: the route has arrived. A node that cannot reach the entity has no entry for it. Distances and steps are functions of the network alone, never of the order a search visits nodes in (principle 10).

It is published as two entry fields, RouteDistance<PathKind::Guest>, named guest-route-distance, and RouteDistance<PathKind::Backstage>, named backstage-route-distance, one per network, so an entity's entries on its two networks stay apart. Every anchored entity is a source on each network it anchors. Only resolved entries are published, so a resolved park's save holds the fields empty.

At a place inside an edge, a source's entry is the better of the edge's two ends, with the distance along the carrier to that end added and Next stepping toward it (principle 4).

routeEntryAt(world, kind, network, place, source) gives exactly the entry sampleField gives that source at the place, or none, reading only that source's entries and allocating nothing beyond the slot's order by place on its first read, so a mover following one source pays for that source alone.

A mover that walks Next from the place it samples, along Next's carrier to Next.To and samples again there, arrives at the source's node, and the lengths it walks add up exactly to the distance it first sampled.
