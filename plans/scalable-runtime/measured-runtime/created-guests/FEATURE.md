# Feature: Created Guests

## Summary

created-guests gives the guests module a public function, addGuest(world, place, stayUntil), that creates a guest at a place on the guest network. The guest's stay ends at the tick given, and its hunger and hunger rate are drawn as an arriving guest's are. It returns the new guest's key. Arrivals create their guests through it, so a guest comes into being one way, and every arriving guest is the same as before, draw for draw. full-park's generator will use addGuest to put about 2,000 guests in the full park without touching the module's private state (principle 6). addGuest refuses a place that does not resolve on the guest network, and changes nothing. The feature changes no observable behavior: existing tests pass unchanged, the cross-build check's output is identical, and a runtime report before and after shows every stage's result unchanged.

## Acceptance criteria

addGuest is simulation code: the test pass writes its tests. Criterion 6 is checked by running the tools, and the feature's report gives their output.

1. For a place that resolves on the guest network, addGuest returns the key world.nextKey() gave before the call. guestRecord of that key gives a guest at the place: wandering, with StayUntil the tick given, Target NULL_KEY, MealsEaten 0, and no meal or choice.
2. The new guest's Hunger and HungerRate are the draws Adding a guest names, made on its key with the world's seed and its tick at the call.
3. addGuest refuses a place that does not resolve on the guest network. It throws std::invalid_argument, and the world is unchanged: worldsEqual to a copy taken before the call, with the same nextKey.
4. The guests that arrive in the cycle stepping t have the records addGuest gives in a copy of the world at tick t, taken before that cycle, when it is called once for each of them in key order with that guest's place and stay.
5. The existing tests pass unchanged, and scripts/cross-build-check.sh passes, so the two builds still simulate identically and the change touches no output it compares.
HungerRate, Forward, and MealRelief are not in GuestRecord. HungerRate shows through the hunger one stepped cycle adds (Stepping), and Forward and MealRelief show only through walking and eating, which the existing walk and visit tests cover for arriving guests. A world with added guests is never equal to one whose guests arrived at the same moment, since arriving steps a cycle, so criterion 4 compares the guests' records.

6. Two runtime reports of winding-path.park, one before the change and one after it, compared by `scripts/runtime-report.sh --compare`, mark no stage `result-changed`. The comparison's line for each stage is in the feature's report, whatever it says of time.

## Medium

This feature introduces, samples, and emits no fields or flows. addGuest reads the guest network through parkNetwork and Network::resolve, as stepping does, and writes only the guest state the module owns.

## Principle checks

- Principle 6: no code outside the guests module writes a guest's components. Guest stays in guests/internal/guest.h, and the private header check, "private header check passes on the tree" in ctest, refuses any include of it from outside src/sim/guests. addGuest is the public way in.
- Principle 10: addGuest called with the same arguments on two equal worlds leaves them equal by worldsEqual. Its draws depend on the world's seed and tick and the guest's key alone, never on a clock or the order of calls elsewhere.
- Principle 2: a refused call leaves the world unchanged (criterion 3), and a world with any number of added guests is a working park that steps like one whose guests arrived.

No other principle applies. Guests already walk by route (principle 4), and addGuest moves no one.

## Spec changes

src/sim/guests/SPEC.md, a new section between Stepping and Arrivals:

> ## Adding a guest
>
> addGuest(world, place, stayUntil) creates a guest and gives its key. With N the guest network, parkNetwork(world, PathKind::Guest), it throws std::invalid_argument and changes nothing when place does not resolve on N. That includes every place before the world's first resolution, when N is empty. Otherwise the guest is a new entity from createEntity, so its key is the world's nextKey() before the call. It stands at place, with Forward true, Activity wandering, StayUntil stayUntil, and Target NULL_KEY. Its MealRelief is 0, its MealsEaten 0, and it has no meal and no choice. For a purpose p, let u(p) be drawUniform(drawKey(world, the guest's key, hashName(p), 0)), so each draw depends on the world's seed and its tick at the call. Its HungerRate is HUNGER_RATE_MIN + u("guest-hunger-rate") * (HUNGER_RATE_MAX - HUNGER_RATE_MIN), with HUNGER_RATE_MIN 1/2700 and HUNGER_RATE_MAX 1/1350 per tick, so hunger rises from 0 to 1 in 45 to 90 s. Its Hunger is u("guest-starting-hunger") * STARTING_HUNGER_MAX, 0.4. Hunger is on the scale from 0 to 1 that MEAL_RELIEF assumes (sim/operations/SPEC.md). A guest first steps in the next cycle stepped after it is added.
>
> addGuest is the one way a guest comes into being. Arrivals use it, and so may code outside the module, such as a park generator, which can name no guest component (principle 6).

src/sim/guests/SPEC.md, the Arrivals section becomes:

> ## Arrivals
>
> In the cycle stepping t, when t + 1 is a multiple of ARRIVAL_INTERVAL, 60 ticks, each entrance of parkEntrances that anchors a node of N admits one guest, in entrance key order. It adds the guest with addGuest at nodePlace of the entrance's lowest anchored node. The guest's stay is drawn on the key addGuest will give it, the world's nextKey(), with u as in Adding a guest: StayUntil is t + STAY_MIN + floor(u("guest-stay") * (STAY_MAX - STAY_MIN)), with STAY_MIN 1800 and STAY_MAX 3600. Its hunger and hunger rate are addGuest's draws. A new guest first walks in the next cycle. An entrance with no guest connector admits no guests, a legitimate state (principle 2).

src/sim/guests/guests.h gains, after guestRecord:

```cpp
// Creates a guest standing at the place, whose stay ends at the tick, with its hunger and hunger
// rate drawn on its key, and gives its key. Throws std::invalid_argument, changing nothing, when
// the place does not resolve on the guest network.
EntityKey addGuest(World &world, const Place &place, uint64_t stayUntil);
```

## Files affected

- Modify: src/sim/guests/SPEC.md, src/sim/guests/guests.h, src/sim/guests/guests.cpp
- Create (by the test pass): tests under tests/sim/guests/

## Dependencies

- runtime-report: scripts/runtime-report.sh and its comparison, met.
- The guests module, its private Guest state, and the private header check: met.
- World::nextKey() and keys from a counter (src/sim/SPEC.md): met.

## Out of scope

- The full park's generator, which calls addGuest: full-park, feature 4.
- Choosing a guest's activity, target, or hunger at creation: a generator that wants guests mid-play steps the park after adding them, as full-park's does.
- Removing a guest from outside the module: nothing needs it.

## Open questions

None.
