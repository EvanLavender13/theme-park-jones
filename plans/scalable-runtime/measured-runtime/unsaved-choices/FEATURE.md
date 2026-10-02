# Feature: Unsaved Choices

## Summary

unsaved-choices stops guests keeping their last choice. That choice held one option per reachable shop, so every save, world copy, and hash grew with guests times shops, and the full park's save would be about 13 MB. Nothing in the simulation read it: only the guest panel did. The guest state and its save line lose it, and guests behave exactly as before. The guest panel still explains a guest's decisions (principle 8). A new function, guestOptions, works out what the guest would weigh if it chose now: the options a choice lists at its place, with their terms, scores, and softmax probabilities, with no draw. The inspector calls it only for the guest it shows, and nothing else does, so no frame scores every guest. The panel's "Last choice" row goes, and its table shows the guest's options now. tests/parks/warm.park, cut.park, and stress/winding-path.park lose the field. Remaking warm.park and cut.park gives the committed files with only the field removed, which shows that guests behave the same through 1,920 ticks.

## Acceptance criteria

guestOptions and the inspection are simulation and legible code: the test pass writes their tests. It also rewrites the existing tests that read a guest's LastChoice or GuestChoice, since both are gone, so each observes guestOptions or the guest's behavior instead. Criterion 6 is checked by running the tools, and the feature's report gives their output.

1. A guest's line in a save holds exactly these fields, in this order: at, forward, activity, hunger, hunger-rate, stay-until, target, meal-relief, meals-eaten, and last-meal.
2. guestOptions gives none when the key holds no guest, and none for a guest whose place does not resolve on the guest network.
3. Otherwise it gives the options Choice lists at the guest's place, in order, with t the world's tick and the guest's Hunger and Target. Each option has its terms and Score as Choice computes them, and its Probability as softmaxPick sets it.
4. guestOptions changes nothing: the world after the call is worldsEqual to a copy taken before it.
5. For a guest present in the world, inspectSubject's Rows are, in order, Activity, Hunger, Target, Stay, Meals eaten, and Last meal. Its Choices have one row per option guestOptions gives for the guest, in order, each formatted as Inspectors describes.
6. `tpj_scenarios --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park` remakes warm.park and cut.park. Each equals, byte for byte, its committed version with every ` last-choice=` field removed to the end of its line. So guests behave identically through the slice's 1,920 ticks. scripts/cross-build-check.sh passes. Its output differs from before only in the hashes of worlds holding guests, since a world's hash no longer folds a last choice. tpj_bench loads tests/parks/stress/winding-path.park once the field is removed from it.

With no record of a choice, tests see a guest's choices only through its Activity and Target and only where the place it chose at is known: at the start of a walk at a node, when its visit comes back, or when it loses its target. The options' contents are tested through guestOptions. A guest's choice and guestOptions score with the same code, and criterion 6 shows 1,920 ticks of choices unchanged bit for bit, so a choice made mid-walk needs no test of its own.

## Medium

This feature introduces, samples, and emits no fields or flows. guestOptions samples what a choice samples, guest route distance and food offer, through sampleField, and reads the guest's own state, inside the guests module.

## Principle checks

- Principle 1: a save holds intent and the state behavior depends on (criterion 1). The guest panel's explanation is derived from the world when asked, never stored.
- Principle 8: a guest's decision stays traceable. guestOptions gives every option it weighs, each term of each score, and each probability (criterion 3), and the inspector shows them (criterion 5).
- Principle 10: guestOptions draws nothing, reads no clock, and changes nothing (criterion 4), so it can never change what a run does.
- Principle 6: guestOptions reads routes and offers only through sampleField, as a choice does. legible and app read guest choices only through guestOptions.

## Spec changes

src/sim/guests/SPEC.md:

- Registration: "MealsEaten, how many meals it has eaten; LastMeal; and LastChoice." becomes "MealsEaten, how many meals it has eaten; and LastMeal."
- Adding a guest: "Its MealRelief is 0, its MealsEaten 0, and it has no meal and no choice." becomes "Its MealRelief is 0, its MealsEaten 0, and it has no meal."
- Choice: the sentence "Its LastChoice becomes a GuestChoice: t, its place, its Hunger, the options with their terms, Scores, and Probabilities, and the index picked." is removed.
- Carrying: the sentence "LastChoice keeps the place the guest chose at." is removed.
- Inspection record: ", LastMeal, none until it has eaten, and LastChoice, none until it has chosen." becomes ", and LastMeal, none until it has eaten." Then a new paragraph follows the section's first:

> A guest keeps no record of its choices: nothing in the simulation reads a past choice, so a save holds only what a guest's behavior depends on. guestOptions(world, key) instead gives what the guest would weigh if it chose now, for display and tests. It gives none when key holds no guest or the guest's place does not resolve on N. Otherwise it gives the options Choice lists at the guest's place, in order, with t the world's tick, R sampled at its place, and its Hunger and Target. Each option has its terms and Score as Choice computes them, and its Probability as softmaxPick sets it. It draws nothing and changes nothing. A call costs one choice, so it is made for a guest being inspected, never for every guest in a frame.

src/legible/SPEC.md, Inspectors:

- The bullet "- Last choice: `none` with no LastChoice, and otherwise `<a> s ago at hunger <h>`, a the seconds of the ticks since its Tick, and h its Hunger with two decimals." is removed.
- The paragraph beginning "Choices has a ChoiceRow for each option of LastChoice" becomes:

> Choices has a ChoiceRow for each option guestOptions gives for the guest, in order (sim/guests/SPEC.md, Inspection record). A row's Option is `shop <Shop>` for an option of kind Offer, and the display name of its Kind otherwise. For an Offer, its Relief, Distance, Wait, and Commitment are the option's terms with two decimals, and for another kind they are empty, since carrying on and heading home have no terms. Its Score and Probability are the option's with two decimals. So the table shows every option the guest would weigh if it chose now, each term of its score, and its probability (principle 8). The Target row says which shop it is heading to.

src/app/SPEC.md, the Inspector paragraph: "when it has Choices, `Last choice` above a table with the columns Option, Relief, Distance, Wait, Commitment, Score, and Chance after a first, unnamed one, a row for each ChoiceRow in order with its texts, and `>` in the first column of the Picked row alone." becomes "when it has Choices, `Options now` above a table with the columns Option, Relief, Distance, Wait, Commitment, Score, and Chance, and a row for each ChoiceRow in order with its texts."

src/sim/guests/guests.h: GuestChoice and its visitFields, and ChoiceOption's visitFields, are removed, since nothing saves an option. GuestRecord loses LastChoice, and its comment loses "and its last choice". After guestRecord's declaration it gains:

```cpp
// The options the guest would weigh if it chose now, each with its terms, score, and softmax
// probability, or none when the key holds no guest or its place does not resolve on the guest
// network. Draws nothing and changes nothing; it costs one choice, so call it for one guest.
std::optional<std::vector<ChoiceOption>> guestOptions(const World &world, EntityKey guest);
```

src/legible/inspect.h: ChoiceRow loses Picked. Its comment and Inspection's say the guest's options now, not its last choice.

## Files affected

- Modify: src/sim/guests/SPEC.md, src/sim/guests/guests.h, src/sim/guests/guests.cpp, src/sim/guests/internal/guest.h
- Modify: src/legible/SPEC.md, src/legible/inspect.h, src/legible/inspect.cpp
- Modify: src/app/SPEC.md, src/app/ui/inspector_window.cpp
- Modify: tests/parks/warm.park and tests/parks/cut.park, remade by tpj_scenarios --slice-parks; tests/parks/stress/winding-path.park, with the field removed
- Modify (by the test pass): the tests that read LastChoice or GuestChoice, under tests/sim/guests/, tests/sim/support/guest_parks.h, tests/legible/inspect_test.cpp, and tests/integration/food_loop_test.cpp; and new tests for guestOptions

## Dependencies

- The guests module's choice (Choice section) and softmaxPick: met.
- legible's inspectors and the app's Inspector window: met.
- tpj_scenarios --slice-parks and tests/parks/fed.park: met.

## Out of scope

- Fuzzy choice, in which a guest weighs a few options it notices rather than every reachable shop: believable-guests' backlog. It changes what guests do, so it belongs there.
- Marking in the table the option the guest is acting on: the Target row says it.
- A guest's other display-only state, LastMeal and MealsEaten: each is a fixed size, so neither grows with shops.

## Open questions

None.
