# Capability: Shared Medium

## Summary

shared-medium implements principles 3 and 6. Things in the park interact only through fields and flows, and nothing reads another thing's internals.

This capability owns the medium itself. It supplies three things:

- the network type that fields and flows live on;
- the interface for publishing and sampling fields, with per-source attribution;
- the ledger that moves conserved flow units between endpoints with a delay.

It owns the mechanism, not any particular field or flow. Food offer, route distance, and supplies belong to the capabilities that produce them.

With the medium, every capability can meet the others without wiring. Every interaction then:

- conserves what it should (principle 3);
- explains where it came from (principle 8);
- doesn't depend on the order systems run in (principle 10).

The capability grows with every new kind of field, flow, network, and storage, and it never finishes.

## Foundation criteria

When you sample any field at any network place, you get its per-source entries in a fixed source order. A scalar field's value equals the ordered sum of those entries, bit for bit.

If a place has no sources, sampling it is still legitimate. The sample has no entries and a scalar value of zero.

If a value is published while stepping tick N, it becomes visible at tick N+1, never earlier. The medium's work for one tick gives the same world and the same state hash, whatever order the producers run in.

Each cycle runs in a fixed order:

1. Step the tick.
2. Swap the buffers, so the tick's publications become readable.
3. Apply queued intent commands.
4. Resolve, if intent changed.

Resolution re-derives the world from intent and publishes straight into what samplers read. Nothing stepped is still pending when it writes.

A source's stepped entries are readable for the tick after they are published. They take precedence over the source's resolved entries. If a resolution changes a source's resolved entries, it clears the source's stepped ones, so the next tick samples every change resolution made. If the player removes a path between ticks N and N+1, tick N+1 samples no route distance entries on the removed edge and no offer that depended on it.

Producers run in the order their inputs require: networks first, then what depends on them.

A resolved world, committed or candidate, can be sampled at once without stepping. Resolving a candidate copy gives the same fields as committing the same intent and resolving.

Every flow unit of every kind is in exactly one state:

- in transit;
- held at an endpoint;
- consumed.

This includes units whose endpoint has been removed. The ledger accounts for them by a rule settled in the first milestone. For each kind, units created equal units in transit plus units held plus units consumed. This holds exactly, in every tick.

If a packet is sent at tick t with delay d, it is delivered to its destination endpoint's stock at the start of tick t+d. The delay d is at least one tick, to match next-tick visibility. Changes to the network in between don't affect delivery.

The medium's state is part of the world as deterministic-simulation defines it. A copy of the world stepped forward equals the original stepped forward. The state hash covers every field entry, packet, and stock.

The park files in tests/parks/ hold the medium's state, so the park world checks cover its copies, saves, and hashes. The cross-build check covers the medium through tpj_scenarios's synthetic scenario for fields and flows. Check the other properties by playing the game, as docs/testing.md says.

A mechanical check fails the build when a file includes another module's private headers (principle 6, decision 0039). It passes on the tree, and it fails on a planted violation.

## Medium

This capability adds the medium's machinery, not any park field or flow of its own. The machinery has three parts: networks, fields, and flows.

A network is nodes, and edges with lengths. navigable-networks publishes the networks, deriving them from path intent. Every edge is a stretch of a carrier. A carrier is a line with a stable key, such as a drawn path or a box's connector. In synthetic networks, each edge is its own carrier.

A network place is a carrier and a distance along it. Every field on a network is sampled at places. Carriers come from intent, so if a holder keeps a place across a re-derivation, the place survives edges being split or merged under it.

The network type offers one carry-over operation. A holder applies it to its own places, using the networks from before and after a re-derivation, so nobody reads another's internals. It has four outcomes:

- If the place's carrier is unchanged, the place keeps its spot.
- If the carrier changed or is gone, and a carrier of the new network passes over the place's old ground position, the place stays at that ground position. It stays on its own carrier if that carrier passes there.
- If no carrier passes there and the carrier's geometry changed, the place moves to the point on the same carrier nearest its old ground position.
- If no carrier passes there and the carrier no longer exists, the place resolves to no place. This is a legitimate state, and the holder handles it.

A node may be anchored to an entity. A shop or depot can then find the places where it meets a network without reading how the network was derived. A nearest-point query finds the place for a ground position. Routing and route distance belong to navigable-networks.

In a field, a producer publishes per-source entries at places. A consumer samples a place and gets the entries in a fixed source order. There are two kinds of field:

- An entry field, such as food offer or route distance, carries typed entries and has no total.
- A scalar field, such as food availability or hungry footfall, also has a value: the ordered sum of its entries.

Publications made while stepping are double-buffered and become visible on the next tick. Resolution runs after the swap that ends a tick, and its publications are visible at once, so a preview needs no stepping (decision 0025, principle 8).

Resolved entries are derived, and stepped entries are state. If a source has stepped entries, a sample uses them instead of its resolved ones (first-field-and-flow's layer rule).

A ground field holds one entry for each entity at a ground position, instead of entries at places. Systems set and drop entries while stepping, and an entry stays until it changes. A consumer samples it within a radius of a ground point and gets the entries there in key order. When an entity is gone, its entry goes too. believable-guests' guest presence is the first ground field.

A flow is integer units of a named kind. Units are created at an endpoint, moved as packets between endpoints, held in endpoint stocks, and consumed. Creating and consuming are the only ways a unit enters or leaves a kind. A conversion, such as supplies into meals, is recorded as both.

The sender gives each packet's delay, sampled from route distance. The ledger keeps the packet in transit until its arrival tick, independent of the network.

A packet may carry an opaque endpoint handle for its units. With it, a shop can address a meal to the guest whose visit it served without knowing anything else about the guest.

Every other capability is on the other side of the medium:

- navigable-networks publishes networks and route distance;
- plausible-operations publishes food offer and moves supplies and meals;
- believable-guests publishes footfall and sends guest visits;
- legible-simulation samples everything for overlays and previews.

The slice's medium map in plans/slices/boxes-and-tubes/SLICE.md names each of these.

## Principles

- Principle 3 is at risk when a new interaction bypasses the medium. Conservation is a property of the ledger: for each kind, units created equal units in transit, held, and consumed. Every interaction between capabilities must appear in a slice or capability medium map, and reviews check that.
- Principle 6 is at risk when a module's private components are included elsewhere. The include check enforces it, and decision 0039 keeps private components out of public headers.
- Principle 10 is at risk from summation order and update order. Ordered entries and next-tick visibility address both. The park world checks show that two runs give the same state, and the cross-build check compares the builds players run.
- Principle 8 depends on attribution being exact. The ordered-sum identity holds bit for bit.
- Principle 2 is at risk when a source, endpoint, or network element is removed while the medium still refers to it. Empty samples are legitimate. first-field-and-flow settled what happens to units held by, or in transit to, a removed endpoint:
  - packets go back to their sender;
  - units with neither endpoint left are consumed as undeliverable;
  - a removed endpoint's stock goes to the entities its units are addressed to, or is consumed as discarded.
- Principle 4 is kept by measuring every distance between places along the network, by navigable-networks. The medium's straight-line measures are the nearest-point queries, carry-over's check that a snapped place lies on a place's old ground, the direction query, and a ground field's sampling radius. The first two are used only to snap a ground position onto the network, the direction query only to scale a direction to unit length, and the radius only to find the entities near a point. None of them measures how far anything travels.

## Dependencies

- deterministic-simulation: the world as a copyable, hashable value, and the tick that the medium's buffers swap on: met (world-as-value, the first member of the boxes-and-tubes slice).
- tpj_sim (decisions 0022 and 0039): met.
- navigable-networks: publishes the park's real networks. The foundation doesn't need it, because the foundation runs on synthetic networks.

## Foundation

The foundation is three pieces, built on synthetic networks with no park content:

- a network type;
- the field interface, with entry and scalar fields;
- the flow ledger.

It is the smallest version that lets the other slice members exchange anything at all.

It is useful on its own. The principles promise exact attribution, exact conservation, and order independence. The foundation establishes these once, here, instead of separately in every capability that uses them.

## Milestones

1. `first-field-and-flow`: the first version of the medium, tested on synthetic networks. It includes:
   - the network type, with carriers, places that survive re-derivation, entity anchors, and a nearest-point query;
   - entry and scalar fields with per-source attribution, visible on the next tick while stepping and at once during resolution;
   - the flow ledger, with integer packets, delays, endpoint stocks, and accounting for creation and consumption;
   - the private-header check.

   It is a member of the boxes-and-tubes slice. Depends on: deterministic-simulation's first milestone.

2. `affordable-sampling`: sampling a field compares each entry's place with the network's stop places and the sampled edge's bounds, instead of resolving the place. A park with guests then steps quickly. Depends on: first-field-and-flow.

3. `carrier-widths`: every carrier gets a walkable width, with queries for the width and direction at a place, and carryOver keeps a place at its ground position when its ground survives an edit under a different carrier key. It is a member of the good-paths slice. Depends on: first-field-and-flow.

4. `ground-fields`: a second kind of field, with one entry for each entity at a ground position, sampled within a radius of a ground point, so guests can sense the guests near them on the paving. It is a member of the good-paths slice. Depends on: first-field-and-flow.

Later milestones come from the deepening candidates, once the slice has shown where the medium is under strain.

## Deepening candidates

- On-demand fields: storage that evaluates emitters when a field is queried, instead of holding published entries. Gated on: a field whose published form would be too large, or too costly to keep current.
- Change-driven resolution: consumers learn when the context they sample has changed in a way that matters, so most of the world can rest most of the time (design notes, "Resolution over time"). Gated on: profiling that shows the per-tick cost of sampling matters.
- Edge capacity: flows limited by the capacity of the network they cross, as decision 0018 expects for supplies. Gated on: a flow whose congestion the player should manage.
- Agent-backed flows: a flow whose units are carried by visible agents, without changing the ledger interface its consumers see (decision 0018).
- Generic field enumeration: the medium lists its fields and their kinds, so tooling can offer an overlay for any field without code written for each field (design notes, "Legibility").

## Open questions

- How long does a sampled world, with its double buffers, take to copy and hash at slice scale? The answer decides whether candidate copies stay cheap enough to rebuild on every preview update. Measure it once deterministic-simulation's copy and hash exist.

## Research notes

- Influence maps are the prior art for fields. Influence maps built on a network match principle 4. Per-source attribution isn't standard, so the medium has to keep it on purpose.
- OpenTTD's cargo packets hold integer amounts and carry their own transit time. That design fits the flow ledger.
- Double-buffering makes a tick's result independent of the order systems run in.
- With GCC, a plain include scan is the practical way to check principle 6.

The details are in RESEARCH.md.
