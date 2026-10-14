# Research: changing-previews

## Where a preview's time goes on full.park

The question is what makes a preview cost about 13 ms on full.park, and so which kind of fix can hold the frame rate while an edit changes every frame.

The runtime report after quiet-commands gives the preview stage a median of 13.2 ms on windows-release, next to 14.9 ms for one resolution. The stage previews a MoveBox that puts a shop back on its own pose (bench/stages.cpp). Four Very Sleepy profiles of `tpj_bench --ticks 1` on full.park gave 5.9 s of samples inside previewEdit (build/perf/previews-1.sleepy to previews-4.sleepy). Each share below is of previewEdit's samples, with the milliseconds it implies at a 13.2 ms preview:

- Route distance, 31%, about 4.1 ms. It runs one Dijkstra per source over the whole network, for both kinds. routeEntries builds the network's list of steps leaving each node again for every source (leavingSteps), though it depends only on the network. 42% of the resolver's time is heap allocation and freeing.
- Path networks, 15%, about 2.0 ms. Almost all of it is findConnections. For each door, it measures the distance to every surface piece and every path, so its work grows with doors times pieces.
- Furniture, 9.5%, about 1.3 ms, spread over its obstacle hash and each segment's surroundings.
- The walked surface, 9%, about 1.2 ms. It derives the whole paved surface again with the connectors' walkways added (networks.cpp resolveWalkedSurface calls parkSurface).
- Best seats, 8%, about 1.0 ms, in bestPlaces.
- Carrying the guests onto the new ground, 7%, about 0.9 ms.
- Copying the world, 7%, about 0.9 ms. Route distance's entries and the 2,000 guests are the largest stores.
- The paved surface, 5%, about 0.6 ms.
- Guest ground 3%, both offers 1.5%, seat offers 0.7%, applying the command 1.4%, the acceptance check 1.3%, and the shop context 0.5%.

Heap allocation and freeing are 19% of the preview, most of it in route distance. Making resolution reuse its storage, which decision 0041 needs before the allocation check can watch previews, saves at most about that share. It can't bring a preview under a frame on its own.

No single resolver dominates. Route distance, the path networks, and the walked surface are about 55% between them. Each holds work that repeats on every resolution for no new answer: the step lists per source, every piece per door, and the paved surface derived twice. The profile can't separate that part from the rest of each resolver, since the step lists are built inside routeEntries next to the search each source needs.

The bench's edit is a box moved onto its own pose. Its acceptance check is cheap. A path edit's check plans the edit and derives the edited surface for the whole park (edits.cpp acceptedPlan), and the app runs it three times in each changed frame: in previewEdit, again when makeCandidate applies the path command (each path command's applyCommand checks its plan), and again in buildGhostMesh. The ghost also plans the edit again for editedSurface and editedSegments. Nothing yet times a path edit's preview, its check, or its ghost, and nothing times the app's frames during a drag.

The preview also runs at every tick while an overlay is shown and the preview holds a candidate. At about 13 ms a tick, that is about 40% of a core at full speed, whether or not the pointer moves. Route distance, the path networks, both surfaces, and furniture derive from intent and the held edit, which ticks don't change, and they are about 70% of the preview. The offers read the shops' stock and deliveries, and carrying guests reads the guests, so those must run again at each tick.

Rejected:

- Only reusing storage, and only trimming repeated work. Reusing storage caps the gain at about a fifth of the preview. lean-routes did both for route distance and took a resolution from 12.3 ms to 9.3 ms. Trimming every resolver the same way would still leave a path frame near 10 ms, because every resolver still works over the whole park.
- A spatial index everywhere. Only findConnections, hiding in the surfaces, and furniture's obstacles search space. A spatial index helps those, inside the local features, but doesn't make the rest local.
- Resolving off the frame's thread instead. The frame would show a candidate one or more frames behind the pointer, which principle 8's exact preview would have to allow. The resolution would still cost a full core while dragging.

Sources: build/perf/previews-1.sleepy to previews-4.sleepy (Very Sleepy on windows-release tpj_bench); build/quiet-commands-after.report; src/sim/routes/route_distance.cpp; src/sim/routes/networks.cpp; src/sim/park/surface.cpp; src/sim/park/edits.cpp; src/legible/preview.cpp.

## What stops a candidate re-deriving only what the edit touches

The question is what each resolver would need so that a changed frame re-derives only what the edit reaches, with a result bit-identical to a full resolution.

The world and its resolution:

- copyWorld clones every store (walk.cpp copyWorld, TypedStore::clone). A store has no call that assigns from another store reusing memory, and no record of which keys changed.
- resolveWorld runs every resolver and finisher. The dependency lists given to addResolver are checked at registration only.
- Each field's resolved entries are retired and published again for every source on every resolution (field.h emptyResolvedEntries). The settling finisher compares every source's old and new entries word by word.
- PreviousNetwork holds the network from before the resolution. carryGuests and carryFootfall read it. A candidate moved from the last frame's candidate must therefore start from the world's network, guests, and footfall, never the last frame's.

Each resolver:

- Paved surface: reads only the path points and segments. A box move leaves it unchanged. One point's change reaches about three point hops, through corner radii and trims. Hiding is spatial: it scans every piece's bounds for each edge line, which is O(edges × pieces). EdgeLine.First and SegmentSide.First are offsets into flat arrays, so a local result spliced into the stored surface needs its offsets rebased. surfaceAround and pointsAffected already derive the surface around changed points, but still run the whole groundOf.
- Path networks: each door's connection depends on the pieces within about 3 m of it, but finding it scans every piece. Nodes are numbered in carrier key order and stop distance. Connector keys sort after every path key, so a new path, a new stop from a split, or a moved connection renumbers every later node.
- Walked surface: derives the whole paved surface a second time with the walkways added.
- Route distance: runs one search per source over the whole network. Entries are summed in floating point. Cutting an edge in two can change the last bit of every distance beyond the cut, so an exact local update would reach most of the network for many sources. Adding lengths in whole units makes the halves sum to exactly the old length. Entries are published in node order, which renumbering changes.
- Furniture: already keeps a chain whose inputs are unchanged, by a hash of what it reads. It still builds its obstacle grid, its chains, and every chain's read hash over the whole park each time. Pieces laid by earlier chains are obstacles for later ones, so a re-laid chain can change later chains near it.
- Guest ground: one component for the whole park. Groups are numbered by union-find over every guest path, so joining or splitting groups renumbers distant cells.
- Seat, facility, food, and drink offers: one slot per piece or box, derived from that piece or box and its anchored nodes. Food and drink offers also read backstage route distance, so a moved depot changes every shop's slot. Finding a shop's depot scans every box per shop.
- Best seats: one multi-source search over the whole network, with the best four labels kept at each node. With only two kinds of seat relief, a seat's influence can reach every node. An exact repair has to start from the changed seats and edges and spread only while labels change.
- carryGuests: visits every guest, carries it onto the new ground, and sets its aim to its place. Every edit re-aims all 2,000 guests on full.park, and a guest that doesn't fit where it stands, such as at a door's end, moves however far away the edit is.
- carryFootfall: entries on stretches the edit left unchanged come out the same, so carrying only the changed stretches is exact.

What the frame reads from the candidate: without an overlay, the walked surface of the edited owners, the connectors' walkways, the guest network, nearest depots by route distance, and furniture changes. With an overlay, also route distance and the offer fields over the whole park. Since principle 8 needs the whole candidate exact, the milestone keeps the whole candidate exact rather than deriving only what the frame reads.

A path edit is planned five times per changed frame: in previewEdit's acceptance check, when the command is applied, in the ghost's acceptance check, in editedOf, and in pathCrossings.

Sources: src/sim/walk.cpp; src/sim/schema.h; src/sim/cycle.cpp; src/sim/medium/field.h; src/sim/park/surface.cpp; src/sim/park/layout.cpp; src/sim/park/edits.cpp; src/sim/routes/networks.cpp; src/sim/routes/route_distance.cpp; src/sim/routes/best_places.cpp; src/sim/furniture/furniture.cpp; src/sim/guests/ground.cpp; src/sim/guests/guests.cpp; src/sim/guests/footfall.cpp; src/sim/operations/operations.cpp; src/legible/preview.cpp; src/render/park_mesh.cpp; src/tools/tools.cpp.

## How other systems re-derive only what changed

The question is which known designs re-derive only what an input change reaches, with a result equal to deriving from nothing, and what each needs from this codebase.

Build systems. Build Systems à la Carte (Mokhov, Mitchell, Peyton Jones) names the two properties this milestone needs:

- Minimality: run a task at most once, and only if it depends, through other tasks, on an input that changed.
- Early cutoff: when a task runs again and gives the same output as before, don't run the tasks that depend on it.

Minimality is only as good as the grain of the tasks. Make rebuilds per file. If one task produces the whole program, any change rebuilds everything. Each resolver here is one task over the whole park, so any edit dirties every one of them. This is the root cause of the whole-park work, not any single resolver. The paper also separates two ways to know what a task depends on:

- Static dependencies, declared before the build (Make).
- Dynamic dependencies, found while the task runs and kept as a trace to check next time (Excel's calc chain, Shake's verifying traces).

Salsa, the engine under rust-analyzer and rustc's incremental compilation, uses dynamic dependencies at the grain of one function call on one key:

- It records which inputs and queries each call read, and the revision each last changed in.
- A call whose reads are all unchanged is reused without running.
- A call that runs again and returns an equal value keeps its old revision (backdating). That gives early cutoff.

Spatial derivations. Recast's tiled navigation mesh splits the mesh into tiles. When an obstacle is added or removed, it rebuilds only the tiles the obstacle's bounds overlap, widened by the agent's reach. The paved surface, the walked surface, door connections, furniture, and guest ground are all spatial with a bounded reach (about 3 m for a door, a few point hops for paving). They fit this model: a change dirties the tiles its old and new bounds overlap, plus the reach.

Connectivity. Parkitect keeps which paths connect to which as a flood fill, recomputed when a path is added or removed. That matches guest ground's groups and the network's components here. Group numbers must not depend on whole-park order, or a local change renumbers everything.

Graph distances. Route distance and best seats can't be tiled: one edit can change a distance anywhere downstream. The dynamic shortest-path literature handles this:

- Ramalingam and Reps update single-source shortest paths after a mix of edge insertions, deletions, and length changes. They touch only the vertices whose distance actually changes, with the edges that change them. Experiments report about 95% less update time than running Dijkstra again.
- Lifelong Planning A* and D* Lite (Koenig, Likhachev) reuse the parts of the previous search tree that are unchanged in the same way.

Both assume that summing edge lengths gives the same distance however an edge is cut. Floating-point sums don't, so an exact update here needs lengths added in whole units. Customizable route planning, which Bing Maps used, partitions the graph into cells and, after a metric change, recomputes only the changed cells' boundary distances. Here route distance publishes a distance at every node for every source, so the published field itself is sources × nodes. A cell scheme would cut the search, but not the size of what is published.

Change records in storage. Bevy's ECS keeps, on each component, the tick when it was added and last changed. A system queries only what changed since it last ran. This is a cheap dirty bit at the grain of one component on one entity.

Candidates without copying. Persistent data structures (HAMTs, immer) share every untouched part between versions, so a copy costs about O(log n) and an edit allocates only along the changed path. An overlay that holds only changed keys, and reads through to the world for the rest, does the same at the grain of one component.

What the codebase already has. Furniture's LaidChains is a verifying trace at the grain of one chain. It hashes everything a chain reads and keeps the chain when the hash matches, which is Salsa's reuse on a small scale. surfaceAround and pointsAffected already compute which points a paving change reaches. No other resolver has either.

What this suggests:

- The fix is one mechanism in the schema, not a change inside each resolver. Derived values are split into units keyed by what they derive from:
    - Spatial units, as tiles or per owner, for paving, connections, furniture, and ground.
    - One unit per source for route distance, and one per seat for best seats.
- Each unit records what it read, as Salsa does. When an edit changes an input, only the units that read it run again. When a unit's output is unchanged, the units downstream of it are cut off.
- The candidate holds only the units and components the edit changed, and reads through to the world for the rest.
- Route distance and best seats keep one unit per source, updated by a dynamic shortest-path algorithm seeded from the changed edges, with lengths added in whole units.
- One check proves it: incremental resolution equals a resolution from nothing, on every park file and every scripted drag. This is the from-scratch consistency property of self-adjusting computation.

Rejected:

- Hand-written reach rules in each resolver (dirty bits, as Make uses). A rule that misses a reach gives a wrong world that no single resolver's code shows. Recorded reads find the reach from the code itself.
- Recording reads at the grain of whole stores. A component store per type is too coarse. Every resolver reads the segment store, so every edit would dirty everything again.
- Tiling route distance by cells. It cuts the search but not the sources × nodes the field publishes, and every source still needs its whole field.

Sources:

- https://www.microsoft.com/en-us/research/uploads/prod/2018/03/build-systems-a-la-carte.pdf: minimality, early cutoff, static and dynamic dependencies, and Excel's calc chain.
- https://rustc-dev-guide.rust-lang.org/queries/incremental-compilation.html: red-green marking, and reuse when the result is unchanged.
- https://people.mpi-sws.org/~hammer/adapton-pldi2014.pdf: demand-driven change propagation, which dirties and then cleans a graph of memoized calls.
- https://recast-rs.readthedocs.io/en/stable/crates/waymark-tilecache.html: rebuilding only the navigation tiles that an obstacle change overlaps.
- https://www.texelraptor.com/blog/devlog-update-77: Parkitect's path connectivity by flood fill, recomputed on path edits.
- https://research.cs.wisc.edu/wpis/abstracts/jalg96.abs.html: Ramalingam and Reps' incremental shortest paths.
- https://arxiv.org/pdf/2102.11169: a survey of dynamic graph algorithms, with RR's measured speedup.
- https://idm-lab.org/bib/abstracts/Koen02e.html: D* Lite and the reuse of unchanged search trees.
- https://i11www.iti.kit.edu/extra/publications/dgpw-crp-11.pdf: customizable route planning, which recomputes only changed cells.
- https://taintedcoders.com/bevy/how-to/component-changed/: Bevy's change ticks.
- https://meetingcpp.com/2025/Talks/items/Persistence_squared__persisting_persistent_data_structures.html: immer and structural sharing.
