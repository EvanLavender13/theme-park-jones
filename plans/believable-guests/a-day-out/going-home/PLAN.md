# Implementation Plan: Going Home

## Goal

Replace the fixed stay with a scored going-home option, choose at closing, tally departures by reason, record arrival ticks, and set the day-scale need rates.

## Approach

stepGuests works out, once per step, the park's entrances, the closing term, and whether the clock has just passed the closing or the opening time, and passes them down to every choice. scoreOptions builds going home's option from its base, five reason terms, and commitment, and choose records the reason when it is picked. walk reports whether a guest walked out of an entrance, and stepGuests adds those guests to the departure tally, a saved component on a derived entity that a resolver creates, after starting it again when the park opens.

## Placement

Decision 0027 places each behavior this feature adds:

- Going home's score, its reason, its commitment bonus, and the choice at closing: guests, src/sim/guests/guests.cpp (scoreOptions, choose, walk). The guests module owns every guest choice.
- The departure tally: guests, a new component in src/sim/guests/internal/departures.h, read through departureTally in src/sim/guests/guests.h. Only guests leave, so only guests count leaving.
- The arrival tick and the recorded reason: guests, the Guest component in src/sim/guests/internal/guest.h, published through GuestRecord.
- Whether hours are open at a second: park, isOpenAt in src/sim/park/intent.h. The park module defines what OpeningHours mean, including hours past midnight and all-day hours. Guests read it now and arrival-curve will.
- The arrival time, reason, and going-home terms on screen: legible, src/legible/inspect.cpp builds them, and src/app/ui/inspector_window.cpp draws the new table. They already own the Inspector's rows and choice table.
- The full park's guests without a stay: scenarios, src/scenarios/full_park.cpp, which owns the stress park.

No change touches src/app/main.cpp or src/scenarios/main.cpp.

## Tasks

### Task 1: Record the full park's baseline timings

Files: none in the repository.

Step 1: Time the full park with the windows-release build of main as it stands (0eb7cef).

Run: `cmake.exe --build --preset windows-release --target tpj_bench > /dev/null 2>&1; build/windows-release/tpj_bench.exe tests/parks/stress/full.park > build/predict-check/going_home_bench_before.txt; echo $?`
Expected: `0`. The file holds a park line and one stage line for each stage.

### Task 2: Update the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md`

Step 1: In Boundary's first paragraph, change "the ground guests walk on and the resolver that derives it," to "the ground guests walk on and the resolver that derives it, the departure tally and the resolver that creates it,". Change the bullet "parkEntrances (decision 0025);" to "parkEntrances, and the opening hours through parkHours and isOpenAt (decision 0025);". Add a bullet after it: "the park's clock, parkTime;". Change "Other modules call guests.h (parkGuests, addGuest, guestRecord, and guestOptions)" to "Other modules call guests.h (parkGuests, addGuest, guestRecord, guestOptions, and departureTally)".

Step 2: In Arriving and leaving, replace the first paragraph with:

    Guests arrive at regular intervals at each entrance connected to the guest network, one per entrance at a time. If an entrance has no connection, it admits no one, and that is a legitimate state (principle 2). A guest leaves the park when it walks out of an entrance heading home. It also leaves when no guest paving is left for it to stand on. A guest that leaves drops its entry in guest presence.

In the second paragraph, change "addGuest(world, place, stayUntil)" to "addGuest(world, place)", and change "A new guest stands at the place's ground position." to "A new guest stands at the place's ground position and records the tick it arrived."

Step 3: In Choosing, change the bullet "heading home, once its stay is over and an entrance is reachable." to "heading home, whenever an entrance is reachable." Change "Once a stay is over, heading home outweighs any offer." to "While the park is closed, heading home outweighs every other option." In the last paragraph, change "and when its visit comes back." to "when its visit comes back, and when the park closes (Going home)."

Step 4: Add this section after Choosing:

    ## Going home

    Heading home's score is the sum of a base, five reason terms, and a commitment term, added in this order:

    - the base, HOME_BASE, which is below 0, so a guest whose needs are met rarely heads home before closing;
    - hunger, thirst, tiredness, and the toilet: each is HOME_NEED_WEIGHT times the need curve at its need's level, the bladder's for the toilet;
    - closing: 0 until CLOSING_LEAD game seconds before the park closes, then rising linearly to CLOSING_SCORE at closing, and CLOSING_SCORE while the park is closed. If the park is open all day, it is always 0;
    - commitment: COMMITMENT_BONUS while the guest is heading home, and otherwise 0.

    For each need, an offer answering it has a larger relief term than the need's term in heading home, so a guest prefers a facility close by to heading home for one need. When several needs are high at once, their terms add up, and heading home can win even with facilities in reach. If nothing in reach answers a need, its term keeps growing as the need does.

    Each time a guest picks heading home, it records the pick's reason: the largest of its five reason terms, or, when several are largest, the first of them in the order hunger, thirst, tiredness, toilet, closing. When the guest walks out of an entrance, the reason it last recorded is the reason it left.

    When the clock passes the closing time, every guest that is walking chooses in that tick. A guest heading home keeps making its choice for each node of its route, so it can still turn aside to a facility it needs on the way. Whether the park is open is judged by its current hours, through isOpenAt.

    ## Departures

    departureTally(world) gives the departure tally: how many guests have walked out of an entrance since the clock last passed the opening time, by the reason they left. When the clock passes the opening time, the tally starts again from 0. If the park is open all day, it never does. A guest that leaves because no guest paving is left isn't counted. The tally is saved state on an entity of its own, which resolving the park creates when it is missing. Before the park is first resolved, departureTally gives 0 for every reason.

Step 5: In Inspection, change "its four needs, stay, target and the need it heads there for, meals eaten, and last meal." to "its four needs, the tick it arrived, its target and the need it heads there for, the reason it last recorded for heading home, meals eaten, and last meal." Replace the guestOptions paragraph's second and third sentences with:

    Each option has its kind, the facility and need of an offer, its terms, heading home's reason, its Score, and its Probability. An offer's terms are Relief, Distance, Wait, and Commitment, added in that order. Heading home's are Base, a term in Reasons for each of HOME_REASONS, and Commitment, added as Going home gives. Every other term is 0. The Score is exactly the sum of the terms.

### Task 3: Update the park, legible, and app specs

Files:
- Modify: `src/sim/park/SPEC.md:9,22`
- Modify: `src/legible/SPEC.md:58-59`
- Modify: `src/app/SPEC.md:73`

Step 1: In src/sim/park/SPEC.md line 9, change "Other modules read the opening hours through parkHours, which returns the public value OpeningHours." to "Other modules read the opening hours through parkHours, which returns the public value OpeningHours, and ask whether hours are open at a second of the day through isOpenAt." At the end of the line-22 bullet, add: "isOpenAt(hours, second) says whether a park with the hours is open at that second of the day: from the opening second up to but not including the closing second, past midnight when the hours run past it, and at every second when the park is open all day."

Step 2: In src/legible/SPEC.md, change the first guest bullet to "its activity, its four needs, when it arrived, its target and the need it heads there for, or while it heads home the reason it heads home for, meals eaten, and last meal". Change the second to "a table of every option the guest would weigh if it chose now, with each offer's facility and need, heading home's reason, each term of an offer's score, each option's score, and its probability, and below it a table of heading home's base and reason terms when heading home is an option (principle 8)".

Step 3: In src/app/SPEC.md line 73, change "with their chances (legible/SPEC.md, Inspectors)" to "with their chances, and heading home's terms (legible/SPEC.md, Inspectors)".

### Task 4: Change the public interfaces

Files:
- Modify: `src/sim/guests/guests.h`
- Modify: `src/sim/park/intent.h:188-189`, `src/sim/park/intent.cpp`
- Modify: `src/sim/guests/guests.cpp` (stubs and the addGuest signature)
- Modify: `src/legible/inspect.cpp:56-58` (the Stay row)
- Modify: `src/scenarios/full_park.h`, `src/scenarios/full_park.cpp:139-153,163`
- Modify: `tests/integration/early_turns_test.cpp:94`

Step 1: In guests.h, after NEEDS, add `enum class HomeReason : uint8_t { Hunger, Thirst, Tiredness, Toilet, Closing };` with a comment saying it is why a guest heads home, one of its needs or closing time, the needs in NEEDS order with the bladder as the toilet. Add its enumNames {"hunger", "thirst", "tiredness", "toilet", "closing"}, and `HOME_REASONS`, the five in that order.

Step 2: In ChoiceOption, after Commitment, add `double Base = 0.0;`, `std::array<double, HOME_REASONS.size()> Reasons{};`, and `HomeReason Reason = HomeReason::Hunger;`. Change its comment's last sentence to say that heading home's terms are Base, Reasons, and Commitment, with its reason the first largest of Reasons, and that carrying on has no terms.

Step 3: In GuestRecord, replace `uint64_t StayUntil = 0;` with `uint64_t Arrived = 0;`, and add `HomeReason Reason = HomeReason::Hunger;` after TargetNeed. Change its comment to say "the tick it arrived" instead of "the tick its stay ends", and add "the reason it last recorded for heading home".

Step 4: Remove STAY_MIN, STAY_MAX, their comment, and HEAD_HOME_SCORE. Set HUNGER_RATE_MIN to 1.0 / 10800.0 and HUNGER_RATE_MAX to 1.0 / 7200.0, with the comment "so from 0 to 1 in 4 to 6 game hours". Set THIRST_RATE_MIN to 1.0 / 8100.0 and THIRST_RATE_MAX to 1.0 / 5400.0, "so from 0 to 1 in 3 to 4.5 game hours". Set TIREDNESS_RATE_MIN to 1.0 / 600.0 and TIREDNESS_RATE_MAX to 1.0 / 400.0, "so from 0 to 1 in 400 to 600 m". Change the comment above CARRY_ON_SCORE to "The score of carrying on." Add after it:

- `HOME_BASE = -2.0` and `HOME_NEED_WEIGHT = 1.5`, commented as heading home's base, and its weight per unit of each need's curve.
- `uint64_t CLOSING_LEAD = 3600`, commented as the game seconds before closing over which heading home's closing term rises.
- `CLOSING_SCORE = 10.0`, commented as the closing term from closing until the park opens.

Step 5: After GuestMeal's visitFields, add `struct DepartureTally { std::array<uint64_t, HOME_REASONS.size()> ByReason{}; bool operator==(const DepartureTally &) const = default; };`, commented as how many guests have walked out of an entrance since the park last opened, by the reason they left, in HOME_REASONS order. Change addGuest to `EntityKey addGuest(World &world, const Place &place);`, and its comment to "Creates a guest standing at the place, arriving at the world's tick, with its needs, rates, speed, and share drawn on its key, and gives its key." keeping the throw sentence. After guestOptions, declare `DepartureTally departureTally(const World &world);`, commented "The park's departure tally, or 0 for every reason before the park is first resolved."

Step 6: In intent.h after parkHours, declare `bool isOpenAt(const OpeningHours &hours, uint64_t second);`, commented "Whether a park with the hours is open at the second of the day: from Opening up to but not including Closing, past midnight when Closing is at or before Opening, and always when the two are equal." In intent.cpp, define it: true when Opening equals Closing, and otherwise when `(second + DAY_SECONDS - Opening) % DAY_SECONDS < (Closing + DAY_SECONDS - Opening) % DAY_SECONDS`. Include sim/clock.h for DAY_SECONDS.

Step 7: In guests.cpp, give addGuest the new signature and drop its StayUntil line for now, and drop the StayUntil line of guestRecord. In src/legible/inspect.cpp, drop the Stay row; Task 12 adds the Arrived row. Define departureTally to return `DepartureTally{}`. In scoreOptions, make the head-home condition `homeEntry(routes, entrances) != nullptr` and its score CLOSING_SCORE, so it builds until Task 8. In admitGuests, drop the stay draw and call `addGuest(world, network.nodePlace(anchored.front()))`.

Step 8: In full_park.h, remove FULL_PARK_STAY and its part of the comments: the first comment ends "and the ticks it steps before it is saved", and makeFullPark's says "2,000 added guests, stepped warmTicks ticks into play". In full_park.cpp, give placeGuests no stay parameter, drop "staying until the tick given." from its comment, call addGuest with the place alone, and call `placeGuests(world)` in makeFullPark.

Step 9: In early_turns_test.cpp line 94, call `addGuest(unchanged, middle)`. Task 13 changes the rest of the test.

Step 10: Build.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 5: Hold the arrival tick and the reason in the guest's state

Files:
- Modify: `src/sim/guests/internal/guest.h`
- Modify: `src/sim/guests/guests.cpp` (addGuest)

Step 1: In Guest, replace `uint64_t StayUntil = 0;` with `uint64_t Arrived = 0;`, and add `HomeReason Reason = HomeReason::Hunger;` after ServingRelief. In visitFields, replace `visitor.field("stay-until", guest.StayUntil);` with `visitor.field("arrived", guest.Arrived);`, and add `visitor.field("home-reason", guest.Reason);` after serving-relief. Change the struct's comment: "the tick its stay ends" becomes "the tick it arrived", and after "the relief of the offer it last picked" add "the reason it last recorded for heading home".

Step 2: In addGuest, set `guest.Arrived = world.Tick;`. In guestRecord, set `record.Arrived = state->Arrived;`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 6: Add the departure tally's component

Files:
- Create: `src/sim/guests/internal/departures.h`
- Modify: `src/sim/guests/guests.cpp` (addGuests, departureTally)

Step 1: Create departures.h with include guard TPJ_SIM_GUESTS_INTERNAL_DEPARTURES_H. It declares `struct Departures { std::array<uint64_t, HOME_REASONS.size()> ByReason{}; };`, commented as state on the departure tally's entity: how many guests have walked out of an entrance since the park last opened, by reason. Its visitFields visits each count as a field named by enumNames(HomeReason) for that index: hunger, thirst, tiredness, toilet, closing.

Step 2: In guests.cpp, add the constant `DEPARTURES_PURPOSE = hashName("departure-tally")` in the anonymous namespace. Add a resolver, `void ensureDepartures(World &world)`, that calls `world.getOrEmplace<Departures>(world.createDerivedEntity(NULL_KEY, DEPARTURES_PURPOSE, 0))`, as emptyGroundEntries in src/sim/medium/ground_field.h does for its field. Only a resolver may create the entity: a save holds no entity's derived origin, and resolving a loaded park sets it again only through createDerivedEntity, so an entity a system created would load back unequal. In addGuests, register `schema.addComponent<Departures>("departure-tally", DataKind::State);` right after the guest component, and `schema.addResolver("departure-tally", &ensureDepartures);` after addGuestGround. Define departureTally: if `world.keysWith<Departures>()` is empty, return `DepartureTally{}`; otherwise return the first one's ByReason in a DepartureTally.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 7: Work out the step's moment once

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: In the anonymous namespace, add `struct ParkMoment { std::vector<EntityKey> Entrances; double Closing = 0.0; bool Closes = false; bool Opens = false; };`, commented as what every guest's step reads from the park and the clock: the entrance keys ascending, heading home's closing term, and whether the clock has just passed the closing or the opening time.

Step 2: Add `double closingTerm(const OpeningHours &hours, uint64_t second)`: 0 when Opening equals Closing; CLOSING_SCORE when `!isOpenAt(hours, second)`; otherwise, with `left = (hours.Closing + DAY_SECONDS - second) % DAY_SECONDS`, 0 when left is at least CLOSING_LEAD, and else `CLOSING_SCORE * static_cast<double>(CLOSING_LEAD - left) / static_cast<double>(CLOSING_LEAD)`.

Step 3: Add `ParkMoment parkMoment(const World &world)`: Entrances from entranceKeys, Closing from closingTerm(parkHours(world), parkTime(world.Tick).Second). When world.Tick is above 0, with `now` and `before` the isOpenAt of this tick's and the previous tick's second under parkHours, Closes is `before && !now` and Opens is `!before && now`. Include sim/clock.h.

Step 4: Replace the `const std::vector<EntityKey> &entrances` parameter of scoreOptions, choose, chooseFor, and walk with `const ParkMoment &moment`, and read `moment.Entrances` where they used entrances. headingOf and traceGuestRoute keep taking the entrances, passed `moment.Entrances`. In stepGuests, build the moment with parkMoment in place of entranceKeys. In guestOptions, pass `parkMoment(world)`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 8: Score going home

Files:
- Modify: `src/sim/guests/guests.cpp` (scoreOptions)

Step 1: Replace the head-home block of scoreOptions. When homeEntry finds an entrance, build the option with Kind HeadHome, Base HOME_BASE, and for each need N, through forEachNeed, `Reasons[static_cast<size_t>(N)] = HOME_NEED_WEIGHT * needCurve(needLevel(guest, N))`. HomeReason lists the needs in NEEDS order, so a need's index is its reason's. Set `Reasons[static_cast<size_t>(HomeReason::Closing)] = moment.Closing`, and Commitment to COMMITMENT_BONUS when `guest.Activity == GuestActivity::HeadingHome`, else 0.

Step 2: Set Score to Base, then add each of Reasons in order, then Commitment. Set Reason to the first index holding the largest term: start at index 0, and move to a later index only when its term is strictly greater than the term at the current one, so ties go to the earlier reason.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 9: Record the reason and choose at closing

Files:
- Modify: `src/sim/guests/guests.cpp` (choose, walk)

Step 1: In choose, when the picked option is HeadHome, also set `guest.Reason = option.Reason`.

Step 2: In walk, start `progress.MustChoose` at `moment.Closes`, so a walking guest chooses in the tick the park closes, once, before it walks. A waiting guest doesn't walk, so it chooses when its visit comes back, as today.

Step 3: In walk's loop, move the block that chooses when `!progress.Chosen && progress.MustChoose` to just after MustChoose is updated from the heading, before the checks for reaching the target or the entrance. Then a guest at its facility's door or its entrance in the closing tick still chooses first. Today only a dropped heading sets MustChoose, and a guest whose heading was dropped has neither a target nor a way home, so the move changes nothing else.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 10: Tally departures

Files:
- Modify: `src/sim/guests/guests.cpp` (walk, stepGuests)

Step 1: Add `enum class WalkEnd : uint8_t { Stays, WalkedOut, Lost };`, commented as how a guest's walk in a step ended: still in the park, out of an entrance heading home, or lost because its place no longer resolves. walk returns it: Lost where it returned false for an unresolved place, WalkedOut where it returned false at the entrance, and Stays where it returned true. Change walk's comment's last sentence to match.

Step 2: In stepGuests, keep a local `std::array<uint64_t, HOME_REASONS.size()> walkedOut{}`. When walk gives WalkedOut, add one at the guest's Reason index, and push the guest to leaving. When it gives Lost, push it to leaving only.

Step 3: After the loop, before dropping the leavers, get the tally with `world.get<Departures>(deriveKey(NULL_KEY, DEPARTURES_PURPOSE, 0))`; the resolver has made it before any step. If `moment.Opens`, set its ByReason to zeros. Then add walkedOut to it. Add to stepGuests' comment: guests that walk out of an entrance are counted in the departure tally by their reason, which starts again from 0 in the tick the park opens.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 11: Publish the reason

Files:
- Modify: `src/sim/guests/guests.cpp` (guestRecord)

Step 1: In guestRecord, set `record.Reason = state->Reason;` after TargetNeed.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 12: Show the arrival, the reason, and going home's terms

Files:
- Modify: `src/legible/inspect.h`, `src/legible/inspect.cpp`
- Modify: `src/app/ui/inspector_window.cpp`

Step 1: In inspect.h, add `std::vector<InspectorRow> HomeTerms;` to Inspection after Choices, and add to its comment "and heading home's base and reason terms when heading home is an option". In ChoiceRow's comment, say carrying on has no need and empty terms, and heading home shows its reason as its need and only its commitment among the terms.

Step 2: In inspect.cpp guestRows, replace the Stay row with an Arrived row after Bladder, showing parkTime(record.Arrived) as `std::format("day {}, {}:{:02}", time.Day, time.Second / 3600, (time.Second % 3600) / 60)`, such as "day 1, 9:03". Make the Target row show "home for " plus displayName(record.Reason) when the activity is HeadingHome, and otherwise as today. Include sim/clock.h.

Step 3: In choiceRows, for a HeadHome option set Need to displayName(option.Reason) and Commitment to twoDecimals(option.Commitment). Add a function homeTerms that, for the first HeadHome option, gives a Base row and one row per reason, labelled capitalized(displayName(reason)), each value twoDecimals of its term, and gives nothing when no option is HeadHome. inspectSubject sets HomeTerms from it next to Choices.

Step 4: In inspector_window.cpp, give drawRows a table id parameter: the rows use "rows", and the new table "home-terms". After drawChoices, when HomeTerms isn't empty, write "Going home" and draw its rows.

Run: `cmake.exe --build --preset windows-debug --target tpj_app 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 13: Close the early-turns test's park

Files:
- Modify: `tests/integration/early_turns_test.cpp:1-4,64,94,105`

Step 1: Before the guest is added, apply a SetOpeningHours of 12:00 to 20:00 (seconds 43200 and 72000) to `unchanged`, requiring isAccepted first. new.park stands at 9:00, and the test steps far less than three game hours, so the park is closed throughout and never passes a closing time. The closing term is then CLOSING_SCORE, so the guest would head home if it chose.

Step 2: Rename the test case to "A point inserted ahead of a wandering guest in a closed park leaves it wandering". In the file's opening comment, change "a wandering guest whose stay is over" to "a wandering guest in a closed park". Change the comment above the Wandering REQUIRE to "The park is closed, so the guest would be heading home if it had chosen."

Run: `cmake.exe --build --preset windows-debug --target tpj_integration_tests > /dev/null 2>&1; build/windows-debug/tpj_integration_tests.exe "*closed park*" | tail -2`
Expected: `All tests passed` with its assertion count.

### Task 14: Rebuild the scenarios

Files: none beyond Task 4.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios tpj_bench 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 15: Remake the parks

Files:
- Modify: `tests/parks/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`, `stocked.park`, `stress/full.park`
- Modify: `src/scenarios/slice_parks.h` and `src/scenarios/SPEC.md:41-43`, as Steps 3 and 4 say

Step 1: Remake the slice parks.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park; echo $?`
Expected: `0`

Step 2: Remake the full park with the release build, which writes the same text in about 18 s.

Run: `cmake.exe --build --preset windows-release --target tpj_bench > /dev/null 2>&1; build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

Step 3: Run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests > /dev/null 2>&1; build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]" | tail -2`
Expected: `All tests passed`. Slower hunger can leave warm.park with no guest waiting at its shop or heading to it, and then the chosen edits' REQUIREs fail. If so, raise WARM_TICKS in src/scenarios/slice_parks.h by 300, update its comment's game seconds, and repeat Steps 1 and 3 until they pass.

Step 4: Count warm-main-street's guests by activity.

Run: `grep -o "activity=[a-z-]*" tests/parks/warm-main-street.park | sort | uniq -c`
Expected: a count for each activity the park holds. Stays no longer end, so guests may no longer "arrive and leave about equally often". In src/scenarios/SPEC.md line 43 and MAIN_STREET_WARM_TICKS's comment, replace that clause with the activities this printed, such as "long enough that guests fill the park wandering, heading to its shops, and waiting there". Keep the sentence about ARRIVAL_INTERVAL.

### Task 16: Rewrite the play parks' guest lines

Files:
- Modify: `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

Run: `sed -i -E 's/ stay-until=[0-9]+ / arrived=0 /; s/( serving-relief=[^ ]+)/\1 home-reason=hunger/' parks/busted-path.park parks/jank.park parks/radii.park; grep -c "home-reason=" parks/busted-path.park parks/jank.park parks/radii.park`
Expected: `parks/busted-path.park:35`, `parks/jank.park:37`, `parks/radii.park:63`, the number of guest lines in each.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 1 parks/busted-path.park parks/jank.park parks/radii.park > /dev/null; echo $?`
Expected: `0`

### Task 17: Check going home on stocked.park and the full park

Files:
- Create: `build/predict-check/going_home_check.cpp` (scratch, not committed)

Step 1: Write a scratch program, in the shape of build/predict-check/needs_check.cpp, that takes a mode and a park path.

In mode `stocked`, it loads the park, resolves it, and applies SetOpeningHours of 9:00 to 11:00. It steps until 11:30, applies SetOpeningHours of 11:45 to 13:00, and steps until 12:00. Before each step, for every guest, it reads guestRecord and guestOptions and checks:

- There is exactly one HeadHome option.
- Its Base is HOME_BASE, each need's term is HOME_NEED_WEIGHT times needCurve of the record's level, and the closing term equals the closing rule for the time and the hours then set.
- Its Commitment is COMMITMENT_BONUS exactly when the guest is heading home, and 0 otherwise.
- Its Score equals Base plus each term of Reasons in order plus Commitment, compared exactly.
- Its Reason is the first largest term of Reasons.

Across each step, it checks:

- Every guest that appears has Arrived equal to the tick before the step.
- In the step taken at the tick whose time is 11:00, every guest that wasn't waiting before it is heading home after it with Reason closing, or has walked out.
- The number of guests gone after a step equals the tally's total growth over the step. No paving is removed in this run, so every guest that leaves walks out. From the step taken at 11:00 on, all of that growth is under closing. A guest can choose again within the step it walks out in, and a guest that arrives while the park is closed walks out in its first step, so the program doesn't compare reasons before 11:00; it prints its own count by the reasons guests held before each step beside the tally instead.
- In the step taken at the tick whose time is 11:45, the tally afterwards holds only the guests that walked out in that step.

It prints the closing term at 10:00, 10:30, and 11:15, the tally just before 11:45, and the number of guests left at 11:44 that arrived before 11:00, and how many of them are heading home or waiting.

With CLOSING_LEAD at one game hour, the closing term passes carrying on by about 10:15, so every guest has left by 11:00. To check the closing moment with guests inside, the program also opens the park again from 11:45 to 14:00, and at 12:30 sets the closing time to 12:30:02. It checks that moment as it checks 11:00, and at 12:44 that every guest that arrived before 12:30:02 is heading home or waiting. (Deviation, minor: added during implementation.)

In mode `full`, it loads the park, resolves it, steps DEFAULT_TICKS (3600) ticks with the default hours, and prints the departure tally by reason and how many guests are heading home at the end.

Step 2: Build it against the debug library and run both modes.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O0 going_home_check.cpp ../windows-debug/src/sim/libtpj_sim.a -o going_home_check.exe && ./going_home_check.exe stocked ../../tests/parks/stocked.park`
Expected: the closing terms `0`, `5`, and `10`; a tally with closing departures; every guest left at 11:44 that arrived before 11:00 heading home or waiting; then `going home check passed`.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O2 going_home_check.cpp ../windows-release/src/sim/libtpj_sim.a -o going_home_check_release.exe && ./going_home_check_release.exe full ../../tests/parks/stress/full.park` (Deviation, minor: the release library, because debug can't step full.park in reasonable time, and release simulates identically.)
Expected: a departure tally whose total is above 0, and a count of guests heading home above 0. This shows that full.park's guests leave by choice during the runtime report's run.

### Task 18: Time the full park after the change

Files: none in the repository.

Run: `build/windows-release/tpj_bench.exe tests/parks/stress/full.park > build/predict-check/going_home_bench_after.txt; echo $?; paste <(awk '/^stage/{print $2, $6}' build/predict-check/going_home_bench_before.txt) <(awk '/^stage/{print $6}' build/predict-check/going_home_bench_after.txt)`
Expected: `0`, then each stage's name with its median in nanoseconds before and after. Every stage's median stays below one tick, 33,333,333 ns. Report the medians in the feature's report.

### Task 19: Run the full checks

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

Step 2: Play. Open tests/parks/stocked.park in build/windows-debug/ThemeParkJones.exe. Its hours are 10:00 to 22:00, so until 10:00 every arriving guest heads home at once, and the park holds no guests. (Deviation, minor: the remade stocked.park holds no guests for that reason.) Let it run past 10:00. Click guests and see the Arrived row and the Going home table. In the Hours panel, set Closes a quarter hour ahead and apply. Watch the closing term rise in the Going home table, then every walking guest turn for the entrance with Target "home for closing", and the park empty.

### Task 20: Commit

Stage only this feature's paths: the files of Tasks 2 to 16, and plans/believable-guests/a-day-out/going-home/. Commit with the commit-hygiene skill, subject "Sim: Score going home and tally departures by reason".
