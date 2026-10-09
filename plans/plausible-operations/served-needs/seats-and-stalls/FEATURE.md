# Feature: Seats and Stalls

## Summary

Benches and toilet blocks serve guests. A bench seats BENCH_SEATS guests at once, each for SITTING_TIME, and a toilet block holds TOILET_STALLS guests, each for TOILET_TIME. Guests beyond those wait at the door in arrival order, and none is turned away. When a guest's time is up, its visit comes back with a rest or a toilet use. Guests visit through the new flow kinds rest-visits and toilet-visits, and are served through rests and toilet-uses. Each bench publishes a rest offer in rest-offer, and each toilet block a toilet offer in toilet-offer, with an exact wait for the next guest to arrive. Each publishes a record of its places, how many are taken, and its queue. The code runs once per facility kind over one list, as shop code runs per good. The food-shop scenario's synthetic guests also visit a bench and a toilet block. No real guest visits them until a-day-out, so nothing changes in play yet.

## Acceptance criteria

1. A bench seats up to BENCH_SEATS guests and a toilet block up to TOILET_STALLS. Each guest keeps its place for SITTING_TIME or TOILET_TIME, and then its visit comes back with a rest or a toilet use. Guests beyond the places wait at the door and take places in arrival order as they free. None is turned away. (check: the scratch program in PLAN.md task 12 prints `bench seats and queues` and `toilet block stalls and queues`)
2. Each bench publishes a rest offer and each toilet block a toilet offer at its guest anchor: REST_RELIEF or TOILET_RELIEF, the wait, and supplied. The wait is exact for the next guest to arrive, including when guests wait at the door. A facility that has not stepped offers a wait of 0. (check: the scratch program prints `rest wait exact at the door`, `toilet wait exact at the door`, and `resolved offers at once`)
3. Rest and toilet units conserve with the ledger's identity every tick. If a bench or toilet block is deleted, the guests at its door get their visits back unserved, and the guests in its places get theirs with a serving. (check: the scratch program prints `deleted bench returned every visit`, `deleted toilet block returned every visit`, and `conserved`; the food-shop scenario, which now sends rest and toilet visits, runs under tpj_scenarios)
4. facilityRecord gives a bench's or toilet block's places, how many are taken, and how many guests wait at its door, and none for any other key. (check: the scratch program prints both records while guests wait at the door, and that a shop and a depot have none)
5. Every park simulates as before. Each slice park remade by tpj_scenarios --slice-parks, and tests/parks/stress/full.park remade by tpj_bench --full-park, differs from the checked-in file only by added lines: the sections of rest-visits-ledger, rests-ledger, toilet-visits-ledger, toilet-uses-ledger, rest-offer-stepped, and toilet-offer-stepped. They are empty except in stocked.park, whose offers hold its bench's and toilet block's entries. (check: PLAN.md task 11's diff removes no line)
6. The park world checks pass over stocked.park, so they cover the new ledgers and the rest and toilet offers. They name the facility service state as unheld, with the reason that no checked park holds a guest visiting a bench or toilet block. (ctest)
7. src/sim/operations/SPEC.md describes benches and toilet blocks as built, and src/scenarios/SPEC.md says the food-shop scenario visits them. (check: read each beside the code)

## Tests

None (docs/testing.md). stocked.park brings the new ledgers and offers under the park world checks. The slice adds the integration test that compares a park with and without a toilet block.

## Medium

- rest-offer and toilet-offer (entry fields, new): each bench publishes one OfferEntry in rest-offer, and each toilet block one in toilet-offer, at its guest anchor. Each is resolved from intent, with a wait of 0, and stepped with the exact wait. Nothing samples them yet. believable-guests reads them in a-day-out, and legible-simulation in explained-needs.
- rest-visits, rests, toilet-visits, and toilet-uses (flows, new): the food-shop scenario's synthetic guests send visits, and get them back with a serving when their time is up. Each unit's handle is its guest.
- Guest anchors on the guest network: sampled, as shops sample theirs. facility-doors anchors benches and toilet blocks by their front doors.
- Park intent: bench and toilet block boxes, read through parkBoxes.
- Inspection records (decision 0025): facilityRecord, read by the scratch program now and by legible's Inspector in explained-needs.

The new state is the four ledgers, the two offers' stepped entries, and the facility service state on a bench or toilet block a guest has visited. Every resolved world holds the ledgers and stepped offers, and tests/parks/stocked.park holds the offers with entries. No checked park holds the service state until a-day-out.

## Spec changes

- src/sim/operations/SPEC.md: Boundary lists what addOperations now registers through addFacilities and what other modules call facilities.h for. A new section, Benches and toilet blocks, gives the places, the time, the queue, the serving, deletion, the offers, and facilityRecord. PLAN.md task 1 has the text.
- src/scenarios/SPEC.md: the food-shop scenario's guests visit a bench and a toilet block (task 2).

## Files affected

- Create: src/sim/operations/facilities.h
- Create: src/sim/operations/facilities.cpp
- Create: src/sim/operations/internal/facility_service.h
- Create: src/sim/operations/internal/offer_entries.h
- Modify: src/sim/operations/operations.h
- Modify: src/sim/operations/operations.cpp
- Modify: src/sim/operations/SPEC.md
- Modify: src/sim/CMakeLists.txt
- Modify: src/scenarios/food_shop.cpp
- Modify: src/scenarios/SPEC.md
- Modify: tests/sim/park_world_checks_test.cpp (UNHELD_TYPES)
- Modify: tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, stocked.park, stress/full.park (remade)

## Dependencies

- served-needs/two-goods: OfferEntry, the per-good pattern, and stocked.park. Merged.
- navigable-networks/facility-doors: benches and toilet blocks anchored on the guest network. Complete.

## Out of scope

- Real guests visiting benches and toilet blocks: a-day-out gives guests rest and toilet needs.
- Showing facility records in the Inspector, and rest and toilet overlays: explained-needs.
- Times that scale with the guest's need: a deepening candidate in MILESTONE.md.

## Open questions

None.
