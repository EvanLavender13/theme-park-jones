# Feature: Flow Ledger

## Summary

flow-ledger adds the medium's second kind of interaction: quantities that are conserved as they move (principle 3). A module defines a flow kind by name and registers it with addFlow, which gives the kind a ledger held as state on a derived-key entity. While stepping, a system acting for an endpoint creates units into its stock, sends units from it as a packet with a delay and a handle, and consumes units from it with a named cause, and no operation touches another endpoint's stock. The kind's swap delivers packets that are due, returns packets whose destination is gone, and settles the stocks of removed endpoints, consuming what cannot go anywhere as undeliverable or discarded. So every unit is held, in transit, or consumed. tpj_scenarios gains a synthetic scenario, stalls, that publishes both layers of an entry field and a scalar field and moves units of two flow kinds, so the cross-build check covers the medium.

## Acceptance criteria

1. addFlow registers the kind's state component type, named its name followed by -ledger, and throws std::invalid_argument when any of its names is malformed or already registered. Every query gives 0, or an empty list, for a world holding no ledger for the kind.
2. createUnits, sendUnits, and consumeUnits change only the acting endpoint's stock, by the units they name under the handle they name, and respectively the kind's created count, its units in transit, and its units consumed with the cause. Each throws std::logic_error when the world is not stepping or holds no ledger for the kind, and std::invalid_argument for a null or dead endpoint and for units below 1. createUnits also throws std::invalid_argument when the created count would exceed the largest int64_t; sendUnits, for a null destination, a delay of 0, or a stock holding fewer units of the handle; and consumeUnits, for a malformed cause or a stock holding fewer units of the handle. An operation that throws changes nothing.
3. For each kind, units created equal units in transit plus units held plus units consumed, after every tick of randomized synthetic runs that create, send, and consume units and create and remove endpoints.
4. A packet sent while stepping tick t with delay d to a destination that is live at its arrival enters the destination's stock, under its handle, at the swap that reaches tick t + d, and at no earlier swap, whatever commands and resolutions that leave the destination live change in between.
5. A packet whose destination is not live at its arrival goes back to its sender, with the same handle and delay, so a sender live throughout holds the units again from tick t + 2d. When the sender is not live either, at that arrival or at the return's, the units are consumed with the cause undeliverable. At the first swap after an endpoint's entity is removed, each handle's units in its stock go to the stock of the handle's entity, under the same handle, when that entity is live, and are consumed with the cause discarded otherwise. Until that swap, the removed endpoint's stock is still held.
6. Ledgers are state. A save holds them, and loading the save of a world with packets in transit and stocks held and resolving gives a world equal to the one saved. Changing any packet, stock, created count, or consumed count changes the world's hash, and a copy stepped forward equals the original stepped forward.
7. Systems whose ledger operations act for different endpoints give the same world and the same hash after every tick whatever order they are registered in.
8. tpj_scenarios registers a scenario named stalls. In its first 3000 cycles, both of its fields hold at least one resolved entry and at least one readable stepped entry at some tick, both of its flow kinds hold units in transit and have consumed units at some tick, and each kind's created units equal its units in transit plus held plus consumed after every cycle.

## Medium

This feature adds the flow ledger and no park flow. Systems act for their own endpoints through it, and anyone holding a world reads its counts and stocks:

- plausible-operations will create supplies at the depot and send them to shops, with a delay from route distance, turn supplies into meals by consuming one and creating the other, send meals to the guests they are addressed to, and return guest visits.
- believable-guests will create visits addressed to itself, send them to shops, and consume the meals it receives.
- legible-simulation reads stocks and consumption causes for inspection.

The stalls scenario is synthetic and owns its fields and kinds, which it exposes in a header only so tests can audit its world.

## Principle checks

- Principle 1: criterion 6. Ledgers are state and saved, and a load resolves back to the world saved.
- Principle 2: criterion 5. A packet to a removed endpoint, and a removed endpoint's stock, always end up held, in transit, or consumed with a named cause, and a world with no ledger answers every query with 0.
- Principle 3: criterion 3, the conservation identity at every tick. Creating and consuming are the only ways units enter or leave a kind.
- Principle 6: criterion 2. An operation changes only the acting endpoint's own stock, and the ledger component is a public medium component (decision 0016), so a test may change it directly to check the hash.
- Principle 8: criteria 2 and 5. Every consumed unit is counted under the cause that consumed it.
- Principle 10: criteria 4 and 7. Arrival depends only on the send tick and the delay, and producers acting for different endpoints commute.

The ledger never reads a network, so criterion 4's "whatever changes" is tested by commands and resolutions between sending and arrival that leave the destination live. Principle 4 is not at risk here: a sender takes its delay from route distance, which navigable-networks will publish.

## Spec changes

src/sim/SPEC.md:

In the paragraph beginning "Component types are registered with a WorldSchema", after "It registers the medium's types first (sim/medium/SPEC.md), and so far nothing else.", add: "The medium's spec classes its data: networks and resolved field entries are derived, and stepped field entries and flow ledgers are state."

src/sim/medium/SPEC.md:

Replace the first paragraph's "the networks, and the fields sampled on them." with "the networks, the fields sampled on them, and the flow ledger."

At the end, add a section:

"## Flows

A flow kind is a quantity of integer units that is conserved as it moves (principle 3). Its owning module defines it as a type with one public static member, Name, a std::string_view of lowercase letters, digits, and hyphens. addFlow registers a kind with a schema: the state component type named its name followed by -ledger, which holds the kind's ledger; the resolver named its name followed by -flow, which creates the entity keyed flowKey(name), deriveKey(NULL_KEY, hashName("flow"), hashName(name)), with an empty ledger if it has none; and a swap function. It throws std::invalid_argument when any of its names is malformed or already registered. Ledgers are state, and saves hold them (principle 1).

Every unit of a kind is held in an endpoint's stock, in transit in a packet, or consumed. An endpoint is any entity. A stock holds its units by handle, the key of the entity they are addressed to, or NULL_KEY for units addressed to no one. A system acting for an endpoint changes its stock while stepping, through three operations:

- createUnits adds units to the endpoint's stock under a handle, and to the kind's created count.
- sendUnits takes units of a handle from the endpoint's stock and puts them in transit as a packet to a destination key, with the same handle, a delay in ticks, and an arrival tick, the current tick plus the delay. While the systems step, World::Tick is the tick being stepped.
- consumeUnits takes units of a handle from the endpoint's stock and adds them to the units consumed with a cause, a name of one or more lowercase letters, digits, and hyphens.

Each throws std::logic_error when the world is not stepping, or holds no ledger for the kind, as when the kind is not registered with its schema. Each throws std::invalid_argument when the endpoint is NULL_KEY or not a live entity, or the units are below 1. createUnits also throws it when the created count would exceed the largest int64_t; sendUnits, when the destination is NULL_KEY, the delay is 0, or the stock holds fewer units of the handle; and consumeUnits, when the cause is malformed or the stock holds fewer units of the handle. The std::logic_error checks come first, so an operation on a kind with no ledger throws it whatever its other arguments. An operation that throws changes nothing. The destination need not be live when a packet is sent. No operation changes another endpoint's stock, so operations acting for different endpoints give the same ledger in any order. Packets are held in the order of their arrival tick, sender, destination, handle, units, delay, and whether they are returning, and stocks in the order of their endpoint and then handle, with no empty entries.

The kind's swap runs among the other swaps in the order addFlow registered it, after World::Tick has advanced to the next tick, T. It first takes each packet whose arrival tick is T or earlier, in order. A packet whose destination is live goes into the destination's stock under its handle, so a packet sent while stepping tick t with delay d to a destination still live at its arrival enters that stock at the swap that reaches tick t + d, and at no earlier swap. A packet whose destination is not live goes back to its sender: a returning packet from the destination to the sender, with the same handle, units, and delay, arriving at T plus the delay. A packet whose sender is not live either, and a returning packet whose destination, its original sender, is not live, are consumed with the cause undeliverable. The swap then settles the stock of each endpoint that is no longer live: each handle's units go to the stock of the handle's entity, under the same handle, when that entity is live, and are consumed with the cause discarded otherwise. Until that swap, a removed endpoint's stock is still held. The ledger never reads a network, so nothing done to the networks changes when a packet arrives. For every kind, units created equal units in transit plus units held plus units consumed, outside the operations and the swap.

The queries take the world by const reference and give 0, or an empty list, for a world holding no ledger for the kind. unitsHeld gives the units an endpoint holds under a handle, or the units held in every stock. stockOf gives an endpoint's holdings, each a handle and its units, handles ascending. unitsInTransit, unitsCreated, and unitsConsumed give the kind's totals, and unitsConsumed with a cause gives the units consumed with it, 0 for a cause never used. The ledger holds each cause as the hashName of its name."

src/scenarios/SPEC.md:

In the paragraph beginning "A Scenario has a name", after "beacons queues commands whose resolution creates derived entities.", add: "stalls derives a network from intent, publishes both layers of an entry field and a scalar field, and moves units of two flow kinds between endpoints that its commands and systems create and remove, so the medium's arithmetic and ordering are compared across builds. Its fields and kinds are declared in scenarios/stalls.h so tests can audit its world."

## Files affected

- Modify: src/sim/SPEC.md
- Modify: src/sim/medium/SPEC.md
- Modify: src/scenarios/SPEC.md
- Create: src/sim/medium/flow.h
- Create: src/sim/medium/flow.cpp
- Modify: src/sim/CMakeLists.txt
- Create: src/scenarios/stalls.h
- Create: src/scenarios/stalls.cpp
- Modify: src/scenarios/synthetic.h
- Modify: src/scenarios/scenarios.cpp
- Modify: src/scenarios/CMakeLists.txt
- Test pass: files under tests/, written by the test-writer agent: tests/sim/medium/flow_test.cpp, a tests/sim/support/synthetic_flows.h for synthetic kinds and endpoint systems, a stalls test under tests/scenarios, and the additions to tests/sim/CMakeLists.txt and tests/scenarios/CMakeLists.txt.

## Dependencies

- stepped-fields: World::isStepping, swaps, and the stepped layer the stalls scenario publishes. Merged.
- resolved-fields and networks-and-places: the fields and network the scenario uses. Merged.
- deterministic-simulation's world-as-value: the cycle, saves, the walk, and the scenario runner. Merged.

## Out of scope

- Packet split and merge: a milestone deepening candidate.
- Delays from route distance: navigable-networks publishes route distance, and plausible-operations takes delays from it.
- Cause names in saves: causes are saved as their hashes, since the walk has no string type.
- Edge capacity and agent-backed flows: capability deepening candidates.
- The private-header check: the milestone's next feature.

## Open questions

None.
