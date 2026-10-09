# Feature: Two Goods

## Summary

Shops order, hold, and serve food and drink as separate goods, by the stock setting the player gives each shop. Drink gets its own orders, supplies, visits, servings, and offer: drink-orders, drink-supplies, drink-visits, drinks, and drink-offer. Food keeps its names, so every saved park loads and every park whose shops all stock food simulates as before. Shop code runs once per good over one list of goods, so a later good adds an entry and its own kinds, not new code paths. A shop is one counter: it serves its queue in arrival order, whatever good each guest came for, and each offer's wait stays exact. If the player switches a shop away from a good, it stops ordering it and sells off what it holds or awaits. The preview and the committed switch both show it still offering the good, because the field keeps that offer through the resolution. The Inspector and the Debug panel show each good's stock and orders. The food-shop scenario's synthetic guests also visit for drink, and tests/parks/stocked.park holds both goods.

## Acceptance criteria

1. A shop orders each good its stock setting names, by its own (s, S) policy, from its nearest depot, and a depot ships both. A shop set to drink orders no food, publishes no food offer, and is not a food source for guests or the food overlay. (check: the scratch program prints `orders by setting ok`; play: the user opens each shop's Inspector in stocked.park, and turns on the food overlay to see that the drink shop's door is not a source)
2. If the player switches a shop away from a good, the shop places no more orders for it. It keeps offering and serving the good from what it holds and what is on its way, and returns at once, unserved, the visits for it beyond those. When it neither holds nor awaits the good, its offer of the good ends. A candidate that switches the shop still has the drink offer the shop last published, and after the committed switch the shop still offers drink. (check: the scratch program in PLAN.md task 15 prints `sell-off ok`)
3. A shop serves its queue in arrival order, one guest per SERVICE_INTERVAL, across both goods. Each guest takes one supply of the good its visit names, and its visit comes back with a meal or a drink. Each offer's wait is exact for the next guest to arrive for that good. That holds when the guest ahead came for the other good, and when the good is still on its way. (check: the scratch program prints `food wait exact`, `drink wait exact behind food`, and `drink wait exact from shipment`)
4. Drink units conserve with the ledger's identity, as food's do, every tick. When a shop is deleted, the guests queued there for either good get their visits back unserved. (check: the scratch program prints `conserved` and `deleted shop returned every visit`; the food-shop scenario, which now sends drink visits, runs under tpj_scenarios)
5. A shop's record gives its stock and supplies on order of each good. The Inspector shows both goods' stock and orders, and the Debug panel lists them for each shop. (check: the scratch program prints both shops' records; play: the user opens the Inspector and the Debug panel in stocked.park)
6. Every park whose shops all stock food simulates as before. Each slice park remade by tpj_scenarios --slice-parks, and tests/parks/stress/full.park remade by tpj_bench --full-park, differs from the checked-in file only by added lines: the empty sections of drink-orders-ledger, drink-supplies-ledger, drink-visits-ledger, drinks-ledger, and drink-offer-stepped. (check: PLAN.md task 17's diff removes no line)
7. tests/parks/stocked.park is made by tpj_scenarios --slice-parks from facilities.park with a depot and backstage path added. It is stepped until a shop holds food, a shop holds drink, and drink is still on its way. The park world checks pass over it. (ctest)
8. src/sim/operations/SPEC.md describes shops with two goods. src/sim/medium/SPEC.md states when a field's owner keeps a source's stepped entries. The legible, app, and scenarios specs describe their changes. (check: read each beside the code)

## Tests

None. The park world checks cover the new state through stocked.park, and they already switch a shop with waiting guests to drink, which now runs the sell-off through copies, saves, and candidates. The slice adds the integration test that compares a park with a shop's stock changed.

## Medium

- drink-offer (entry field, new): each shop that sells drink, or still holds or awaits it, publishes one OfferEntry at its guest anchor. Nothing samples it yet. believable-guests reads it in a-day-out.
- food-offer (entry field): now published only by shops that sell food, or still hold or await it. believable-guests and legible's food availability sample it as before.
- drink-visits and drinks (flows, new): sent by the food-shop scenario's synthetic guests, and returned to them with a drink when served. Each unit's handle is its guest.
- guest-visits and meals (flows): unchanged.
- drink-orders and drink-supplies (flows, new, inside operations): between a shop and its nearest depot, as supply-orders and supplies run.
- Route distance (backstage) and guest anchors: sampled as before.
- Park intent: each shop's stock setting, read through parkBoxes.
- Inspection records: shopRecord gains each good's stock and orders, read by legible's Inspector and park summary, and the render module's starved mark, which reads only Starved.
- Medium rule: a field's owner may give it a keepStepped rule. The field's finisher then keeps a source's stepped entries through a resolution that changes its resolved entries when the rule says to. Operations gives both offers the rule: keep a shop's offer of a good its setting no longer names while it holds or awaits that good.

The new state is the four drink ledgers and drink-offer's stepped entries. Every resolved world holds them. tests/parks/stocked.park holds them with units in them, so the park world checks cover them.

## Spec changes

- src/sim/operations/SPEC.md: Boundary lists the drink kinds and drink-offer. A new Goods section says each good has its own kinds and offer. Supply orders by good, and switching away stops orders. Service is one counter across goods, with a guest's food visits taken first, and the cover rule for a starved shop or a good no longer named. Food offer becomes Offers, with the sell-off. Inspection gives each good's stock and orders, and the Supply factor per good. PLAN.md task 1 has the text.
- src/sim/medium/SPEC.md: the field owner's keepStepped rule (task 2).
- src/legible/SPEC.md: the Inspector shows each good's stock and orders (task 3).
- src/app/SPEC.md: the Debug panel lists each good's stock and orders (task 3).
- src/scenarios/SPEC.md: the food-shop scenario sells both goods, and --slice-parks makes stocked.park (task 4).

## Files affected

- Modify: src/sim/medium/field.h
- Modify: src/sim/medium/SPEC.md
- Modify: src/sim/operations/operations.h
- Modify: src/sim/operations/operations.cpp
- Modify: src/sim/operations/SPEC.md
- Modify: src/legible/inspect.cpp
- Modify: src/legible/SPEC.md
- Modify: src/app/ui/debug_panel.cpp
- Modify: src/app/SPEC.md
- Modify: src/scenarios/food_shop.cpp
- Modify: src/scenarios/slice_parks.h
- Modify: src/scenarios/slice_parks.cpp
- Modify: src/scenarios/main.cpp
- Modify: src/scenarios/SPEC.md
- Create: tests/parks/stocked.park
- Modify: tests/parks/fed.park, warm.park, cut.park, warm-main-street.park, stress/full.park (remade)
- Modify: plans/plausible-operations/served-needs/MILESTONE.md (the Medium bullet names the owner's rule)

## Dependencies

- effortless-building/facilities-and-hours: each shop's stock setting and SetShopStock. Merged.
- plausible-operations/supplied-food-shop: the shop, depot, food offer, and food-shop scenario. Complete.

## Out of scope

- Real guests visiting for drink. a-day-out gives guests thirst and sends drink-visits.
- A drink overlay. explained-needs adds need overlays.
- Benches and toilet blocks serving guests: seats-and-stalls.
- A counter for each good: a deepening candidate in MILESTONE.md.

## Open questions

None.
