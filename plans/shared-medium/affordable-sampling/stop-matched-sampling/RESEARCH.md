# Research: stop-matched-sampling

The milestone's research settles the approach: sampling compares each entry's place with the network's stop places and the sampled edge's bounds instead of resolving it (plans/shared-medium/affordable-sampling/RESEARCH.md). This file holds the questions left for the feature.

## Is comparing places exactly what resolving gives?

resolve gives node n for a place exactly when the place's carrier is in the network, its distance is not NaN and lies in the carrier's range, and the first stop at or beyond it is at exactly its distance and names n. The stops' distances are the only distances that give a node, so the place is one of n's stop places, the carrier's key and that stop's distance. Conversely, a stop place resolves to its stop's node. Place's equality compares distances as numbers, so a place at -0 equals a stop at 0, and resolve treats -0 as 0 too, since -0 is not below 0 and compares equal to the first stop. A NaN distance equals nothing and resolves nowhere. A carrier absent from the network has no stop places, and resolves nowhere.

A place resolves strictly inside edge e exactly when its carrier is e's carrier and its distance lies strictly between e's FromDistance and ToDistance: those are consecutive stops, so the first stop at or beyond the distance is e's To stop, not at the distance. Its offsets from resolve are the distance minus FromDistance and ToDistance minus the distance, the same two subtractions a comparison computes, so they agree bit for bit. A place inside e is never a stop place, so the two tests never both hold, and each entry lands in the same list, in the same order, as resolving it would put it.

An edge whose two ends are one node, a carrier looping back to where it started, has one list of stop places for both ends, so an entry there is given to both AtFrom and AtTo, as the spec says.

## How does the network hold and give stop places?

The constructor already visits every carrier in key order and every stop in distance order to build the edges and node places, so filling each node's stop places in that pass gives them in ascending carrier key and then distance with no sort. They are held in one flat list, node by node, with each node's start in a second list, which costs two allocations per network rather than one per node. stopPlaces returns a std::span over a node's run, which the medium's header can return without copying. nodePlace becomes the first of a node's stop places, which is its current definition, so the separate list of node places goes. The node count check that no node lacks a stop moves to the count pass.

Rejected: a std::vector<std::vector<Place>>. It allocates per node on every resolution, and a network is rebuilt on every edit. Rejected: returning a vector by value. Sampling calls it for every sample, which would put an allocation back on the hot path.

## How are before and after measured?

The milestone's criterion names the measurements: perf's inclusive share of Network::resolve over tests/parks/supply.park stepped 3000 ticks on linux-debug, and wall time for the same run on windows-debug and linux-debug. The app's --hash option prints the final world's hash, so the same runs also show the final state is unchanged. The before figures are taken on the feature branch before any code changes, since earlier figures came from a different machine load. The cross-build check's linux-debug output, build/cross-build-check/linux-debug.txt, is copied aside before and compared with cmp after, which checks every scenario's and park's hashes at once.

perf runs as /usr/lib/linux-tools/6.8.0-142-generic/perf, since the perf wrapper refuses the WSL kernel.

Sources: src/sim/medium/network.cpp, the constructor and resolve; src/sim/medium/field.h, sampleSlotAtNode and sampleSlotInEdge; scripts/cross-build-check.sh.
