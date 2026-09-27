# Capability: Shared Medium

## Summary

shared-medium implements principles 3 and 6. Things in the park interact only through fields and flows, and nothing reads another thing's internals.

This capability owns the medium itself. It supplies the network type that fields and flows live on, the interface for publishing and sampling fields with per-source attribution, and the ledger that moves conserved flow units between endpoints with a delay. It owns the mechanism, not any particular field or flow: food offer, route distance, and supplies belong to the capabilities that produce them.

Its value is that every other capability can meet the others without wiring. Every interaction then conserves what it should (principle 3), explains where it came from (principle 8), and does not depend on the order systems run in (principle 10). It deepens with every new kind of field, flow, network, and storage, and never finishes.

## Foundation criteria

- On random synthetic networks, sampling any field at any network place returns its per-source entries in a fixed source order. A scalar field's value equals the ordered sum of those entries, bit for bit.
- A place with no sources is a legitimate sample: it has no entries and a scalar value of zero.
- A value published while stepping tick N becomes visible at tick N+1, never earlier. The medium's work for one tick gives the same world, and the same state hash, whatever order the producers run in.
- Each cycle runs in a fixed order: step the tick, swap the buffers so the tick's publications become readable, then resolve if intent changed. Resolution re-derives the world from intent and publishes directly into what samplers read, so nothing stepped is still pending when it writes and its output is exactly what the next tick samples. After the player removes a path between ticks N and N+1, tick N+1 samples no route distance entries on the removed edge and no offer that depended on it. Producers run in the order their inputs require: networks, then what depends on them. A resolved world, committed or candidate, can be sampled at once without stepping, and resolving a candidate copy gives the same fields as committing the same intent and resolving.
- Every flow unit of every kind is in exactly one state: in transit, held at an endpoint, or consumed. This includes units whose endpoint has been removed, which the ledger accounts for by a rule settled in the first milestone. For each kind, units created equal units in transit plus units held plus units consumed. This holds exactly, in every tick of randomized synthetic runs.
- A packet sent at tick t with delay d, where d is at least one tick to match next-tick visibility, is delivered to its destination endpoint's stock at the start of tick t+d. Changes to the network in between do not affect this.
- The medium's state is part of the world as deterministic-simulation defines it. A copy of the world stepped forward equals the original stepped forward, and the state hash covers every field entry, packet, and stock.
- A mechanical check fails the build when a file includes another module's private headers (principle 6, decision 0016). It passes on the tree, and fails on a planted violation.

## Medium

This capability introduces the medium's machinery rather than any park field or flow of its own:

- Networks: nodes, and edges with lengths, published by navigable-networks from path intent. Network places (an edge and a distance along it) are what every field is sampled at. A nearest-point query lets a ground position find its place. Routing and route distance belong to navigable-networks.
- Fields: a producer publishes per-source entries at places. A consumer samples a place and receives the entries in a fixed source order. An entry field, such as food offer or route distance, carries typed entries and has no total. A scalar field, such as food availability or hungry footfall, also has a value: the ordered sum of its entries. Publications made while stepping are double-buffered and visible from the next tick. Resolution runs after the swap that ends a tick, and its publications are visible at once, so a preview needs no stepping (decision 0025, principle 8).
- Flows: integer units of a named kind, created at an endpoint, moved as packets between endpoints, held in endpoint stocks, and consumed. Creating and consuming are the only ways a unit enters or leaves a kind, so a conversion such as supplies into meals is recorded as both. A sender supplies each packet's delay, sampled from route distance. The ledger keeps the packet in transit until its arrival tick, independent of the network. A packet may carry an opaque endpoint handle for its units, so a shop can address a meal to the guest whose visit it served without knowing anything else about the guest.

Every other capability is on the other side: navigable-networks publishes networks and route distance, plausible-operations publishes food offer and moves supplies and meals, believable-guests publishes footfall and sends guest visits, and legible-simulation samples everything for overlays and previews. The slice's medium map in plans/slices/boxes-and-tubes/SLICE.md names each of these.

## Principles

- Principle 3 is at risk when a new interaction bypasses the medium. Conservation is checked by randomized ledger tests at every tick. Every cross-capability interaction must appear in a slice or capability medium map, and reviews check that.
- Principle 6 is at risk when a module's private components are included elsewhere. The include check enforces it, and decision 0016 keeps private components out of public headers.
- Principle 10 is at risk from summation order and update order. Ordered entries and next-tick visibility address both. They are checked by running producers in shuffled order and comparing state hashes.
- Principle 8 depends on attribution being exact. The ordered-sum identity is tested bit for bit.
- Principle 2 is at risk when a source, endpoint, or network element is removed while the medium still refers to it. Empty samples are legitimate, and what happens to units held by, or in transit to, a removed endpoint is settled in the first milestone (see Open questions).
- Principle 4 is kept by sampling only at network places. Distances between places are measured only along the network, by navigable-networks. The one straight-line measure in the medium is the nearest-point query, and it is used only to snap a ground position onto the network.

## Dependencies

- deterministic-simulation: the world as a copyable, hashable value, and the tick the medium's buffers swap on. Unmet; to be planned first, as the first member of the boxes-and-tubes slice.
- tpj_sim with EnTT (decisions 0016 and 0022): met.
- navigable-networks: publishes the park's real networks. Not needed for the foundation, which is tested on synthetic networks.

## Foundation

A network type, the field interface with entry and scalar fields, and the flow ledger, all proved on synthetic networks with no park content. This is the smallest version that lets the other slice members exchange anything at all. It produces value on its own, because the properties the principles promise (exact attribution, exact conservation, order independence) are established and tested once, here, instead of separately in every capability that uses them.

## Milestones

1. `first-field-and-flow`: the network type with places and a nearest-point query, entry and scalar fields with per-source attribution, next-tick visibility while stepping, and immediate visibility during resolution, the flow ledger with integer packets, delays, endpoint stocks, and creation and consumption accounting, and the private-header check, tested on synthetic networks. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's first milestone.

Later milestones are drawn from the deepening candidates once the slice has shown where the medium strains.

## Deepening candidates

- Ground-domain fields: fields sampled at ground positions that spread over open ground rather than along routes, such as noise and smell. Gated on: a mechanic that needs one.
- On-demand fields: storage that evaluates emitters at query time instead of holding published entries. Gated on: a field whose published form would be too large or too costly to keep current.
- Change-driven resolution: consumers learn when the context they sample changed meaningfully, so most of the world rests most of the time (design notes, "Resolution over time"). Gated on: profiling showing per-tick sampling cost matters.
- Edge capacity: flows limited by the capacity of the network they cross, as decision 0018 anticipates for supplies. Gated on: a flow whose congestion the player should manage.
- Agent-backed flows: a flow whose units are carried by visible agents without changing the ledger interface its consumers see (decision 0018).
- Generic field enumeration: the medium lists its fields and their kinds, so tooling can offer an overlay for any field without per-field code (design notes, "Legibility").

## Open questions

- What happens to units when their endpoint is removed, both the stock it holds and packets in transit to it? They could be returned to their sender, held at a place in the network, or recorded as consumed with a cause. Conservation and principle 2 constrain the answer. Resolved while planning first-field-and-flow.
- How long a sampled world and its double buffers take to copy and hash at slice scale, and so whether candidate copies stay cheap enough to rebuild on every preview update. Resolved by measurement once deterministic-simulation's copy and hash exist.

## Research notes

- Influence maps are the prior art for fields. Network-based influence maps match principle 4. Per-source attribution is not standard and must be kept deliberately.
- OpenTTD's integer cargo packets, which carry their own transit time, fit the flow ledger.
- Double-buffering gives order-independent ticks.
- A plain include scan is the practical principle 6 check with GCC.

Depth is in RESEARCH.md.
