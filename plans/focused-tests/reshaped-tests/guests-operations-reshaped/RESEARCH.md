# Research: guests-operations-reshaped

## Which statements of the guests and operations specs are trimmed?

src/sim/guests/SPEC.md fixes these tuning numbers:

- HUNGER_RATE_MIN's 1/2700, HUNGER_RATE_MAX's 1/1350, and the 45 to 90 s they give;
- STARTING_HUNGER_MAX's 0.4;
- ARRIVAL_INTERVAL's 60 ticks;
- STAY_MIN's 1800 and STAY_MAX's 3600;
- the four weights, CARRY_ON_SCORE's 0.5, HEAD_HOME_SCORE's 10, and the 2.3 an offer scores at most;
- HUNGER_CURVE's four points;
- CHOICE_TEMPERATURE's 0.25;
- WALK_SPEED's 1.3 m/s;
- VISIT_DELAY's 1 tick;
- FOOTFALL_TIME's 300 ticks.

It also copies these formulas from the code:

- the hunger rate, starting hunger, and stay draws as arithmetic on u(p);
- the hunger curve's interpolation;
- the softmax weights and their division;
- the order of a node's steps and the weights of 1 a wanderer draws with;
- the moving average's update;
- readKept's simExp and simLog.

Each number goes. Each formula becomes the property it computes:

- each draw lies in its range;
- the curve is linear between its points, so it never falls;
- the probabilities add up to 1, never rank against the Scores, and tie for equal Scores;
- a wanderer picks among the other steps, each equally likely;
- a new footfall value lies between the old value and the summed hunger;
- a value read later never moves away from 0 and never passes it.

The score's terms keep their weighted form, since decision 0019 makes a score a sum of weighted factor terms that reproduce it. They add the weights' signs, so a test can say which way each term moves. The draw keys stay, since they say what each draw depends on (principle 10). The arrival rule, a cycle stepping t when t + 1 is a multiple of ARRIVAL_INTERVAL, stays, since it names its constant and many tests reach their first guest by it.

src/sim/operations/SPEC.md fixes SUPPLY_SPEED's 2 m/s, REORDER_POINT's 8, ORDER_UP_TO's 24, ORDER_DELAY's 30 ticks, RETURN_DELAY's 1 tick, SERVICE_INTERVAL's 90 ticks, and MEAL_RELIEF's 0.5. It copies shipmentDelay's ceil and the wait's T_i recurrence, and states the resolved wait as ORDER_DELAY + D. The numbers go. shipmentDelay becomes a delay rounded up from the distance at SUPPLY_SPEED, at least 1 and at most the largest uint32_t, never falling as the distance grows. The wait keeps its definition: what it counts from, to which cycle, and which supply units are available when. The recurrence goes, since the Service section's rule already says how a shop takes guests. The resolved offer keeps "as for a shop that has not stepped" and drops the sum.

Other modules name only WALK_SPEED, ARRIVAL_INTERVAL, SERVICE_INTERVAL, EATEN_CAUSE, and the public functions. legible's spec states WALK_SPEED's 1.3 m/s itself, and legible-reshaped trims it. render's OVERLAY_FULL 0.5 copies MEAL_RELIEF by value. That copy is already filed in sound-architecture's deepening candidates.

## Which property did a trimmed number stand in for?

Two:

- HEAD_HOME_SCORE's 10 against an offer's most of 2.3 is what makes heading home dominate once a stay is over. The spec now says HEAD_HOME_SCORE exceeds RELIEF_WEIGHT times MEAL_RELIEF plus COMMITMENT_BONUS. That is the most an offer of MEAL_RELIEF can score, since hungerCurve's values lie from 0 to 1 and the other two weights are negative.
- REORDER_POINT's 8 below ORDER_UP_TO's 24 is what makes every order hold a unit. The spec now says REORDER_POINT is below ORDER_UP_TO.

## Which unlisted setups lean on trimmed numbers?

The previous feature took the sizes and the connection reach out of the park and routes specs. It left the setups of later modules, which place boxes beside paths at literal gaps, to those modules' features. Here they are mostly fixtures shared by every guests and operations test file. Evan chose to add them to the inventory, with the cases whose own code holds such a literal, so the test-writer rebuilds them from the constants by name:

- guest_parks.h's northGate places an entrance at z = 126.5, its back on the park's edge only while PARK_SIZE is 256 m and ENTRANCE_SIZE 3 m deep. Its door's lines start at z = 123, 2 m beyond the door, inside a 4 m reach. legsWorld starts its far leg 6 m from the door, "beyond CONNECTION_REACH". eatingWorld places its shops 3.5 m and its depot 3 m from the lines they connect to, from a 6 m shop depth and an 8 m depot depth.
- line_park.h's shopAt, depotAt, and depotAcross put every door 3 m from the backstage line, from the box depths and the reach.
- add_guest_test's gatesIntent, arrivals_test's gatesWorld, and walks_test's crossingWorld lay their guest paths from z = 123, against northGate's door. So does walks_test's hunger case, a listed case whose entry names only its copied formula, so the entry gains its path's start.
- food_offer_test's guestLine runs at z = -12, 3 m from every shop's front door.
- footfall_test's "Hungry footfall holds at most one kept entry…" and "A stretch that exactly one footfall entry is carried into…" cross the eating park's gate path at a literal z = 121.5, inside its 123 to 119.
- service_test's "A shop's queue holds an entry for each visit unit…" and "A starved shop keeps the visits that reached it earliest…" expect a returned visit held by its guest one tick after its return. That holds only while RETURN_DELAY is 1.

Paths placed away from the doors, such as the eating park's spine and cross, stay where they are, so cases that name points on them, such as visits_test's join path, need no change.

Loose cycle limits, such as a REQUIRE that a run finishes within 2000 ticks where it takes a few hundred, are hang guards. They are not rebuilt.

## What does each listed case check, and where is each check after the rewrite?

A rewrite that keeps only an entry's property can lose a check the old case made beside it. So every check of the 34 listed cases is accounted for here.

- **add_guest's refusal:**
  - Three places that do not resolve, each refused with std::invalid_argument, leaving the world and its next key unchanged.
  - One place stays, with both checks. The others go (rule 4).
- **arrivals' tick-by-tick count:**
  - Two guests at each arrival cycle and none between, from two connected entrances and one unconnected.
  - It becomes connected entrances admitting alike and the unconnected one none.
  - The arrival cycle itself stays shown by arrivals' unlisted parkGuests case, which requires four guests after two arrival cycles, and by every test that reaches its first guest at FIRST_ARRIVAL.
- **arrivals' key order and new-guest record cases**, merged:
  - Key counter order and entrance key order go (rule 1).
  - Each arrival standing at its own entrance's node place, wandering, with its record's Position on the ground there, stays.
  - StayUntil and Hunger become their ranges.
  - No target, no meals, and no last meal are addGuest's unlisted first case's, which arrivals reach through addGuest.
- **choice's hungerCurve interpolation:**
  - The bit-exact interpolation goes.
  - Never falling, and lying between neighbouring points' values, stays.
  - The points and the clamp stay in their unlisted cases.
- **choice's softmax case:**
  - Probability as weight over total, bit for bit, goes.
  - Leaving every other field unchanged stays.
  - The returned index as drawPick's is wiring (rule 5).
  - It becomes probabilities that add up to 1, rank with the Scores, and tie for equal Scores. That a pick follows the probabilities stays in the unlisted statistical case.
- **choice's "A guest chooses where it stands…":**
  - The exact pick through test::choiceOptions and checkPick goes.
  - Activity and Target following one of the options listed where the guest chose stays.
  - Each kind picked, and a pick made at a shop's anchor, which makes the guest wait, stay as what the run must show.
- **The three walk-step cases** (choice's heading to a shop, walks' wandering, walks' heading home):
  - They merge into one walking property.
  - WALK_STEP of route distance per cycle stays.
  - A heading guest's distance falls until it waits at its shop's anchor or leaves at its entrance's.
  - The reflect-at-dead-end arithmetic goes. Turning at a dead end stays shown by a wanderer that walks a line with dead ends and keeps covering WALK_STEP on it.
- **The three lost-route cases** (choice's offer stops being reachable, choice's heading home with no entrance entry, walks' no entrance entry):
  - They merge into one property: a guest never keeps heading for a destination it has no route to, and takes heading home up again once a route exists.
  - The exact pick goes.
  - The supply cut and the shop deletion stay as two causes of one rule.
  - The stranded guest wandering on past its stay, then heading home and leaving once the route is drawn again, stays.
  - Moving every cycle is the walking property's.
- **footfall's update and decay cases**, merged:
  - Exact equality to the average and to simExp of simLog goes.
  - A stretch with guests moving toward their summed hunger, and an idle entry's read value never moving away from 0, stay.
  - The run must still show a held value and an idle entry.
- **guest_edits' candidate case:**
  - Its four sections go to a few inputs.
  - The fallback chain's exact places go.
  - Nothing but the place changing, and a guest whose carrier survives keeping its ground point, stay.
  - Every place resolving after a retiring edit stays in the unlisted randomized case.
- **guestOptions' two none cases:**
  - They merge.
  - The five key kinds go to one key that holds no guest.
  - A guest whose place does not resolve stays.
- **guestOptions' comparison with test::choiceOptions:**
  - The comparison goes.
  - It becomes an offer never scoring lower when the guest is hungrier and scoring lower when farther or with a longer wait.
  - Heading home appears only once a stay is over, and then outscores every offer.
  - Each Score is its terms added in order.
  - The entry asked for "scores higher when hungrier". Only "never lower" holds for every authored curve, so the entry is corrected.
- **visits' send case:**
  - VISIT_DELAY == 1 and the shop holding the visit at the cycle's end go.
  - One visit created per guest that starts waiting, at its Target's anchor, sent to its Target with the delay VISIT_DELAY by name, stays.
- **visits' meal case:**
  - The hunger arithmetic goes.
  - Meals consumed as eaten matching meals counted stays, with MealsEaten rising by 1, LastMeal's tick, its After being the Hunger and below its Before, and no Hunger below 0.
- **walks' hunger case:**
  - min(1, h + drawn rate) goes.
  - Hunger never falling and never exceeding 1 stays.
  - Each cycle's rise being one per-guest amount between the two rate bounds, until it reaches 1, stays as the range the spec now gives.
- **walks' wander pick:**
  - The draw key and step order go.
  - Never taking the way back when another step exists stays.
  - The other steps being equally likely, which the old case showed through the exact draw, becomes a statistical share over guests leaving one junction (rule 4).
- **food offer's resolved case:**
  - MEAL_RELIEF == 0.5 and the wait as ORDER_DELAY + D go.
  - One source per shop box, one entry at its guest anchor or none, MEAL_RELIEF by name and Supplied with a nearest depot, and OfferEntry{} without, stay, after a first resolution and after a cut.
  - The resolved wait stays shown by the unlisted prediction case's placed shop.
- **shipmentDelay's two cases:**
  - They merge into a valid, never-falling delay.
  - Inputs stay at the reasoned edges: 0, a negative, both infinities, a NaN, and a distance whose delay passes the largest uint32_t.
- **inventoryPosition's 13 sections:** they go to the combined case, whose counted kinds each hold a different number of units, so the sum shows which kinds count.
- **The reorder pair:**
  - They merge.
  - The pins of REORDER_POINT, ORDER_UP_TO, and ORDER_DELAY go.
  - Ordering at or below REORDER_POINT and not above it, ORDER_UP_TO minus the position, to the nearest depot, with the delay ORDER_DELAY by name, stays.
- **service's serving case:**
  - SERVICE_INTERVAL == 90 goes.
  - The supplied and starved shops stay. The starved one shows that serving needs only stock.
- **The queue-order pair:** they merge into one test with visits arriving at two swaps and two guests sharing one.
- **service's starved cover case:**
  - RETURN_DELAY == 1 and its seven sections go to the combined case.
  - Each turned-away visit is sent back to its guest with the delay RETURN_DELAY by name and no meal.
- **food_loop's case:**
  - HUNGER_TICKS and cut.park go.
  - It cuts warm.park's backstage paths itself and runs until warm.park has served a guest the cut park has not.

## Which test support can the rewrites leave unused?

A check by name over the includers of guest_parks.h, line_park.h, ledger_writes.h, and integration's park_files.h finds these names that no includer uses:

- guest_parks.h's guestDraw, LEGS_GATE, STARVED_SHOP, EATING_DEPOT, offerOf, checkFollowsOffer, and checkFollows. Each is referred to by a used definition in its own header.
- ledger_writes.h's ledgerIn, used by its own header.
- ledger_writes.h's conserved, which nothing uses. flow_test keeps its own copy.
- park_files.h's ParkFile and readParkText, used by its own header.

After the rewrites, drawnStayUntil, drawnHungerRate, drawnStartingHunger, choiceOptions, and checkPick are likely to go unused, since they copy the draws and the scoring formula.
