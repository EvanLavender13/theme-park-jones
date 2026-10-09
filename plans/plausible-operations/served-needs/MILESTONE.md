# Milestone: Served Needs

Slice: day-at-the-park

Status: planned

## Summary

served-needs makes every facility serve a need. Shops order, stock, and serve food and drink as separate goods, by the stock setting the player gives each shop. Benches hold guests in seats and toilet blocks hold them in stalls, for a fixed time, with no supplies. Every facility publishes an offer for each need it answers, with an exact expected wait, so guests can weigh them from a distance. Food keeps the names it has today, so every saved park still loads and guests still eat as they do now. It is member 4 of day-at-the-park. facilities-and-hours gave the boxes and stock settings, facility-doors put benches and toilet blocks on the guest network, and a-day-out needs these offers and servings before guests can have four needs.

## Acceptance criteria

1. A shop orders, holds, and serves food and drink as separate goods. It orders a good while its stock setting names it, each good by its own (s, S) policy from the nearest depot, and a depot ships both. A shop set to food behaves exactly as shops do today. (play: set a shop's stock in its Inspector and watch the Inspector's stock of each good; check: stocked.park, criterion 8)
2. A shop publishes a food offer while its stock setting names food, or while it still holds or awaits food, and a drink offer the same way for drink. If the player switches it away from a good, it places no more orders for that good. It keeps offering and serving the good until it has served what it held and what was on its way. It returns visits for that good beyond those at once, unserved, and then its offer for that good ends. A candidate that switches a shop's stock publishes the same offers the committed switch gives, so the preview shows a shop that still holds food keeping its food offer. (check: the scratch program of criterion 3 switches stocked.park's both-stocked shop to food, sends drink visits, and checks the drink offer, the drinks served, and the visits returned, tick by tick)
3. A shop serves its queue in arrival order, one guest per SERVICE_INTERVAL. Each guest takes one supply of the good its visit asks for, and its visit comes back with a meal or a drink. Each offer's wait is exact for the next guest to arrive for that good. (check: a scratch program steps tests/parks/stocked.park, sends one visit for each good, reads the offer the tick before, and compares its wait with the tick the shop takes that guest)
4. A bench seats BENCH_SEATS guests for SITTING_TIME, and a toilet block holds TOILET_STALLS guests for TOILET_TIME. Guests beyond those wait at the door in arrival order, and none is turned away. When a guest's time is up, its visit comes back with a rest or a toilet use. Each publishes a rest or toilet offer whose wait is exact for the next guest to arrive. (check: the same scratch program, sending visits to the bench and the toilet block until guests wait at the door)
5. Every unit conserves with the ledger's identity: drink orders and supplies, and the visits and servings of every need. If a bench or toilet block is deleted, the guests queued there get their visits back unserved. (check: the scratch program checks, every tick, that each kind's units created equal those in transit, held, and consumed, while it deletes a shop, a bench, and a toilet block; the cross-build check runs the food-shop scenario)
6. Each shop's inspection record gives its stock and supplies on order for each good, and the Inspector shows both. Benches and toilet blocks publish records of their places, how many are taken, and their queue (decision 0025). (play: open a both-stocked shop's Inspector in stocked.park; check: the scratch program of criterion 3 reads the bench's and toilet block's records while guests wait at the door)
7. Every park whose shops all stock food simulates as before. Each slice park remade by tpj_scenarios --slice-parks, and tests/parks/stress/full.park remade by tpj_bench --full-park, differs from the checked-in file only by the new kinds' empty ledger and offer sections, which every resolved world holds. (diff against tests/parks: no line removed or changed, and every added line is in a new kind's empty section)
8. tests/parks/stocked.park is made by tpj_scenarios --slice-parks from facilities.park with a depot and backstage path added, run until its shops hold food and drink and a drink shipment is on its way. The park world checks pass over it, so they cover the new drink state and the rest and toilet offers. (ctest)
9. src/sim/operations/SPEC.md describes the module as built, src/sim/medium/SPEC.md states when a resolver keeps a source's stepped entries, and the other specs the features touch describe their changes. (check: read each beside the code)

## Medium

Cross-capability, as the slice's medium map assigns:

- Need offers (entry fields at each facility's guest anchor, resolved and stepped): food-offer stays as it is and is hunger's offer. two-goods adds drink-offer, and seats-and-stalls adds rest-offer and toilet-offer. Each entry is the OfferEntry food-offer has today: the relief a serving gives, the wait, and whether the facility is supplied. believable-guests reads food-offer as today, and reads the others in a-day-out. legible-simulation reads them for need overlays in explained-needs.
- Guest visits (flows, one kind per need): guest-visits stays the kind for food. two-goods adds drink-visits, and seats-and-stalls adds rest-visits and toilet-visits. Each unit's handle is its guest. Synthetic guests send them in this milestone, and believable-guests sends the new ones in a-day-out. Every visit comes back to its guest, served or not.
- Servings (flows, one kind per need): meals stays the kind for food. two-goods adds drinks, and seats-and-stalls adds rests and toilet-uses. Each is sent to the guest served, with its visit.
- Candidate resolution (decision 0025): a stock change or a new facility previews on a candidate world, as placements do today. Resolution stays intent-only: it publishes a resolved offer only for each good a shop's stock setting names, as for an empty shop, from intent and route distance. Today, when a command changes a source's resolved entries, the medium clears its stepped entries. two-goods lets a resolver keep a source's stepped entries through that change, and a shop keeps the stepped offer of a good it no longer orders while it still holds or awaits that good. So a candidate and the committed switch both sample the sell-off offer from the last step, and the shop's next step republishes it from its state (principle 8). The resolved layer still depends on intent alone, so a save loads and resolves equal mid-sell-off. A starved shop's offers all say it has none, as its food offer does today. src/sim/medium/SPEC.md gains the rule.
- Route distance (navigable-networks): sampled as today. Benches and toilet blocks find their guest anchors from facility-doors' connectors.
- Inspection records (decision 0025): shop records with stock per good, and bench and toilet block records. legible-simulation's Inspector reads the shop record now, and the bench and toilet block records in explained-needs.
- Park intent (decision 0025): shop, depot, bench, and toilet block boxes, and each shop's stock setting, through parkBoxes.

Internal to this capability:

- Supplies and supply orders (flows, one pair per good): supply-orders and supplies stay food's. two-goods adds drink-orders and drink-supplies, between a shop and its nearest depot, as food's run today.

The saved names stay because a stepped offer and every ledger is saved state, and the loader refuses a section it doesn't know (RESEARCH.md). The slice's medium map says need offers are one field per need for the same reason.

Every resolved world holds each new kind's ledger and stepped offer, empty or not. Only the seat and stall state needs guests: no park holds a guest at a bench or toilet block until a-day-out's parks do. Until then the park world checks name that one component as unheld, with that reason. a-day-out removes the name once noon.park holds it.

## Dependencies

- effortless-building/facilities-and-hours: bench and toilet block boxes, and each shop's stock setting. Complete.
- navigable-networks/facility-doors: benches and toilet blocks anchored on the guest network. guest-doors merged; the milestone closes when the user has played it.
- plausible-operations/supplied-food-shop: the shop, depot, food offer, and food-shop scenario this milestone extends. Complete.

## Core feature

`two-goods` is the core. The player sees it at once: a shop set to drink orders drink, its Inspector shows drink stock, and it drops out of the food overlay once its food is gone, so guests stop going there to eat. It also builds the per-need offers, visits, and servings that seats-and-stalls repeats for rest and the toilet.

## Features

1. `two-goods`: food and drink as separate goods, with drink-orders, drink-supplies, drink-visits, drinks, and drink-offer. A shop orders by its stock setting and sells off a good it is switched away from. It serves one queue across both goods. Its record and Inspector show each good. The food-shop scenario's synthetic guests also visit for drink, and tests/parks/stocked.park is added. Depends on: none.
2. `seats-and-stalls`: seats on benches and stalls in toilet blocks, with rest-visits, rests, toilet-visits, toilet-uses, rest-offer, and toilet-offer. Each offer has its exact wait. Benches and toilet blocks publish records, and the food-shop scenario's synthetic guests visit them. Depends on: feature 1.

Neither feature adds a test (docs/testing.md). stocked.park brings the new drink state under the park world checks. The slice adds the integration tests that compare parks with and without a toilet block, and with a shop's stock changed.

## Tuning values

Each is set here, and adjusted while planning a-day-out by watching day.park through a day. Times are in ticks, 2 game seconds each.

- BENCH_SEATS: 3, about one person per 0.6 m of a 2 m bench.
- SITTING_TIME: 300 ticks, 10 game minutes.
- TOILET_STALLS: 4.
- TOILET_TIME: 90 ticks, 3 game minutes, the same as SERVICE_INTERVAL.
- DRINK_RELIEF: 0.5, as MEAL_RELIEF.
- REST_RELIEF: 0.5.
- TOILET_RELIEF: 1, so one use answers the need in full.
- Drink orders by the same REORDER_POINT and ORDER_UP_TO as food.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Sitting and toilet times that scale with how great the guest's need is, as RollerCoaster Tycoon's do, if fixed times look wrong in play. Gated on: a-day-out's play.
- A shop with a counter for each good, as a larger shop kind the player chooses, if one counter for both proves too slow.

## Open questions

None.

## Research notes

- food-offer and the food flows keep their names, and drink, rest, and the toilet get their own fields and flows, because a stepped offer and each ledger is saved and an unknown section refuses to load.
- A shop is one counter for both goods, so stocking both is a trade-off, not a free gain.
- A bench or toilet block is a counter with several places and a fixed time, and its wait stays exact.

Depth is in RESEARCH.md.
