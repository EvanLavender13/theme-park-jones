# Research: targeted-route-samples

## What does a walking guest read from route distance?

walk (src/sim/guests/guests.cpp) samples guest route distance at the guest's place on every pass and hands the sample to headingOf and choose. headingOf reads one entry from it: the Target's, through entryOf, for a guest heading to a shop, and the least entrance entry, through homeEntry, for one heading home. A wandering guest reads nothing from it until it chooses. choose scores every source with an entry, so it alone needs the whole sample. Passes far outnumber choices: a guest walks about 4 cm a tick and chooses only at the nodes it leaves. headingOf also calls suppliedOffer for its Target, which samples the food offer at the shop's lowest anchored node for every source and takes the Target's entry, after allocating the shop's anchored nodes; scoreOptions does the same for every source in a choice.

## How can one source's entry equal what the full sample gives?

sampleField chooses each source's slot by the layer rule, then applies the node rule or the field's edge rule to that slot alone; no source's entries affect another's. Slots are kept in ascending source order in both the resolved and the stepped lists (publishResolved and publishStepped insert at lower_bound), so one source's slot is a binary search away. At a node, the source's sampled entries are its entries among the node's stop places, in order, so the first such entry is what entryOf took. Inside an edge, route distance's rule takes the least of the source's From-end entries, then its To-end entries, each offset by the place's offsets, ties to the earlier; skipping entries strictly inside the edge, as the medium's edge sample does, and making the same additions in the same order gives the same bits. Route distance publishes one entry per node per source, so a source has at most one entry at a place, and the least entrance entry over the entrances in ascending key order equals homeEntry's over the sample.

Rejected: a generic one-source sampleField in the medium — a field's edge rule returns a vector, so it would allocate; and routes restating the edge rule — the rule moves into helpers that sampleEdge and routeEntryAt share.

Sources: src/sim/guests/guests.cpp (walk, headingOf, choose, suppliedOffer); src/sim/medium/field.h (layeredSlots, sampleSlotAtNode, sampleSlotInEdge, publishResolved, publishStepped); src/sim/routes/route_distance.cpp (sampleRouteEdge); src/sim/routes/SPEC.md; Very Sleepy profile in plans/scalable-runtime/indexed-sampling/RESEARCH.md.
