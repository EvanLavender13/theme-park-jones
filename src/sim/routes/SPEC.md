# routes

This module serves the navigable-networks capability (plans/navigable-networks). It derives the park's guest and backstage networks from park intent. It also computes route distance over them, which tells any mover the shortest way to any connected entity (principles 4 and 6). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

routes reads intent only through parkSegments, parkEntrances, parkBoxes, footprintOf, parkSurface, and pieceDistance (sim/park/SPEC.md, decision 0025). It publishes networks in the medium's Network type and route distance in the medium's fields (sim/medium/SPEC.md). Other modules read both through parkNetwork, previousNetwork, the Network type's queries, sampleField, and routeEntryAt, and the doors connectors start from through parkDoors, and their walkways through connectorWalkways and keyedWalkways, not through how they were derived.

addRoutes registers the derived type PreviousNetwork, the resolver path-networks, and route distance's two fields and its resolver. makeParkSchema calls it after addParkEdits. addDropPreviousNetworks registers the finisher that removes the previous networks. addPark calls it after every finisher that reads them.

## Networks

Each resolution derives one network for each PathKind from intent alone, so saves never hold networks (principle 1). parkNetwork returns a kind's network. Before the world's first resolution, it returns an empty network.

A kind's network has a carrier for each segment of that kind whose ground line is not empty, keyed by the segment's key, following its ground line, and with the segment's width. It also has a carrier for each connector, as wide as its kind's path (pathWidth, sim/park/SPEC.md), or as its door's face if that is narrower. Every edge is a stretch of a segment or a connector, measured along it (principle 4). Paths of different kinds never share a node, even where they cross.

Carriers join only at path points and at connectors' ends. Every carrier whose segment ends at a point stops there, and all of them share one node. Segments of one kind that cross, touch, or overlap without sharing a point don't join. At a rounded corner, the node lies at the middle of the arc, where the two segments' ground lines meet.

If nodes are closer together along a carrier than JUNCTION_TOLERANCE, they are one node.

## Connectors

Doors sit at the middle of an entity's front and back faces.

- An entrance's front door serves the guest network.
- A shop's front door serves the guest network, and its back door serves the backstage network.
- A depot's front door serves the backstage network.
- A bench's and a toilet block's front doors serve the guest network.

parkDoors gives the doors serving a kind: the entrances' first, then the boxes', each in key order of its entity. A ParkDoor has its entity, its face, its position, the unit direction out of its face, and the width of its face.

A door's distance from a segment is how far it stands from the segment's paving, as parkSurface draws it without walkways. A segment's paving is the pieces it owns and the pieces of the points it ends at. A door on that paving is at distance 0. If a segment of the kind a door serves lies within CONNECTION_REACH of it, the door gets a connector to the nearest such segment. If several lie equally near, within JUNCTION_TOLERANCE, the one whose ground line lies nearest the door wins, then the lower key. A connector is a short carrier keyed connectorKey(entity, face). It runs from the door straight out of the door's face to where that line first meets paving of its kind, and on from there to the nearest point on that segment's ground line, and stops that segment's carrier there at a node. Its length counts both parts. If that line meets no paving of its kind within CONNECTION_REACH, if the door already stands on paving, or if the nearest point on the ground line from the door lies straight out of its face, the connector runs straight from the door to the nearest point on the ground line from the door. If no segment of its kind is within reach, a door has no connector, and nothing else about its entity changes (principle 2). If a door is flush against a segment's edge, it still gets a connector. If it lies on the nearest segment's ground line, within JUNCTION_TOLERANCE, which only a hand-written save allows, it has no connector.

Each connector's door end is a node of its own, anchored to the door's entity. These are the networks' only anchors. An entity finds where it meets a network through anchoredNodes. If an entity has no connector on a network, it is not on that network.

connectorWalkways gives a walkway (sim/park/SPEC.md) for each connector of a kind's network, in key order of the connector. It has the connector's width and is joined to that segment. It starts at the connector's door, along the door's face, and runs straight along the connector's first part. If the connector runs straight to the ground line, the walkway ends there. Otherwise it ends straight out of the door, past where the connector meets the paving by half its width, or by the length of the connector's part from there to the ground line if that is less, so its square end lies inside the paving. If the connector meets the segment at a path point, it is joined to the lowest-keyed segment of the kind that ends there.

keyedWalkways gives the same walkways as connectorWalkways, in the same order, each with the key of the connector it was made for.

## Previous networks

previousNetwork returns a kind's network as the previous resolution derived it. Finishers use it to carry places held on the old network to the new one with carryOver (sim/medium/SPEC.md). In a world's first resolution, it returns none. Each resolution drops the previous networks before it ends, so a world between cycles, its copies and saves, and every candidate hold none.

## Route distance

Route distance covers each entity anchored on a network and each node that can reach it. For each such pair, it gives the least length of a route along edges from the node to the entity, and the next step to take. The result is a RouteEntry of a Distance and a Next RouteStep. A RouteStep is an edge of a carrier, walked from one stop toward another. At the entity's own nodes, Next has carrier NULL_KEY to show the route has arrived. If a node can't reach the entity, it has no entry for it. Distances and steps depend only on the network, not on the order a search visits nodes in (principle 10).

Route distance is published as two entry fields, one per network:

- RouteDistance<PathKind::Guest>, named guest-route-distance;
- RouteDistance<PathKind::Backstage>, named backstage-route-distance.

This keeps an entity's entries on its two networks apart. If an entity is anchored on a network, it is a source on that network. Only resolved entries are published, so a resolved park's save holds the fields empty.

At a place inside an edge, a source's entry is the better of the edge's two ends. Its distance adds the distance along the carrier to that end, and its Next steps toward that end (principle 4).

routeEntryAt(world, kind, network, place, source) returns exactly the entry sampleField gives that source at the place, or none. If a mover follows one source, it pays for that source alone, because routeEntryAt reads only that source's entries and allocates nothing except the slot's order by place, on its first read.

Suppose a mover samples at a place, walks Next along Next's carrier to Next.To, and samples again there. Doing this repeatedly, it arrives at the source's node. The lengths it walks add up exactly to the distance it first sampled.
