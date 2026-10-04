# Feature: Guests Operations Reshaped

## Summary

The guests and operations specs drop their tuning numbers and the formulas copied from their code. Each constant keeps its name, and each formula is replaced by the property it computes. plans/focused-tests/RESEARCH.md lists 33 of their test cases and food_loop_test's integration case as rewrite. This feature adds 12 more entries: the shared fixtures and file-local fixtures that place entrances, boxes, and paths at literal gaps picked against the park's sizes and the connection reach, and four cases whose own code holds such a literal or leans on RETURN_DELAY's value. The test-writer rewrites all 46 in rewrite mode, merged where the entries say, so each asserts its entry's property. The entries then leave the inventory, and test support no remaining test uses is deleted.

## Acceptance criteria

1. The guests', operations', and food loop's tests assert each of these properties once, in place of the cases the inventory lists:
    - addGuest refuses a place that does not resolve on the guest network, changing nothing, the world's next key included. Shown with one such place.
    - Every entrance with a guest connector admits guests alike, and one with none admits none.
    - Each arrival stands at the node place of its own entrance's lowest anchored node, wandering, its record's Position the ground point there. Its StayUntil lies at least STAY_MIN and below STAY_MAX ticks after the tick of the cycle that admitted it, and its Hunger lies between 0 and STARTING_HUNGER_MAX.
    - hungerCurve never falls as hunger rises, and between consecutive points of HUNGER_CURVE it lies between their Ys.
    - softmaxPick sets probabilities that add up to 1 within rounding. An option with a higher Score never has a lower Probability, and options with equal Scores have equal ones. Nothing of an option changes but its Probability.
    - After a guest chooses, its Activity and Target are those of one of the options guestOptions lists where it chose. A run shows offers, carrying on, and heading home each picked, and an offer picked at its shop's anchor, which makes the guest wait.
    - A walking guest covers WALK_STEP of route distance each cycle, however the network is cut into edges. So a guest heading to a shop or home comes nearer its destination by route distance each cycle until it waits at the shop's anchor or leaves at the entrance's. A wanderer on a line with dead ends keeps covering WALK_STEP along it, turning at each end.
    - A guest never keeps heading for a destination it has no route to, and takes heading home up again once a route exists. Shown with a shop whose supply route is cut, a shop that is deleted, and a guest cut off from every entrance past its stay, which wanders on and heads home once a path back is drawn again.
    - Each stretch of hungry footfall with guests on it moves toward their summed hunger: its new value lies between its old value and that sum. A stretch with none never moves away from 0 as it is read later, and never passes it.
    - An edit changes nothing of a guest but its place, and a guest whose carrier survives the edit keeps its ground point.
    - guestOptions gives none exactly when the key holds no guest standing on the guest network.
    - An offer never scores lower when the guest is hungrier, and scores lower when it is farther or its wait is longer. An offer's Score is its Relief, Distance, Wait, and Commitment added in that order. Heading home is listed only at or after the guest's StayUntil, and then outscores every offer.
    - A guest sends one visit, addressed to its Target, only in a cycle in which it starts waiting at that shop's guest anchor.
    - Eating a meal lowers the guest's hunger, never below 0, and counts the meal: the meal is consumed as eaten, MealsEaten rises by 1, and LastMeal holds the tick, the hunger before, and the hunger after, which is the guest's Hunger.
    - An unfed guest's hunger never falls and never exceeds 1. Until it reaches 1, it rises each cycle by the guest's one rate, which lies between HUNGER_RATE_MIN and HUNGER_RATE_MAX.
    - At a node, a wandering guest never takes its way back when another step exists, and takes each other step about equally often: over guests leaving one junction, each step's share lies within a statistical tolerance of an equal share, with the smallest sample that tolerance allows.
    - Each shop box has one food-offer source, with one entry at its guest anchor's node place, or none without a guest anchor. A shop with a nearest depot offers MEAL_RELIEF and says it has meals. A starved one offers OfferEntry{}. Shown after a world's first resolution and after a command that cuts the supply route.
    - shipmentDelay gives a delay of at least 1 and at most the largest uint32_t for every distance, a NaN included, and never a smaller delay for a greater distance.
    - inventoryPosition counts exactly the supplies the shop holds, the supplies heading to it, and its orders heading to or held by a depot box. Shown in one world holding a different number of units of each kind that counts, beside units that do not.
    - A shop orders only at or below REORDER_POINT, and then ORDER_UP_TO less its position, to its nearest depot, with the delay ORDER_DELAY.
    - Serving trades one supply, consumed as served, for one meal, sent to the guest with one of its visits, both with the delay SERVICE_INTERVAL.
    - A shop serves visits in the order they arrived, and visits that arrived at the same swap in guest key order.
    - A starved shop keeps as many visits as the supplies it holds or has coming, and sends every other visit back to its guest with no meal.
    - Cutting a shop's supply route leaves guests hungrier than in the same park uncut. Shown from tests/parks/warm.park, cut in the test by deleting its backstage paths, and run, beside the uncut park, until the uncut park has served a guest the cut one has not.
2. These fixtures and cases, which the inventory did not list, keep their properties, with placements built from the constants by name, never from the dropped values:
    - guest_parks.h's northGate places its entrance with its back on the park's edge from PARK_SIZE and ENTRANCE_SIZE, and gives where the lines leading in from its door start, within CONNECTION_REACH of the door and clear of its face by more than half a guest path's width.
    - legsWorld starts its far leg beyond CONNECTION_REACH of the gate's door.
    - eatingWorld places its gate path's start, its shops, and its depot within reach of the lines they connect to, from boxSize, pathWidth, and CONNECTION_REACH. Its starved shop's back door reaches no backstage line. The spine and the cross stay where they are.
    - line_park.h's shopAt, depotAt, and depotAcross place every door within reach of the line, and depotAcross's door meets the line where depotAt's does.
    - add_guest_test's gatesIntent, arrivals_test's gatesWorld, and walks_test's crossingWorld and hunger case lay their guest paths from northGate's line start.
    - food_offer_test's guestLine runs within reach of every line-park shop's front door.
    - footfall_test's "Hungry footfall holds at most one kept entry in each stretch of the guest network, at the stretch's midpoint" and "A stretch that exactly one footfall entry is carried into holds that entry's value and tick" cross the eating park's gate path at a place taken from its points.
    - footfall_test's spreadWorld steps the eating park until a stretch holds a footfall entry with no guest on it, not to a tick picked against the park's old layout.
    - service_test's "A shop's queue holds an entry for each visit unit it holds, so a guest with two visits takes two places" and "A starved shop keeps the visits that reached it earliest and returns the later ones" take a returned visit as addressed to its guest, in transit or held, not as held one tick later.
3. src/sim/guests/SPEC.md and src/sim/operations/SPEC.md read as Spec changes gives. Each changed statement is asserted by a test, except VISIT_DELAY's and RETURN_DELAY's, which only name the tuning delay a visit travels with (rule 1):
    - the hunger rate, starting hunger, and stay ranges by criterion 1's hunger and arrival properties;
    - the arrival rule by arrivals_test's "parkGuests gives the guests' keys ascending, and guestRecord gives a record exactly for those keys", which requires four guests after two arrival cycles;
    - the score's terms, their signs, their sum, and HEAD_HOME_SCORE outscoring every offer by criterion 1's scoring property;
    - hungerCurve by criterion 1's curve property, with choice_test's unlisted points and clamp cases;
    - softmaxPick by criterion 1's softmax property and choice_test's "Over 10000 keys differing only in index, softmaxPick picks each option within 4.5 standard deviations and 1 of its Probability's share";
    - WALK_STEP by criterion 1's walking property;
    - a node's steps and the wanderer's equal chances by criterion 1's way-back property;
    - the footfall update and readKept by criterion 1's footfall property;
    - shipmentDelay by criterion 1's delay property;
    - REORDER_POINT, ORDER_UP_TO, and REORDER_POINT below ORDER_UP_TO by criterion 1's ordering property, whose order at the reorder point holds at least one unit;
    - SERVICE_INTERVAL by criterion 1's serving property and service_test's "A shop that served while stepping tick t serves no other guest before tick t + SERVICE_INTERVAL, and serves the next one then";
    - MEAL_RELIEF by criterion 1's offer property;
    - the wait and the resolved offer by food_offer_test's "A shop's offered wait predicts the cycle in which it serves the next guest to arrive".
4. The 12 entries of criterion 2 are added to plans/focused-tests/RESEARCH.md before the test pass. The guestOptions scoring entry is corrected there to "never scores lower when the guest is hungrier", and walks_test's hunger entry gains its path's literal start. After the pass, every integration/, sim/guests/, sim/operations/, and sim/support/ entry is removed from it.
5. No definition in tests/sim/support/guest_parks.h, line_park.h, or ledger_writes.h, or tests/integration/support/park_files.h, is left that no test uses, directly or through another definition in its header.
6. Nothing under src/ changes except src/sim/guests/SPEC.md and src/sim/operations/SPEC.md, and tpj_scenarios's windows-debug output is identical before and after.

## Medium

None. The feature changes tests and two specs. It samples, emits, draws, and supplies no fields or flows. Each rewritten test reaches guests and operations through their public headers. Flow conservation stays sim/medium's, and the world-as-value standards stay the sim root's (rule 2).

## Spec changes

src/sim/guests/SPEC.md:

- "Its HungerRate is HUNGER_RATE_MIN + u("guest-hunger-rate") * (HUNGER_RATE_MAX - HUNGER_RATE_MIN), with HUNGER_RATE_MIN 1/2700 and HUNGER_RATE_MAX 1/1350 per tick, so hunger rises from 0 to 1 in 45 to 90 s. Its Hunger is u("guest-starting-hunger") * STARTING_HUNGER_MAX, 0.4." becomes "Its HungerRate is drawn by u("guest-hunger-rate") between HUNGER_RATE_MIN and HUNGER_RATE_MAX per tick, and its Hunger by u("guest-starting-hunger") between 0 and STARTING_HUNGER_MAX."
- "when t + 1 is a multiple of ARRIVAL_INTERVAL, 60 ticks, each" becomes "when t + 1 is a multiple of ARRIVAL_INTERVAL, each".
- "StayUntil is t + STAY_MIN + floor(u("guest-stay") * (STAY_MAX - STAY_MIN)), with STAY_MIN 1800 and STAY_MAX 3600." becomes "StayUntil is t plus a whole number of ticks drawn by u("guest-stay"), at least STAY_MIN and below STAY_MAX."
- In the paragraph beginning "An offer's terms come from", the text from "Its Relief is RELIEF_WEIGHT" to the paragraph's end becomes "Its Relief is RELIEF_WEIGHT times hungerCurve(Hunger) times O.Relief, and RELIEF_WEIGHT is positive, so Relief never falls as the guest grows hungrier. Its Distance is DISTANCE_WEIGHT times E.Distance, and its Wait is WAIT_WEIGHT times O.Wait in seconds. Both weights are negative, so an offer scores less the farther it is and the longer its wait. Its Commitment is COMMITMENT_BONUS when its source is the guest's Target and 0 otherwise. Its Score is Relief + Distance + Wait + Commitment, added in that order, so the terms reproduce it exactly (decision 0019). Carrying on has the Score CARRY_ON_SCORE, and heading home HEAD_HOME_SCORE, each with every term 0. HEAD_HOME_SCORE exceeds RELIEF_WEIGHT times MEAL_RELIEF plus COMMITMENT_BONUS, the most an offer of MEAL_RELIEF can score, so heading home dominates once a stay is over."
- The paragraph beginning "hungerCurve is the authored" becomes "hungerCurve is the authored piecewise-linear curve through the points of HUNGER_CURVE (decision 0020), whose X values run from 0 to 1, hunger's whole range, and whose Y values lie from 0 to 1 and never fall. A hunger below 0 counts as 0, and one above 1 as 1. At a point's X it is that point's Y, and between consecutive points it is linear, so it lies between their Ys and never falls as hunger rises."
- The paragraph beginning "softmaxPick(key, options) sets" becomes "softmaxPick(key, options) sets each option's Probability and returns the index it picks, with one draw on the key, picking each option with its Probability. The probabilities are the softmax of the Scores at the temperature CHOICE_TEMPERATURE: they add up to 1, up to rounding, an option with a higher Score never has a lower Probability, and options with equal Scores have equal ones. It throws std::invalid_argument, changing nothing, for an empty list or a Score that is not finite."
- "WALK_SPEED 1.3 m/s times SIM_TICK_SECONDS" becomes "WALK_SPEED times SIM_TICK_SECONDS".
- "At a node, the node's steps are, for each edge in edges() order, {carrier, FromDistance, ToDistance} when the edge starts at the node, then {carrier, ToDistance, FromDistance} when it ends there." becomes "At a node, the node's steps are one for each end of an edge at the node, each leaving the node along its edge."
- "A wandering guest picks among the node's other steps by drawPick over integer weights of 1, one for each, keyed" becomes "A wandering guest picks among the node's other steps, each equally likely, with one draw keyed".
- "the delay VISIT_DELAY, 1 tick." becomes "the delay VISIT_DELAY."
- "with time constant FOOTFALL_TIME, 300 ticks, of" becomes "with time constant FOOTFALL_TIME, of".
- "Its new value is V + (S - V) / FOOTFALL_TIME, computed in that order with FOOTFALL_TIME converted to double, and stepFootfall" becomes "Its new value is one step of that average from V toward S, so it lies between V and S, and stepFootfall".
- "HungryFootfall's readKept(value, k) is value * simExp(k * simLog(1 - 1 / FOOTFALL_TIME)), with k and FOOTFALL_TIME converted to double and 1 / FOOTFALL_TIME computed first. Each step of V + (0 - V) / FOOTFALL_TIME multiplies V by 1 - 1 / FOOTFALL_TIME up to rounding, so the rule is k such steps up to rounding." becomes "HungryFootfall's readKept(value, k) is k steps of that average from value toward 0, up to rounding, so a value read later never moves away from 0 and never passes it."
- "readKept(value, 0) is value, as the formula gives, so" becomes "readKept(value, 0) is value, so".

src/sim/operations/SPEC.md:

- The paragraph beginning "shipmentDelay(distance) is" becomes "shipmentDelay(distance) is a shipment's delay in ticks: the ticks a shipment takes over the distance at SUPPLY_SPEED, rounded up. It is at least 1 and at most the largest uint32_t for every distance, a NaN included, and never falls as the distance grows."
- "its inventoryPosition is REORDER_POINT, 8, or below, it creates ORDER_UP_TO, 24, minus that position" becomes "its inventoryPosition is REORDER_POINT or below, it creates ORDER_UP_TO minus that position".
- "and the delay ORDER_DELAY, 30 ticks. A starved shop sends no orders." becomes "and the delay ORDER_DELAY. REORDER_POINT is below ORDER_UP_TO, so an order always holds a unit. A starved shop sends no orders."
- "the delay RETURN_DELAY, 1 tick, and no meal" becomes "the delay RETURN_DELAY, and no meal".
- "the delay SERVICE_INTERVAL, 90 ticks, the time service takes" becomes "the delay SERVICE_INTERVAL, the time service takes".
- "offers {MEAL_RELIEF, its wait, true}, with MEAL_RELIEF 0.5." becomes "offers {MEAL_RELIEF, its wait, true}."
- The paragraph beginning "The wait is counted from" becomes "The wait is counted from the cycle in which a shop first holds the next visit to arrive, behind every guest queued, to the cycle that takes its guest, as if no other visit arrived, with guests taken by the Service section's rule. Let r be the tick of that first cycle, n the guests queued, and F the shop's FreeAt. The shop's supply units, in order, are its stock under its own handle, each available at r, then the units of the supplies packets addressed to it whose destination is the shop, in ascending arrival, each available at its packet's arrival, and then any number more, each available when an order placed in the offer's cycle would bring it: that cycle's tick plus ORDER_DELAY plus D. D is shipmentDelay of supplyRouteLength(world, depot, shop) for the nearest depot, the length the depot ships by, or of the nearest depot's Distance when that is none. So when no other visit arrives and the shop's supplies come as these units say, it takes that guest in the cycle the wait gives."
- "with its first guest waiting for the order it places in its first cycle, so its wait is ORDER_DELAY + D." becomes "with its first guest waiting for the order it places in its first cycle."

## Files affected

- Modify: src/sim/guests/SPEC.md, src/sim/operations/SPEC.md
- Modify (the test-writer): tests/sim/guests/add_guest_test.cpp, arrivals_test.cpp, choice_test.cpp, footfall_test.cpp, guest_edits_test.cpp, guest_options_test.cpp, visits_test.cpp, walks_test.cpp; tests/sim/operations/food_offer_test.cpp, operations_test.cpp, service_test.cpp; tests/integration/food_loop_test.cpp; tests/sim/support/guest_parks.h, line_park.h
- Modify: tests/sim/support/guest_parks.h, line_park.h, ledger_writes.h, and tests/integration/support/park_files.h, for definitions no remaining test uses
- Modify: plans/focused-tests/RESEARCH.md

## Dependencies

park-routes-reshaped, merged: the park's and routes' tests are in their final shape, and tests/sim/support/connected_parks.h gives gapWithinReach, a door-to-line distance within CONNECTION_REACH.

## Out of scope

- The draw keys, the arrival rule, the score's weighted terms, footfall's stretches, sampling, and carrying, and the Service section's rules, which stay as RESEARCH.md says.
- Loose cycle limits that guard a run against hanging, such as food_offer_test's LONGEST_WAIT and the 2000- and 3000-cycle bounds, which exceed what the runs take several times over.
- The park files in tests/parks/, such as warm.park and cut.park, which are content and scenarios' slice parks' output, not test setups.
- legible's statement of WALK_SPEED's value, which legible-reshaped trims, and render's OVERLAY_FULL copying MEAL_RELIEF, already filed in sound-architecture.

## Open questions

- food_offer_test reads food-offer's resolved and stepped entries through world.Registry, as it did before this feature, and as many sim tests read medium components. Whether a test may read them, or must sample through the medium's public queries, is the closing audit's to judge against docs/testing.md.
