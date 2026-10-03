# Research: place-indexed-entries

## Where should a slot's index live, and when is it built?

A slot's entries change in only a few ways, all in src/sim/medium/field.h: publishResolved and publishStepped insert a new slot; the swap moves the pending slots into readable; the finisher erases whole readable slots; the resolver moves the resolved slots into Previous. Copies copy each component whole (copyWorld's type.Copy), and loads build new slots from a save through visitFields. No code changes a slot's Entries in place once the slot exists, and the tests that write slots directly insert new ones. So an index kept beside the entries, inside the slot, is correct for as long as the slot lives, and it travels with every move and copy of the slot.

Building it on a slot's first sample, rather than at each place a slot is made, covers every path at once: a published slot, a loaded one, and a copied one that was never sampled all start without an index and get one when first read. A slot that is published and replaced without being read, such as most of a resolution's Previous slots, never pays for one. The index is a member the walk does not visit, so saves, hashes, and worldsEqual are unchanged, and it is marked mutable so sampling, which reads a const world, can build it.

Rejected: an index component per field, rebuilt by the finisher and the swap — a second owner and a second path for each change, and loads and copies need their own hooks. Building at publish — loads and test-written slots bypass publish. Sorting the published entries themselves — changes the source's order, which saves, hashes, and samples expose. An index by node or edge — ties it to one network, while sampleField takes the network as an argument and candidates sample other networks.

## How does an index by place answer a sample exactly as the scan did?

The scan's comparisons are place equality, against each of a node's stop places, and, inside an edge, the edge's carrier with a distance strictly between its two stop distances. Ordering a slot's entry positions by carrier, then distance, then position answers equality with an equal range and the inside of an edge with a range on one carrier between two bounds. A distance that is NaN equals nothing and lies strictly between nothing, so such an entry is never sampled by the scan, and the index leaves it out, which also keeps the ordering a strict weak order. Plus and minus zero compare equal under both the scan and the ordering. Matches are gathered as positions, sorted, and made unique, so they come out in the source's order, once each, as the scan gave them, even if a node listed one place twice.

Sources: src/sim/medium/field.h (publishing, swapping, settling, sampling); src/sim/walk.cpp (copyWorld); src/sim/medium/SPEC.md (sampling rules); tests/sim/operations/food_offer_test.cpp (slots written by tests).
