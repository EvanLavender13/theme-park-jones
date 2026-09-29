# Milestone: Hungry Guests

Slice: boxes-and-tubes

## Summary

hungry-guests puts guests in the park. It adds the module src/sim/guests. Guests arrive at the entrance at a steady rate, each with a stay and a hunger rate from keyed draws. They wander the guest network, get hungry, and at every node score the food offers they can reach against carrying on, and against heading home once their stay is over. They pick by softmax with a keyed draw, walk to the chosen shop by route distance's next edges, send it a visit, wait, eat the meal that comes back, and eventually leave by the entrance. Each guest carries its place across edits, and publishes hungry footfall and an inspection record that explains its last choice. Guests are drawn on the paths as small shapes colored by hunger. It comes sixth in the slice. The medium, the networks, route distance, and the shops' offers, visits, and meals are all in place, and it is the last member the simulation needs: once it lands, the park's layout matters to someone, and the slice's park files can be made.

## Acceptance criteria

1. makeParkSchema registers the guests module after the operations module. Guests arrive at the node anchored to the entrance on the guest network, one every ARRIVAL_INTERVAL. Each has a stay, a hunger rate, and a starting hunger from keyed draws on its key. An entrance with no guest connector admits no guests, a legitimate state.
2. A guest's hunger rises every tick at its own rate, on the scale from 0 to 1 that MEAL_RELIEF assumes, and stays at 1 once it gets there. Eating a meal lowers it by the relief of the offer the guest chose, to no lower than 0. The inspection record gives the hunger before and after the guest's last meal, and every served guest's hunger fell.
3. A guest walks WALK_SPEED along the guest network, deciding at each node it reaches within the tick and walking on with the rest of the tick's distance. Toward a target it follows route distance's Next steps. Carrying on, it takes a keyed draw among the node's edges other than the one it came in on, and at a node with only that edge it turns back.
4. At every node, a guest scores each reachable food offer, carrying on, and, once its stay is over, heading home (decision 0019). An offer is reachable when guest route distance has an entry for its shop at the guest's place and the offer's Supplied is true. An offer's score is the sum of its terms: relief shaped by the guest's hunger through an authored curve (decision 0020), route distance, wait, and a commitment bonus for its current target. The guest picks by softmax over the scores with a keyed draw and simExp. The recorded terms reproduce every score exactly, and over many draws the picks match the softmax probabilities within a stated tolerance.
5. At its target shop's guest anchor, a guest sends one guest-visits unit with its key as handle to that shop, and never to a shop it did not target. It waits there until the visit returns, whatever its target's offer or route distance says meanwhile, so it never leaves or sends another visit with one outstanding. A served guest consumes the meal that came with its visit as eaten. Then it scores again.
6. A guest heading home walks to the entrance by route distance and leaves the park at its anchor. With no route home, it keeps wandering until one exists.
7. Every guest's place is carried across each re-derivation with carryOver, during the resolution, so every world the cycle leaves and every candidate holds carried places. A guest whose place is retired moves to nearestPlace of its last ground position, and with no guest network left it leaves the park. A guest walking toward a target that has no route distance entry at its place, or whose offer says no meals, drops the target and scores again at once. A guest waiting on a visit keeps waiting (criterion 5). Every world randomized edit sequences with guests reach is legitimate (principle 2), a saved world loads back and resolves equal to the world saved, and a candidate made with an edit equals the world that commits it.
8. Guests publish hungry-footfall, a scalar field on the guest network's edges: each stretch's exponential moving average, with time constant FOOTFALL_TIME, of the summed hunger of the guests on it each tick, with no hunger threshold. So it counts guests weighted by hunger: while the same guests stand on a stretch, its value approaches their summed hunger, and ten hungry guests passing weigh ten times one.
9. Each guest publishes an inspection record (decision 0025): its state, ground position, hunger, target, the hunger before and after its last meal, and its last choice with every option's terms and probability. The scene draws each guest as a small box on the paths, colored by hunger, and the Debug panel shows the guest count and mean hunger. A --capture shows guests on the paths.
10. tests/parks/fed.park, warm.park, and cut.park are checked in as the slice defines them. Slice criteria 1, 2, 3, 6, and 7 hold for them, and the cross-build check passes with them.
11. src/sim/guests/SPEC.md describes the module as built, and the specs of every module it changes describe their changes. linux-debug builds without warnings and its tests pass, and windows-debug builds.

## Medium

Cross-capability, as the slice's medium map assigns:

- Route distance (navigable-networks): sampled on guest-route-distance at the guest's place. wandering-guests follows the entrance's Next steps home. eating-guests reads each shop's Distance to score it and its Next steps to walk to it.
- Food offer (plausible-operations): sampled by eating-guests at each shop's entries' places, reached through route distance, for Relief, Wait, and Supplied.
- Guest visits (eating-guests supplies, plausible-operations draws): one unit per visit, created under the guest's own key, sent to its target shop at the shop's guest anchor, and returned to the guest served or unserved, which consumes it.
- Meals (plausible-operations supplies, eating-guests draws): a guest consumes the meal that arrives with its visit as eaten. Hunger stays private to the guest.
- Hungry footfall (hungry-footfall produces): a scalar field on the guest network's edges, sampled later by legible-simulation for preview context. Tests sample it here.
- Networks and carry-over (shared-medium, navigable-networks): guests walk parkNetwork(world, Guest) through the Network type's queries. carried-guests has path-networks keep each previous network in a derived slot, as a field keeps its previous entries, and a guests finisher carries every guest's place with carryOver from it. A routes finisher that addPark registers after the guests module drops the previous networks, so every holder's finisher reads them first and no resolution ends holding them. Guests never clear routes' data.
- Park intent (decision 0025): the entrance through parkEntrances, whose key is the anchor guests arrive at and the route distance source they head home to.
- Keyed draws and simExp (deterministic-simulation): stays, hunger rates, wandering, and the softmax pick.
- Inspection records (decision 0025): read by tests, by the guest rendering and the Debug panel, and later by legible-simulation's guest inspector.

No guest reads a shop's state, and no shop reads a guest's hunger (principle 6).

## Dependencies

- deterministic-simulation's world-as-value: keyed draws, simExp, the cycle, finishers, and candidates. Met.
- shared-medium's first-field-and-flow: networks, places, carryOver, nearestPlace, scalar and entry fields, and the ledger. Met.
- navigable-networks' paths-become-routes: the guest network, the entrance's anchor, and route distance with Next steps. Met.
- plausible-operations' supplied-food-shop: the food offer, and visits served with meals or returned. Met.
- effortless-building's sketch-a-park: the entrance, the scene and its meshes, the Debug panel, and --capture. Met.

## Core feature

wandering-guests. It creates the module and puts guests in the park at once: they arrive, walk, wander, get hungrier, go home, and leave, drawn on the paths and colored by hunger. Every later feature changes what guests do on the network it makes them walk, so nothing else starts without it, and it is visible in the app from its first commit.

## Features

1. `wandering-guests`: the guests module, arrivals at the entrance's anchor at a steady rate, stays, hunger rates, and starting hunger from keyed draws, hunger rising, walking at WALK_SPEED with keyed wandering and turning back at dead ends, heading home by the entrance's route distance when the stay is over and leaving, wandering on with no route home, a guest whose place stops resolving leaving the park until carried-guests replaces that rule, the inspection record's state, position, and hunger, guests drawn as boxes colored by hunger, and the Debug panel's count and mean hunger. Depends on: none.
2. `eating-guests`: the scored choice at every node over reachable offers, carrying on, and heading home, with the hunger curve, commitment bonus, and softmax pick, walking to the target by Next steps, dropping a target that becomes unreachable or unsupplied, sending the visit at the anchor and waiting, eating the meal, and the record's last meal and last choice with every option's terms and probability. Depends on: feature 1.
3. `carried-guests`: path-networks keeping the previous networks until a routes finisher registered after the guests module drops them, a guests finisher carrying every place with carryOver, stranded guests moving to the nearest point of the network or leaving when there is none, and randomized edit runs with guests. Depends on: feature 2.
4. `hungry-footfall`: the hungry-footfall scalar field on the guest network's edges, each stretch's moving average of passing guests' hunger, carried with its stretch across edits. Depends on: feature 3.
5. `slice-parks`: tests/parks/fed.park, warm.park, and cut.park, a way to write a park after stepping it so warm.park and cut.park can be regenerated, and the cross-build check with them. Depends on: features 2 and 3.

## Tuning values

The capability leaves these to this milestone. They are starting values; each feature that introduces one fixes it in its spec, and playing adjusts them.

- ARRIVAL_INTERVAL: 60 ticks (2 s) between arrivals.
- Stay: drawn uniformly from 1800 to 3600 ticks (60 to 120 s).
- Hunger rate: drawn so hunger rises from 0 to 1 in 45 to 90 s, 1/2700 to 1/1350 per tick. Starting hunger: drawn from 0 to 0.4.
- WALK_SPEED: 1.3 m/s.
- MEAL_RELIEF: 0.5, adopted as operations set it, on the 0 to 1 scale defined here.
- Hunger curve: piecewise linear through (0, 0), (0.3, 0.1), (0.7, 0.8), and (1, 1), over hunger's whole range.
- Weights: relief 4 per unit of curve times relief, distance -0.01 per meter, wait -0.02 per second, commitment bonus 0.3.
- Carrying on: 0.5. Heading home: 10, which dominates once the stay is over.
- Softmax temperature: 0.25.
- FOOTFALL_TIME: 300 ticks (10 s).

With these, a guest 30 m from a shop with a short wait takes it about one time in fifteen at hunger 0.3, and nearly always at hunger 0.7. The carry-on value is tuned with two shops present, since logit's independence of irrelevant alternatives lets each added shop draw extra share.

## Deepening candidates

Unordered pool this milestone draws later features from.

- The ghost draws its candidate's guests, so deleting a path under guests previews where they will stand.
- A queue drawn as a line of guests at the shop, instead of queued guests standing on its anchor together.
- Guests facing the way they walk, and interpolated between ticks at high frame rates.
- An arrival rate that varies over the day.

## Open questions

- Whether the tuning values give a slice that reads well: guests visibly get hungry, most eat once, and a cut shop leaves some hungry. Resolved by watching fed.park and cut.park in the app once eating-guests lands, and adjusting.
- The player-facing form of guest explanations (docs/open-questions.md). Evan decides. The inspection record and legible-simulation's ImGui inspector stand in.

## Research notes

- Carrying places during the resolution follows the field's Previous slot: path-networks keeps the previous networks, a guests finisher carries each place with carryOver, and a routes finisher registered after it drops them before the resolution ends, so saves and first resolutions are unaffected. Evan chose it over re-snapping at the guest's step.
- A guest decides at a node within the tick and walks on with the rest, so its speed does not depend on how nodes are spaced.
- Hungry footfall is an exponential moving average per stretch of the summed hunger on it, with one number per stretch, one time constant, and no threshold.
- Guests get a mesh of their own, rebuilt every frame from their records, since the park mesh is rebuilt only when intent changes.

Depth is in RESEARCH.md.
