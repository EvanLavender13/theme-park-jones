# Feature: Arrival Curve

## Summary

Entrances stop admitting a guest every 60 ticks. Instead, each tick, each entrance connected to the guest network admits a guest by one keyed draw, at the rate the arrival curve gives. The curve is a piecewise-linear table over the fraction of the opening hours that has passed: a few guests at opening, a rush in the first third, a thinning stream after it, and none over the last quarter or while the park is closed. arrivalCurve(world, second) publishes the rate at a second of the day under the park's hours, so a candidate with tentative hours gives the curve those hours would admit guests by. Every park in tests/parks that holds guests is remade.

## Acceptance criteria

1. Each entrance connected to the guest network admits guests at random by one keyed draw a tick, at arrivalCurve's rate on average. Over three days of main-street.park, each clock hour's arrivals, summed over the days, lie within 4 standard deviations of the count arrivalCurve predicts. This holds with the default hours and with hours from 13:00 to 1:00. (check: the scratch program of PLAN.md Task 6 prints each hour's count beside its prediction)
2. No guest arrives while the park is closed, or over the last quarter of the open hours, when the curve is 0. Every guest has left by the next opening, and guests arrive again after it. (check: the same program; play: in the Hours panel, close the park for five game hours, and watch the Debug panel's guest count fall to 0, stay there, and rise again after opening)
3. arrivalCurve gives ARRIVAL_CURVE at the fraction of the opening hours that has passed, stretched to whatever hours are set, including hours past midnight, and 0 while the park is closed. On a copy of a world with SetOpeningHours applied, it gives the new hours' curve, and the original world's curve is unchanged. (check: the same program compares it with exact values at fixed seconds under both sets of hours)
4. Every park in tests/parks that holds guests is remade. warm.park still holds a guest waiting at its shop and one heading there, and warm-main-street.park still holds guests in every activity. The park world checks and the integration tests pass. (check: PLAN.md Task 5; ctest)
5. tests/parks/stress/full.park admits guests by the curve during the runtime report's run and keeps nearly all of its 2,000 guests through it. Its release stage medians are reported before and after. (check: PLAN.md Tasks 6 and 7)
6. src/sim/guests/SPEC.md describes arrivals by the curve and arrivalCurve. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Task 8)

## Tests

None. Arrivals add no saved state, so the park world checks need no new park. The slice's closing adds the integration tests for the slice's criteria.

## Medium

- Reads intent: parkHours and isOpenAt for the opening hours, and parkEntrances, as today (decision 0025).
- Reads the clock: parkTime(world.Tick). It is part of the tick, not a dependency between entities.
- Samples the guest network for each entrance's anchored nodes, as today.
- Draws: one keyed draw for each connected entrance and tick, on the entrance's key, the world's seed, and the tick (principle 8). It needs no saved state.
- Inspection records: arrivalCurve publishes the rate at a second of the day. explained-needs will read it from a candidate with tentative hours.

Guests read nothing new from other entities (principle 6). The feature registers no state.

## Spec changes

- src/sim/guests/SPEC.md: Boundary adds arrivalCurve to the functions other modules call. Arriving and leaving says guests arrive at random by the curve, one draw per entrance and tick, and describes arrivalCurve (PLAN.md Task 2).
- src/scenarios/SPEC.md: what warm-main-street holds, only if its activities change (PLAN.md Task 5).

The public interface changes in src/sim/guests/guests.h:

- ARRIVAL_INTERVAL goes.
- A new constant ARRIVAL_CURVE, a std::array<CurvePoint, 6> of {0, 5}, {0.15, 15}, {0.3, 15}, {0.5, 6}, {0.75, 0}, and {1, 0}: guests per game hour at each entrance, at fractions of the opening hours.
- A new function `double arrivalCurve(const World &world, uint64_t second);`.

## Tuning values

From the milestone: the curve's points above. Over the default 12 hours, that is about 79 guests a day at each entrance.

## Files affected

- Modify: src/sim/guests/SPEC.md, guests.h, guests.cpp
- Modify: src/scenarios/slice_parks.h (the comment on MAIN_STREET_WARM_TICKS, and WARM_TICKS or MAIN_STREET_WARM_TICKS if the remade parks need a longer warm-up)
- Modify: src/scenarios/SPEC.md, only if warm-main-street's activities change
- Modify: tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, stocked.park, stress/full.park (remade)
- Modify: plans/scalable-runtime/CAPABILITY.md (a deepening candidate)

## Dependencies

- going-home: guests leave by choice and at closing. Merged.
- deterministic-simulation/park-clock: parkTime. Complete.
- effortless-building/facilities-and-hours: the opening hours. Complete.

## Out of scope

- The Hours panel's preview of the curve: explained-needs.
- Arrivals that depend on how well the park serves its guests: already in plans/BACKLOG.md.
- The slice's four parks and tuning the curve by watching a day: day-parks.
- full.park emptying over hours of play: its 30 counters serve about 600 meals and drinks a game hour, well below what 2,000 guests want, so thirst sends guests home within a few game hours. The runtime report's run of 3,600 ticks, two game hours, keeps nearly all of them. Added to scalable-runtime's deepening candidates.

## Open questions

None.
