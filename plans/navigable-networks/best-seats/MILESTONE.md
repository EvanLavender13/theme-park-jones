# Milestone: Best Seats

Slice: somewhere-to-sit

Status: planned

## Summary

best-seats gives guests a way to find a seat without a route search per seat. It derives two resolved entry fields on the guest network: best rest seats and best meal seats. Sampled anywhere on the guest paving, each gives the few pieces of furniture that serve its need best from there, counting the walk, and the next step toward each. Guests weigh walk against relief by their own needs, so each field keeps every piece that fewer than SEAT_CHOICES others beat on both walk and its relief. If a guest scores seats by that field's relief and the walk, its own best few are always among the pieces the field gives. One search per field, seeded from every seat at once, derives it in resolution. Its work grows with the network and the pieces, never with the two multiplied. It is member 3 of somewhere-to-sit. served-seats publishes the offers it ranks, and seated-meals needs its fields before guests can choose and walk to a seat.

## Acceptance criteria

1. Best rest seats and best meal seats are resolved entry fields on the guest network, named best-rest-seats and best-meal-seats. Each has one source. Its entries name a piece, and each gives the piece's key, its seat place, the route distance to that place, the relief a seat there gives for the field's need, the segment that lays the piece, and the next step toward it. (check: read the fields' definitions beside the new module's SPEC.md)
2. At every node of the guest network, each field gives the pieces that fewer than SEAT_CHOICES other pieces dominate there. A piece dominates another when its route distance is no longer, its relief is no lower, and it comes first in the field's tie order. Pieces with no relief for the need are left out. Rounding can make routes of different lengths equal, and then the search's tie order decides, so the spec defines the fields by what the search keeps. (check: a scratch program on tests/parks/furnished-paths.park and tests/parks/stress/full.park runs one route distance search per piece, finds each node's dominated counts by brute force, and prints that every node matches)
3. Sampled at a place strictly inside an edge, including at and beside a seat place, each field gives exactly that place's pieces by the same rule. At a piece's own seat place, its entry has distance 0 and says the guest has arrived. (check: the same scratch program, at the midpoint of every edge and at every seat place)
4. From any place, a mover that follows a piece's next steps, sampling again at the end of each, reaches the piece's seat place (principle 4). From a node, the lengths it walks add up exactly to the distance it first sampled. From a place inside an edge, they add up to it allowing for rounding, because the walk to a seat on the same edge is measured from the edge's end. (check: the same scratch program walks every entry at every node and every edge's midpoint)
5. The fields derive from intent alone. A candidate gives exactly what its commit gives (principle 8), and a loaded park resolves them equal. (ctest: the park world checks, through furnished-paths.park and noon.park's chosen furnishing edit)
6. The fields are derived only in resolution, so ticks are unchanged (decision 0031). On full.park, resolution rises by less than a tenth of build/seat-service/after.report's 14.1 ms, with 644 pieces. A sample of either field does a few lookups in one slot, so its cost doesn't grow with the pieces the park holds. (runtime report compared with build/seat-service/after.report, and review)
7. Otherwise the simulation is unchanged, and both builds derive the fields alike. tpj_scenarios output of windows-debug and windows-release is the same by --compare, with the food-shop scenario's furnished path resolving the fields. Each slice park that tpj_scenarios --slice-parks remakes differs from the checked-in file only by empty sections that every resolved world holds: seat-places-stepped, seat-visits-ledger, and seat-offers-stepped, which earlier members added without remaking the parks, and the new best-rest-seats-stepped and best-meal-seats-stepped. (diff, and tpj_scenarios --compare)
8. The new module sits in its own layer between operations and guests, and the layer check passes. The routes spec describes the search, and the new module's SPEC.md describes the two fields. (ctest, and review)

## Medium

Cross-capability, as the slice's medium map assigns:

- Seat places (resolved entry field, from effortless-building's furnished-paths): read in resolution for each piece's key, its seat place, and its segment, which is the source of its entry.
- Seat offers (resolved entry field, from plausible-operations' served-seats): read in resolution for each piece's rest relief and meal relief.
- Best rest seats and best meal seats (resolved entry fields, produced here): believable-guests' seated-meals samples them to choose a seat and walk to it. legible-simulation's explained-seating samples them for the rest and eating-seat overlays and for naming the serving segment.

Internal to this capability:

- The guest network (from routes): the search walks its edges.
- The search: routes gives a search over a network from many seeds at once. Each seed has a place, a value, and a key. At each node, the search keeps the seeds that fewer than a given number of others dominate, with the distance and the next step toward each. It reads no field, so bins can use it later with one value for every bin.

The fields don't know which seats are taken. A guest learns that only by visiting a piece (principle 6).

## Dependencies

- effortless-building/furnished-paths: the seat places field, with each piece's key, place, and segment. Complete.
- plausible-operations/served-seats: the seat offers field. Complete.
- navigable-networks/wide-routes: the guest network the search walks. Complete.

## Core feature

`best-seats` is the only feature. The search alone gives seated-meals nothing to sample, and the fields need the search to exist. Together they are about the size of paths-become-routes' route-distance, which they follow closely.

## Features

1. `best-seats`: the search over many seeds in routes, a new module between operations and guests that defines the two fields and seeds them from seat places and seat offers, the fields' rule for places inside an edge, and the layer table's new layer. Depends on: none.

It adds no test (docs/testing.md). furnished-paths.park and full.park bring the fields under the park world checks. The slice adds the integration tests that compare parks with tables near and far.

## Tuning values

- SEAT_CHOICES: 4. Each field keeps, at each place, the pieces that fewer than 4 others dominate. A guest that finds pieces full can move on to its next best up to 4 times before the field gives it nothing new. With two relief levels, a field holds at most 8 pieces at a place. seated-meals may change it after watching noon.park.

## Deepening candidates

Unordered pool this milestone draws later features from.

- A distance limit, beyond which a field gives no piece, so far seats cost nothing to keep. Gated on: the fields' size mattering in larger parks.
- Updating the fields only near an edit, with results equal to a full rebuild. Gated on: resolution time mattering in larger parks.

## Open questions

None.

## Research notes

- The pieces a field keeps are the k-skyband of database skyline queries. It holds every guest's own k best seats, whatever weights the guest gives walk and relief.
- One label-setting search per field, seeded from every piece, derives it. Every node on a kept piece's route also keeps it, so next steps always lead to the piece.
- One source per field keeps a sample's cost the same however many pieces there are, and lets the field's rule inside an edge give exactly that place's pieces.
- The search goes in routes. The fields go in a new module above operations, because they read seat offers.

The details are in RESEARCH.md.
