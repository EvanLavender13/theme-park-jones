# Capability: Believable Guests

## Summary

believable-guests deepens how convincingly the park's visitors behave. Guests arrive, wander, get hungry, weigh the offers they can reach, walk, queue, eat, and leave. Each does so for reasons it can state in terms of its actual decision factors (principle 8, decision 0019). Like a Sim reading the advertisements of the objects around it, a guest keeps its needs private and responds only to what the park publishes (principles 3 and 6).

Its value is that the park's layout matters to someone. A shop's reach, its wait, and its supply show up as where guests go and how hungry they end up. It deepens with more needs, rides, groups, personality, satisfaction, and scale, and never finishes.

## Foundation criteria

- Guests arrive at the entrance's anchor at a steady rate. Each has a length of stay and a hunger rate taken from keyed random draws. An entrance with no guest path connected admits no guests, a legitimate state.
- A guest's hunger rises every tick at its own rate. Eating a meal lowers it by the meal's relief. The inspection record shows the value before and after, so the slice can check that every served guest's hunger fell.
- At every junction a guest scores each reachable food offer and a carry-on option. A food offer is reachable when route distance has an entry for the shop at the guest's place and the offer says meals are available. Each offer's score is a sum of weighted terms (decision 0019): relief shaped by the guest's hunger through an authored curve (decision 0020), route distance, expected wait, and a commitment bonus for its current target. Once the stay is over, a head-home option joins the options and dominates. The guest picks by softmax with a keyed draw. The recorded terms reproduce every score exactly, and over many draws the picks match the softmax probabilities within a stated tolerance.
- A guest moves along the guest network at walking speed. Toward a target it follows route distance's next edges. When carrying on it takes a keyed random choice among a junction's other edges, avoiding the edge it came in on. At a dead end, such as the end of a path or an anchor it did not target, it turns back along the only edge.
- At its target shop's anchor, a guest sends its visit through the guest-visit flow and waits. A guest never sends a visit to a shop it did not target. It resumes when the visit comes back, served or unserved, and a served guest eats the meal that arrives for it.
- A guest heading home walks to the entrance by route distance and leaves the park there. A guest whose place resolves to no place after an edit, because its path was deleted, moves to the nearest point of the guest network. With no guest network left, it leaves the park. A guest whose target has no route distance entry at its place, because the shop or a path ahead was deleted, or whose target's offer now says no meals, drops the target at once and re-scores where it stands rather than waiting for a junction. A guest with no route home when its stay ends keeps wandering until a route exists. Every one of these is a legitimate state, tested (principle 2).
- Guests publish hungry footfall on the guest network: footfall weighted by each passing guest's hunger, averaged over a recent window, with no hunger threshold.
- Each guest publishes an inspection record (decision 0025): its state, ground position (for picking in tooling), hunger, current target, and its last choice with every option's terms and probability. Guests are drawn as simple shapes on the paths.
- Every behavior is tested with synthetic networks and synthetic offers, and with plausible-operations' shop once it lands.

## Medium

- Route distance: sampled from navigable-networks at the guest's place, for choice (distances to shops), for movement (next edges), and for going home (the entrance's entries).
- Food offer: sampled from plausible-operations at each reachable shop's anchor, for choice.
- Guest visits: a flow this capability supplies to plausible-operations' shops, returned served or unserved.
- Meals: a flow drawn from plausible-operations. A guest consumes its meal when it eats, and hunger stays private.
- Hungry footfall: a scalar field this capability produces on the guest network. legible-simulation samples it for preview context.
- Networks and anchors: guests move on the guest network, and arrive and leave at the entrance's anchor, through shared-medium's network type. A guest carries its place over each re-derivation with shared-medium's carry-over operation.
- Park intent (decision 0025): the entrance from effortless-building, whose entity key identifies the anchor guests arrive at and the route distance source they head home to.
- Keyed random draws and the simulation's exp, from deterministic-simulation, for softmax and variation.
- Inspection records (decision 0025): read by legible-simulation and tests.

## Principles

- Principle 8 is this capability's hardest obligation. Every choice keeps its terms and probabilities, and tests check that the terms reproduce the score and that picks match probabilities.
- Principle 6 is at risk from guests reading shops' state, or shops reading hunger. Guests read only offers and route distance, and hunger never leaves the guest.
- Principle 10 is at risk from choices depending on iteration order or on the C runtime's exp. Draws are keyed on the guest, purpose, tick, and index, and exp is the simulation's own.
- Principle 5 is at risk from thresholds. Hunger acts through a curve against a carry-on option, and footfall is weighted rather than thresholded.
- Principle 2 is at risk from edits under a walking guest. Deleted paths, dead ends, starved or deleted targets, cut routes to a target, and cut routes home are each defined in the criteria above and tested.
- Principle 4 is kept by using route distance for every choice and movement. The one straight-line use is snapping a stranded guest onto the network with shared-medium's nearest-point query.

## Dependencies

- deterministic-simulation (world-as-value): keyed draws, exp, the cycle, and state registration. Met.
- shared-medium (first-field-and-flow): fields, flows, and place carry-over. Met.
- navigable-networks (paths-become-routes): the guest network, the entrance anchor, and route distance. Met.
- plausible-operations (supplied-food-shop): food offers, and serving visits and meals. Met.
- effortless-building (sketch-a-park): the entrance, and rendering of the park the guests are drawn in. Met.

## Foundation

The foundation is hungry guests on one network with one need, choosing between food and carrying on by scored utility, and walking, queuing, eating, and leaving, with every choice explained. That is the smallest version in which the park's layout visibly matters to someone. It produces value on its own: a park with shops can be watched and interrogated guest by guest.

## Milestones

1. `hungry-guests`: arrivals, stays, and hunger from keyed draws, junction choice by softmax over food offers, carry-on, and head-home with a commitment bonus, movement by next edges and keyed wandering, queuing through guest visits, eating meals, leaving, the rules for stranded guests, hungry footfall, inspection records with choice explanations, and guests drawn as simple shapes. As the first holder of places, it also designs how the previous networks stay available until guests carry their places over, which navigable-networks' paths-become-routes handed to it. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value, shared-medium's first-field-and-flow, navigable-networks' paths-become-routes, and plausible-operations' supplied-food-shop.

Later milestones are drawn from the deepening candidates once the slice has been played.

## Deepening candidates

- More needs: thirst, tiredness, and bladder, each a private motive answered by offers.
- Rides as offers of fun and spectacle, with queues through the same visit flow.
- Satisfaction and leaving early when needs go unmet, instead of a fixed stay.
- Personality: per-guest scaling of decision weights (decision 0019).
- Curiosity in wandering: a preference for less-visited edges or new sights.
- Groups and families that choose together.
- Crowding: guests avoiding busy paths, through navigable-networks' crowding cost.
- Thoughts: short player-facing statements drawn from choice explanations, as RollerCoaster Tycoon does. Gated on: the player-facing UI question.
- Walking off a deleted path, instead of moving to the nearest point of the network.
- Aggregate guests at scale: distant or numerous guests as flows, with individuals shown near the camera. Gated on: profiling.

## Open questions

- Arrival rate, stay length, hunger rates, walking speed, softmax temperature, the carry-on utility, and the commitment bonus are tuning values. They are set while planning hungry-guests, with the carry-on utility tuned with several shops present because of logit's independence of irrelevant alternatives.
- The player-facing form of guest explanations (docs/open-questions.md). Evan decides. ImGui inspectors stand in, through legible-simulation.

## Research notes

- The Sims' motives and advertising objects match hunger and food offers.
- Utility AI's response curves support decision 0020, and summed terms keep choices explainable.
- Softmax is McFadden's logit; duplicated options attract extra share, so the carry-on option is tuned with several shops.
- RollerCoaster Tycoon's lost and fixated guests are avoided by routing on the distance field and re-scoring at junctions.

Depth is in RESEARCH.md.
