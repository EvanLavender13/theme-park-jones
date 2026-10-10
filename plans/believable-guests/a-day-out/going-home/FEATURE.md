# Feature: Going Home

## Summary

Going home becomes a scored option and replaces the fixed stay. Whenever an entrance is reachable, its score is a base plus one term for each reason: hunger, thirst, tiredness, the toilet, and closing time. Each need's term grows with that need. The closing term rises over the hour before closing and is at its full value while the park is closed. When the park closes, every walking guest chooses at once. A guest heading home keeps choosing at the nodes of its route, with the commitment bonus on going home. Each pick of going home records its largest reason term as the guest's reason. When the guest walks out of an entrance, it is counted under that reason in the departure tally, which starts again each time the park opens. Guests record the tick they arrived, and need rates move to the milestone's day-scale values. The Inspector shows the arrival time, the reason a guest heads home, and going home's terms. Arrivals stay steady until arrival-curve.

## Acceptance criteria

1. Going home is an option whenever an entrance is reachable. Its score is HOME_BASE, plus a term for each of hunger, thirst, tiredness, and the toilet that is HOME_NEED_WEIGHT times the need curve at that need's level, plus the closing term, plus the commitment bonus while the guest is heading home. The score is exactly the sum of those terms, and its reason is the first largest of the five reason terms. (check: the scratch program of PLAN.md Task 17 compares every guest's going-home option with these rules at every tick)
2. The closing term is 0 until CLOSING_LEAD before closing. It then rises linearly to CLOSING_SCORE at closing, and is CLOSING_SCORE while the park is closed. If the park is open all day, it is always 0. (check: the same program, which prints the term at three times)
3. When the park closes, every guest that is walking chooses in that tick and heads home with the reason closing. A guest that is waiting heads home when its visit comes back. (check: the same program; play: set the closing time a few minutes ahead in the Hours panel and watch every guest walk out)
4. The departure tally counts the guests who have walked out of an entrance since the park last opened, by the reason of their latest pick of going home. It starts again from 0 when the park opens. A guest that leaves because no guest paving is left isn't counted. (check: the same program's own count of departures equals the tally's total, every departure after closing is under closing, and after the park reopens the tally holds only the guests who walked out in the opening tick)
5. Every guest records the tick it arrived. The Inspector shows the guest's arrival time, and while it heads home, the reason. The choice table's going-home row shows its reason, commitment, score, and chance, and a Going home table below it shows the base and each reason's term. (play: open stocked.park, click guests before and after closing)
6. Hunger, thirst, and tiredness rise at the milestone's day rates: hunger from 0 to 1 in 4 to 6 game hours, thirst in 3 to 4.5 game hours, and tiredness in 400 to 600 m of walking. (check: read the constants in src/sim/guests/guests.h)
7. Every park in tests/parks that holds guests is remade, and the play parks in parks/ still load. The early-turns integration test makes its guest's park closed instead of ending its stay, and keeps its assertion. tests/parks/stress/full.park's guests leave by choice during the runtime report's run, and its release stage medians are reported before and after. (check: PLAN.md Tasks 15, 16, and 18; ctest)
8. src/sim/guests/SPEC.md, src/sim/park/SPEC.md, src/legible/SPEC.md, src/app/SPEC.md, and src/scenarios/SPEC.md describe the change. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Task 19)

## Tests

None. The early-turns test changes its setup only, as the milestone says. Every resolved park holds the departure tally, so the park world checks cover it. The slice's closing adds the integration tests for the needs' consequences.

## Medium

- Samples: guest route distance at the guest's place, as today, for the entrances' entries.
- Reads intent: parkHours and isOpenAt for the opening hours, and parkEntrances, as today (decision 0025).
- Reads the clock: parkTime(world.Tick). It is part of the tick, not a dependency between entities.
- Supplies and draws: unchanged.
- Inspection records: the guest record gains the arrival tick and the reason it heads home. Each choice option gains going home's base, reason terms, and reason. departureTally publishes the tally. legible-simulation's Inspector reads the first two, and explained-needs will read the tally.

Guests read nothing new from other entities (principle 6). The tally is new saved state on its own entity, which a resolver creates. Every park the world checks open holds it, so they cover it.

## Spec changes

- src/sim/guests/SPEC.md: Boundary adds the hours and the clock. Arriving and leaving drops the stay and adds the arrival tick. Choosing gives going home's place among the options. A new Going home section gives its terms, its reason, and the choice at closing. A new Departures section gives the tally. Inspection adds the arrival tick, the reason, and going home's terms (PLAN.md Task 2).
- src/sim/park/SPEC.md: isOpenAt (PLAN.md Task 3).
- src/legible/SPEC.md and src/app/SPEC.md: the guest inspector's arrival time, reason, and Going home table (PLAN.md Task 3).
- src/scenarios/SPEC.md: what warm-main-street holds once stays are gone (PLAN.md Task 15).

The public interface changes:

- src/sim/guests/guests.h:
  - A new enum HomeReason {Hunger, Thirst, Tiredness, Toilet, Closing} with enumNames {"hunger", "thirst", "tiredness", "toilet", "closing"}, and HOME_REASONS, the five in that order.
  - ChoiceOption gains Base, Reasons (one term for each of HOME_REASONS), and Reason.
  - GuestRecord's StayUntil becomes Arrived, and it gains Reason after TargetNeed.
  - STAY_MIN, STAY_MAX, and HEAD_HOME_SCORE go. HOME_BASE, HOME_NEED_WEIGHT, CLOSING_LEAD, and CLOSING_SCORE are new.
  - HUNGER_RATE_MIN, HUNGER_RATE_MAX, THIRST_RATE_MIN, THIRST_RATE_MAX, TIREDNESS_RATE_MIN, and TIREDNESS_RATE_MAX take the day rates.
  - A new struct DepartureTally and function departureTally.
  - addGuest loses its stayUntil argument.
- src/sim/park/intent.h: a new function isOpenAt(hours, second).
- src/legible/inspect.h: Inspection gains HomeTerms.

## Tuning values

From the milestone:

- HUNGER_RATE_MIN 1/10800 and HUNGER_RATE_MAX 1/7200 per tick: 4 to 6 game hours from 0 to 1.
- THIRST_RATE_MIN 1/8100 and THIRST_RATE_MAX 1/5400 per tick: 3 to 4.5 game hours.
- TIREDNESS_RATE_MIN 1/600 and TIREDNESS_RATE_MAX 1/400 per meter: 400 to 600 m.
- HOME_BASE -2 and HOME_NEED_WEIGHT 1.5.
- CLOSING_LEAD 3600 game seconds, one game hour. CLOSING_SCORE 10, today's HEAD_HOME_SCORE.

Starting needs, the bladder's fills, the offer weights, carrying on, the commitment bonus, and the softmax temperature stay as they are.

## Files affected

- Modify: src/sim/guests/SPEC.md, guests.h, guests.cpp, internal/guest.h
- Create: src/sim/guests/internal/departures.h
- Modify: src/sim/park/SPEC.md, intent.h, intent.cpp
- Modify: src/legible/SPEC.md, inspect.h, inspect.cpp
- Modify: src/app/SPEC.md, src/app/ui/inspector_window.cpp
- Modify: src/scenarios/SPEC.md, full_park.h, full_park.cpp, and slice_parks.h if the warm-ups change
- Modify: tests/integration/early_turns_test.cpp (the park is closed instead of the stay being over)
- Modify: tests/parks/warm.park, cut.park, warm-main-street.park, stocked.park, stress/full.park (remade)
- Modify: parks/busted-path.park, jank.park, radii.park (guest lines rewritten)

## Dependencies

- four-needs: the four needs and their curve. Merged.
- deterministic-simulation/park-clock: parkTime. Complete.
- effortless-building/facilities-and-hours: the opening hours. Complete.

## Out of scope

- Arrivals by the curve, and no arrivals while closed: arrival-curve. Until then, guests who arrive while the park is closed head home at once.
- The departures panel and the hours panel's preview: explained-needs.
- The slice's four parks and the tuning by watching a day: day-parks.
- Counting guests who leave because no guest paving is left: already a deepening candidate in the milestone.

## Open questions

None.
