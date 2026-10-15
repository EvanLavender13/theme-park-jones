# routes

This module serves the navigable-networks capability (plans/navigable-networks). It derives the park's guest and backstage networks from park intent. It also computes route distance over them, which tells any mover the shortest way to any connected entity (principles 4 and 6). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

routes reads intent only through parkSegments, parkEntrances, parkBoxes, footprintOf, parkSurface, resolvedSurface, changedPavingOwners, pieceDistance, and walkwayPiece, and its units read park's units only through the functions in sim/park/unit_reads.h (sim/park/SPEC.md, decision 0025). It publishes networks in the medium's Network type and route distance in the medium's fields (sim/medium/SPEC.md). Other modules read both through parkNetwork, previousNetwork, the Network type's queries, sampleField, and routeEntryAt, the best places search through bestPlaces and sampleBestPlaceEdge, and the doors connectors start from through parkDoors, and their walkways through connectorWalkways and keyedWalkways, and the paved surface with every walkway through walkedSurface, not through how they were derived.

addRoutes registers the derived type PreviousNetwork, the resolver path-networks, and route distance's two fields and its resolver. makeParkSchema calls it after addParkEdits. addRoutes also registers the units its connectors and walked surface derive in, named door-connections, connection-paths, door-walkways, walkway-cells, walkway-joins, walked-point-edges, and walked-segment-edges, so those names are taken. addDropPreviousNetworks registers the finisher that removes the previous networks. addPark calls it after every finisher that reads them.

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

parkDoors gives the doors serving a kind: the entrances' first, then the boxes', each in key order of its entity. A ParkDoor has its entity, its face, its position, the unit direction out of its face, and the width of its face. parkDoors can also put the doors in a list the caller keeps, clearing it first, keeping the lists it works with in TickBuffers the caller keeps. It then allocates nothing once they have grown.

A door's distance from a segment is how far it stands from the segment's paving, as parkSurface draws it without walkways. A segment's paving is the pieces it owns and the pieces of the points it ends at. A door on that paving is at distance 0. If a segment of the kind a door serves lies within CONNECTION_REACH of it, the door gets a connector to the nearest such segment. If several lie equally near, within JUNCTION_TOLERANCE, the one whose ground line lies nearest the door wins, then the lower key. A connector is a short carrier keyed connectorKey(entity, face). It runs from the door straight out of the door's face to where that line first meets paving of its kind, and on from there to the nearest point on that segment's ground line, and stops that segment's carrier there at a node. Its length counts both parts. If that line meets no paving of its kind within CONNECTION_REACH, if the door already stands on paving, or if the nearest point on the ground line from the door lies straight out of its face, the connector runs straight from the door to the nearest point on the ground line from the door. If no segment of its kind is within reach, a door has no connector, and nothing else about its entity changes (principle 2). If a door is flush against a segment's edge, it still gets a connector. If it lies on the nearest segment's ground line, within JUNCTION_TOLERANCE, which only a hand-written save allows, it has no connector.

Each connector's door end is a node of its own, anchored to the door's entity. These are the networks' only anchors. An entity finds where it meets a network through anchoredNodes. If an entity has no connector on a network, it is not on that network.

connectorWalkways gives a walkway (sim/park/SPEC.md) for each connector of a kind's network, in key order of the connector. It has the connector's width and is joined to that segment. It starts at the connector's door, along the door's face, and runs straight along the connector's first part. If the connector runs straight to the ground line, the walkway ends there. Otherwise it ends straight out of the door, past where the connector meets the paving by half its width, or by the length of the connector's part from there to the ground line if that is less, so its square end lies inside the paving. If the connector meets the segment at a path point, it is joined to the lowest-keyed segment of the kind that ends there.

keyedWalkways gives the same walkways as connectorWalkways, in the same order, each with the key of the connector it was made for. Both can also append their walkways to a list the caller keeps, keeping the entrances and boxes they read in TickBuffers the caller keeps. Then they allocate nothing once the lists have grown.

walkedSurface gives the paved surface parkSurface gives with every connector's walkway, the guest walkways first, as the walked-surface resolver kept it when the world last resolved. The walked-surface resolver runs after path-networks. Before a world first resolves, walkedSurface gives an empty surface.

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

## Best places

bestPlaces finds the best few of many seeds from every node of a network, in one search. A PlaceSeed has a key, an owner, a place on the network, and a value, where a higher value is better. If a seed's place doesn't resolve, it is left out. If two seeds share a key, bestPlaces throws std::invalid_argument.

A seed's distance from a place is the least length of a route along edges from that place to the seed's place, added from the seed's end, as route distance's are (principle 4). From a place, one seed dominates another when its distance there is no longer, its value is no lower, and it comes first in order of distance, then higher value, then lower key. At each node, bestPlaces gives the seeds that fewer than a given number of other seeds dominate there, in that order. Each comes as a BestPlace: the seed's key, owner, place, and value, its distance, and the next step toward it. A node's entries are at its nodePlace. A seed whose place lies strictly inside an edge also gets one entry at its own place. Where a seed's place lies, its entry has distance 0 and a Next with carrier NULL_KEY, to show the route has arrived.

A next step follows an edge as route distance's does, except that a step into the edge holding the seed's place ends at that place. Of the steps that achieve a node's distance, the one first by route distance's rule is taken, so the result depends only on the network and the seeds (principle 10).

Every node a next step leads to also gives that seed. Suppose a mover starts at a node, walks Next along its carrier to Next.To, and samples there. Repeating this, it reaches the seed's place, and the lengths it walks add up exactly to the distance it first sampled. Rounding can make routes of different lengths equal. A node then gives the seeds the search keeps by the order above, which can differ slightly from what a separate search for each seed would find.

sampleBestPlaceEdge is the rule for a place strictly inside an edge, for a field whose entries bestPlaces made. A seed's distance there is the least of three walks: to the edge's From end plus the seed's distance there, to its To end plus the seed's distance there, and, if the seed's place lies on the edge, along the edge to it. The rule gives the seeds that fewer than the given number of others dominate there, in the same order, each with the next step of its least walk. A mover that follows them reaches the seed, and the lengths it walks add up to the distance it sampled, allowing for rounding. The rule allocates nothing but out's growth, so movers can sample on the tick path.

bestPlaces' work grows with the network's edges times the number it keeps times the number of different values among the seeds. It doesn't grow with the seeds times the network.
