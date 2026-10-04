# medium

The shared medium: the networks things are placed on, the fields sampled on them, and the flow ledger that moves conserved units between entities. It is how one module affects another without reading its internals (principles 3 and 6, decision 0005). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

Modules include sim/medium/network.h for networks, places, and carry-over, sim/medium/field.h for fields, sim/medium/kept_field.h for kept fields, and sim/medium/flow.h for flows. The medium owns no network, field, or flow kind of its own. Each owning module defines its fields and flow kinds as types and registers them with addField, addKeptField, or addFlow, which register their components and resolvers with the schema. addNetworkComponent registers Network as the derived component type network, for producers such as routes to put on entities they derive. The medium reads nothing from other modules.

## Networks

A Network is a value built from carriers, a node count, and anchors. A carrier is a line with a stable key, such as a drawn path or a box's connector, given as a polyline of ground points with the producer's own arc lengths. Nodes stop along carriers, and the stretches between consecutive stops are the network's edges, so every distance on a network is a distance along a line the producer drew (principle 4). A node may be anchored to one entity, and an entity may anchor several nodes, so an entity finds where it meets the network without knowing how the network was derived.

The constructor refuses a malformed network with std::invalid_argument. A network that exists is therefore well formed: carrier keys are unique and not NULL_KEY, each carrier's distances start at 0 and strictly increase, its stops start at 0 and end at its length, so its edges tile it exactly, every node is a stop of some carrier, and a node has at most one anchor. The order carriers and anchors are given in makes no difference. The default network is empty.

edges() lists the edges, each with its carrier, its two end nodes, and their distances along the carrier. nodePlace, nodeAnchor, anchoredNodes, firstAnchoredNode, stopPlaces, and edgeEnds answer the questions a holder asks about a node or an entity: where a node is, which entity anchors it, which nodes an entity anchors, and which places and edge ends meet a node. A node not below the node count is refused with std::out_of_range. firstAnchoredNode allocates nothing, for callers on the tick path.

## Places

A Place is a carrier key and a distance along it. It names the same ground position however the carrier is cut into edges, so a place survives nodes being added to or removed from its carrier. resolve gives a place's position in a network: the node whose stop is exactly at its distance, or the edge that encloses it with its offsets from both ends. A place on a carrier the network lacks, or off the carrier's ends, resolves to nothing. That is a legitimate result, not an error (principle 2). The places that resolve to a node are exactly its stopPlaces.

groundPoint gives a place's ground position, interpolated along the carrier's polyline. nearestPlace snaps a ground position to the nearest place on any carrier, and nearestPlaceOn to the nearest place on one carrier. A junction is equally near on every carrier that stops there, and ties are broken deterministically. These are the medium's only straight-line measures, and they are used only to snap a ground position onto a network (principle 4).

## Carry-over

carryOver moves a place a holder keeps across a re-derivation, given the network before and after. Each holder applies it to its own places. A place whose carrier keeps its geometry stays exactly where it was, so adding or removing nodes along a carrier moves nothing. A place whose carrier changed shape moves to the nearest place on the same carrier in the new network. A place that did not resolve before, or whose carrier is gone, is retired, a legitimate state its holder handles (principle 2). Place has a visitFields, so holders can keep places in their own components.

## Fields

A field is a set of entries published at places, each attributed to the source that published it (principles 3 and 8). Its owning module defines it as a type naming its entry type, its name, and its kind, entry or scalar, where a scalar field's entries are doubles. The owner may give the field its own rule for places inside an edge, sampleEdge, which is how route distance reads a place between nodes (principle 4). addField registers a field and refuses a malformed or repeated name with std::invalid_argument. A module registers a field before the resolvers that publish into it.

A field has two layers. Resolved entries are derived data (principle 1): resolvers publish them with publishResolved, they are readable at once, and every resolution replaces every source's entries in full. Stepped entries are state that saves hold: systems publish them with publishStepped while stepping, and they become readable at the swap that ends the tick, replacing that field's previous stepped entries. Each source publishes into a field at most once per resolution or per tick. Publishing outside the right stage, from the null source, or twice is refused, and a refused publish changes nothing.

A source with readable stepped entries is sampled by them, and otherwise by its resolved entries. When a command changes a source's resolved entries, its stepped entries are cleared, so the tick after a command sees every change the command made. Loading the save of a resolved world and resolving gives the world saved.

sampleField gives a field's entries at a place on a network, each with its source, in ascending source order. At a node it gives each source's entries at that node, whatever carrier they name. Strictly inside an edge it gives the entries at exactly that place, or what the field's sampleEdge makes of the entries at and between the edge's ends. A place that does not resolve gives nothing, a legitimate sample (principle 2). fieldValue sums a scalar field's entries at a place, with 0.0 where it has none. sampleResolvedField samples the resolved layer alone, for resolvers, since stepped entries are state.

A sample costs a few binary searches per source, not a pass over the source's entries. sourceSlot and sourceEntryAtNode read one source's entries only and allocate nothing beyond an index built on first use, so a mover following one source pays for that source alone.

## Kept fields

A kept field holds values that accumulate where movers go and persist between ticks without any source republishing them (decision 0031). Its owner defines a scalar field with a rule for reading a kept value as it ages, readKept, and may give it a rule at nodes, sampleNode, as well as sampleEdge. addKeptField registers it, and a field cannot be registered both ways. Kept entries are state, which saves, hashes, and worldsEqual see (principle 1).

A system calls keepEntry while stepping to set a source's value at a place. A source holds at most one entry per place. Changes become readable at the swap that ends the tick, the last change at a place in a tick wins, and an entry then stays readable unchanged until its place is changed again. A change at a place the source already holds costs a binary search. sampleField and fieldValue read kept values through the owner's rules, keptValue reads one source's value at exactly one place, and keptEntries gives a source's entries as held, so its owner can carry them. The owner carries its entries across a resolution in its own finisher with replaceKeptEntries, which refuses entries it cannot hold, such as two at one place, and changes nothing when it refuses.

## Flows

A flow kind is a quantity of integer units that is conserved as it moves (principle 3). Its owning module defines it as a type naming it, and addFlow registers it and refuses a malformed or repeated name. Ledgers are state, and saves hold them (principle 1).

Every unit of a kind is in an endpoint's stock, in transit in a packet, or consumed, and units created always equal units held plus in transit plus consumed. An endpoint is any entity. A stock holds its units by handle, the key of the entity they are addressed to, or NULL_KEY for units addressed to no one. A system acting for an endpoint changes only that endpoint's stock, while stepping, through three operations: createUnits adds units, sendUnits puts units in transit to a destination with a delay of at least one tick, and consumeUnits uses units up with a named cause. Operations for different endpoints give the same ledger in any order. An operation outside stepping, on an unregistered kind, or asking for more units than the stock holds is refused, and a refused operation changes nothing.

At each tick's swap, a packet that is due arrives in its destination's stock, so units sent with delay d arrive exactly d ticks later and no sooner. A packet whose destination is gone returns to its sender after the same delay, and units with nowhere to go are consumed with the cause undeliverable. The stock of an endpoint that is gone passes to each handle's entity, or is consumed with the cause discarded. The ledger never reads a network, so nothing done to the networks changes when a packet arrives.

The queries take the world by const reference and give 0, or an empty list, for a kind with no ledger. unitsHeld, stockOf, and addressedTo give an endpoint's holdings and the units in stocks and packets addressed to an entity. unitsInTransit, unitsCreated, and unitsConsumed give the kind's totals, and unitsConsumed with a cause gives the units consumed with it.
