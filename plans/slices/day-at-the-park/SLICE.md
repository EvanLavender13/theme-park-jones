# Slice: Day at the Park

Status: planned

## Summary

The park gets a day. A clock runs from morning to night, the player sets the hours the park is open, and guests arrive over the morning, eat, drink, sit down, and use the toilet, and go home when they are tired, when a need goes unmet, or when the park closes. Shops stock food, drink, or both, and benches and toilet blocks are placed beside the paths like shops. The player reads why guests left, sees where each need is poorly served, and watches the light change with the hour. It is the right next goal because it turns the boxes-and-tubes loop, one need with a fixed stay, into management: how long guests stay now depends on how well the layout serves them, so the player has something to improve and a way to see what is failing. It deepens believable-guests and plausible-operations through the medium they already share, and adds the park's first player policies (principle 9).

## End-to-end scenario

A new park opens at 9:00 on its first day, with opening hours of 9:00 to 21:00. One game minute passes per real second at normal speed, so the open hours take twelve real minutes, and pause, 1x, 2x, and 4x speed controls sit beside a clock showing the time of day.

The player draws a guest path from the entrance and places two shops beside it, a depot behind them, and a backstage path between them. They select the first shop and set it to stock food, and the second to stock drink. Each shop orders only the goods it stocks, and the depot ships both. Further along the path they set a bench, a small box with a few seats, and a toilet block with a few stalls. Both connect to the guest path by themselves. Neither needs supplies.

Guests arrive along an authored curve stretched over the opening hours: a few at opening, a rush in the late morning, a thinning stream into the afternoon, and none in the last hours before close. Each guest carries hunger, thirst, tiredness, and a bladder, all private. Hunger and thirst rise with time, tiredness rises as the guest walks, and every meal and drink fills its bladder, drinks more than meals. Every facility publishes an offer for each need it answers, with the relief it gives and the expected wait, and a guest scores those offers at each junction as it scores food offers today (decision 0019). So a thirsty guest walks to the drink shop, a tired one to the bench if a seat is likely, and one that has been drinking to the toilet block, queuing when its stalls are full.

Going home is one of those scored options (decision 0019) with no fixed stay behind it. Its score rises with the guest's tiredness, with the discomfort of needs it has not met, and as closing time nears, and after closing it dominates. A guest in a park that serves it well stays for hours, and one that cannot find a toilet goes home early. The guest's choice records the head-home terms, and the largest one when it picks going home is its reason for leaving.

The player opens the departures panel and sees today's departures by reason: tired, closing time, hunger, thirst, or the toilet. Many left for the toilet, and only a single toilet block serves the whole park. They switch the overlay to toilets and see the far end of the park unshaded. Hovering lists the one block's contribution. Clicking a guest heading home shows its needs and the head-home terms that decided it. The player places a second toilet block at the far end, with the overlay previewing the coverage it adds before they commit it. Later, with the food overlay on, they switch the drink shop to stock both. The overlay previews the food coverage the shop would add before they confirm, and once confirmed the shop starts ordering food beside its drinks. A shop switched away from a good sells off what it holds of it and orders no more.

In the afternoon the departures for the toilet slow. The sun lowers and the light warms. At 21:00 the gate admits no one. The guests still inside walk home, and the park is empty well before midnight. The scene is dark but readable: lit by a dim, cool night light, boxes and paths keep their shapes. The player speeds up to 4x through the night, the depot keeps restocking the shops, and at 9:00 the gate opens and the first guests of the next day arrive.

## Acceptance criteria

Two park files are checked in: tests/parks/day.park, with an entrance, a guest path, a food shop, a drink shop, a bench, a toilet block, a depot, and a backstage path, hours of 9:00 to 21:00, starting at 9:00 with no guests, and tests/parks/no-toilet.park, which is day.park without the toilet block. tpj_scenarios --slice-parks regenerates both, alongside the boxes-and-tubes parks. A full day is 43200 ticks. Integration tests run each park for the shortest length that shows their property.

1. Running day.park from 9:00 through the next day's opening: no guest arrives while the park is closed, every guest admitted has left before the next opening, and guests arrive again after it. (integration test)
2. Supplies of each good, meals, drinks, guest visits to every kind of facility, and the servings of benches and toilet blocks are conserved in every tick of a day of day.park and of no-toilet.park. (integration test)
3. Running day.park and no-toilet.park through one day: more guests leave no-toilet.park with the toilet as their reason, and its guests' mean stay is shorter. (integration test)
4. Switching day.park's drink shop to stock food only at noon, compared with running day.park unchanged: the shop places no drink order after the switch, its drink offer ends once the drink it held and had on its way is served, it serves no drink after that, and at closing the mean thirst of the guests in the park is higher. (integration test)
5. Each need's availability at every sampled place can be reconstructed exactly from its attributed per-facility contributions. (module tests: legible-simulation's member)
6. In day.park, the preview of placing a second toilet block, and the preview of switching a shop's stock, each equal the resolved park immediately after the edit is committed. (integration test)
7. Loading a saved park and saving it again gives an identical file, and a world regenerated from a save equals the world saved, for both parks at noon and at night, including hours, stock settings, and the departure tally. (integration test)
8. Two runs of a park give identical state hashes, and the cross-build check runs day.park and no-toilet.park through a full day and gets the same hash at every tick on the Windows and Linux builds (decision 0022). (integration test, plus the cross-build check)
9. A capture of day.park at 12:00 shows the park in daylight, with guests on the paths and at the facilities. A capture at 23:00 shows the empty park at night with every box and path still distinguishable from the ground. (scripted capture: stylized-presentation, believable-guests)
10. A capture of day.park at 12:00 with the toilet overlay on shows coverage around the toilet block, and one with the drink overlay on shows coverage around the drink shop and none around the food shop. (scripted capture: legible-simulation)
11. In the running app, the speed controls, the clock, setting the opening hours with the arrival curve previewed, placing benches and toilet blocks, setting a shop's stock with its preview, switching the overlay between needs with hover attribution, the departures panel, and the guest, bench, and toilet inspectors all work as the scenario describes. (manual)

## Medium

Fields and flows between capabilities, with the abbreviations of the boxes-and-tubes slice, and SP for stylized-presentation.

- Need offer: entry field on guest network nodes, replacing the food offer. Each facility publishes, at its guest anchor, one entry per need it answers: the need (hunger, thirst, rest, or toilet), the relief a serving gives, the expected wait, and whether it is available. A shop answers hunger while its stock setting names food or it still holds or awaits food supplies, and thirst likewise for drink, so a shop switched away from a good sells off what it holds through its ordinary offer. A bench answers rest, and a toilet block the toilet. Produced by PO. Consumed by BG (choice) and LS (overlays, attribution).
- Route distance: entry field over the networks, unchanged, with benches and toilet blocks as sources once they anchor guest nodes. Produced by NN. Consumed by BG (choice, movement, going home), PO (supply routes), LS (overlay discount, preview context).
- Networks: the guest and backstage graphs, unchanged, with the front doors of benches and toilet blocks serving the guest network. Produced by NN, through SM's network type. Consumed by every capability as before.
- Guest visits: flow of guests into a facility's queue and back out, to shops, benches, and toilet blocks alike. A visit names the need its guest chose the facility's offer for, as one guest-visit kind per need, so a shop stocking both goods knows whether to serve a meal or a drink without reading the guest. Produced by BG, consumed by PO, which returns every guest to BG, served or unserved, and serves a visit only with the serving of the need it names.
- Servings: flows, conserved, one kind per need, from a facility to the guest it served, sent with the returned visit. Meals and drinks are made one for one from supplies of their good. Rests and toilet uses are created by the bench or toilet block when it releases the guest, so benches and toilets need no supplies. Produced by PO, consumed by BG, which applies the relief to its own need (needs stay private to the guest).
- Supplies and supply orders: flows, conserved, one pair per good (food and drink), from shops to the nearest depot and back along the backstage network. Produced and consumed within PO, through SM's ledger.
- Hungry footfall: unchanged. Produced by BG, consumed by LS for a shop ghost's context.

Dependencies that are not fields or flows. Decision 0025 rules on the first three. The clock is the cycle itself.

- Park intent: opening hours, a shop's stock setting, and bench and toilet block boxes, authored by EB and saved (principle 1). BG reads the hours for arrivals and the closing term. PO reads stock settings. NN derives doors and LS picks and previews from them. Changing the hours or a stock setting is an edit command, applied between ticks like any other edit.
- Candidate resolution: a stock change or a new facility previews on a candidate world as placements do today, resolved by NN and PO, and sampled by LS through the field interface. An hours change previews the same way: BG's arrival-curve record is a function of the hours in intent, so LS reads it from a candidate world with the tentative hours applied and never computes the curve itself.
- Inspection records: a guest's needs and its last choice with the head-home terms, a bench's and a toilet block's occupancy and queue, a shop's stock per good, the departure tally of today's departures by reason, and the arrival curve BG admits guests by, published so the hours panel can preview it. Produced by BG and PO. Read only by LS, tests, and the app.
- The clock: the time of day and the day number, a pure function of World::Tick and the new-park start time, provided by DS. The tick is already part of the cycle every system reads, so the clock adds no interaction between entities. BG reads it for arrivals and closing, SP for the light, and LS for the clock panel. Rendering reads it one way only and never feeds back (principle 10).

## Members

Ordered for building. Milestone slugs are provisional until each capability plans them.

Prerequisites, not members, since they add nothing to the scenario: sound-architecture's `layered-dependencies` and `composed-app`. Decision 0027 forbids new concerns in src/app/main.cpp until composed-app restructures it, and this slice adds speed controls and panels. The simulation parts of members 1 through 5 can start before composed-app lands. Their app parts, and members 6 and 7, wait for it.

1. `deterministic-simulation/park-clock`: the clock as a function of the tick, with day number and time of day, a new park starting at 9:00 on day 1, and pause, 1x, 2x, and 4x speed in the app's frame clock, which changes only how many ticks a frame steps, so a park's hash at a tick is the same at any speed. Depends on: none, and composed-app for the speed controls.
2. `effortless-building/facilities-and-hours`: bench and toilet block box kinds with their sizes, placing, moving, and deleting them with ghosts, drawn as boxes, a shop's stock setting and the park's opening hours as intent with their edit commands, saved and loaded, and the tooling controls that issue those edits. Depends on: member 1, and composed-app for the controls.
3. `navigable-networks/facility-doors`: the front doors of benches and toilet blocks serving the guest network, so they connect to nearby guest paths and become route distance sources. Depends on: member 2.
4. `plausible-operations/served-needs`: food and drink as separate goods with orders and supplies per good, shops ordering only the goods their stock setting names and selling off what they hold of the rest, each visit served with the serving of the need it names, drink service, benches with seats and toilet blocks with stalls that hold each guest for a sitting or use and return it with a serving, the need offer replacing the food offer, and inspection records for benches, toilet blocks, and shop stock per good. Depends on: members 2 and 3.
5. `believable-guests/a-day-out`: thirst, tiredness, and the bladder with their couplings, choice over need offers, going home scored by tiredness, unmet needs, and closing time instead of a fixed stay, arrivals by the authored curve over the opening hours, the departure tally by reason, tuning to game hours, and tpj_scenarios --slice-parks writing day.park and no-toilet.park. Depends on: members 1, 2, and 4.
6. `stylized-presentation/day-and-night` (capability to be planned): sun direction and color, sky color, and a hemispheric ambient authored as ramps over the time of day, with a cool, readable night floor and a dim moon light, read from the clock alone. Depends on: member 1, and composed-app.
7. `legible-simulation/explained-needs`: an overlay per need with a selector, hover attribution, and --overlay options for each need, previews of stock changes and new facilities, inspectors showing every need and the head-home terms and for benches and toilet blocks, the departures panel, the clock and hours panel with the arrival curve previewed for tentative hours. Depends on: members 1 through 5, and composed-app.

The boxes-and-tubes parks are regenerated in member 5, and their integration tests keep passing; a park with no opening hours set starts at 9:00 with the default hours, so their guests still arrive at once.

## Out of scope

Money, prices, and guest budgets, a later slice. Staff, cleaning, and dirty toilets. Accidents, vomit, and nausea. Placeable lamps and lit buildings at night, and a toggle to keep the view in daylight. Shops closing at night. Weather and temperature. Ingredients and couplings between goods, such as salty food raising thirst. Guests buying in passing without choosing. Reputation and arrivals that depend on the park. Guest groups and personality. Supplies needed by benches or toilets. Hungry footfall for other needs, and a ghost's context for needs other than hunger. Player-facing UI: ImGui panels stand in, and the question stays open (docs/open-questions.md).

## Open questions

- Need rates, couplings, curve shapes, head-home weights, seats and stalls, sitting and use times, and the arrival curve are tuning values. They are set while planning a-day-out and served-needs, and adjusted by watching day.park through a day.
- Whether a full day of the cross-build check stays affordable before pushes. Resolved by measuring day.park's stepping time on both builds when member 5 lands; a shorter run that still crosses closing is the fallback.

## Research notes

- No shipped park game scores going home against the guest's other options; RCT gates leaving on floors and a random roll, and Planet Coaster 2 accumulates fatigue by mood bucket. A scored outside option keeps leaving a gradient and states its cause.
- Needs become decisions rather than chores when they couple to each other and to space and their causes are visible; benches noticed only underfoot feel pointless, so facilities publish offers guests weigh at a distance, as The Sims' advertisements do.
- Hours that drive arrivals and departures give the player a decision; cosmetic hours and skipped nights do not. Real parks fill in the late morning, peak in the early afternoon, and empty in waves to a closing exodus.
- Night stays readable with a hemispheric ambient floor that is cooler and less saturated, not just darker; uniform dimming is the genre's most common complaint.

Depth is in RESEARCH.md.
