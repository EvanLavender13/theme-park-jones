# Implementation Plan: Four Needs

## Goal

Guests carry hunger, thirst, tiredness, and bladder, choose among every need offer they can reach, visit for that need, take its serving, and show their needs in the Inspector and their shading.

## Approach

guests.cpp gains a traits template NeedKinds over GuestNeed, as operations.cpp has GoodKinds, naming each need's offer field, visit and serving flows, consuming cause, and bladder fill, and a forEachNeed walk over NEEDS. Scoring, sending a visit, and taking a serving each walk the needs instead of naming food. The guest's state gains the three new levels, two rates, and the target's need, with the meal relief renamed to the serving relief. Parks with no drink, bench, or toilet offers then give the same options in the same order with the same draws, so they simulate exactly as before.

## Placement

Decision 0027 places each behavior this feature adds:

- The four needs, their rises and couplings, choosing among the four offer fields, and each need's visit and serving: guests, src/sim/guests/guests.cpp with its state in src/sim/guests/internal/guest.h. Guests own their private motives and their choices (principle 6).
- The need enum, the option's need, the record's new fields, and the new constants: guests, src/sim/guests/guests.h, the module's public header, which the Inspector and render read.
- The guest Inspector's need rows and the choice table's facility and need: legible, src/legible/inspect.h and inspect.cpp, which build every inspector from records. The table's new column is drawn by src/app/ui/inspector_window.cpp, which draws the inspection.
- Shading guests by their most pressing need: render, src/render/guest_mesh.h and guest_mesh.cpp, which owns how guests are drawn.
- The activity's new name in the world checks: tests/sim/park_world_checks_test.cpp, a rename only.

## Tasks

### Task 1: Update the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md`

Step 1: In the opening paragraph, replace "Guests arrive, wander, grow hungry, choose shops, eat, and go home." with "Guests arrive, wander, grow hungry, thirsty, and tired, choose shops, benches, and toilet blocks, and go home." Replace "A guest's hunger leaves the module only as part of hungry footfall, summed over guests, so the park responds to crowds, not to individual guests (principles 3, 5, and 6)." with "A guest's hunger leaves the module only as part of hungry footfall, summed over guests, so the park responds to crowds, not to individual guests (principles 3, 5, and 6). Its other needs never leave the module."

Step 2: In Boundary's list, replace "- the food-offer field;" with "- the need offer fields food-offer, drink-offer, rest-offer, and toilet-offer;". Replace "- its own guest-visits and meals stocks;" with "- its own stocks of each need's visits and servings: guest-visits and meals, drink-visits and drinks, rest-visits and rests, and toilet-visits and toilet-uses;". Replace "A guest visits shops and receives meals through the guest-visits and meals flows." with "A guest visits facilities, which are shops, benches, and toilet blocks, and receives servings through those flows."

Step 3: In Arriving and leaving, replace "A new guest's hunger, hunger rate, speed, and share of the width are drawn, so guests differ from one another." with "A new guest's hunger, thirst, and bladder, the rates its hunger, thirst, and tiredness rise at, its speed, and its share of the width are drawn, so guests differ from one another. Its tiredness starts at 0."

Step 4: Replace the section "## Hunger" and its paragraph with:

    ## Needs

    A guest has four needs: hunger, thirst, tiredness, and bladder. Each runs from 0 to 1. Hunger and thirst rise each tick at the guest's own rates. Tiredness rises with each meter the guest walks across the ground, at its own rate, so it doesn't rise while the guest waits. Each meal the guest eats fills its bladder by BLADDER_PER_MEAL, and each drink by BLADDER_PER_DRINK. No need rises past 1.

    A serving relieves the need it answers by the relief the facility offered when the guest chose it: a meal relieves hunger, a drink thirst, a rest tiredness, and a toilet use the bladder. No need falls below 0, and a serving relieves no other need.

Step 5: In Choosing, replace the list item "- each shop it can reach that offers meals;" with "- each offer it can reach: one for each need a facility offers, so a shop that sells both goods gives two;". Replace "A shop scores higher the hungrier the guest is." with "An offer scores higher the greater the need it answers." Replace "If the guest is already heading to a shop, that shop scores a little higher, so guests don't flip between shops." with "If the guest is already heading to a facility, that facility's offer for the same need scores a little higher, so guests don't flip between facilities." Replace "Once a stay is over, heading home outweighs any meal." with "Once a stay is over, heading home outweighs any offer."

Step 6: Replace "A guest finds shops only through route distance and the food-offer field. If it can't walk to a shop, or the shop's food-offer shows no meals, that shop is never an option. An entrance is never an option for food." with "A guest finds offers only through route distance and the four need offer fields. If it can't walk to a facility, or the facility's entry in a need's field doesn't say it can serve, that need's offer there is never an option. An entrance is never an option for any need."

Step 7: Replace "A guest heading to a shop or home makes no choice for the node it is heading to. A guest also chooses as soon as its target becomes unreachable or stops offering meals, and when its visit comes back." with "A guest heading to a facility or home makes no choice for the node it is heading to. A guest also chooses as soon as its target becomes unreachable or stops offering the need it heads there for, and when its visit comes back."

Step 8: In Walking, replace "When a guest heads to a shop or home" with "When a guest heads to a facility or home", "So a guest heading for a shop across a wide street" with "So a guest heading for a facility across a wide street", and "A guest that starts waiting at a shop drops its entry" with "A guest that starts waiting at a facility drops its entry". In Carrying across an edit, replace "If it is heading to a shop, it keeps its target until its next walk." with "If it is heading to a facility, it keeps its target until its next walk."

Step 9: Replace the section "## Visits and meals" and its paragraph with:

    ## Visits and servings

    When a guest arrives at its target facility, it sends the facility a visit for the need it chose: guest-visits for hunger, drink-visits for thirst, rest-visits for tiredness, and toilet-visits for the bladder. It then waits there until the visit comes back, whatever else changes. If the visit comes back with that need's serving, a meal, a drink, a rest, or a toilet use, the guest was served, and that need falls. A guest that eats a meal also records it. If the visit comes back without a serving, the guest was turned away. Either way, it then chooses what to do next. A guest has at most one visit outstanding. It consumes returned visits with FINISHED_CAUSE, meals with EATEN_CAUSE, drinks with DRUNK_CAUSE, rests with RESTED_CAUSE, and toilet uses with USED_CAUSE, which other modules count.

Step 10: In Inspection, replace "The record holds its activity, its place, the ground position it stands on, hunger, stay, target, meals eaten, and last meal." with "The record holds its activity, its place, the ground position it stands on, its four needs, stay, target and the need it heads there for, meals eaten, and last meal." Replace "Each option has its terms (Relief, Distance, Wait, Commitment), its Score, and its Probability." with "Each option has its kind, the facility and need of an offer, its terms (Relief, Distance, Wait, Commitment), its Score, and its Probability."

### Task 2: Update the legible and render specs

Files:
- Modify: `src/legible/SPEC.md:58-59`
- Modify: `src/render/SPEC.md:64`

Step 1: In src/legible/SPEC.md, replace "- its activity, hunger, target shop, time left in its stay, meals eaten, and last meal" with "- its activity, its four needs, its target and the need it heads there for, time left in its stay, meals eaten, and last meal". Replace "- a table of every option the guest would weigh if it chose now, with each term of its score and its probability (principle 8)" with "- a table of every option the guest would weigh if it chose now, with each offer's facility and need, each term of its score, and its probability (principle 8)".

Step 2: In src/render/SPEC.md, replace "The box is shaded from blue when the guest is sated to pink when it is hungry." with "The box is shaded from blue when every need of the guest is at 0 to pink when its most pressing need is at 1."

### Task 3: Change the public guest interface

Files:
- Modify: `src/sim/guests/guests.h`
- Modify: `src/sim/guests/guests.cpp:145,213,243,752-765`
- Modify: `src/legible/inspect.cpp:60`
- Modify: `tests/sim/park_world_checks_test.cpp:142,277,522`

Step 1: In guests.h, rename GuestActivity::HeadingToShop to HeadingToFacility, and its name "heading-to-shop" to "heading-to-facility". Update the comment to "heading to a shop, bench, or toilet block".

Step 2: After GuestActivity, add:

    // A need a guest has, which one kind of offer answers: hunger by meals, thirst by drinks,
    // tiredness by rests, and the bladder by toilet uses.
    enum class GuestNeed : uint8_t { Hunger, Thirst, Tiredness, Bladder };

    constexpr std::array<std::string_view, 4> enumNames(GuestNeed /*value*/) {
      return {"hunger", "thirst", "tiredness", "bladder"};
    }

    // Every need, in the order a guest scores a facility's offers.
    inline constexpr std::array<GuestNeed, 4> NEEDS{GuestNeed::Hunger, GuestNeed::Thirst,
                                                    GuestNeed::Tiredness, GuestNeed::Bladder};

Step 3: In ChoiceOption, rename Shop to Facility and add `GuestNeed Need = GuestNeed::Hunger;` after it. Its comment becomes "One option of a choice: its kind, the facility and need for an offer, its terms, its score, and the probability the softmax gave it. Carrying on and heading home have no terms."

Step 4: In GuestRecord, add `double Thirst = 0.0;`, `double Tiredness = 0.0;`, and `double Bladder = 0.0;` after Hunger, and `GuestNeed TargetNeed = GuestNeed::Hunger;` after Target. Its comment becomes "...the ground position it stands on, its four needs, the tick its stay ends, the facility it is heading to or waiting at and the need it goes there for, and the meals it has eaten and the last of them."

Step 5: After STARTING_HUNGER_MAX, add:

    // A guest's thirst rises by a rate per tick drawn from these, so from 0 to 1 in 33 to 67 game
    // minutes.
    inline constexpr double THIRST_RATE_MIN = 1.0 / 2000.0;
    inline constexpr double THIRST_RATE_MAX = 1.0 / 1000.0;
    // A guest arrives with a thirst drawn from 0 up to this.
    inline constexpr double STARTING_THIRST_MAX = 0.4;
    // A guest's tiredness rises by a rate per meter walked drawn from these, so from 0 to 1 in 100
    // to 150 m. It arrives with none.
    inline constexpr double TIREDNESS_RATE_MIN = 1.0 / 150.0;
    inline constexpr double TIREDNESS_RATE_MAX = 1.0 / 100.0;
    // A guest arrives with a bladder drawn from 0 up to this.
    inline constexpr double STARTING_BLADDER_MAX = 0.2;
    // How much each meal and each drink a guest takes fills its bladder.
    inline constexpr double BLADDER_PER_MEAL = 0.2;
    inline constexpr double BLADDER_PER_DRINK = 0.35;

Step 6: Rename HUNGER_CURVE to NEED_CURVE, with the comment "How much an offer's relief matters at each level of the need it answers." Rename hungerCurve(double hunger) to needCurve(double level), with the comment "The need curve's value at the level, clamped to [0, 1]."

Step 7: After EATEN_CAUSE, add `DRUNK_CAUSE = "drunk"`, `RESTED_CAUSE = "rested"`, and `USED_CAUSE = "used"` as inline constexpr std::string_view, and change the comment above the causes to "The causes a guest consumes its returned visit and each serving with."

Step 8: Make the code compile with the renames. In guests.cpp, rename the uses of HeadingToShop, HUNGER_CURVE, hungerCurve, and option.Shop. In src/legible/inspect.cpp:60, use option.Facility. In tests/sim/park_world_checks_test.cpp, replace GuestActivity::HeadingToShop with GuestActivity::HeadingToFacility at lines 142, 277, and 522, and leave the helper names as they are.

Step 9: Build.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 4: Add the new needs to the guest's state

Files:
- Modify: `src/sim/guests/internal/guest.h`

Step 1: In Guest, after HungerRate, add `double Thirst = 0.0;`, `double ThirstRate = 0.0;`, `double Tiredness = 0.0;`, `double TirednessRate = 0.0;`, and `double Bladder = 0.0;`. After Target, add `GuestNeed TargetNeed = GuestNeed::Hunger;`. Rename MealRelief to ServingRelief. Update the comment: "...how hungry it is and how fast that rises, how thirsty it is and how fast that rises, how tired it is and how fast that rises with each meter walked, how full its bladder is, how fast it walks..., the facility it is heading to or waiting at and the need it goes there for, the relief of the offer it last picked...".

Step 2: In visitFields, after "hunger-rate", add "thirst", "thirst-rate", "tiredness", "tiredness-rate", and "bladder", in that order. After "target", add "target-need". Rename "meal-relief" to "serving-relief" and its field to ServingRelief.

Step 3: Rename MealRelief to ServingRelief in guests.cpp, and build tpj_sim.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 5: Draw the new needs when a guest is added

Files:
- Modify: `src/sim/guests/guests.cpp:730-750` (addGuest)

Step 1: After the hunger draws, add, each with guestDraw on the guest's key:

- ThirstRate from THIRST_RATE_MIN to THIRST_RATE_MAX, purpose "guest-thirst-rate", as HungerRate is drawn.
- Thirst from 0 to STARTING_THIRST_MAX, purpose "guest-starting-thirst".
- TirednessRate from TIREDNESS_RATE_MIN to TIREDNESS_RATE_MAX, purpose "guest-tiredness-rate".
- Bladder from 0 to STARTING_BLADDER_MAX, purpose "guest-starting-bladder".

Leave every existing draw and its purpose as it is, so the existing draws give the same values.

### Task 6: Add each need's kinds

Files:
- Modify: `src/sim/guests/guests.cpp` (anonymous namespace, before suppliedOffer)

Step 1: Include "sim/operations/facilities.h". Add a traits template `template <GuestNeed N> struct NeedKinds;` with one specialization for each need, each giving `using Offer`, `using Visits`, `using Servings`, `static constexpr std::string_view Cause`, and `static constexpr double BladderFill`:

- Hunger: FoodOffer, GuestVisits, Meals, EATEN_CAUSE, BLADDER_PER_MEAL.
- Thirst: DrinkOffer, DrinkVisits, Drinks, DRUNK_CAUSE, BLADDER_PER_DRINK.
- Tiredness: RestOffer, RestVisits, Rests, RESTED_CAUSE, 0.0.
- Bladder: ToiletOffer, ToiletVisits, ToiletUses, USED_CAUSE, 0.0.

Step 2: Add forEachNeedAt and forEachNeed, which call the action's operator()<N>() for each need of NEEDS in order, written as forEachGoodAt and forEachGood are in src/sim/operations/operations.cpp:116-124.

Step 3: Add `double &needLevel(Guest &guest, GuestNeed need)` and a const overload, returning Hunger, Thirst, Tiredness, or Bladder by a switch.

Step 4: Make suppliedOffer a template over the offer field: `template <typename Offer> std::optional<OfferEntry> suppliedOffer(...)`, reading `sourceEntryAtNode<Offer>`. Add `std::optional<OfferEntry> needOffer(const World &, const Network &, EntityKey source, GuestNeed need)`, which walks forEachNeed and returns suppliedOffer<typename NeedKinds<N>::Offer> for the need that matches. Update suppliedOffer's comment to name "the offer field's" entry instead of food-offer's.

### Task 7: Score every need's offers

Files:
- Modify: `src/sim/guests/guests.cpp:135-260` (ScoredOptions, scoreOptions, choose, headingOf)

Step 1: In scoreOptions, for each route entry in order, walk the needs with forEachNeed. For each need whose suppliedOffer at the source gives an entry, push an Offer option with Facility the source and Need the need. Its Relief term is RELIEF_WEIGHT times needCurve of the guest's level of that need times the offer's relief. Its Distance and Wait are as today. Its Commitment is COMMITMENT_BONUS when the source is the guest's Target and the need is its TargetNeed. Keep the offer's relief in Reliefs as today. Carrying on and heading home stay as they are.

Step 2: In choose, when the guest picks an offer, set TargetNeed to the option's need and ServingRelief to its relief, as MealRelief is set today.

Step 3: In headingOf, drop the target when needOffer for the guest's TargetNeed at its Target gives none, instead of suppliedOffer's food check.

Step 4: Build tpj_sim.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 8: Visit for the chosen need and take its serving

Files:
- Modify: `src/sim/guests/guests.cpp:223-228` (sendVisit) and `:502-523` (awaitVisit)

Step 1: sendVisit takes the need, and walks forEachNeed to create and send one unit of that need's Visits, as it sends GuestVisits today. walk passes the guest's TargetNeed.

Step 2: awaitVisit walks forEachNeed. For the guest's TargetNeed only, it reads the visits held of that need's Visits kind. If there are none, the guest is still waiting. Otherwise it consumes them with FINISHED_CAUSE, then takes the servings held of that need's Servings kind. If there are any, it consumes them with the need's Cause, lowers that need by ServingRelief to no less than 0, and fills the bladder by BladderFill times the servings, to no more than 1. For hunger only, it also counts the meal and records LastMeal from the hunger before and after, as today. Then it stops waiting as today.

Step 3: Update both functions' comments to name visits and servings of the chosen need.

### Task 9: Raise thirst with time and tiredness with walking

Files:
- Modify: `src/sim/guests/guests.cpp:530-562` (stepGuests)

Step 1: Where hunger rises, also raise thirst by ThirstRate, to no more than 1.

Step 2: After walk, for a guest that didn't leave, raise tiredness by TirednessRate times the straight-line meters between where it stood before walking and where it stands now, to no more than 1. A waiting guest doesn't walk, so it doesn't tire. Update stepGuests' comment to say each guest gets hungrier and thirstier, and tires with the ground it walks.

Step 3: Build tpj_sim.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 10: Publish the needs in the record

Files:
- Modify: `src/sim/guests/guests.cpp:696-713` (guestRecord)

Step 1: Set the record's Thirst, Tiredness, Bladder, and TargetNeed from the state.

### Task 11: Show the needs in the Inspector

Files:
- Modify: `src/legible/inspect.h` (ChoiceRow)
- Modify: `src/legible/inspect.cpp:35-73` (guestRows, choiceRows)
- Modify: `src/app/ui/inspector_window.cpp:26-46` (drawChoices)

Step 1: In ChoiceRow, add `std::string Need;` after Option, and update its comment to say an offer has its need.

Step 2: In inspect.cpp, add a helper that gives a box's kind by displayName and its key, such as "bench 5" or "toilet block 6", from parkBoxes, and "box <key>" when no box has the key.

Step 3: In guestRows, replace the Hunger row with four rows, "Hunger", "Thirst", "Tiredness", and "Bladder", each with two decimals. The Target row becomes "none" or the helper's text followed by " for " and the target need's display name, such as "bench 5 for tiredness".

Step 4: choiceRows takes the world. For an offer, Option is the helper's text for the option's facility, and Need is the option's need's display name. Other options leave Need empty.

Step 5: In drawChoices, make the table 8 columns, with the headings "Option", "Need", "Relief", "Distance", "Wait", "Commitment", "Score", "Chance", and draw choice.Need second.

Step 6: Build the app.

Run: `cmake.exe --build --preset windows-debug --target ThemeParkJones 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 12: Shade guests by their most pressing need

Files:
- Modify: `src/render/guest_mesh.h`
- Modify: `src/render/guest_mesh.cpp`

Step 1: Rename GUEST_HUNGRY_COLOR to GUEST_DISTRESSED_COLOR. guestColor's parameter becomes `need`, the guest's most pressing need, and its comment "The sated color moved toward the distressed one by the need, clamped to [0, 1]." appendGuest's parameter becomes `need`, with the comment "Adds a guest's box standing at the point, in its most pressing need's color." Update the size constants' comment to "colored from sated to distressed".

Step 2: In buildGuestMesh, color each guest by the greatest of the record's Hunger, Thirst, Tiredness, and Bladder.

Step 3: Build the app.

Run: `cmake.exe --build --preset windows-debug --target ThemeParkJones 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 13: Remake the parks and check the food-only ones simulate as before

Files:
- Modify: `tests/parks/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`, `stocked.park`, `stress/full.park`

Step 1: Remake the slice parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios > /dev/null 2>&1; build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park; echo $?`
Expected: `0`

Step 2: Remake the full park. It takes about four minutes.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench > /dev/null 2>&1; build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

Step 3: Undo the guest lines' changes in text, and compare each park but stocked.park with its checked-in file.

Run: `for f in fed warm cut warm-main-street stress/full; do diff -q <(git show HEAD:tests/parks/$f.park) <(sed -E 's/ thirst=[^ ]+ thirst-rate=[^ ]+ tiredness=[^ ]+ tiredness-rate=[^ ]+ bladder=[^ ]+//; s/ target-need=hunger serving-relief=/ meal-relief=/; s/activity=heading-to-facility/activity=heading-to-shop/' tests/parks/$f.park) > /dev/null && echo "$f same"; done`
Expected: `fed same`, `warm same`, `cut same`, `warm-main-street same`, `stress/full same`

If a park differs, a choice or draw changed in a park with no new offers. Treat it as a deviation.

### Task 14: Rewrite the play parks' guest lines

Files:
- Modify: `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

Step 1: Add the new fields to each guest line, with no thirst, tiredness, or bladder and fixed mid-range rates, and rename the changed field and activity.

Run: `sed -i -E 's/( hunger-rate=[^ ]+) speed=/\1 thirst=0 thirst-rate=0.00075 tiredness=0 tiredness-rate=0.008 bladder=0 speed=/; s/( target=[0-9]+) meal-relief=/\1 target-need=hunger serving-relief=/; s/activity=heading-to-shop/activity=heading-to-facility/' parks/busted-path.park parks/jank.park parks/radii.park; grep -c "tiredness-rate=" parks/busted-path.park parks/jank.park parks/radii.park`
Expected: `parks/busted-path.park:35`, `parks/jank.park:37`, `parks/radii.park:63`, the number of guest lines in each.

Step 2: Load and step each.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 1 parks/busted-path.park parks/jank.park parks/radii.park > /dev/null; echo $?`
Expected: `0`

### Task 15: Check the needs on stocked.park

Files:
- Create: `build/predict-check/needs_check.cpp` (a scratch program, not committed)

Step 1: Write a program that loads tests/parks/stocked.park with makeParkSchema, resolves it, and steps it 9000 cycles. Before each step it keeps every guest's guestRecord, and after the step it compares each guest present both times. It fails, naming the guest, the tick, and the record, when any of these breaks:

- each of the four needs is within 0 to 1;
- if a need fell, the guest was waiting before the step, the need is the TargetNeed of its record before the step, and no other need fell;
- if the guest was waiting before and after the step, its tiredness didn't rise;
- hunger and thirst each rose, unless that need fell or was 1 already;
- if hunger fell, the bladder became the lesser of 1 and its value before plus BLADDER_PER_MEAL;
- if thirst fell, the bladder became the lesser of 1 and its value before plus BLADDER_PER_DRINK;
- if neither hunger nor thirst fell, the bladder didn't rise.

It also counts the steps in which a guest that wasn't waiting before or after had its tiredness rise. That count must be above 0 at the end, or the check fails.

After the steps it prints `meals <n>`, `drinks <n>`, `rests <n>`, and `toilet uses <n>`, from unitsConsumed of Meals with EATEN_CAUSE, Drinks with DRUNK_CAUSE, Rests with RESTED_CAUSE, and ToiletUses with USED_CAUSE, then `needs check passed` if nothing failed.

Step 2: Build and run it.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O0 needs_check.cpp ../windows-debug/src/sim/libtpj_sim.a -o needs_check.exe && ./needs_check.exe ../../tests/parks/stocked.park`
Expected: the four count lines, each count above 0, then `needs check passed`.

### Task 16: Run the full checks

Files: none.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

Then play: open tests/parks/stocked.park in the app, let guests arrive, and click guests heading to the drink shop, the bench, and the toilet block. Each Inspector shows four needs, a target with its need, and options with their facility and need. Guests grow pinker as any need rises.

### Task 17: Commit

Files: everything above, and plans/believable-guests/a-day-out/MILESTONE.md.

Step 1: In MILESTONE.md's Tuning values, after "These are starting values, adjusted in day-parks by watching day.park through a day.", add "four-needs keeps hunger's rate and scales thirst and tiredness to today's stays of 1 to 2 game hours. going-home sets these values when stays become game hours."

Step 2: Commit the feature's paths by pathspec with the commit-hygiene skill, subject "Sim: Give guests thirst, tiredness, and a bladder".
