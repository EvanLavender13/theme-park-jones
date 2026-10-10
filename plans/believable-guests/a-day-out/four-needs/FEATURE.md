# Feature: Four Needs

## Summary

Guests carry four needs: hunger, thirst, tiredness, and bladder. Hunger and thirst rise with time, tiredness rises with each meter walked, and each meal or drink fills the bladder. A guest scores every offer it can reach in the four need fields, one option for each need a facility offers. When it arrives, it sends that need's visit and takes that need's serving. The guest Inspector shows the four needs and each option's need, and guests are shaded by their most pressing need. Stays and arrivals stay as they are; going-home and arrival-curve replace them. Parks whose shops all sell food, with no bench or toilet block, simulate exactly as before.

## Acceptance criteria

1. Each guest's four needs stay within 0 to 1. Hunger and thirst rise each tick at the guest's drawn rates. Tiredness rises with the meters the guest walks, and doesn't rise while it waits. A meal fills the bladder by BLADDER_PER_MEAL and a drink by BLADDER_PER_DRINK. When a need falls, it is the need the guest's visit was for, and no other need falls in that tick. (check: the scratch program of PLAN.md Task 15 steps tests/parks/stocked.park and checks every guest every tick)
2. Guests in stocked.park eat, drink, rest on the bench, and use the toilet block. Each serving is consumed with its need's cause. (check: the same program prints the meals, drinks, rests, and toilet uses consumed, each above 0)
3. A shop that sells both goods gives a guest two options, one for each need. Every option names its facility and need. The Inspector's table shows each option's facility, need, terms, score, and chance. Its rows show the four needs and the target with the need the guest heads there for. (play: open stocked.park, click guests, and watch their needs and options)
4. Guests are shaded from blue when every need is 0 to pink when their most pressing need is 1. (play)
5. Every park in tests/parks that holds guests is remade. Each one other than stocked.park equals its checked-in file once the new guest fields are removed and the renamed field and activity are named back. (check: PLAN.md Task 13)
6. The play parks parks/busted-path.park, jank.park, and radii.park still load. (check: PLAN.md Task 14)
7. src/sim/guests/SPEC.md, src/legible/SPEC.md, and src/render/SPEC.md describe the change. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Task 16)

## Tests

None. stocked.park holds guests with the new state, so the park world checks cover it. The slice's closing adds the integration tests for the needs' consequences.

## Medium

- Samples: food-offer, drink-offer, rest-offer, and toilet-offer at each facility's guest anchor, through sourceEntryAtNode; guest route distance, as today.
- Supplies: guest-visits, drink-visits, rest-visits, and toilet-visits, each created by the guest and sent to its target facility. plausible-operations returns each one.
- Draws: meals, drinks, rests, and toilet-uses that come back with a visit. The guest consumes each as eaten, drunk, rested, or used.
- Emits: hungry footfall, unchanged, still weighted by hunger alone.
- Inspection records: the guest record gains thirst, tiredness, bladder, and the target's need. Each choice option gains its need. legible-simulation's Inspector and render's guest shading read them.

Guests read only the offer fields, route distance, and their own stocks (principle 6). Thirst, tiredness, and bladder never leave the guest except in its inspection record. The new state is in the guest component, which stocked.park holds, so the park world checks cover it.

## Spec changes

- src/sim/guests/SPEC.md: the opening, Boundary, Arriving, Choosing, Walking, and Inspection sections name facilities and the four offer fields instead of shops and food-offer. Hunger becomes Needs, and Visits and meals becomes Visits and servings (PLAN.md Task 1).
- src/legible/SPEC.md: the guest inspector shows the four needs, the target with its need, and each option's facility and need (PLAN.md Task 2).
- src/render/SPEC.md: guests are shaded by their most pressing need (PLAN.md Task 2).

The public interface changes in src/sim/guests/guests.h:

- GuestActivity::HeadingToShop becomes HeadingToFacility, named heading-to-facility.
- A new enum GuestNeed {Hunger, Thirst, Tiredness, Bladder} with enumNames {"hunger", "thirst", "tiredness", "bladder"}, and NEEDS, the four in that order.
- ChoiceOption's Shop becomes Facility, and it gains GuestNeed Need after it.
- GuestRecord gains Thirst, Tiredness, and Bladder after Hunger, and GuestNeed TargetNeed after Target.
- HUNGER_CURVE becomes NEED_CURVE, and hungerCurve becomes needCurve.
- New constants THIRST_RATE_MIN, THIRST_RATE_MAX, STARTING_THIRST_MAX, TIREDNESS_RATE_MIN, TIREDNESS_RATE_MAX, STARTING_BLADDER_MAX, BLADDER_PER_MEAL, BLADDER_PER_DRINK, DRUNK_CAUSE, RESTED_CAUSE, and USED_CAUSE.

## Tuning values

The milestone's need rates assume stays of game hours, which going-home brings. Stays are still 1 to 2 game hours, so this feature keeps hunger's rate and scales the others to it, in the milestone's proportions. going-home sets the milestone's values for all four.

- THIRST_RATE_MIN 1/2000 and THIRST_RATE_MAX 1/1000 per tick: thirst goes from 0 to 1 in 33 to 67 game minutes. STARTING_THIRST_MAX 0.4.
- TIREDNESS_RATE_MIN 1/150 and TIREDNESS_RATE_MAX 1/100 per meter: tiredness goes from 0 to 1 in 100 to 150 m, about 75 to 115 game minutes of walking. Tiredness starts at 0.
- BLADDER_PER_MEAL 0.2, BLADDER_PER_DRINK 0.35, STARTING_BLADDER_MAX 0.2.

## Files affected

- Modify: src/sim/guests/SPEC.md, guests.h, guests.cpp, internal/guest.h
- Modify: src/legible/SPEC.md, inspect.h, inspect.cpp
- Modify: src/app/ui/inspector_window.cpp
- Modify: src/render/SPEC.md, guest_mesh.h, guest_mesh.cpp
- Modify: tests/sim/park_world_checks_test.cpp (the activity's new name)
- Modify: tests/parks/warm.park, cut.park, warm-main-street.park, stocked.park, stress/full.park (remade)
- Modify: parks/busted-path.park, jank.park, radii.park (guest lines rewritten)
- Modify: plans/believable-guests/a-day-out/MILESTONE.md (the tuning note)

## Dependencies

- plausible-operations/served-needs: the drink, rest, and toilet offers, visits, and servings. Complete.

## Out of scope

- Going home scored by need, and the longer stays it brings: going-home.
- Arrivals by the curve: arrival-curve.
- An Inspector for benches and toilet blocks: explained-needs.
- Footfall for needs other than hunger: in plans/BACKLOG.md.

## Open questions

None.
