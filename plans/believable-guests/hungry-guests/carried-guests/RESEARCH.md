# Research: carried-guests

The milestone's research settles how places are carried: path-networks keeps the previous networks, a guests finisher carries every place with carryOver, and a routes finisher registered after the guests module drops the previous networks (plans/believable-guests/hungry-guests/RESEARCH.md). This file holds the questions left for the feature.

## Where does the previous network live between the resolver and the finisher?

A field keeps its previous entries in a second slot of its own derived component, so the previous resolution's data sits on the same entity as the current one and goes with it. path-networks can do the same: when the network entity already holds a network, it moves that network into a second derived component on the same entity before putting the new one there, and the dropping finisher removes that component. Nothing is created or destroyed, so the finisher needs no entity bookkeeping, and a copy made between cycles holds the same components as the world it copies. A world's first resolution, whose entity holds no network yet, gets no previous network, so the carrying finisher does nothing then, as the sim contract requires.

Rejected: a second derived entity holding the previous network in the existing Network component. It reuses the component type, but the finisher would destroy an entity every resolution, and the walk would see entities come and go. Rejected: registering the dropping finisher inside addRoutes. Finishers run in registration order, and addRoutes registers before the guests module, so it would drop the previous networks before guests read them.

Sources: src/sim/medium/field.h, ResolvedEntries and settleSteppedEntries; src/sim/routes/networks.cpp, resolvePathNetworks.

## When does a guest with no guest network left leave?

With no carrier left on the guest network, carryOver retires every place and nearestPlace finds none, so the guest has nowhere to stand. The step already makes a guest whose place does not resolve leave the park, which also covers a hand-written save holding such a place, since a world's first resolution carries nothing. Keeping that rule as the way out means the finisher only moves places and never destroys entities, and the guest leaves in the next cycle, with its stocks settled by the ledger as for any guest that leaves. The one visible cost is that a candidate deleting the last guest path still holds its guests, with places that do not resolve and records with no position, until the cycle that commits it steps. The ghost does not draw guests yet, so nothing shows it.

Rejected: destroying the guest in the finisher. A candidate would show the guests gone at once, but the step's rule is still needed for loaded saves, and a finisher that destroys entities widens the sim contract further than carrying state does.

Sources: src/sim/SPEC.md, finishers; src/sim/guests/SPEC.md, Stepping.
