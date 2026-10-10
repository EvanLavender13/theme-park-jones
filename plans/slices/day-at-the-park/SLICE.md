# Slice: Day at the Park

Status: planned

## Summary

When this slice lands, the park has a day. A clock runs from morning to night, and the player sets the opening hours. Guests arrive through the morning. They eat, drink, sit down, and use the toilet. They go home when they're tired, when a need goes unmet, or when the park closes. The player sets each shop to stock food, drink, or both, and places benches and toilet blocks beside the paths like shops. The player can see why guests left, where each need is poorly served, and the light changing with the hour. This is the right next goal because it turns the boxes-and-tubes loop into management. In that loop, guests have one need and stay for a fixed time. In this slice, how long guests stay depends on how well the layout serves them. The player gets something to improve and a way to see what is failing. The slice deepens believable-guests and plausible-operations through the fields and flows they already share, and it adds the park's first player policies (principle 9).

## End-to-end scenario

A new park opens at 9:00 on its first day, with opening hours of 9:00 to 21:00. At normal speed, one game minute passes each real second, so the open hours take twelve real minutes. A clock shows the time of day, with speed controls beside it for pause, 1x, 2x, and 4x.

The player draws a guest path from the entrance. They place two shops facing it, a depot behind them, and a backstage path between them. They select the first shop and set it to stock food, then set the second to stock drink. Each shop orders only the goods it stocks, and the depot ships both. Further along the path, they place a bench and a toilet block. The bench is a small box with a few seats, and the toilet block has a few stalls. Each snaps to face the path as a shop does, connects to it by itself, and needs no supplies.

Guests arrive along an authored curve stretched over the opening hours. A few come at opening, a rush comes in the late morning, a thinning stream follows into the afternoon, and none come in the last hours before close. Each guest carries four needs, all private to it. Hunger and thirst rise with time. Tiredness rises as the guest walks. The bladder fills with each meal and drink, and more with drinks.

Every facility publishes an offer for each need it answers, stating the relief it gives and the expected wait. A little before each junction, a guest scores those offers the way it scores food offers today (decision 0019). A thirsty guest walks to the drink shop. A tired guest walks to the bench if a seat is likely to be free. A guest who has been drinking walks to the toilet block, and waits at its door if its stalls are full.

Going home is one of those scored options (decision 0019), and it replaces the fixed stay. Its score rises with the guest's tiredness, with the discomfort of needs the guest hasn't met, and as closing time nears. After closing, going home outscores everything else. If the park serves a guest well, the guest stays for hours. If it can't find a toilet, it goes home early. The guest's choice records the head-home terms, and when the guest picks going home, the largest term is its reason for leaving.

The player opens the departures panel. It shows today's departures by reason: tired, closing time, hunger, thirst, or the toilet. Many guests left for the toilet, and only one toilet block serves the whole park. The player switches the overlay to toilets and sees the far end of the park unshaded. Hovering lists the one block's contribution. They click a guest who is heading home, and the inspector shows its needs and the head-home terms that decided it.

The player places a second toilet block at the far end. Before they commit it, the overlay previews the coverage it adds. Later, with the food overlay on, they switch the drink shop to stock both. Before they confirm, the overlay previews the food coverage the shop would add. Once they confirm, the shop starts ordering food beside its drinks. If a shop is switched away from a good, it sells off what it holds of that good and orders no more.

In the afternoon, fewer guests leave for the toilet. The sun lowers and the light warms.

At 21:00 the gate stops admitting guests. The guests still inside walk home, and the park is empty well before midnight. The scene is dark but readable. A dim, cool night light keeps the shapes of boxes and paths clear. The player speeds up to 4x through the night, and the depot keeps restocking the shops. At 9:00 the gate opens, and the first guests of the next day arrive.

## Acceptance criteria

Four park files are checked in under tests/parks:

- day.park holds an entrance, a guest path, a food shop, a drink shop, a bench, a toilet block, a depot, and a backstage path. The two shops stand at opposite ends of the guest path. explained-needs sets each need overlay's discount curve in game time, so each shop's overlay fades before it reaches the other. Its hours are 9:00 to 21:00. It starts at 9:00 with no guests.
- no-toilet.park is day.park without the toilet block.
- noon.park is day.park run to 12:00, with guests in the park.
- evening.park is day.park run to shortly before 21:00, so the cross-build check's run of it crosses closing.

a-day-out makes tpj_scenarios --slice-parks write all four, alongside the boxes-and-tubes parks and warm-main-street.park. Guests change in a-day-out, so it makes every park that holds guests again: warm.park, cut.park, warm-main-street.park, and tests/parks/stress/full.park. The integration tests over them must keep passing, and the runtime report still measures full.park full of guests. A full day is 43200 ticks.

The park world checks and the cross-build check read every park in tests/parks. They check that each park copies, saves, hashes, and replays exactly, on every build players run (decision 0022). Once these four parks are there, those checks cover the slice's new state without a new test. That state includes the opening hours, stock settings, benches and toilet blocks, guests' needs, shops' stock per good, and the departure tally.

a-day-out adds three edits of noon.park to the world checks' chosen edits, so each preview is checked against its commit (principle 8). The edits place a second toilet block, switch the drink shop to stock both goods, and change the opening hours.

1. Removing the toilet block makes more guests leave for the toilet: over one day, more guests leave no-toilet.park with the toilet as their reason than leave day.park for it. (integration test)
2. Removing the toilet block shortens guests' stays: over one day, the mean stay in no-toilet.park is shorter than in day.park. (integration test)
3. Switching noon.park's drink shop to stock food only leaves the guests thirstier: at closing, their mean thirst is higher than in noon.park unchanged. (integration test)
4. A capture of noon.park shows the park in daylight, with guests on the paving and at the facilities' doors. (scripted capture: stylized-presentation, believable-guests)
5. A capture of day.park at 23:00 shows the empty park at night, with every box and path still distinguishable from the ground. (scripted capture: stylized-presentation, believable-guests)
6. A capture of noon.park with the toilet overlay on shows the guest paving shaded around the toilet block. (scripted capture: legible-simulation)
7. A capture of noon.park with the drink overlay on shows the guest paving shaded around the drink shop and not around the food shop. (scripted capture: legible-simulation)
8. In the running app, no guest arrives while the park is closed, every guest admitted has left before the next opening, and guests arrive again after it. (manual)
9. In the running app, if the player switches a shop away from drink, the shop places no more drink orders. Its drink offer ends once it has served the drink it held and the drink already on its way, and it serves no drink after that. (manual)
10. In the running app, hovering a need's overlay lists each facility's contribution, and the contributions match the value shown, to the precision shown. (manual)
11. In the running app, these work as the scenario describes: the speed controls and the clock, setting the opening hours with the arrival curve previewed, placing benches and toilet blocks facing the paths, setting a shop's stock with its preview, undoing and redoing each of these, switching the overlay between needs with hover attribution, the departures panel, and the guest, bench, and toilet block inspectors. (manual)

## Medium

Abbreviations: DS is deterministic-simulation, SM is shared-medium, NN is navigable-networks, BG is believable-guests, PO is plausible-operations, LS is legible-simulation, EB is effortless-building, and SP is stylized-presentation.

- Need offers: entry fields on guest network nodes, one for each need, produced by PO. food-offer is hunger's and keeps its name, and drink-offer, rest-offer, and toilet-offer join it, so every saved park still loads. BG consumes them for choice, and LS consumes them for overlays and attribution. Each facility publishes an entry at its guest anchor in the field of each need it answers. Each entry holds the relief a serving gives, the expected wait, and whether it is available. A shop answers hunger while its stock setting names food, or while it still holds or awaits food supplies. It answers thirst the same way for drink. If a shop is switched away from a good, it sells off what it holds through its ordinary offer. A bench answers rest, and a toilet block answers the toilet.
- Route distance: an entry field over the networks, produced by NN and unchanged in kind. BG consumes it for choice, the route a guest walks, and going home. A guest walks on the paving along its route, which is traveling that route (decision 0038). PO consumes it for supply routes, and LS for the overlay discount and preview context. Benches and toilet blocks become sources once they anchor guest nodes.
- Networks: the guest and backstage graphs, a shared structure that fields and flows live on, unchanged. NN produces them through SM's network type, and every capability consumes them as before. The front doors of benches and toilet blocks serve the guest network, with a connector and a walkway each, as shops' front doors do.
- Guest visits: a flow of guests into a facility's queue and back out, produced by BG and consumed by PO. PO returns every guest to BG, served or unserved. Shops, benches, and toilet blocks receive visits alike. There is one guest-visit kind per need, so each visit names the need the guest came for. guest-visits stays the kind for food, and meals its serving, under their saved names. If a shop stocks both goods, the visit tells it whether to serve a meal or a drink, without reading the guest. PO serves a visit only with the serving of the need it names.
- Servings: conserved flows, one kind per need, produced by PO and consumed by BG. Each serving goes from a facility to the guest it served, sent with the returned visit. A shop makes each meal or drink from one supply of its good. A bench or toilet block creates a rest or a toilet use when it releases the guest, so benches and toilets need no supplies. BG applies the relief to the guest's own need, so needs stay private to the guest.
- Supplies and supply orders: conserved flows, one pair for each good (food and drink), produced and consumed by PO through SM's ledger. They run between shops and the nearest depot along the backstage network.
- Hungry footfall: a field on guest network edges, unchanged. BG produces it, and LS consumes it for a shop ghost's context.

Some dependencies are not fields or flows. Decision 0025 rules on the first three. The clock is part of the tick cycle itself.

- Park intent: the opening hours, each shop's stock setting, and bench and toilet block boxes. EB authors them, and they are saved (principle 1). BG reads the hours for arrivals and the closing term. PO reads stock settings. NN derives doors from the boxes, and LS picks and previews them. Changing the hours or a stock setting is an edit command, applied between ticks like any other edit, and undo and redo restore it like any other edit. If a park has no opening hours set, it starts at 9:00 with the default hours, so guests still start arriving at once in the parks made before this slice. If a shop has no stock setting, it stocks food only, as shops do today.
- Candidate resolution: a stock change or a new facility previews on a candidate world, as placements do today. NN and PO resolve the candidate, and LS samples it through the field interface. An hours change previews the same way. BG's arrival-curve record is a function of the hours in intent. LS reads the curve from a candidate world with the tentative hours applied, and never computes the curve itself.
- Inspection records: BG and PO publish these, and only LS, tests, and the app read them. BG publishes a guest's needs and its last choice with the head-home terms, the departure tally of today's departures by reason, and the arrival curve it admits guests by. PO publishes a bench's and a toilet block's occupancy and queue, and a shop's stock per good. The hours panel previews the arrival curve from its record.
- The clock: the time of day and the day number. DS provides it as a pure function of World::Tick: tick 0 is 9:00 on day 1. Every system already reads the tick as part of the cycle, so the clock adds no interaction between entities. BG reads it for arrivals and closing, SP for the light, and LS for the hours panel. The app shows it in a Time panel with the speed controls. Rendering only reads it and never feeds anything back (principle 10). The speed controls change only how many ticks a frame steps, so a park's hash at a given tick is the same at any speed.

## Members

The whole slice builds on good-paths, which is complete: wide paths, the frontage snap, undo and redo, and guests walking on the paving.

1. `deterministic-simulation/park-clock`: The clock gives the day number and time of day as a function of the tick, and the app gets pause, 1x, 2x, and 4x speed. Depends on: none.
2. `effortless-building/facilities-and-hours`: The player places benches and toilet blocks, which front the paths with the frontage snap as shops do, and sets the opening hours and each shop's stock, all as saved intent that undo and redo cover. Depends on: member 1.
3. `navigable-networks/facility-doors`: Benches and toilet blocks connect to nearby guest paths by their front doors and become route distance sources. Depends on: member 2.
4. `plausible-operations/served-needs`: Shops order and serve food and drink as separate goods, benches and toilet blocks hold guests in seats and stalls, and every facility publishes need offers. Depends on: members 2 and 3.
5. `believable-guests/a-day-out`: Guests arrive by the curve over the opening hours, carry four needs, and weigh need offers against going home. Depends on: members 1, 2, and 4.
6. `stylized-presentation/day-and-night` (capability to be planned): The sun, sky, and a hemispheric ambient light change with the time of day alone, and night keeps a dim, cool light that stays readable. Depends on: member 1.
7. `legible-simulation/explained-needs`: Each need gets an overlay with hover attribution, previews, and an --overlay option, with its discount curve set in game time so day.park's two shops shade apart, and the player gets the departures panel, inspectors for guests, benches, and toilet blocks, and an hours panel that previews the arrival curve. Depends on: members 1 through 5.

## Out of scope

- Money, prices, and guest budgets, which are a later slice.
- Staff, cleaning, and dirty toilets.
- Accidents, vomit, and nausea.
- Placeable lamps and lit buildings at night, and a toggle to keep the view in daylight.
- Shops closing at night.
- Weather and temperature.
- Ingredients and couplings between goods, such as salty food raising thirst.
- Guests buying in passing without choosing.
- Reputation, and arrivals that depend on the park.
- Guest groups and personality.
- Supplies for benches or toilets.
- Guests lining up in front of a facility's door. Guests waiting at a bench or toilet block stand at its door, as guests waiting at a shop do today.
- A guest's walking speed changing as it tires.
- Measuring distance across the paving instead of along the network (decision 0038).
- Hungry footfall for other needs, and a ghost's context for needs other than hunger.
- Player-facing UI. ImGui panels stand in, and the question stays open (docs/open-questions.md).

## Open questions

- Tuning values: need rates, couplings, curve shapes, head-home weights, seats and stalls, sitting and use times, and the arrival curve, all in game hours. They are set while planning a-day-out and served-needs, and adjusted by watching day.park through a day.

## Research notes

- No shipped park game scores going home against a guest's other options. RCT decides leaving by fixed floors and a random roll. Planet Coaster 2 builds up fatigue at a rate set by the guest's mood bucket. Scoring going home as an option makes the pull to leave rise smoothly, and records its cause.
- Needs become decisions rather than chores when they affect each other, depend on where things are, and have visible causes. If guests use a bench only when they happen to step on it, benches feel pointless. Here facilities publish offers that guests weigh from a distance, as The Sims' advertisements do.
- Opening hours give the player a decision only if they drive arrivals and departures. Cosmetic hours and skipped nights give none. Real parks fill in the late morning, peak in the early afternoon, and empty in waves, ending in a rush out at closing.
- Night stays readable when a hemispheric ambient light keeps a floor that is cooler and less saturated, not just darker. Uniform dimming is the genre's most common complaint about night.

Depth is in RESEARCH.md.
