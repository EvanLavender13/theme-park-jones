# Slice: Day at the Park

Status: planned

## Summary

The park gets a day. A clock runs from morning to night, and the player sets the opening hours. Guests arrive through the morning. They eat, drink, sit down, and use the toilet. They go home when they're tired, when a need goes unmet, or when the park closes.

Shops stock food, drink, or both. Benches and toilet blocks sit beside the paths, like shops. The player can see why guests left, where each need is poorly served, and the light changing with the hour.

This is the right next goal because it turns the boxes-and-tubes loop into management. In that loop, guests have one need and stay for a fixed time. In this slice, how long guests stay depends on how well the layout serves them. The player gets something to improve and a way to see what is failing.

The slice deepens believable-guests and plausible-operations through the fields and flows they already share. It also adds the park's first player policies (principle 9).

## End-to-end scenario

A new park opens at 9:00 on its first day. Its opening hours are 9:00 to 21:00. At normal speed, one game minute passes each real second, so the open hours take twelve real minutes. A clock shows the time of day, and speed controls sit beside it: pause, 1x, 2x, and 4x.

The player draws a guest path from the entrance. They place two shops beside it, a depot behind them, and a backstage path between them. They select the first shop and set it to stock food, and set the second to stock drink. Each shop orders only the goods it stocks, and the depot ships both.

Further along the path, they place a bench and a toilet block. A bench is a small box with a few seats. A toilet block has a few stalls. Both connect to the guest path by themselves, and neither needs supplies.

Guests arrive along an authored curve stretched over the opening hours:

- a few at opening;
- a rush in the late morning;
- a thinning stream into the afternoon;
- none in the last hours before close.

Each guest carries four needs, all private to it:

- hunger, which rises with time;
- thirst, which rises with time;
- tiredness, which rises as the guest walks;
- the bladder fills with each meal and drink, more with drinks.

Every facility publishes an offer for each need it answers. The offer states the relief the facility gives and the expected wait. At each junction, a guest scores those offers the way it scores food offers today (decision 0019). A thirsty guest walks to the drink shop. A tired guest walks to the bench if a seat is likely to be free. A guest who has been drinking walks to the toilet block, and queues if its stalls are full.

Going home is one of those scored options (decision 0019), and it replaces the fixed stay. Its score rises:

- with the guest's tiredness;
- with the discomfort of needs the guest hasn't met;
- as closing time nears.

After closing, going home outscores everything else. A guest in a park that serves it well stays for hours. A guest who can't find a toilet goes home early. The guest's choice records the head-home terms. When the guest picks going home, the largest term is its reason for leaving.

The player opens the departures panel. It shows today's departures by reason:

- tired;
- closing time;
- hunger;
- thirst;
- the toilet.

Many guests left for the toilet, and only one toilet block serves the whole park. The player switches the overlay to toilets and sees the far end of the park unshaded. Hovering lists the one block's contribution. Clicking a guest who is heading home shows its needs and the head-home terms that decided it.

The player places a second toilet block at the far end. Before they commit it, the overlay previews the coverage it adds.

Later, with the food overlay on, they switch the drink shop to stock both. Before they confirm, the overlay previews the food coverage the shop would add. Once they confirm, the shop starts ordering food beside its drinks. If a shop is switched away from a good, it sells off what it holds of that good and orders no more.

In the afternoon, fewer guests leave for the toilet. The sun lowers and the light warms.

At 21:00 the gate stops admitting guests. The guests still inside walk home, and the park is empty well before midnight. The scene is dark but readable. A dim, cool night light keeps the shapes of boxes and paths clear. The player speeds up to 4x through the night, and the depot keeps restocking the shops. At 9:00 the gate opens and the first guests of the next day arrive.

## Acceptance criteria

Four park files are checked in under tests/parks:

- day.park holds an entrance, a guest path, a food shop, a drink shop, a bench, a toilet block, a depot, and a backstage path. Its hours are 9:00 to 21:00. It starts at 9:00 with no guests.
- no-toilet.park is day.park without the toilet block.
- noon.park is day.park run to 12:00, with guests in the park.
- evening.park is day.park run to shortly before 21:00, so the cross-build check's run of it crosses closing.

tpj_scenarios --slice-parks makes all four, alongside the boxes-and-tubes parks. A full day is 43200 ticks.

The park world checks and the cross-build check read every park in tests/parks. They check that each park copies, saves, hashes, and replays exactly, on every build players run (decision 0022). Once these four parks are there, those checks cover the slice's new state without a new test. That state includes the opening hours, stock settings, benches and toilet blocks, guests' needs, shops' stock per good, and the departure tally.

a-day-out adds three edits of noon.park to the world checks' chosen edits, so each preview is checked against its commit (principle 8):

- placing a second toilet block;
- switching the drink shop to stock both goods;
- changing the opening hours.

1. Removing the toilet block makes more guests leave for the toilet: over one day, more guests leave no-toilet.park with the toilet as their reason than leave day.park for it. (integration test)
2. Removing the toilet block shortens guests' stays: over one day, the mean stay in no-toilet.park is shorter than in day.park. (integration test)
3. Switching noon.park's drink shop to stock food only leaves the guests thirstier: at closing, their mean thirst is higher than in noon.park unchanged. (integration test)
4. A capture of noon.park shows the park in daylight, with guests on the paths and at the facilities. (scripted capture: stylized-presentation, believable-guests)
5. A capture of day.park at 23:00 shows the empty park at night, with every box and path still distinguishable from the ground. (scripted capture: stylized-presentation, believable-guests)
6. A capture of noon.park with the toilet overlay on shows coverage around the toilet block. (scripted capture: legible-simulation)
7. A capture of noon.park with the drink overlay on shows coverage around the drink shop and none around the food shop. (scripted capture: legible-simulation)
8. In the running app, no guest arrives while the park is closed, every guest admitted has left before the next opening, and guests arrive again after it. (manual)
9. In the running app, if the player switches a shop away from drink, the shop places no more drink orders. Its drink offer ends once it has served the drink it held and the drink already on its way, and it serves no drink after that. (manual)
10. In the running app, hovering a need's overlay lists each facility's contribution, and the contributions match the value shown, to the precision shown. (manual)
11. In the running app, these work as the scenario describes (manual):
    - the speed controls and the clock;
    - setting the opening hours, with the arrival curve previewed;
    - placing benches and toilet blocks;
    - setting a shop's stock, with its preview;
    - switching the overlay between needs, with hover attribution;
    - the departures panel;
    - the guest, bench, and toilet block inspectors.

## Medium

These are the fields and flows between capabilities. The abbreviations are those of the boxes-and-tubes slice, with SP for stylized-presentation.

- Need offer: an entry field on guest network nodes. It replaces the food offer. Each facility publishes entries at its guest anchor, one for each need it answers. Each entry holds:
  - the need: hunger, thirst, rest, or toilet;
  - the relief a serving gives;
  - the expected wait;
  - whether it is available.

  A shop answers hunger while its stock setting names food, or while it still holds or awaits food supplies. It answers thirst the same way for drink. If a shop is switched away from a good, it sells off what it holds through its ordinary offer. A bench answers rest, and a toilet block answers the toilet. PO produces the need offer. BG consumes it for choice, and LS consumes it for overlays and attribution.
- Route distance: an entry field over the networks, unchanged. Benches and toilet blocks become sources once they anchor guest nodes. NN produces it. BG consumes it for choice, movement, and going home. PO consumes it for supply routes, and LS for the overlay discount and preview context.
- Networks: the guest and backstage graphs, unchanged. The front doors of benches and toilet blocks serve the guest network. NN produces the networks through SM's network type, and every capability consumes them as before.
- Guest visits: a flow of guests into a facility's queue and back out. Shops, benches, and toilet blocks receive them alike. Each visit names the need the guest came for. There is one guest-visit kind per need. A shop that stocks both goods can tell from the visit whether to serve a meal or a drink, without reading the guest. BG produces visits. PO consumes them and returns every guest to BG, served or unserved. PO serves a visit only with the serving of the need it names.
- Servings: conserved flows, one kind per need, from a facility to the guest it served. Each serving is sent with the returned visit. A shop makes each meal or drink from one supply of its good. A bench or toilet block creates a rest or a toilet use when it releases the guest, so benches and toilets need no supplies. PO produces servings. BG consumes them and applies the relief to the guest's own need, so needs stay private to the guest.
- Supplies and supply orders: conserved flows, one pair for each good (food and drink). They run between shops and the nearest depot along the backstage network. PO produces and consumes them, through SM's ledger.
- Hungry footfall: unchanged. BG produces it, and LS consumes it for a shop ghost's context.

Some dependencies are not fields or flows. Decision 0025 rules on the first three. The clock is part of the tick cycle itself.

- Park intent: the opening hours, each shop's stock setting, and bench and toilet block boxes. EB authors them, and they are saved (principle 1). BG reads the hours for arrivals and the closing term. PO reads stock settings. NN derives doors from the boxes, and LS picks and previews them. Changing the hours or a stock setting is an edit command, applied between ticks like any other edit.
- Candidate resolution: a stock change or a new facility previews on a candidate world, as placements do today. NN and PO resolve the candidate, and LS samples it through the field interface. An hours change previews the same way. BG's arrival-curve record is a function of the hours in intent. LS reads the curve from a candidate world with the tentative hours applied, and never computes the curve itself.
- Inspection records: BG and PO publish these, and only LS, tests, and the app read them:
  - a guest's needs, and its last choice with the head-home terms;
  - a bench's and a toilet block's occupancy and queue;
  - a shop's stock per good;
  - the departure tally, which counts today's departures by reason;
  - the arrival curve BG admits guests by, so the hours panel can preview it.
- The clock: the time of day and the day number. DS provides it as a pure function of World::Tick and the new park's start time. Every system already reads the tick as part of the cycle, so the clock adds no interaction between entities. BG reads it for arrivals and closing, SP for the light, and LS for the clock panel. Rendering only reads it and never feeds anything back (principle 10).

## Members

The members are in build order. Milestone slugs are provisional until each capability plans them.

Two milestones are prerequisites rather than members, because they add nothing to the scenario: sound-architecture's `layered-dependencies` and `composed-app`. Both have landed. composed-app restructured src/app/main.cpp, so the slice's speed controls and panels can go into the app's components (decision 0027).

1. `deterministic-simulation/park-clock`: the clock as a function of the tick, giving the day number and the time of day. A new park starts at 9:00 on day 1. The app's frame clock gets pause, 1x, 2x, and 4x speed. Speed changes only how many ticks a frame steps, so a park's hash at a given tick is the same at any speed. Depends on: none.
2. `effortless-building/facilities-and-hours`: bench and toilet block box kinds, with their sizes. The player places, moves, and deletes them with ghosts, and they are drawn as boxes. A shop's stock setting and the park's opening hours become intent, with their edit commands, and are saved and loaded. The tooling gets the controls that issue those edits. Depends on: member 1.
3. `navigable-networks/facility-doors`: the front doors of benches and toilet blocks serve the guest network. They connect to nearby guest paths and become route distance sources. Depends on: member 2.
4. `plausible-operations/served-needs`: food and drink become separate goods, with orders and supplies for each good. Shops order only the goods their stock setting names, and sell off what they hold of the rest. Each visit is served with the serving of the need it names, and shops serve drinks. Benches have seats and toilet blocks have stalls. Each holds a guest for a sitting or a use and returns it with a serving. The need offer replaces the food offer. Benches, toilet blocks, and shops' stock per good get inspection records. Depends on: members 2 and 3.
5. `believable-guests/a-day-out`: guests gain thirst, tiredness, and the bladder, with their couplings, and choose among need offers. Going home is scored by tiredness, unmet needs, and closing time, instead of following a fixed stay. Guests arrive by the authored curve over the opening hours. The departure tally counts departures by reason. Tuning is set in game hours. tpj_scenarios --slice-parks writes day.park, no-toilet.park, noon.park, and evening.park, and the world checks' chosen edits gain the three edits of noon.park. Depends on: members 1, 2, and 4.
6. `stylized-presentation/day-and-night` (capability to be planned): sun direction and color, sky color, and a hemispheric ambient light, authored as ramps over the time of day and read from the clock alone. Night has a cool, readable floor of ambient light and a dim moon light. Depends on: member 1.
7. `legible-simulation/explained-needs`: an overlay for each need, with a selector, hover attribution, and an --overlay option for each need. It adds previews of stock changes and new facilities, and the departures panel. The guest inspector shows every need and the head-home terms, and benches and toilet blocks get inspectors. The clock and hours panel previews the arrival curve for tentative hours. Depends on: members 1 through 5.

Member 5 regenerates the boxes-and-tubes parks, and their integration tests keep passing. If a park has no opening hours set, it starts at 9:00 with the default hours, so the guests in those parks still arrive at once.

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
- Hungry footfall for other needs, and a ghost's context for needs other than hunger.
- Player-facing UI. ImGui panels stand in, and the question stays open (docs/open-questions.md).

## Open questions

- Tuning values: need rates, couplings, curve shapes, head-home weights, seats and stalls, sitting and use times, and the arrival curve. They are set while planning a-day-out and served-needs, and adjusted by watching day.park through a day.

## Research notes

- No shipped park game scores going home against a guest's other options. RCT decides leaving by fixed floors and a random roll. Planet Coaster 2 builds up fatigue at a rate set by the guest's mood bucket. Scoring going home as an option makes the pull to leave rise smoothly, and records its cause.
- Needs become decisions rather than chores when they affect each other, depend on where things are, and have visible causes. Benches that guests use only when they happen to step on one feel pointless. Here facilities publish offers that guests weigh from a distance, as The Sims' advertisements do.
- Opening hours give the player a decision only if they drive arrivals and departures. Cosmetic hours and skipped nights give none. Real parks fill in the late morning, peak in the early afternoon, and empty in waves, ending in a rush out at closing.
- Night stays readable when a hemispheric ambient light keeps a floor that is cooler and less saturated, not just darker. Uniform dimming is the genre's most common complaint about night.

Depth is in RESEARCH.md.
