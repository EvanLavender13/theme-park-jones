# Research: Seats and Stalls

## Where does the bench and toilet block code live?

Operations owns shops and depots in operations.h and operations.cpp. Benches and toilet blocks are a different concern: they hold no stock, order nothing, and keep guests for a fixed time. They share only two things with shops: the OfferEntry an offer holds, and publishing an entry at each node anchored to the box on the guest network. Decision 0027 gives a new concern its own component, so benches and toilet blocks get src/sim/operations/facilities.h and facilities.cpp. The shared entry placement moves to a private header in operations, so both components publish offers the same way. The routes module already calls benches and toilet blocks facilities, so the name is in use.

The facility code runs once per kind over a list of facility kinds, as shop code runs once per good. Each kind names its visit, serving, and offer kinds, its relief, its places, and its time. A later facility kind adds an entry and its own kinds.

addOperations calls addFacilities at its end, so addPark and src/sim/SPEC.md's registration order don't change.

Rejected: growing operations.cpp — it would hold two concerns, and the shop's supply rules would sit beside code that never uses them. A shop with several places and no supply — the shop's queue, cover, and wait code all assume supplies, and each would need a branch for facilities.

Sources: src/sim/operations/operations.cpp — offerEntries, forEachGood, GoodKinds; src/sim/routes/networks.cpp — doorsServing calls benches and toilet blocks facilities; docs/decisions/0027-code-architecture.md.

## What does a facility keep, and how is its wait exact?

A facility keeps its queue of guest keys, in arrival order, and the tick each taken place frees. When it seats a guest, it sends the guest's visit and its serving back at once, with a delay of the facility's time. That is how a shop sends a meal after SERVICE_INTERVAL. The place's free tick is the seating tick plus that time. So the facility never holds a seated guest's visit, and deleting it returns only the visits at its door, which the ledger passes back to their guests.

The next arrival's wait follows from that state. Each place is free at its free tick, or at once if it isn't taken. Each guest at the door takes the earliest free place in turn and keeps it for the time. The arrival takes the earliest place left. With a few places this is a short walk with no search. If no other visit arrives, the arrival is seated in the cycle the wait gives, so the wait is exact.

Rejected: keeping each seated guest's visit until its time is up — the state would need each place's guest, and the step would release them, for the same timing the delayed packet gives. Turning guests away when every place is taken — a gate (principle 5); they wait at the door.

Sources: src/sim/operations/operations.cpp — serveFront sends the meal and visit with SERVICE_INTERVAL; src/sim/medium/SPEC.md — a gone endpoint's stock passes to each handle's entity.

## How do the park world checks cover the new state?

Every resolved world holds the four new ledgers and the two new stepped offers, empty or not. stocked.park, remade, holds rest and toilet offers with entries, because facilities.park's bench and toilet block are anchored on its guest network. The facility's own state, its queue and free ticks, exists only once a guest has visited. No checked park holds a guest that visits a bench or toilet block until a-day-out's parks do, so the world checks name that component as unheld, with that reason, as the milestone says. a-day-out removes the name.

Rejected: emplacing the state at every facility from its first step — an empty component on every bench and toilet block in every save, to satisfy a check, when the check exists to show real state is covered. Sending synthetic visits while making stocked.park — real guests would hold rests they never use.

Sources: tests/sim/park_world_checks_test.cpp — UNHELD_TYPES; tests/parks/stocked.park; plans/plausible-operations/served-needs/MILESTONE.md — Medium.
