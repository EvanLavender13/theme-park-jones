# Implementation Plan: Two Goods

## Goal

Shops order, hold, and serve food and drink as separate goods by their stock setting, through one counter, with an exact offer for each good and a sell-off that previews correctly.

## Approach

Operations gains drink's four flow kinds and its offer field. Its shop and depot code becomes a template over a good, run for each good in the list GOODS, so food and drink run the same rules on their own kinds. Adding a good later means one enum value, one traits entry, its registered types, and its place in GOODS. The queue stays a list of guest keys, and a guest's entries take its visits in the order of GOODS, so food-only parks save the same text. The medium's field finisher asks the field's owner, through an optional keepStepped rule, whether to keep a changed source's stepped entries, and operations keeps the offer of a good a shop is selling off. The scenarios library makes tests/parks/stocked.park from facilities.park, and the food-shop scenario's synthetic guests also visit for drink.

## Placement

Decision 0027 places each behavior this feature adds:

- Keeping a source's stepped entries through a resolution, by the owner's rule: medium, src/sim/medium/field.h (settleSteppedEntries, HasKeepRule). The field's finisher already decides which stepped entries a resolution clears.
- Drink's orders, supplies, visits, servings, and offer; ordering, serving, and offering by good; the sell-off and its keep rule: operations, src/sim/operations/operations.h and operations.cpp. Operations owns shops, depots, and their flows and offers.
- Each good's stock and orders in the shop record: operations, shopRecord. The record is the shop's published view of itself (decision 0025).
- Showing each good in the Inspector: legible, src/legible/inspect.cpp (shopRows), which already turns records into rows.
- Listing each good in the Debug panel: app, src/app/ui/debug_panel.cpp, which already lists each shop's record.
- Synthetic drink visits: scenarios, src/scenarios/food_shop.cpp, which owns the food-shop scenario's guests.
- Making stocked.park: scenarios, src/scenarios/slice_parks.h and slice_parks.cpp (makeStockedPark), beside the other slice park generators. src/scenarios/main.cpp gains only the two extra paths for --slice-parks and the call, which is composition.

## Tasks

### Task 1: Update the operations spec

Files:
- Modify: `src/sim/operations/SPEC.md`

Step 1: In Boundary, make the first list read:

- the flow kinds supply-orders, supplies, guest-visits, meals, drink-orders, drink-supplies, drink-visits, and drinks;
- a shop's service state;
- the fields food-offer and drink-offer and the resolvers that publish them;
- the systems that step shops and then depots.

Step 2: After "Every shop box of parkBoxes is a shop, and every depot box is a depot." add "A shop sells the goods its stock setting names (park/SPEC.md)."

Step 3: Make the second list's first two items read:

- the GuestVisits and Meals flow kinds, through which guests visit shops for food and receive meals, and DrinkVisits and Drinks, for drink;
- the FoodOffer and DrinkOffer fields and their OfferEntry;

Step 4: Add a section after Boundary:

    ## Goods

    A shop sells food, drink, or both. Each good has its own kinds: supply-orders, supplies, guest-visits, meals, and food-offer for food, and drink-orders, drink-supplies, drink-visits, drinks, and drink-offer for drink. A shop set to food behaves as shops did before drink.

Step 5: In Supply, replace the second paragraph with:

    If a shop is supplied, it keeps its stock of each good it sells topped up by ordering from its nearest depot. It orders a good whenever what it holds of it plus what it has on order falls to REORDER_POINT, and it orders up to ORDER_UP_TO. Orders take ORDER_DELAY to reach the depot. Supplies travel back at SUPPLY_SPEED, so they take longer on a longer backstage route. A depot is an unlimited source of both goods.

In the third paragraph, replace "A shop never holds more than ORDER_UP_TO, counting what is on its way." with "A shop never holds more than ORDER_UP_TO of a good, counting what is on its way." After that paragraph add: "If the player switches a shop away from a good, it orders no more of it. Orders it has already sent are still shipped."

In the last paragraph, replace "so the supplies created always equal the orders filled, and the meals created always equal the supplies served" with "so each good's supplies created always equal its orders filled, and its servings created always equal its supplies served".

Step 6: Replace the Service section's paragraphs with:

    A shop is one counter. It serves the guests who visit it one at a time, at most one per SERVICE_INTERVAL, in the order their visits arrived, whatever good they came for. Its servings are limited by the scarcest of demand, supply, and service rate. Each guest served takes one supply of the good its visit names. If the shop holds none of that good, the guest at the front waits for it, and so do the guests behind. If a guest has visits for more than one good at one shop, they are taken in the order of the goods, food first. When a guest is served, its visit comes back with a meal or a drink, in the same packet timing. If a visit comes back with a serving, the guest was served, and if it comes back without one, the guest was not.

    A shop's cover of a good is the supplies of it the shop holds or has on the way. If a shop is starved, it turns away, with no serving, every queued guest beyond its cover of the good the guest came for. It does the same for a good it no longer sells. It turns away only those, since supplies already shipped always arrive. If a shop is supplied, it never turns away a guest for a good it sells, however long its queue.

    When a shop is deleted, the guests waiting at it get their visits back with no serving. If a visit or serving is sent to a guest who has left, it is consumed as abandoned.

Step 7: Rename the section Food offer to Offers, and replace its text with:

    Each shop publishes what it offers to guests: food in the entry field food-offer, and drink in drink-offer. In each, it publishes one OfferEntry at its guest anchor, holding:

    - the need a serving relieves, on a scale from 0 to 1: MEAL_RELIEF for food and DRINK_RELIEF for drink;
    - the ticks the next guest to arrive for the good would wait to be served;
    - whether it has the good at all.

    A shop offers each good it sells. It also offers a good it no longer sells while it still holds or awaits some of it. If a shop is starved, each of its offers gives no relief, no wait, and no serving. An offer of a good the shop no longer sells gives the same when the next guest for it would be beyond the shop's cover. If a shop has no guest connection, it publishes nothing.

    The wait is exact under two assumptions: no other visit arrives, and supplies come as the shop expects. Then the next guest for the good is served in the cycle the wait says, whatever goods the guests ahead of it came for. If a shop is newly placed, or is in a candidate, it offers each good it sells at once, before it has stepped. If an edit cuts or restores a shop's supply route, its offers of the goods it sells change from that tick. If an edit switches a shop away from a good it still holds or awaits, the shop's offer of that good stays as the shop last published it, in a candidate and in the committed world, until the shop steps. Other edits before that step leave it as it was too.

Step 8: In Inspection, replace "The record holds its stock, its queue, what it has on order, whether it is starved, and its limiting factor." with "The record holds its stock and what it has on order of each good, its queue of guests for either good, whether it is starved, and its limiting factor." Replace the Supply and ServiceRate items with:

    - Supply when the shop holds fewer of some good than guests queued for it;
    - ServiceRate when it holds enough of each good for everyone queued for it.

### Task 2: Update the medium spec and the milestone's medium

Files:
- Modify: `src/sim/medium/SPEC.md:61`
- Modify: `plans/plausible-operations/served-needs/MILESTONE.md:21` and `:30`

Step 1: In src/sim/medium/SPEC.md, after "When a command changes a source's resolved entries, its stepped entries are cleared, so the tick after a command sees every change the command made." add "The owner may give the field a rule, keepStepped, that keeps a source's stepped entries through that change. If the rule keeps them, they stay as they were."

Step 2: In MILESTONE.md's criterion 9, replace "states when a resolver keeps a source's stepped entries" with "states when a field's owner keeps a source's stepped entries".

Step 3: In MILESTONE.md's Candidate resolution bullet, replace "two-goods lets a resolver keep a source's stepped entries through that change" with "two-goods lets a field's owner keep a source's stepped entries through that change, by a rule the field's finisher asks".

### Task 3: Update the legible and app specs

Files:
- Modify: `src/legible/SPEC.md:62`
- Modify: `src/app/SPEC.md:65`

Step 1: In src/legible/SPEC.md, replace "For a shop it shows stock, queue, units on order, what limits it, and whether it is starved." with "For a shop it shows its stock and units on order of each good, its queue, what limits it, and whether it is starved."

Step 2: In src/app/SPEC.md, replace "It lists each shop's stock, queue, orders, and what limits it." with "It lists each shop's stock and orders of each good, its queue, and what limits it."

### Task 4: Update the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md:27` and `src/scenarios/SPEC.md:39-45`

Step 1: Replace the list item "a food shop whose supply is cut and restored" with "shops selling food and drink, whose supply is cut and restored".

Step 2: Make the Slice parks paragraph start "tpj_scenarios --slice-parks fed warm cut main-street warm-main-street facilities stocked makes the boxes-and-tubes slice's parks from fed, the good-paths slice's warm park from main-street, and the day-at-the-park slice's stocked park from facilities:". In the warm-main-street item, replace "main-street is read and never written." with "main-street and facilities are read and never written." Add the item:

    - stocked is facilities with a depot and a backstage path added, stepped until a shop holds food, a shop holds drink, and drink is still on its way. If that doesn't happen within STOCKED_LIMIT_TICKS cycles, it fails.

Step 3: Make the paragraph after the list start "tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, and stocked.park are its output."

### Task 5: Let a field's owner keep stepped entries

Files:
- Modify: `src/sim/medium/field.h:133-140` and `src/sim/medium/field.h:235-250`

Step 1: After the HasEdgeRule concept, add a concept HasKeepRule, satisfied by a FieldDefinition with `static bool keepStepped(const World &world, EntityKey source)`. Its comment: "A field whose owner may keep a source's stepped entries through a resolution that changes the source's resolved entries."

Step 2: In settleSteppedEntries, erase a source's readable stepped slot only when its entry words differ and, for a field with HasKeepRule, `F::keepStepped(world, slot.Source)` is false. Make its comment "clears the readable stepped entries of each source whose resolved entries changed, unless the field's owner keeps them, then drops the previous."

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: the last line names linking libtpj_sim.a, with no warning above it.

### Task 6: Declare drink's kinds, the goods, and the record

Files:
- Modify: `src/sim/operations/operations.h`

Step 1: After Meals, add the flow kinds, each a struct with a Name, as the food kinds are:

- DrinkOrders, "drink-orders": "Orders for drink a shop sends a depot, one unit for each supply wanted, addressed to the shop."
- DrinkSupplies, "drink-supplies": "Drink a depot ships to a shop, addressed to the shop."
- DrinkVisits, "drink-visits": "A guest's visit to a shop for drink, addressed to the guest, which the shop returns to it."
- Drinks, "drinks": "A drink a shop sends the guest it served."

Step 2: Make OfferEntry's comment: "A shop's offer of a good: the need a serving relieves, on a scale from 0 to 1, the ticks the next guest to arrive for the good waits to be taken, and whether the shop has the good. A starved shop's offer is OfferEntry{}, which says none."

Step 3: Give FoodOffer a declaration `static bool keepStepped(const World &world, EntityKey source);` with the comment "Keeps a shop's stepped food offer while it no longer sells food but holds or awaits some." Add DrinkOffer after it: Entry OfferEntry, Name "drink-offer", Kind FieldKind::Entry, the same keepStepped declaration for drink. Its comment: "The drink offer field: each shop's drink offer, at its guest anchor."

Step 4: After MEAL_RELIEF, add `inline constexpr double DRINK_RELIEF = 0.5;` with the comment "The thirst a drink relieves, on a scale from 0 to 1."

Step 5: Before LimitingFactor, add `enum class Good : uint8_t { Food, Drink };` with the comment "A good a shop sells.", an `enumNames(Good)` giving "food" and "drink", in the shape intent.h's enumNames take, and `inline constexpr std::array<Good, 2> GOODS{Good::Food, Good::Drink};` with the comment "Every good, in the order shops order, serve, and offer them." Add `bool sells(ShopStock stock, Good good);` with the comment "Whether a shop with the stock setting sells the good." Include sim/park/intent.h for ShopStock.

Step 6: Add before ShopRecord:

    // A shop's holding of one good: the supplies it holds, and those on their way or on order.
    struct GoodRecord {
      int64_t Stock = 0;
      int64_t OnOrder = 0;

      bool operator==(const GoodRecord &) const = default;
    };

Replace ShopRecord's Stock and OnOrder with `std::array<GoodRecord, GOODS.size()> Goods;`, indexed by the good, keeping Queue, Starved, and Limit. Its comment: "What a shop publishes about itself for display and tests (decision 0025): its holding of each good, the guests queued for either good, whether it has no route to a depot, and what limits its service. Nothing in the park reads it."

Step 7: Make inventoryPosition `int64_t inventoryPosition(const World &world, EntityKey shop, Good good);` with the comment "The supplies of the good the shop holds or has coming, and its orders for the good on their way to or held by a depot." Make the LimitingFactor comment "What limits a shop's service now: no guests queued, fewer of a good than guests for it, the service rate, or no route to a depot." Make addOperations' comment list the drink kinds and the drink-offer field and resolver.

### Task 7: Order and ship each good

Files:
- Modify: `src/sim/operations/operations.cpp:85-99`, `:301-326`, `:348-401`, and `:417-428`

Step 1: In the anonymous namespace, add a traits template GoodKinds<Good>, with a specialization for each good naming its Orders, Supplies, Visits, Servings, and Offer kinds and its Relief: SupplyOrders, Supplies, GuestVisits, Meals, FoodOffer, and MEAL_RELIEF for food, and DrinkOrders, DrinkSupplies, DrinkVisits, Drinks, DrinkOffer, and DRINK_RELIEF for drink. Add `template <typename Action> void forEachGood(Action &&action)`, which calls `action.template operator()<G>()` for each good G of GOODS in order, so code that runs per good is written once. GoodKinds and forEachGood are the only places that name each good.

Step 2: Define sells: Both sells both goods, Food sells food, and Drink sells drink.

Step 3: Make inventoryPosition a template over the good in the anonymous namespace, its body as now with the good's Supplies and Orders kinds. The public inventoryPosition calls it for the good it is given.

Step 4: Make orderSupplies a template over the good, taking whether the shop sells it. It takes back the good's returned orders as now. It returns before ordering when the shop doesn't sell the good, has no route, or its position for the good is above REORDER_POINT.

Step 5: Make stepShops loop over parkBoxes' shop boxes, so it has each shop's stock. For each, after nearestDepot, call orderSupplies for each good through forEachGood.

Step 6: In stepDepots, move the body that handles one kind of returned supplies and orders into a template over the good, and call it for each good through forEachGood for each depot. Make its comment say the depot does this for each good.

Step 7: In shopRecord, fill each good's Stock from the good's supplies held and OnOrder from its position less its stock. Count Queue from the live guests' visits of every good. Make Limit Supply when, for some good, the live visits for it exceed its stock.

Step 8: In addOperations, register DrinkOrders, DrinkSupplies, DrinkVisits, and Drinks after Meals.

Step 9: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: the last line names linking libtpj_sim.a, with no warning above it.

### Task 8: Serve one queue across both goods

Files:
- Modify: `src/sim/operations/operations.cpp:101-201`

Step 1: Make abandonGone consume as abandoned the servings of every good the shop holds, and the visits of every good it holds for gone guests.

Step 2: In reconciledQueue, take each guest's held units as the sum of its visits of every good. Append each guest's further units in ascending key order, as now.

Step 3: Add `std::vector<Good> queueGoods(const World &world, EntityKey shop, const std::vector<EntityKey> &queue)`. It gives each entry's good: a guest's entries take its held visits of each good in the order of GOODS. Comment: "The good each queue entry came for: a guest's entries take its visits in the order of the goods."

Step 4: Make supplyCover a template over the good. Make returnUncovered take, for each good, whether to return it, as an array indexed by the good. It walks the queue with queueGoods, counting each good's entries. It drops each entry of a good it returns once that good's count exceeds the good's cover. It sends the dropped visits back as the good's Visits kind, one packet per guest and good, goods in GOODS order and guests ascending. If it drops nothing, it changes nothing.

Step 5: In serveFront, take the front entry's good as the first good in GOODS for which the shop holds a visit of that guest. Serve only when the shop holds a supply of that good. Consume it, create the good's serving, and send the serving and the visit as the good's kinds.

Step 6: Make serveGuests take the shop's stock setting. It returns a good when the shop is starved or doesn't sell it.

Step 7: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: the last line names linking libtpj_sim.a, with no warning above it.

### Task 9: Offer each good with an exact wait

Files:
- Modify: `src/sim/operations/operations.cpp:203-299` and `:417-428`

Step 1: Give nextWait each good's supply, in an array indexed by the good, instead of one stock and shipment list. A good's supply holds its stock, its shipments in arrival order, and the tick an order placed now would arrive, or none when the shop doesn't sell the good. nextWait takes the queue's goods and the arriving guest's good. It walks the queued guests and then the arriving one, as now, each taking its unit from its own good's stock, then that good's shipments, then that good's order. It gives none if a guest's good has no unit left and no order. For a queue of food alone it computes what it does now.

Step 2: Make publishOffer a template over the good, taking whether the shop sells it and the queue's goods. If the shop doesn't sell the good and its position for the good is 0, it publishes nothing. Otherwise its offer is OfferEntry{} unless the shop has a route and nextWait gives a wait. Then the offer is the good's Relief, that wait, and supplied. stepShops publishes each good's offer through forEachGood.

Step 3: Make resolveFoodOffers a template, resolveOffers, over the good. It publishes the offer only for shops that sell the good, with the good's Relief.

Step 4: Define FoodOffer::keepStepped and DrinkOffer::keepStepped through one template: true when the source is a shop box that doesn't sell the good and its position for the good is above 0.

Step 5: In addOperations, after addField<FoodOffer>, add addField<DrinkOffer>. Register the resolver food-offer with resolveOffers for food, as now, and then the resolver drink-offer with resolveOffers for drink and the dependencies path-networks, route-distance, and drink-offer-field.

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | tail -1`
Expected: the last line names linking libtpj_sim.a, with no warning above it.

### Task 10: Show each good in the Inspector and the Debug panel

Files:
- Modify: `src/legible/inspect.cpp:75-81`
- Modify: `src/app/ui/debug_panel.cpp:24-31`

Step 1: Make shopRows give, for each good in GOODS, the rows "<Good> stock" and "<Good> on order", the good's name from enumNames with its first letter capitalized, so Food stock, Food on order, Drink stock, Drink on order. Then Queue, Limit, and Starved.

Step 2: Make the Debug panel's shop line "Shop <key>:", then for each good in GOODS "<name> <stock> + <on order>,", then "queue <queue>, <limit>", built with std::format and shown with ImGui::TextUnformatted. For stocked.park's both shop it reads like "Shop 8: food 24 + 0, drink 24 + 0, queue 0, demand".

Step 3: Build everything.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 11: Send synthetic drink visits

Files:
- Modify: `src/scenarios/food_shop.cpp`

Step 1: In populateFoodShop, after adding the first shop, apply SetShopStock to give it ShopStock::Both. Find its key as the only box in parkBoxes at that point.

Step 2: In stepGuests, when a guest's visit has come back, consume the visits of both kinds it holds as finished, its meals as eaten, and its drinks with a new local cause DRUNK_CAUSE, "drunk". Keep shop boxes as ParkBox, not keys. After picking the shop, pick the good: the one it sells, or for Both, food when drawUniform with index 3 is below 0.5, and drink otherwise. Send that good's visit kind.

Step 3: Update the scenario's comments to say the first shop sells both goods and guests visit for food or drink.

Step 4: Build and run the scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c "warning:\|error:"; build/windows-debug/tpj_scenarios.exe > /dev/null 2>&1; echo $?`
Expected: `0` and then `0`

### Task 12: Make stocked.park

Files:
- Modify: `src/scenarios/slice_parks.h`
- Modify: `src/scenarios/slice_parks.cpp`
- Modify: `src/scenarios/main.cpp`

Step 1: In slice_parks.h, add:

    // Cycles makeStockedPark steps at most before it gives up.
    inline constexpr uint64_t STOCKED_LIMIT_TICKS = 600;

    // Loads the text with makeParkSchema, adds a depot and a backstage path past both shops' backs,
    // resolves it, and steps it with no commands until a shop holds food, a shop holds drink, and
    // drink is in transit, and gives its save. Throws LoadError as loadWorld does, and
    // std::logic_error when the world refuses the depot or the path, or the condition doesn't hold
    // within STOCKED_LIMIT_TICKS cycles.
    std::string makeStockedPark(std::string_view text);

Step 2: In slice_parks.cpp, implement it. Apply each edit only when isAccepted, as full_park.cpp's applyAccepted does, throwing "stocked park: the world refuses the depot" or "the backstage path". The edits are:

- AddBox{BoxKind::Depot, Pose{-17.0, 93.0, 1.0, 0.0}}: its front door, at (-13, 93), is 1 m from the backstage path's paving.
- AddPath{PathKind::Backstage, {NewPoint{{11.0, 107.5}}, NewPoint{{11.0, 93.0}}, NewPoint{{-11.0, 93.0}}, NewPoint{{-11.0, 104.5}}}}: the drink shop's back door, at (8.5, 107.5), and the both shop's, at (-8.5, 104.5), are each 1.5 m from its paving.

Say in a comment that the both shop's route to the depot is short and the drink shop's long, so the both shop's goods arrive first while the drink shop's drink is still on its way. After each cycle, test the condition with unitsHeld of Supplies and of DrinkSupplies at each shop box, and unitsInTransit of DrinkSupplies. If it fails after STOCKED_LIMIT_TICKS cycles, throw "stocked park: no shop held food and drink with drink on its way".

Step 3: In main.cpp, make --slice-parks take seven files, FED WARM CUT MAIN_STREET WARM_MAIN_STREET FACILITIES STOCKED, in the header comment, writeSliceParks' comment, and its message "takes seven files". Read FACILITIES as MAIN_STREET is read, and make the stocked park before writing any file, catching LoadError with FACILITIES' path. Write STOCKED last.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 13: Remake the parks

Files:
- Create: `tests/parks/stocked.park`
- Modify: `tests/parks/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`, `stress/full.park`

Step 1: Remake the slice parks and make stocked.park.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park; echo $?`
Expected: `0`

Step 2: Remake the full park. It takes about four minutes.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench > /dev/null 2>&1; build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

### Task 14: Check that food-only parks are unchanged

Files: none.

Step 1: Count the lines the remade parks add and remove.

Run: `git diff --numstat -- tests/parks`
Expected: `15	0` for each of tests/parks/cut.park, fed.park, stress/full.park, warm-main-street.park, and warm.park.

Step 2: List the added lines, with entity keys replaced.

Run: `git diff -U0 -- tests/parks | grep '^+' | grep -v '^+++' | sed 's/^+[0-9]* /+KEY /' | sort | uniq -c`
Expected: 25 blank `+` lines, 5 each of `+[drink-offer-stepped]`, `+[drink-orders-ledger]`, `+[drink-supplies-ledger]`, `+[drink-visits-ledger]`, and `+[drinks-ledger]`, 20 of `+KEY packets=[] stocks=[] created=0 consumed=[]`, and 5 of `+KEY readable=[] pending=[]`.

### Task 15: Check the goods, the sell-off, and conservation

Files:
- Create: `build/predict-check/two_goods_check.cpp` (a scratch program, not committed)

Step 1: Write a program that loads tests/parks/stocked.park with a schema made by addPark plus its own guest component and a system after addPark's. The system sends the visits the checks below call for, and records each guest's returned visit and serving. Call the box set to Both B and the box set to Drink D. Read an offer as guests.cpp does: sourceEntryAtNode at the shop's firstAnchoredNode on the guest network. In every check, the guest sends its visit with delay 1 in the cycle before the shop first holds it. The park's own guests visit B for food, since opening hours don't hold them back yet. If another guest's visit reaches the shop in the same cycle as a checked visit, the check sends a new visit later instead. The program reads the offer that is readable in the cycle the shop first holds it. That offer is the one the shop published the cycle before. A guest's wait is exact when it first holds its returned visit in that cycle plus the wait plus SERVICE_INTERVAL.

Each check starts from a fresh load and prints its line only if it passes, and otherwise what it saw:

- `orders by setting ok`: at load, D's food position is 0 and its drink position 24. B's position for each good is above 0. D has no food offer, and B and D have drink offers.
- `food wait exact`: one food visit to B.
- `drink wait exact behind food`: a food visit to B, and a drink visit to B arriving the next cycle while the food guest is served.
- `drink wait exact from shipment`: one drink visit to D in the first cycle, while D's drink is still in transit.
- `sell-off ok`: makeCandidate switches B to food. The candidate's drink offer at B equals the world's, and is supplied. A cycle commits the switch, and B still offers drink. Then 30 drink visits go to B, one every 50 cycles. B makes no drink order after the switch. B serves 24 drinks, and 6 visits come back without one. Once all are back, B has no drink offer.
- `conserved`: in every cycle of every check, each of the eight kinds has units created equal to units in transit plus units held by every entity plus units consumed.
- `deleted shop returned every visit`: three food and three drink visits reach B in one cycle, and the next cycle deletes B. Every guest gets its visit back, and every one queued at the deletion comes back without a serving.
- the record: it prints shopRecord for B and D at load.

Step 2: Build and run it.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O0 two_goods_check.cpp ../windows-debug/src/sim/libtpj_sim.a -o two_goods_check.exe && ./two_goods_check.exe ../../tests/parks/stocked.park`
Expected: the seven check lines in the order above, then B's record, with stock or orders of each good, and D's, with food 0 and 0, and drink 0 in stock and 24 on order.

### Task 16: Ask the user to play stocked.park

Files: none.

Step 1: After the merge, rebuild windows-release, and ask the user to play tests/parks/stocked.park. They open the both shop's and the drink shop's Inspectors and see each good's stock and orders, and the Debug panel lists both goods for each shop. With the food overlay on, the drink shop's door is not a food source. Choosing Drink in the both shop's Inspector, the food overlay's preview still shows it as a food source, since it still holds food.

### Task 17: Run the full checks

Files: none.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

### Task 18: Commit

Step 1: Stage only this feature's paths: the files listed in FEATURE.md's Files affected and this feature's plans directory. Commit with the commit-hygiene skill, subject "Operations: Order, hold, and serve drink beside food".
