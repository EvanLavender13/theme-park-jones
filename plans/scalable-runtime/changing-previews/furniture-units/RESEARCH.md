# Research: furniture-units

## Where furniture's time goes in a drag frame

After route-updates, furniture is 22.8% of a shop-drag frame and 14.4% of a path-drag frame on full.park (build/perf/route-updates-1.sleepy to route-updates-4.sleepy). Laying chains again is under 1% of that, because layFurniture already keeps every chain whose reads hash the same. The cost is finding out which chains changed:

- Obstacles::hashNear, which hashes every chain's nearby obstacles, is 54% of furniture in the shop drag and 62% in the path drag.
- surroundingsOf builds a grid of every paving piece, walkway, box, entrance, and frontage strip in the park, with a new list per cell, on every resolution. It and its allocations are most of the rest.
- membersOf, chainsOf, publishSeatPlaces, and the removal loop each walk every furnished side or every piece.

## How to find the changed chains without reading the park

Laying a chain reads its sides, their furnishing, and the obstacles within its reach: paving of either kind, walkway pieces, boxes, the entrance, frontage strips, and the pieces of chains laid before it. Park and routes already file paving and walkway pieces in 8 m cells (point-cells, segment-cells, walkway-cells). A chain whose sides and furnishing didn't change, and none of whose reach cells holds a changed obstacle, lays the same pieces.

An obstacle changed in a cell when the cell's index entry changed, which covers an obstacle that arrived or left, or when a unit filed there changed its value, which covers one that moved inside the same cells. Park's unit space records both: changed index entries on the change list, and each index's Filed map from a unit to its cells.

Pieces of earlier chains are the one obstacle that comes from furniture itself. When a chain is laid again and its pieces change, every later chain whose reach covers the cells of its old or new pieces is laid again too, in chain order. A later chain never changes an earlier one, so this ends.

Rejected:

- Chains as a unit kind with recorded reads. A unit can't read units of its own kind, and laying reads the pieces of earlier chains.
- Letting chains ignore each other's pieces, then removing overlaps afterwards. It changes where pieces stand next to close parallel paths, and furniture must stay exact.
- Keeping the hash and making it cheaper. It still reads every chain's surroundings on every resolution.

Sources: src/sim/furniture/furniture.cpp; src/sim/units.h; src/sim/unit_space.h; src/sim/park/unit_reads.cpp; src/sim/park/internal/surface_units.h; src/sim/routes/connections.cpp; src/sim/routes/walked_surface.cpp.
