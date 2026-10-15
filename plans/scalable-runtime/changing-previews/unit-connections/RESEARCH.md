# Research: unit-connections

unit-surface's RESEARCH.md answers two questions this feature builds on: how the walked surface can change only where walkways change, and whether a door's connection and walkway can be units. This file adds what those answers left open.

## How routes can keep units on park's entities without reading park's internals

A unit kind is kept over an intent component, and its derivation reads intent components by type. Every entity routes' units belong to is one of park's: a box or an entrance for a door, and a path point or segment for the walked surface. All four intent types are private to park (src/sim/park/internal/components.h, decision 0039). A routes kind that names them breaks principle 6.

The mechanism can let a kind be kept over earlier kinds' units instead of an intent component. Resolution then needs only each table's keys, which a virtual on UnitTable can give without the value type. typeIdOf works on a type that is declared but not defined, so park's public header can declare its kinds by name alone. Routes names them only to say which keys its units belong to. It reads park's units only through public park functions that take UnitReads, so every read is still recorded.

Those functions must read only park's units, never its components, because a derivation may read only the components its own kind's Reads name. Everything routes needs is already a unit: ground lines, the segment-ends index, the point and segment paving, the cell indexes, and the group labels. The boxes and entrances need two new park kinds.

Rejected: moving doors, connections, walkways, and the walked surface into park. It moves a public API that furniture, tools, render, guests, legible, and the tests use. It also splits the network's stop grouping across two modules, because a walkway's segment depends on how stops group on its path. Including park's internal components in routes breaks principle 6.

Sources: src/sim/units.h; src/sim/schema.cpp (addUnitTable); src/sim/world.h (has needs the complete type); src/sim/type_id.h.

## Which pieces a door's connection can be found from

Each point's paving holds its own pieces and the corner pieces it gives its segments. Each segment's paving holds copies of those corner pieces, then its body. point-cells files a point under the cells of all its pieces, corner pieces included, and segment-cells files a segment under the cells of its body (paveSegmentWith, PointCells, SegmentCells). So every piece of the paved surface lies in some cell it is filed under, as itself or as an identical copy. A door's connection reads only pieces within CONNECTION_REACH plus JUNCTION_TOLERANCE of it. The least distance per owner and the first ray entry are minima, so extra pieces farther away and repeated copies change nothing.

The connection measures distances to the line of each nearby carrier with Network::nearestPlaceOn and groundPoint, on a network of every carrier built for the purpose (linesOnly). Both depend only on the one carrier's key and points (projectOnto, findCarrierOf). A form that takes the key and points gives the same result without building a network.

Sources: src/sim/park/paving.cpp (paveSegmentWith, segmentInputOf); src/sim/park/surface_units.cpp; src/sim/medium/network.cpp.

## Which segment a walkway joins

keyedWalkways joins a walkway to the lowest-keyed segment at the node where its connector meets its path. That node is the connector's stop group on the path, as groupStarts makes it from the path's ends and the distances of every connection on that path. If the group is the path's first, it also holds the path's From point, where every segment of the kind with a ground line that ends there stops too. If it is the last, the same holds for the To point. A segment's first group holds its far end only when its ground line is no longer than JUNCTION_TOLERANCE, whatever connections it has, so the node reaches a further point only across such a segment. Lines shorter than MIN_SEGMENT_LENGTH, 0.01 m, are empty (geometry.cpp), so this happens only if rounding leaves a line that short. The only other carriers at the node are connectors. A walkway therefore reads its own connection, the other connections on its path through an index of connections by path, the path's ground line, the segments at its path's points, and, across any segment that short, the segments at the points beyond.

Sources: src/sim/routes/networks.cpp (pointMeetings, groupStarts, groupHolding, addStops, keyedWalkways).

## Why the walked surface can copy the paved surface's runs

hideOwner hides each edge line only with the hiders whose bounds overlap it. The paved hidden-edges unit gathers every joined piece in the cells the edge lines cover, which holds every piece that overlaps them. So a point's walked edge lines equal its paved ones unless a walkway piece shares a cell with them. When one does, hiding again with the paved pieces and the walkway pieces gives what walkedSurfaceOf gives, because hiding doesn't depend on the order of the hiders. A segment's walked run is its paved run, plus the pieces of the walkways joined to it in connector key order, plus those walkways' edge lines after hiding. A surface walks as flat lists, so leaving out an empty owner doesn't change its words.

Sources: src/sim/park/paving.cpp (hideOwner, hideJoined, walkedSurfaceOf, addWalkway); src/sim/park/surface_units.cpp (HiddenEdges).
