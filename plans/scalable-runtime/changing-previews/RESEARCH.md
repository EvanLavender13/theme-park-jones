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

Rejected, for now:

- Only reusing storage. It caps the gain at about a fifth of the preview.
- A spatial index everywhere. Only findConnections, and possibly furniture's obstacles, search space. Route distance and best seats are graph work, and the surfaces are derived per segment.
- Resolving only what the edit touches, first. Route distance is the largest share, and a path edit or a moved shop changes the network every source's distances run over. Keeping it exact incrementally means dynamic shortest paths with the same ties, which is a large change to do before the waste above is gone.
- Resolving off the frame's thread, first. It is already a capability deepening candidate, gated on resolution still being too slow for a frame. The frame would show a candidate one or more frames behind the pointer, which principle 8's exact preview has to allow.

Sources: build/perf/previews-1.sleepy to previews-4.sleepy (Very Sleepy on windows-release tpj_bench); build/quiet-commands-after.report; src/sim/routes/route_distance.cpp; src/sim/routes/networks.cpp; src/sim/park/surface.cpp; src/sim/park/edits.cpp; src/legible/preview.cpp.
