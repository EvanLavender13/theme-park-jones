# Research: hungry-footfall

The milestone's research settles what hungry footfall is: each stretch's exponential moving average, with time constant FOOTFALL_TIME, of the summed hunger of the guests on it each tick, with no threshold (plans/believable-guests/hungry-guests/RESEARCH.md). This file holds the questions left for the feature.

## Where does each stretch's average live between ticks?

A moving average needs its last value, so it is state, and state that must survive an edit, which re-derives the network and re-cuts its edges. The field's own stepped layer cannot hold it alone. Only systems publish into it, it becomes readable one swap later, and nothing carries its entries across a resolution, so an edit would leave the next tick reading the previous network's entries. A finisher could not rewrite them either, since the layer is the medium's and publishStepped refuses anyone but a system.

So the guests module keeps the averages in a private state component, holds each stretch's value at a place, and carries those places in a finisher of its own, as it carries guests. Each tick a system reads the held values, updates each stretch of the guest network, holds the results, and publishes the same list into the field. After the swap that ends a step, the field's stepped layer holds the held list as published, followed by the node entries described below. The cost is that a save holds the list twice, once as private state and once as the field's readable entries.

Both sit on the field's entity, which the field's resolver creates in every resolution, and the field's entries name it as their source. That needs no new resolver and draws nothing from the key counter, so no guest's key, and so no guest's draws, change. A counter entity would shift every later guest's key.

Rejected: keeping the averages only in the field's stepped layer, read back through sampleField. Carrying them would need the sampling rule to reconstruct re-cut edges from entries it cannot tell apart by carrier at a junction, and an edit would leave the average reading the old network's entries. Rejected: an entity from createEntity to hold the state. It shifts the key of every guest that arrives after it.

Sources: src/sim/medium/field.h, publishStepped, swapSteppedEntries, and settleSteppedEntries; src/sim/medium/SPEC.md, the layer rule; src/sim/guests/guests.cpp, carryGuests.

## How does a stretch's value carry across an edit?

An edit can leave a stretch as it was, split it with a new connector, join two stretches when a connector goes, move its path, or delete it. Holding each stretch's value at its midpoint, and carrying that place with carryOver as guests' places are carried, handles every case with the medium's one operation. An untouched stretch keeps its midpoint and its value. A split stretch's value goes to the half holding the old midpoint, and the other half starts at 0. Joined stretches' values add, which is what the sum of hunger on the joined stretch would have been. A moved path's midpoint follows its carrier. A deleted path's midpoints are retired, and their values go with them, as the milestone's research expects.

Which stretch a held place belongs to is decided by its carrier and distance, not by resolve, so a midpoint that a new connector lands exactly on still has one stretch: the one of its own carrier that starts there. The same rule places guests: a guest counts on the stretch of its own carrier whose FromDistance it is at or above and whose ToDistance it is below, or on the carrier's last stretch at the carrier's end. A guest at a junction therefore counts once, on its own carrier, and a guest waiting at a shop's anchor, the end of the shop's connector, counts on the connector.

Rejected: splitting a value in proportion to overlap. It keeps a split stretch's two halves nearer their true averages, but needs each held value to carry an interval instead of a place, and a rule for mapping intervals across moved paths that carryOver does not give. The whole value settles to the true average within a few FOOTFALL_TIMEs either way. Rejected: assigning a held place by resolve. A midpoint that lands on a new node would belong to every stretch meeting there, or to none.

Sources: src/sim/medium/SPEC.md, Carry-over; plans/believable-guests/hungry-guests/RESEARCH.md, How is hungry footfall averaged?

## What does sampling give inside a stretch and at a node?

The field needs a sampleEdge rule, since the medium's default rule inside an edge gives only entries at exactly the sampled place, and a stretch's one entry sits at its midpoint. The rule returns the source's entries strictly inside the edge, so fieldValue anywhere inside a stretch is its value.

A node needs a value too. legible-simulation's shop ghost wants the footfall where its connector would meet a path, and a connection is often a node: nearestPlace clamps to a path's end, and a connection near a junction lands on it. The medium has no owner rule at nodes, but its node rule gives the entries at the node, so the system also publishes one entry per node, the mean of the new values of the stretches meeting it. A path's end then reads its one stretch, and a junction reads the average of its stretches. Node entries are published only, never held or carried, since the next step derives them again from the stretches. The field's published entries are then the held list followed by the node entries.

Rejected: a node sampling 0. A ghost beside the end of a busy dead-end path would report no footfall. Rejected: a node as the sum of its stretches. A junction would read higher than any path meeting it, so a busy junction and a busy path would not compare. Rejected: an owner rule at nodes in the medium. It widens the medium when published node entries already serve.

Sources: src/sim/medium/SPEC.md, Networks and Fields; src/sim/routes/SPEC.md, Connectors; plans/legible-simulation/CAPABILITY.md, the shop ghost's context.
