# Research: food-offer

## How is the wait computed?

The wait is for the next visit to arrive, behind every guest queued, counted from the cycle in which the shop first holds it to the cycle in which the shop takes its guest. Under meal-service's rule the shop takes the guest at its queue's front when it is free and holds a supply unit, so the take ticks chain: the first guest is taken at the latest of the first cycle, the tick the shop is next free, and its unit's tick, and each later guest at the later of the previous take plus SERVICE_INTERVAL and its own unit's tick. The units come, in order, from the shop's stock, from the shipments on their way to it in order of arrival, and beyond those from an order placed in the offer's cycle, which arrives after ORDER_DELAY and the shipment's delay. The shipment's delay uses the route length the depot ships by, sampled at the depot's anchor with the shop as source, so the wait of a newly placed shop is exactly when its first guest is taken. So the wait is exact against the service rule whenever the units ahead come from stock and shipments, and it is the capability's formula read with each guest's turn following the one before it.

The meal follows SERVICE_INTERVAL after the take, as meal-service settled. A guest adds its own walk to the wait.

Rejected: the literal later of the next arrival's turn at the full service rate and its own unit's tick. When supplies are late for the guests ahead, it underestimates the wait: with an empty shelf, three units arriving at tick 100, and two guests queued, it says 180 while the shop takes the guest at 280. Evan chose the chained take. Rejected: counting orders on their way to or held by a depot as units, each arriving after its shipment's delay. It is more exact for a shop that has just ordered, but it reads depots' reach and distances for orders in transit, and the capability lists only stock, shipments, and the order placed now. It is a deepening candidate. Rejected: the wait including SERVICE_INTERVAL. meal-service's research settled that the wait ends when the shop takes the guest.

Sources: plans/plausible-operations/CAPABILITY.md, foundation criteria; plans/plausible-operations/supplied-food-shop/meal-service/RESEARCH.md.

## How does the resolved offer stay derived from intent alone?

The resolved offer is the offer of a shop that has not stepped: nothing queued, held, or shipped, and its first guest waits for the order it places in its first cycle. It needs only whether the shop has a nearest depot and the route length, both from backstage route distance. sampleField chooses a source's stepped entries when it has them, and stepped entries are state, which resolvers never read (src/sim/SPEC.md). Route distance never publishes stepped entries, but a hand-written save can hold them. So the medium gains sampleResolvedField, which samples a field's resolved entries alone, and the offer's resolver samples route distance with it.

Rejected: sampleField in the resolver. It would read state whenever a save holds stepped route distance entries, and a world loaded from such a save and resolved later would then differ from the one saved. Rejected: making route distance a field with no stepped layer, the shared-medium deepening candidate. It solves this too, but it changes addField and the saves of every park, which is more than this feature needs.

Sources: src/sim/SPEC.md, on resolvers; src/sim/medium/SPEC.md, on the layer rule.

## What does a starved shop offer?

OfferEntry{}: no relief, no wait, and not supplied. A guest that reads relief alone is never drawn to a shop that says no meals.

Rejected: MEAL_RELIEF with Supplied false. The relief would advertise a meal the offer says is not there.

Sources: none; follows from the capability's "says no meals and has no wait".
