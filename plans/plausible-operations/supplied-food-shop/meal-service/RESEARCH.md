# Research: meal-service

## How does service take time?

The shop takes the guest at the front of its queue when it is free and holds a supply unit, consumes the unit, and sends the visit and the meal back to the guest with the delay SERVICE_INTERVAL. It takes no other guest for SERVICE_INTERVAL ticks. So the delay is the service: every guest waits the full 3 s for a meal, the returned visit arriving is the end of service, and the shop needs no in-service state beyond the tick it is next free. The food offer's wait (food-offer) is then the time until the shop takes the next arrival, and the meal follows SERVICE_INTERVAL later.

Rejected: an instant handoff after one tick with a cooldown of SERVICE_INTERVAL. At an idle shop the first guest would get food almost at once, so service would not take time (principle 7). Rejected: a separate in-service slot that completes after SERVICE_INTERVAL. It gives the same timing as the delay, with one more piece of state to save and keep consistent.

Sources: none; Evan chose the delay model while planning.

## Where does the queue live?

The ledger keeps a stock per endpoint and handle and merges the units that arrive, so it forgets the order visits arrived in. The shop therefore keeps its own queue in a state component on its box's entity, as the milestone asks, one guest key per visit unit it holds, with the tick it is next free. The component is private to the operations module (docs/conventions.md, decision 0016). Every step reconciles the queue with the units the shop holds: a guest's earliest entries are kept up to its held units, and further units are appended, guests in ascending key order. One rule covers new arrivals, arrivals in the same tick, and guests whose visits were consumed because they left, and it keeps any world legitimate even when a hand-written save gives a queue that disagrees with the ledger (principle 2).

Rejected: ordering the queue by guest key, which needs no state. It would let a later guest with a lower key jump the queue, which is neither plausible nor what the milestone asks. Rejected: reading arrival order back from meal or visit packets. Delivered packets leave the ledger, so nothing remains to read.

Sources: src/sim/medium/SPEC.md, on stocks and packets.

## What does a starved shop cover?

A starved shop keeps serving from its stock and from the supplies packets on their way to it. The ledger never reads a network, so a shipment in transit arrives even when the route is cut after it left, and counting it is exact. Orders are not counted: a depot that cannot reach the shop sends them back. The queued visits beyond that cover are returned at once, with a delay of one tick, which the ledger's minimum requires, and later arrivals the same way.

Rejected: returning every queued visit the moment the shop is starved. Guests would be turned away while stock sits on the shelf, which is the opposite of the local, legible failure the capability wants.

Sources: src/sim/medium/SPEC.md, on the swap's delivery.

## What happens to a visit or meal for a guest who is gone?

A meal only comes back to a shop when its guest is gone, and a visit the shop holds for a guest that is gone can never be returned. The shop consumes both as abandoned. Sending them again would bounce them between the shop and a key that will never be live again, since keys are never reused. The rule covers a visit that came back, one that arrived after its guest left, and one whose guest left while it was queued.

Rejected: holding them until the shop is deleted and letting the ledger discard them. They would count as queue entries and block service.

Sources: src/sim/SPEC.md, on keys that are never reused.

## How do synthetic guests reach the scenario runner?

The food-shop scenario needs the park's schema plus a guest system of its own. The park's registrations move into addPark, which makeParkSchema calls, and the scenario calls addPark and then registers its guests. Tests do not need a guest system: a test states a world between cycles by creating guest entities and writing visit packets into the ledger, as the supply-chain tests write orders.

Rejected: copying makeParkSchema's list of registrations into the scenario, which would drift. Rejected: a guest system inside the operations module, which would be test scaffolding in the simulation.

Sources: src/scenarios/SPEC.md; tests/sim/operations/operations_test.cpp.
