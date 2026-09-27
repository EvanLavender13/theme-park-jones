# Research: shared-medium

## How should fields be stored and sampled, and how do they attribute to sources?

Game AI's influence maps are the closest prior art. Sources deposit a value at their origin and radiate it outward with a falloff over distance, and the map is read by sampling a location. Practitioners use several representations behind the same idea: fine grids, coarse area graphs, and waypoint networks. Waypoint networks avoid grid problems such as walls and multiple levels, because influence propagates only along connections. That is the same move as principle 4. A second family evaluates sources on demand at the queried point instead of storing a grid, which trades memory and update cost for query cost. Mike Lewis describes this for Guild Wars 2 as infinite-resolution influence mapping.

What this suggests for the medium:

- The sampling interface should hide the storage, as decision 0005 already says.
- A network-place field can be stored as values on nodes, or computed on demand from sources and route distance.
- Per-source attribution is not standard in influence maps. They keep one summed layer per kind of influence and discard where it came from. Keeping attribution means holding a per-source term at each sample, or recomputing it on demand.
- The additive rule Evan chose makes the reconstruction exact, provided the summation order is fixed. Floating-point addition is not associative, so the total must be the ordered sum of the reported contributions, not a separately accumulated value.

Rejected:

- A single summed layer per field with no source terms: it cannot answer "which shops contribute" (principle 8).
- Grid storage for the foundation: the slice needs only network places, and a grid measures straight-line distance, contradicting principle 4 for these fields.

Sources:

- https://www.gamedev.net/tutorials/programming/artificial-intelligence/the-core-mechanics-of-influence-mapping-r2799/: point sources, falloff, and propagation between connected nodes.
- https://www.andrewshunt.com/influence-maps: grid, area graph, and waypoint network representations.
- http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter29_Escaping_the_Grid_Infinite-Resolution_Influence_Mapping.pdf: on-demand evaluation instead of a stored grid. Only the title and abstract were reachable.

## How should conserved flows in transit be represented?

OpenTTD represents cargo as packets. Each packet has an unsigned 16-bit integer count, its source, the station it first came from, the next hop it wants, and the time it has spent in transit. Packets can be split into two, or merged into one, which adds their counts and keeps their metadata. Cargo is therefore conserved exactly by integer arithmetic: moving cargo means moving or splitting packets, never scaling a real number. Transit time lives on the packet, not on the track, which matches the slice's rule that a shipment carries its arrival time and still arrives after its route is removed.

Factorio's pre-2.0 fluid system simulated flow between each individual pipe box. Its developers called it unpredictable and unintuitive, because throughput fell with distance at an inconsistent rate. Fluids 2.0 replaced it with segments: aggregates with a volume, where anything pushed in is immediately available anywhere along the segment. The lesson for a game is that aggregate, predictable flow beats per-cell physical propagation. Decision 0018 reaches the same conclusion.

What this suggests for the medium:

- A flow is a set of integer-count packets, each with a source endpoint, a destination endpoint, and an arrival tick.
- The transport ledger is the single holder of packets in transit.
- Conservation is checked as an exact identity over three sums: counts sent, counts in transit, and counts delivered.
- Split and merge operations must preserve the sum, which is a natural property test.

Rejected:

- Continuous amounts with per-edge propagation, like old Factorio fluids: approximate conservation and unpredictable delivery.
- Moving packets edge by edge along the network: a removed path would strand them, contradicting the slice, and the foundation has no use for edge capacity yet.

Sources:

- https://docs.openttd.org/source/df/dd6/structCargoPacket: packet fields, integer counts, split and merge.
- https://github.com/OpenTTD/OpenTTD/blob/master/src/cargopacket.cpp: implementation of the packet operations.
- https://www.factorio.com/blog/post/fff-416: why per-pipe fluid simulation was replaced with aggregate segments.

## How do ticks stay independent of the order systems run in?

The double buffer pattern keeps a current state that everyone reads and a next state that everyone writes, then swaps them at the end of the step. Every actor sees the world as it was at the start of the tick, so the order of updates within a tick cannot change the result, and all updates appear simultaneous. The cost is a one-step lag and a second copy of whatever is buffered.

What this suggests for the medium:

- Only the medium needs double-buffering: published field values and packets in transit.
- An entity's private state is only ever read by that entity, so it can be updated in place.
- A field publication written during tick N becomes readable at tick N+1.
- A packet with arrival tick N is delivered at the start of tick N, before anything acts.
- Two writers to the same field slot within a tick must still combine deterministically. Additive fields do so if their contributions are kept per source and summed in a fixed order.

Rejected:

- A declared system order with immediate visibility: results depend on the order, and adding a system means finding its slot.

Sources:

- https://gameprogrammingpatterns.com/double-buffer.html: the pattern and its use for order-independent actor updates.

## How can module-private headers be enforced mechanically?

Three approaches exist.

Clang's layering check uses module maps. A header marked private cannot be included from outside its module, and -Wprivate-header reports it. Bazel generates these module maps automatically. The check requires Clang with module maps, however, and this project builds with GCC. It would also have to run inside clang-tidy, which is awkward.

CMake's PRIVATE include directories hide a directory from the targets that link against a library. That only works if the private headers cannot be reached through a shared include root. Here every header is included relative to src/, so this approach would mean restructuring the include roots.

A plain scan of #include lines is the third approach. It fails when a file includes another module's internal directory, for example `#include "guests/internal/x.h"` from outside src/sim/guests/. It is simple, works with every toolchain, and can run as a ctest test or in the pre-push hook. It can be proven to catch violations by planting one.

Rejected:

- Clang module maps: GCC is the primary compiler, and module maps add a second build configuration to maintain.
- PRIVATE include directories: would require per-module include roots.

Sources:

- https://maskray.me/blog/2022-09-25-layering-check-with-clang: Clang layering check, private headers, and Bazel's generated module maps. Summarized from search results; the page itself was not reachable.
- https://cmake.org/cmake/help/latest/command/target_include_directories.html: PRIVATE include scope.
