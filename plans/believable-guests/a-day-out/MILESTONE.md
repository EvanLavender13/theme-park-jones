# Milestone: A Day Out

Slice: day-at-the-park

Status: planned

## Summary

a-day-out gives guests a day at the park. Guests arrive along an authored curve over the opening hours, and none arrive while the park is closed. Each guest carries four private needs: hunger, thirst, tiredness, and bladder. It weighs the food, drink, rest, and toilet offers it can reach, walks to the one it picks, and takes the serving that answers that need. Going home becomes one of the scored options and replaces the fixed stay. Its terms grow with tiredness, with needs the guest hasn't met, and as closing nears, so a park that serves its guests well keeps them for hours. Each departure is tallied by its reason. The milestone also makes the slice's four parks. It is member 5 of day-at-the-park. The clock, the hours, the stock settings, the facilities, and every need offer, visit, and serving are in place. This is the member that makes the park's layout decide how long guests stay.

## Acceptance criteria

1. Each guest holds four needs, each from 0 to 1. Hunger and thirst rise every tick at the guest's own drawn rates. Tiredness rises with each meter the guest walks, at its own drawn rate, and doesn't rise while it waits. Each meal fills the bladder by BLADDER_PER_MEAL, and each drink by the larger BLADDER_PER_DRINK. A serving lowers only the need it answers, by the relief the offer gave when the guest chose it. Needs never leave the guest except as hunger in hungry footfall, which is unchanged. (play stocked.park and day.park, watching the guest Inspector's needs)
2. A guest scores every offer it can reach in the four need fields: food-offer, drink-offer, rest-offer, and toilet-offer. A shop that stocks both goods is two options, one for each need. Each offer scores as a food offer does today, with the relief shaped by its own need's level. When the guest arrives, it sends the visit kind of the need it chose, and it consumes the serving that comes back with it. (play: the Inspector's choice table lists each option's need, and guests walk to the drink shop, the bench, and the toilet block)
3. Going home is scored once an entrance is reachable, whatever the guest's stay. Its score is a base term plus one term for each reason: tiredness, hunger, thirst, the toilet, and closing time. Each need's term grows with that need's level. The closing term rises over CLOSING_LEAD before closing, and from closing until the park opens it outscores every other option. If the park is open all day, the closing term is always 0. A guest chooses as soon as the park closes. A guest heading home keeps choosing at the nodes of its route, as a guest heading to a shop does, and going home gets the commitment bonus while it is the guest's heading. Each time a guest picks going home, its reason becomes that pick's largest reason term. When it walks out of an entrance, the reason of its latest pick is the reason it left. (play: the Inspector shows each option's terms, and after closing every guest heads home; check: a scratch program steps day.park through closing and reads each choice's terms and the reasons recorded)
4. Each entrance connected to the guest network admits guests by a keyed draw each tick. The chance is the arrival curve's rate at the fraction of the opening hours that has passed. The rate is 0 while the park is closed, and 0 over the last part of the open hours. arrivalCurve gives the curve as a function of the hours alone, so a candidate with tentative hours gives the curve those hours would admit by (decision 0025). (check: the scratch program counts arrivals in each game hour of day.park's first day and compares them with arrivalCurve; play: criterion 8 of the slice)
5. The departure tally counts the guests who have walked out of an entrance since the park last opened, by the reason they picked going home for. A guest who leaves because no guest paving is left isn't counted. The guest record holds the tick the guest arrived, so a stay's length can be read. (check: the scratch program's tally equals its own count of departures by reason)
6. Over one day from 9:00, more guests leave no-toilet.park for the toilet than leave day.park for it, and the mean stay in no-toilet.park is shorter. Switching noon.park's drink shop to food only leaves its guests' mean thirst at closing higher than in noon.park unchanged. These are the slice's criteria 1 to 3, which the slice's closing writes as integration tests. (check: the scratch program runs each comparison and prints both sides)
7. tpj_scenarios --slice-parks makes day.park, no-toilet.park, noon.park, and evening.park as the slice describes them. noon.park holds guests at the bench or toilet block, and a facility's seat and stall state. It also remakes every park that holds guests. The park world checks cover the new state with facility-service removed from their unheld types. They check three new edits of noon.park against their commits: placing a second toilet block, switching the drink shop to stock both goods, and changing the opening hours. The integration tests over the remade parks keep passing. (ctest)
8. Guests are shaded from the sated color to the distressed color by their most pressing need. The guest Inspector shows the four needs and the arrival tick, and its choice table shows each option's need and terms, with going home's terms by reason. (play, and a --capture of noon.park)
9. src/sim/guests/SPEC.md describes guests as built, and the other specs the features touch describe their changes. (check: read each beside the code)

## Medium

Cross-capability, as the slice's medium map assigns:

- Need offers (plausible-operations): guests sample food-offer, drink-offer, rest-offer, and toilet-offer at each reachable facility's anchor, for choice. four-needs reads the three new fields.
- Route distance and networks (navigable-networks): sampled as today, for choice, routes, and going home. Benches and toilet blocks are already sources.
- Guest visits (flows supplied to plausible-operations): four-needs sends drink-visits, rest-visits, and toilet-visits as well as guest-visits. Each returns served or unserved.
- Servings (flows drawn from plausible-operations): four-needs consumes drinks, rests, and toilet-uses as well as meals. The relief stays inside the guest.
- Hungry footfall (to legible-simulation): unchanged, still weighted by hunger alone.
- Park intent (decision 0025): going-home and arrival-curve read the opening hours through parkHours, and guests read entrances as today.
- The clock (deterministic-simulation): going-home and arrival-curve read the time of day through parkTime(world.Tick). It is part of the tick, not a dependency between entities.
- Inspection records (decision 0025), read by legible-simulation, tests, and the app. four-needs publishes a guest's needs and each option's need. going-home adds going home's terms, the arrival tick, and the departure tally. arrival-curve publishes the arrival curve. explained-needs later builds the departures panel and the hours panel's preview from these.

Internal to this capability:

- Guest presence: unchanged.

Guests read only offers, route distance, their own stocks, intent, and the clock (principle 6). The departure tally is new saved state on its own entity, as the opening hours are. Arrivals need no saved state, because each tick's draw is keyed.

## Dependencies

- deterministic-simulation/park-clock: parkTime. Complete.
- effortless-building/facilities-and-hours: benches, toilet blocks, stock settings, and opening hours. Complete.
- navigable-networks/facility-doors: benches and toilet blocks on the guest network. Complete.
- plausible-operations/served-needs: every need offer, visit, and serving. Complete.

## Core feature

`four-needs` is the core. As soon as it lands, the player sees guests walk to the drink shop, the bench, and the toilet block for reasons the Inspector states, in stocked.park and every park after. going-home and the parks build on the needs it adds.

## Features

1. `four-needs`: guests carry hunger, thirst, tiredness, and bladder, with their rates and couplings. They score the four need fields' offers, send the matching visit, and take the matching serving. The guest Inspector shows the needs and each option's need. Guests are shaded by their most pressing need. The fixed stay and steady arrivals stay as they are. Depends on: none.
2. `going-home`: going home is scored with its base, its reason terms, and the closing term, and it replaces the fixed stay. A guest chooses when the park closes. Going home gets the commitment bonus while the guest heads home, and each pick of it records its largest reason term. A guest is counted in the departure tally, under its latest pick's reason, when it leaves. The guest record gains the arrival tick, and the Inspector shows going home's terms. Depends on: feature 1.
3. `arrival-curve`: entrances admit guests by keyed draws from the arrival curve over the opening hours, and none while closed. arrivalCurve publishes the curve. Depends on: feature 2.
4. `day-parks`: tpj_scenarios --slice-parks makes day.park, no-toilet.park, noon.park, and evening.park. day.park's two shops stand more than 312 m apart by route, the distance at which food availability's discount reaches 0 at the reference speed (240 s). The world checks drop facility-service from their unheld types and gain the three noon.park edits. The scratch program checks criteria 3 to 6, and the tuning values are adjusted by watching day.park through a day. Depends on: feature 3.

Each of features 1 to 3 changes how guests step. Each one remakes every park in tests/parks that holds guests, with --slice-parks and tpj_bench --full-park. It also rewrites the guest lines of the play parks in parks/ (busted-path, jank, and radii) so they still load. Their integration tests must keep passing. warm.park's world checks need a guest waiting at its shop and one heading there, so a feature that loses those adjusts WARM_TICKS.

The early-turns integration test needs a guest that would head home if it chose. Today its stay is over. going-home makes the park closed for it instead, by its hours, which keeps what the test asserts.

No feature adds a test (docs/testing.md). The slice's closing adds the integration tests for criterion 6.

## Tuning values

These are starting values, adjusted in day-parks by watching day.park through a day. four-needs keeps hunger's rate and scales thirst and tiredness to today's stays of 1 to 2 game hours. going-home sets these values when stays become game hours. One game hour is 1800 ticks.

- Hunger rate: drawn so hunger goes from 0 to 1 in 4 to 6 game hours. Starting hunger: drawn up to 0.4.
- Thirst rate: drawn so thirst goes from 0 to 1 in 3 to 4.5 game hours. Starting thirst: drawn up to 0.4.
- Tiredness: drawn so tiredness goes from 0 to 1 in 400 to 600 m of walking, about 5 to 8 game hours. Starting tiredness: 0.
- BLADDER_PER_MEAL: 0.2. BLADDER_PER_DRINK: 0.35. Starting bladder: drawn up to 0.2.
- Each need's relief is shaped by today's HUNGER_CURVE, which becomes the curve of every need.
- Going home: a base of -2, and a weight of 1.5 for each of tiredness, hunger, thirst, and the toilet, each times its need's curve. Each weight is below its offer's relief term per unit of curve: 2 for food, drink, and rest (RELIEF_WEIGHT 4 times relief 0.5), and 4 for the toilet. So an offer in reach answering one high need outscores that need's pull home. When several needs are high at once, their home terms add up, and going home can win even with facilities in reach.
- CLOSING_LEAD: 1 game hour. The closing term rises over it to 10, today's HEAD_HOME_SCORE.
- The arrival curve, in guests per game hour for each entrance, at fractions of the opening hours: 5 at opening, 15 from 0.15 to 0.3, 6 at 0.5, and 0 from 0.75 to closing. Over the default hours that is about 80 guests a day. It is sized from the guests inside at once, not from arrivals. With stays of 4 to 5 game hours, about 45 guests are inside at the peak. At these need rates, each guest wants about 0.5 drinks and 0.4 meals a game hour, so 45 guests want about 24 drinks and 18 meals. Each shop serves 20 an hour, so the drink shop queues at the peak and the food shop keeps up.
- Carrying on, the offer weights, the commitment bonus, and the softmax temperature stay as they are.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Waiting in a queue tires guests, as Planet Coaster 2's energy does, if long queues look too cheap in play.
- A curve of its own for each need, if one shared curve reads wrong in play.
- Guests who leave because no guest paving is left counted in the departure tally under a reason of their own.
- Guests shaded by the hue of their most pressing need, if the single ramp hides too much in play.

## Open questions

- Whether the tuning values give a day that reads well: guests stay for hours when served, a park with no toilet loses them early, and the park empties well before midnight. Resolved in day-parks by watching day.park through a day and adjusting.

## Research notes

- Arrivals are one keyed Bernoulli draw for each entrance and tick, at the curve's rate. That matches a Poisson process closely and needs no saved state.
- Going home is a sum of terms, one for each reason and a base. Its largest reason term names the reason, which multiplied considerations can't do.
- Tiredness rises with meters walked, so a spread-out park tires its guests.
- Guests are shaded by their most pressing need on the existing ramp. The Inspector and the overlays say which need.

Depth is in RESEARCH.md.
