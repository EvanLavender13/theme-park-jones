# Research: supplied-food-shop

## How should a starved shop show itself?

In Parkitect, a shop that runs out of stock shows an icon above it, and its details window says it is out of stock. Players find the cause by following the chain back: whether the depot is linked to the warehouse, whether haulers are zoned close enough, whether there are enough of them. The icon is what makes the failure local. Players see where the problem is before they know why.

For this milestone, the mark sits on the box, as effortless-building's capability asks, and the why is the inspection record's limiting factor, which legible-simulation's shop inspector shows later. Starved here means no route to any depot, which follows from intent through route distance alone. So the mark changes only when intent changes, and a mesh rebuilt on each change of intent always shows it correctly. A ghost's candidate world has the same marks, so deleting a backstage path shows the shop starving before the click commits it (principle 8).

Rejected: marking a shop whose stock is empty. A shop waiting for a shipment is not failing, and a mark that flickers with every delivery would say nothing about the layout. The limiting factor already reports supply as the constraint.

Sources: https://steamcommunity.com/app/453090/discussions/1/1694922526912033557/, which identifies the out-of-stock symbol above a shop; https://steamcommunity.com/app/453090/discussions/1/1326718197205805932/, on out-of-stock shops and how players trace the cause; https://steamcommunity.com/app/453090/discussions/1/350532536104448927/, on depots connected to the warehouse and by path.

## How should the tuning values relate?

A reorder point normally covers the demand expected during the lead time, plus a safety margin, and the gap between s and S sets the batch size. In this milestone, the fastest a shop can use supplies is one meal per service interval. Its lead time is the order's delay plus the shipment's route delay. So a reorder point that covers the demand during a typical lead time, at the full service rate, keeps a busy shop on a short route from running out, while a long supply route makes the shop run dry between shipments. Starving through distance is the gradient the capability wants (principle 5). A fixed s and S are tuning values here. The capability's deepening candidates let the player set them later as a standard (principle 9).

With a 90-tick (3 s) service interval, a 30-tick order delay, and supplies moving at 2 m/s, a 40 m backstage route gives a lead time of about 630 ticks, which is 7 meals at the full rate. So a reorder point of 8 and an order-up-to level of 24 cover it, with batches of about 16.

Rejected: deriving s from each shop's route length. It is a better policy, but it is a standard, and principle 9 leaves standards to the player, so it belongs with the player-set standards among the deepening candidates.

Sources: https://smartcorp.com/inventory-control/inventory-control-policies-software/, on min-max (s, S); https://abcsupplychain.com/reorder-point-formula/, on the reorder point as demand over the lead time plus safety stock.

## How does a guest tell a served visit from an unserved one?

Both kinds of return travel back through the guest-visit flow, and the ledger's packets carry no tag. A meal is its own flow, though. A shop sends the visit and the meal to the guest in the same tick, with the same delay, so both arrive at the same swap. A returned visit that arrives with a meal was served, and one without a meal was not. The ledger's own return of a deleted shop's queued visits sends no meal, so those count as unserved, just as the capability requires. Nothing new is needed in the medium. The rule is part of the guest-visit flow's contract, which hungry-guests reads.

Rejected: a second flow for unserved returns. A visit would then change kind on its way back, and conserving it would have to span two ledgers. Handles that encode the outcome are rejected too, because the handle has to name the guest for the ledger to return a deleted shop's queue.

Sources: none; this follows from src/sim/medium/SPEC.md's flow rules.
