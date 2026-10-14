# Research: lean-routes

## What route distance repeats, and where its memory goes

The question is which work in the route-distance resolver repeats with no new answer, and which memory it makes and frees at every resolution. The findings come from reading src/sim/routes/route_distance.cpp, src/sim/medium/field.h, src/sim/cycle.cpp, and src/sim/schema.h, and from the milestone's profiles.

resolveRouteDistance publishes, for each kind of network, the entries of every entity anchored on it. For each source, routeEntries calls leavingSteps, which builds a list of the steps leaving each node from the network's edges. That list depends only on the network, so a resolution builds the same lists once for every source. routeEntries then runs a Dijkstra search with a std::priority_queue, an entries list, and a settled list, all new for each source. publishRouteDistance makes a new placed list for each source and moves it into the field with publishResolved.

The field's resolved layer frees its memory at every resolution. emptyResolvedEntries replaces the field's ResolvedEntries with a new value and moves the old slots into Previous. settleSteppedEntries compares Previous with the new slots and then clears Previous, which frees every slot's entries. The stepped layer already keeps its memory: its swap moves emptied slots into Retired, which publishStepped takes from, and which walks, saves, hashes, and comparisons never see. The resolved layer can do the same. publishResolved and publishStepped also build the field's name as a std::string at every call, for their error messages. The route-distance fields' names are longer than a string's own buffer, so every publish allocates and frees one.

Resolvers take only the world, so they have nowhere to keep lists between resolutions. Systems and swaps take a TickBuffers that the stepping caller keeps (decision 0041), and stepWorld already holds one when it resolves. Giving resolvers and finishers the same buffers lets every resolver keep its lists, as systems do. resolveWorld, makeCandidate, and commitCommands then take the caller's buffers too. Their forms without buffers stay for callers that resolve once, as stepWorld's do.

A std::priority_queue pushes with push_heap and pops with pop_heap on its container. The same calls on a kept vector, with the same comparison, give the same order. The search's results don't depend on the order it settles ties in anyway, as routeEntries says.

In the profiles, route distance is 31% of a full.park preview, and 42% of its time is heap work. routeEntries' own time, the search and the step lists, is 27% of the preview.

Rejected:

- Keeping the step lists in the Network. The network is derived data in the world, so it would be copied with every candidate and walked by every comparison. The lists belong to the resolver's buffers.
- A second publishResolved that keeps the by-value form. Six resolvers in src/sim and one in the scenarios publish, and one form that hands out a kept list serves them all.
- Keeping lists in the derived components themselves, unvisited. A candidate's copy would copy them too, and nothing would own clearing them.

Sources: src/sim/routes/route_distance.cpp; src/sim/medium/field.h; src/sim/cycle.cpp; src/sim/schema.h; src/sim/tick_buffers.h; plans/scalable-runtime/changing-previews/RESEARCH.md.
