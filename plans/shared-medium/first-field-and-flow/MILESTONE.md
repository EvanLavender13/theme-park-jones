# Milestone: First Field and Flow

Slice: boxes-and-tubes

## Summary

first-field-and-flow builds the medium every other slice member meets through: the network type with carriers, places, and anchors, fields published at places and sampled with per-source attribution, and the flow ledger that moves conserved integer units between endpoints with a delay. It lives in tpj_sim as the module src/sim/medium, with its own SPEC.md, and every part of it is registered with world-as-value's walk and runs inside its cycle. It is proven on synthetic networks, fields, and flows, with no park content. It comes second in the slice because navigable-networks, plausible-operations, and believable-guests all publish into it and sample from it, and the properties the principles promise (exact attribution, exact conservation, order independence) are established and tested here once instead of in each of them. It also settles two questions left to it: which medium contents are state and which are derived, and what happens to units whose endpoints are removed.

## Acceptance criteria

1. A network is a value of nodes and edges with lengths, where every edge is a stretch of a carrier with a stable key and a ground geometry, and a node may be anchored to an entity key. A place is a carrier key and a distance along it. A network resolves a place to a node, or to a point on an edge with its offsets to both ends, or to no place when the carrier is not in the network or the distance lies outside it. On random synthetic networks, where each edge is its own straight carrier, every place strictly inside an edge resolves to that edge with offsets that sum to its length, and every node is found at the place of each carrier that ends there.
2. An anchored entity's nodes are found through the network by its key, with their places, without reading how the network was derived. A key with no anchor finds nothing.
3. A ground position finds the nearest place on a network. No point of any carrier is nearer, ties break by carrier key and then by distance along, and an empty network gives no place.
4. The carry-over operation takes a place and the networks from before and after a re-derivation. A place on an unchanged carrier keeps its distance. A place on a carrier whose geometry changed moves to the point of the same carrier nearest its old ground position. A place on a carrier the new network lacks resolves to no place. A place held while a new node splits its edge names the same ground position afterwards.
5. A field is registered by its owning module with a name, an entry type, a kind (entry or scalar), and a sampling rule. Sampling a field at a place gives its per-source entries in ascending source key order. The default rule gives the entries published at the same network position as the place: the same node, or the same point of the same edge. An owner may supply a rule that is given the place's edge, its offsets to both ends, and the entries published at both end nodes and along the edge, which is how route distance will read a place between nodes. A scalar field's value is the ordered sum of the entries its sample gives, bit for bit, and a place with no sources gives no entries and a value of zero.
6. Resolvers publish into a field during resolution, and the entries are readable at once. Resolved entries are derived data: they are never saved, each resolution replaces every source's resolved entries in full, so a removed source or carrier has none afterwards, and resolvers read only intent and derived data. A candidate world samples exactly as the world that commits the same commands does.
7. Systems publish into a field while stepping, and the entries are not readable until the swap that ends the tick. They are readable for that one following tick, after which the next swap replaces them with what was published during it. Stepped entries are state and are saved. A stepped entry is sampled against the current network, so one whose place no longer resolves is not sampled. Running the synthetic producers in shuffled orders gives the same world and the same hash every tick.
8. A source with stepped entries in a field is sampled by them, and otherwise by its resolved entries. A resolution after a command clears a source's stepped entries when it changes that source's resolved entries, including removing them, so a removed path changes the offers that depended on it in the very next tick. Loading the save of a resolved synthetic world with both layers in flight and resolving gives a world equal to the one saved.
9. Flow kinds are registered by name. Units are integers. The ledger creates units into an endpoint's stock, sends them from a stock as a packet with a delay of at least one tick and an optional handle (an entity key its units are addressed to), and consumes them from a stock with a named cause. A stock holds its units by handle, so a delivered packet's units keep theirs. These happen only while stepping. For each kind, units created equal units in transit plus units held plus units consumed, exactly, in every tick of randomized synthetic runs. Packets and stocks are held in an order fixed by their contents, and while stepping an endpoint's stock is changed only by its own entity's operations, so ledger operations made in shuffled producer orders give the same world and the same hash.
10. A packet sent while stepping tick t with delay d is in its destination's stock from tick t + d on, and not before, whatever happens to the networks in between.
11. A packet whose destination no longer exists at its arrival goes back to its sender with the same delay. When the sender no longer exists either, at that point or when the return arrives, its units are consumed with the cause "undeliverable". At the next swap after an endpoint's entity is removed, the units in its stock whose handle names a live entity go to that entity's stock, as a deleted shop's queued guest visits go back to their guests and a deleted depot's held orders go back to their shops. The rest of its stock is consumed with the cause "discarded". So every unit is in transit, held by a live endpoint or by one removed since the last swap, or consumed.
12. Networks, resolved entries, stepped entries, packets, stocks, and the ledger's created and consumed counts are registered with the walk. A copy stepped forward equals the original stepped forward, and changing any stepped entry, packet, stock, or count changes the hash.
13. tpj_scenarios gains a synthetic scenario that publishes both layers of an entry field and a scalar field and moves units of two flow kinds, so the cross-build check covers the medium.
14. A header in a directory named internal may be included only by files under that directory's parent. A ctest test checks every file under src and tests, passes on the tree, and fails on a planted violation. docs/conventions.md states the rule.
15. src/sim/medium/SPEC.md describes the network type, fields, and the ledger as built, and src/sim/SPEC.md says how medium state is classed. linux-debug builds without warnings, its tests pass, and the cross-build check passes.

## Medium

This milestone introduces the medium's machinery, not any park field or flow. What its features provide to each other and to the slice:

- The network type, places, anchors, and nearest-point query (networks-and-places): every field is sampled at places, and the carry-over, fields, and scenario features build on them. navigable-networks produces the park's networks in this type. believable-guests holds places, and plausible-operations finds anchored nodes.
- Carry-over (place-carry-over): believable-guests carries each guest's place across a re-derivation. How the previous networks stay available for it is designed with believable-guests' hungry-guests, the first holder.
- Resolved fields and owner-supplied sampling rules (resolved-fields): navigable-networks publishes route distance with its between-nodes rule. plausible-operations publishes the resolved layer of the food offer from intent and route distance.
- Stepped fields and the layer rule (stepped-fields): plausible-operations republishes the food offer from its state, and believable-guests publishes hungry footfall. legible-simulation samples every field in committed and candidate worlds.
- The ledger (flow-ledger): plausible-operations moves supply orders, supplies, and meals, and exchanges guest visits with believable-guests, taking each packet's delay from route distance.
- The include check (private-header-check): every later module's internal headers.

Medium state is classed as follows, which settles deterministic-simulation's open question. Networks and resolved entries are derived. Stepped entries, packets, stocks, and the ledger's counts are state. Each field is held on an entity with a derived key in two registered components, a derived one for resolved entries and a state one for stepped entries, and the ledger in state components on another such entity, so the walk covers them as it stands. Field entries sit in slots ordered by source key, and packets and stocks in an order fixed by their contents, whatever order producers publish and send in.

## Dependencies

- deterministic-simulation's world-as-value: the walk, keys, the cycle with its swap functions, candidates, saves, and the cross-build check. Met.
- No other slice member. navigable-networks, plausible-operations, and believable-guests build on this milestone.

## Core feature

networks-and-places is the core. Fields are sampled at places, carry-over moves places, and navigable-networks produces its graphs in this type, so nothing else in the milestone or the slice starts without it. It is usable on its own: a synthetic network resolves places, finds anchors, and snaps ground positions, and it is copied, hashed, and re-derived by resolvers like any derived data.

## Features

1. `networks-and-places`: the network type with nodes, edges, lengths, carriers with ground geometry, anchors, places and their resolution to network positions, the nearest-point query, registration as derived data, and a random synthetic network generator for tests. Depends on: none.
2. `place-carry-over`: the carry-over operation across a re-derivation, for unchanged, changed, and removed carriers. Depends on: feature 1.
3. `resolved-fields`: field registration with entry and scalar kinds, publication by resolvers, per-source entries in source key order, the default and owner-supplied sampling rules, and scalar values as ordered sums. Depends on: feature 1.
4. `stepped-fields`: publication while stepping, the swap that makes it readable for one tick, stepped entries as saved state, the layer rule with clearing on changed resolved entries, and the shuffled-producer and load tests. Depends on: feature 3.
5. `flow-ledger`: flow kinds, creation, packets with delays and handles, stocks, consumption with causes, delivery at the swap, return to sender, handles and the return of a removed endpoint's addressed units, the undeliverable and discarded rules, the conservation identity, and the synthetic scenario for the cross-build check. Depends on: feature 4, for the scenario's fields.
6. `private-header-check`: the include scan over src and tests as a ctest test, its planted violation, and the rule in docs/conventions.md. Depends on: none.

## Deepening candidates

- Per-field relocation policies: a field or holder that keeps its distance along an edited carrier rather than its ground position, as linear referencing systems allow per kind of event. Gated on: a holder that needs it.
- Resolved entries from state: letting a resolver read a source's state, with a rule that keeps a loaded world equal to the one saved, so a moved shop's offer is exact in the tick it moves. Gated on: the one-tick intent-only offer being visible in play.
- Resolved-only fields: a field that is only ever resolved, such as route distance, registers no stepped layer, so a resolved park's save holds nothing for it.
- Packet split and merge: dividing a shipment between destinations, or combining packets on the same route, preserving the count. Gated on: a flow that needs it.
- The capability's deepening candidates (ground-domain fields, on-demand fields, change-driven resolution, edge capacity, agent-backed flows, generic field enumeration) stay in CAPABILITY.md.

## Open questions

- Copy and hash cost of the medium's tables at slice scale. Resolved by measurement with tpj_scenarios once believable-guests lands, as the capability's open question says.

## Research notes

- A place is linear referencing's route and measure, and the capability's carry-over is its Stay Put rule for edited routes and Retire for deleted ones.
- Route distance between nodes is the phantom-node rule, the smaller of each end's distance plus the offset to it, so sampling rules belong to each field's owner.
- Games players trust keep undeliverable units, and accounting records a loss as a named write-off, which is what "undeliverable" and "discarded" are.
- One table per field, on a keyed entity, with ordered slots, gives fixed source order and is covered by the walk unchanged.

Depth is in RESEARCH.md.
