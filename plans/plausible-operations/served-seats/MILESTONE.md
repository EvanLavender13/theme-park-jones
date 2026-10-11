# Milestone: Served Seats

Slice: somewhere-to-sit

Status: planned

## Summary

served-seats makes each piece of furniture serve guests. Each piece publishes a seat offer: the rest relief and the meal relief a seat there gives, by kind. A table serves a meal best and rests a guest a little, and a bench rests a guest best and serves a meal less well. Each piece takes seat visits. While it has a free seat, it seats the guest for a fixed time and then returns the visit with a rest. When every seat is taken, it returns the visit at once, unseated, so the guest can move on. It is member 2 of somewhere-to-sit. furnished-paths lays the pieces and publishes seat places. best-seats needs seat offers to build its fields, and seated-meals needs pieces that take visits before guests can sit.

## Acceptance criteria

1. Seat offers is a resolved entry field on the guest network, named seat-offers. Each piece with a seat place is the source of one entry, at the same place as its seat place. The entry holds the rest relief and the meal relief a seat there gives, by the piece's kind, as Tuning values gives them. A candidate's pieces have their offers at once, and a piece that a resolution no longer lays has none. (check: a scratch program samples seat-offers at each piece's seat place in tests/parks/furnished-paths.park and finds that piece's entry with its kind's reliefs; the park world checks, through furnished-paths.park and noon.park's chosen furnishing edit)
2. A piece takes the seat visits it holds in the order they arrived. While one of its seats is free, it gives the seat to the guest and keeps it for SEAT_TIME. Then the visit comes back with a rest, and the seat is free from that tick. A piece has furnitureSeats of its kind's seats. If a visit arrives when every seat is taken, it comes back the next tick with no serving. No guest waits at a piece. (check: a scratch program steps a copy of furnished-paths.park, sends visits to one bench and one table beyond their seats, and checks tick by tick which visits come back, when, and with or without a rest)
3. Seat visits and rests conserve with the ledger's identity. If an edit removes a piece, visits on their way to it and visits it holds go back to their guests with no serving. Guests it has seated still get their rest when their time is up. If a visit or rest is sent to a guest who has left, it is consumed as abandoned. (check: the scratch program of criterion 2 places a shop over a piece while guests sit there and checks the identity every tick)
4. A tick steps only the pieces that hold seat visits, so its work grows with the guests visiting seats, never with the number of pieces (decision 0031). On full.park, which no guest visits a seat in yet, the runtime report's ticks stay within their noise of the report from seat-places. Resolution rises only by publishing one offer for each piece. (runtime report compared with build/seat-places/after.report, and review)
5. The food-shop scenario furnishes a guest segment, and its synthetic guests visit a piece of furniture often enough that one is turned away. So the cross-build check runs seat service, seating and turning away, on every release build. (check: a scratch program runs the scenario for tpj_scenarios' default ticks and counts the seat visits that come back with a rest and those that come back without one, and both counts are above 0; tpj_scenarios output from windows-debug and windows-release compared with --compare)
6. Otherwise the simulation is unchanged. Each slice park that tpj_scenarios --slice-parks remakes differs from the checked-in file only by empty sections that every resolved world holds: seat-places-stepped, which furnished-paths added without remaking the parks, and the new seat-visits-ledger and seat-offers-stepped. (diff against tests/parks)
7. The park world checks name the seat state as unheld, with the reason that no park holds a guest seated at furniture until seated-meals' parks do. seated-meals removes the name. (ctest)
8. src/sim/operations/SPEC.md describes seat offers, seat visits, and seat service as built, and src/scenarios/SPEC.md names the furnished segment and its seat visits among what the scenarios exercise. (review)

## Medium

Cross-capability, as the slice's medium map assigns:

- Seat places (resolved entry field, from effortless-building's furnished-paths): read in resolution to find each piece, its place, its kind, and its seats. Each furnished segment is the source of its pieces' entries.
- Seat offers (resolved entry field, produced here): each piece is the source of one entry at its seat place, with its rest relief and meal relief. navigable-networks' best-seats reads it to build the best-seats fields. believable-guests' seated-meals reads the relief of the seat a guest uses.
- Seat visits (flow, produced by believable-guests in seated-meals, consumed here): a guest sends one visit to a piece's key. The piece returns it, after SEAT_TIME with a rest if it seated the guest, or the next tick with nothing if it was full. Synthetic guests send them in this milestone.
- Rests (flow, produced here and by bench boxes until the slice retires them): one rest to each guest a piece seats, sent with its visit.

Internal to this capability:

- Seat state, on each piece's entity: the tick each taken seat frees. It is saved, because guests seated at a piece are simulation state. It goes when a resolution no longer lays the piece.

Free seats and sitting times stay private to plausible-operations. A guest learns a piece is full only from its visit coming back unseated (principle 6).

## Dependencies

- effortless-building/furnished-paths: pieces as entities, and the seat places field. Complete.
- plausible-operations/served-needs: the rests flow, and the facility service that seat service follows. Complete.

## Core feature

`seat-service` is the only feature. Seat offers alone give guests nothing to visit, and seat visits alone give best-seats no reliefs to rank, so the two land together. Together they are about the size of served-needs' seats-and-stalls.

## Features

1. `seat-service`: the seat-offers field and its resolver, the seat-visits flow, the seat state, and the system that seats or turns away guests at the pieces holding visits. The food-shop scenario's synthetic guests visit a piece, and the park world checks name the seat state as unheld. Depends on: none.

It adds no test (docs/testing.md). furnished-paths.park brings seat offers under the park world checks. The slice adds the integration tests that compare parks with tables near and far, and with and without furniture.

## Tuning values

Each is set here, and adjusted while planning seated-meals by watching noon.park. Times are in ticks, 2 game seconds each.

- SEAT_TIME: 300 ticks, 10 game minutes, as a bench box's SITTING_TIME. One time for both kinds, whether the guest sits to rest or to eat.
- A bench's rest relief: REST_RELIEF, 0.5, as a bench box's. Its meal relief: 0.4.
- A table's rest relief: 0.2. Its meal relief: MEAL_RELIEF, 0.5.
- Seats: furnitureSeats, 2 on a bench and 4 at a table, as furnished-paths set them.

seated-meals sets how much a meal relieves when eaten standing, below both kinds' meal relief.

## Deepening candidates

Unordered pool this milestone draws later features from.

- An inspection record for each piece, with its seats and how many are taken, for the Inspector. Gated on: explained-seating, or play, asking for it.
- Guests reluctant to share a piece with strangers, so a table of four seats fewer unrelated guests. Gated on: seated-meals' play.

## Open questions

None.

## Research notes

- One sitting time serves both resting and eating, so a piece needs one flow of seat visits and never reads why a guest sat.
- A full piece returns the guest at once instead of queueing it, as people walk on from a full bench. With no queue, the offer has no wait, so it can be resolved from intent alone, as best-seats needs.
- A tick steps only the pieces holding visits, found from the seat-visits ledger (decision 0031).

Details are in RESEARCH.md.
