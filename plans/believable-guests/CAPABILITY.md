# Capability: Believable Guests

## Summary

believable-guests makes the park's visitors behave convincingly. Guests arrive, wander, and get hungry. They weigh the offers they can reach, then walk, queue, eat, and leave.

Each guest can state the reasons for what it does, in terms of the factors it actually weighed (principle 8, decision 0019).

A guest keeps its needs private and responds only to what the park publishes (principles 3 and 6). It works like a Sim, which reads the advertisements of the objects around it.

This capability matters because it makes the park's layout matter to someone. A shop's reach, its wait, and its supply decide where guests go and how hungry they end up.

Later milestones add more needs, rides, groups, personality, satisfaction, and scale. The capability never finishes.

## Foundation criteria

Guests arrive at the entrance's anchor at a steady rate. Each guest gets a length of stay and a hunger rate from keyed random draws. If no guest path connects to the entrance, the entrance admits no guests, and this is a legitimate state.

A guest's hunger rises every tick at the guest's own rate. When the guest eats a meal, its hunger falls by the meal's relief. The inspection record shows hunger before and after the meal, so the slice can check that every served guest's hunger fell.

At every junction, a guest scores each food offer it can reach, and a carry-on option. A food offer is reachable when both of these hold:

- route distance has an entry for the shop at the guest's place;
- the offer says meals are available.

Each offer's score is a sum of weighted terms (decision 0019):

- relief, shaped by the guest's hunger through an authored curve (decision 0020);
- route distance;
- expected wait;
- a commitment bonus, if the offer is the guest's current target.

When the guest's stay is over, a head-home option joins the options and dominates them. The guest picks by softmax with a keyed draw. The recorded terms reproduce every score exactly, and the guest picks each option with its softmax probability.

A guest moves along the guest network at walking speed:

- Toward a target, it follows route distance's next edges.
- When carrying on, it makes a keyed random choice among the junction's other edges, and avoids the edge it came in on.
- At a dead end, such as the end of a path or an anchor it did not target, it turns back along the only edge.

When a guest reaches its target shop's anchor, it sends its visit through the guest-visit flow and waits. A guest sends a visit only to the shop it targeted. When the visit comes back, served or unserved, the guest resumes. If it was served, it eats the meal that arrives for it.

When a guest heads home, it walks to the entrance by route distance and leaves the park there.

Edits can strand a guest. Each case has a defined result:

- If the paving under a guest is deleted, the guest moves to the nearest point of the guest paving where its box fits. If no guest paving is left, it leaves the park.
- If its target has no route distance entry at its place, because the shop or a path ahead was deleted, the guest drops the target at once. It re-scores where it stands instead of waiting for a junction.
- If its target's offer now says no meals, the guest also drops the target at once and re-scores where it stands.
- If the guest has no route home when its stay ends, it keeps wandering until a route exists.

Each of these is a legitimate state (principle 2). Check them by playing.

Guests publish hungry footfall on the guest network. It is footfall weighted by each passing guest's hunger and averaged over a recent window. It has no hunger threshold.

Each guest publishes an inspection record (decision 0025). The record holds:

- the guest's state;
- its ground position, for picking in tooling;
- its hunger;
- its current target;
- its last choice, with every option's terms and probability.

Guests are drawn as simple shapes on the paths.

Guest state goes into park files in tests/parks/, where the park world checks cover it. Check guest behavior by playing and with `--capture` (docs/testing.md, decision 0037).

## Medium

- Route distance: sampled from navigable-networks at the guest's place. Guests use it for choice (distances to shops), for movement (next edges), and for going home (the entrance's entries).
- Food offer: sampled from plausible-operations at each reachable shop's anchor, for choice.
- Guest visits: a flow this capability supplies to plausible-operations' shops. Each visit comes back served or unserved.
- Meals: a flow drawn from plausible-operations. A guest consumes its meal when it eats. Hunger stays private.
- Hungry footfall: a scalar field this capability produces on the guest network. legible-simulation samples it for preview context.
- Networks and anchors: through shared-medium's network type, guests take their routes from the guest network, and arrive and leave at the entrance's anchor. Each guest's place on the network is where it has got to along its route, the point of its route nearest where it stands.
- Park intent's derived shape (decision 0025): guests walk on the guest paving's pieces from sim/park, with each walkway's piece and its connector's key. Each guest keeps its ground position across each re-derivation while paving still lies there.
- Guest presence: a ground field in shared-medium's medium that this capability produces and samples. Each guest publishes where it stood and how it moved, and steps aside from the guests near it, without reading their state.
- Park intent (decision 0025): the entrance, from effortless-building. Its entity key identifies the anchor where guests arrive, and the route distance source they head home to.
- Keyed random draws and the simulation's exp, from deterministic-simulation, for softmax and variation.
- Inspection records (decision 0025): read by legible-simulation and tests.

## Principles

- Principle 8 is this capability's hardest obligation. Every choice keeps its terms and probabilities, and the terms reproduce the score. Check this in legible-simulation's inspector while playing.
- Principle 6 is at risk if guests read shops' state, or if shops read hunger. Guests read only offers and route distance. Hunger never leaves the guest.
- Principle 10 is at risk if choices depend on iteration order or on the C runtime's exp. Draws are keyed on the guest, purpose, tick, and index. The exp is the simulation's own.
- Principle 5 is at risk from thresholds. Hunger acts through a curve against a carry-on option. Footfall is weighted, not thresholded.
- Principle 2 is at risk when edits happen under a walking guest. The criteria above define each case: deleted paths, dead ends, starved or deleted targets, cut routes to a target, and cut routes home.
- Principle 4 is kept because every choice uses route distance, and a guest walking anywhere on its route's paving travels that route (decision 0038). The straight-line measures are a guest's walk across the paving toward points of its route, guest presence's sampling radius, the nearest approach between two guests, and moving a stranded guest to the nearest paving. None of them measures how far a guest travels.

## Dependencies

- deterministic-simulation (world-as-value): keyed draws, exp, the cycle, and state registration. Met.
- shared-medium (first-field-and-flow): fields, flows, and place carry-over. Met.
- navigable-networks (paths-become-routes): the guest network, the entrance anchor, and route distance. Met.
- plausible-operations (supplied-food-shop): food offers, and serving visits and meals. Met.
- effortless-building (sketch-a-park): the entrance, and rendering of the park the guests are drawn in. Met.

## Foundation

The foundation is hungry guests on one network with one need. They choose between food and carrying on by scored utility. They walk, queue, eat, and leave, and every choice is explained.

This is the smallest version in which the park's layout visibly matters to someone. It is useful on its own: a player can watch a park with shops and inspect it guest by guest.

## Milestones

1. `hungry-guests`: arrivals, stays, and hunger from keyed draws. At junctions, guests choose by softmax among food offers, carrying on, and heading home, with a commitment bonus. They move by next edges and wander by keyed draws. They queue through guest visits, eat meals, and leave. The milestone also adds the rules for stranded guests, hungry footfall, inspection records that explain choices, and guests drawn as simple shapes. Guests are the first holders of places. navigable-networks' paths-become-routes handed this milestone the design of how the previous networks stay available until guests carry their places over. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value, shared-medium's first-field-and-flow, navigable-networks' paths-become-routes, and plausible-operations' supplied-food-shop.

2. `spread-guests`: guests walk on the paving itself, each with its own ground position and speed. Each walks straight toward the farthest point of its network route it can reach with its box on the paving, keeps to its own side, and steps around nearby guests, which it senses through guest presence. Its place on the network follows from where it stands. Guests decide each turn about 10 m before it and walk through shaping points without choosing. Member of the good-paths slice. Depends on: hungry-guests, shared-medium's carrier-widths and ground-fields, and navigable-networks' wide-routes.

3. `a-day-out`: guests arrive by an authored curve over the opening hours and carry four needs: hunger, thirst, tiredness, and bladder. They weigh food, drink, rest, and toilet offers, and going home is a scored option that replaces the fixed stay. Each departure is tallied by its reason. Member of the day-at-the-park slice. Depends on: deterministic-simulation's park-clock, effortless-building's facilities-and-hours, and plausible-operations' served-needs.

Later milestones come from the deepening candidates once the slice has been played.

## Deepening candidates

- Rides as offers of fun and spectacle, with queues through the same visit flow.
- Personality: each guest scales the decision weights in its own way (decision 0019).
- Curiosity in wandering: guests prefer less-visited edges or new sights.
- Groups and families that choose together.
- Crowding: guests avoid busy paths, through navigable-networks' crowding cost.
- Thoughts: short statements for the player, drawn from choice explanations, as in RollerCoaster Tycoon. Gated on: the player-facing UI question.
- Walking off deleted paving, instead of moving to the nearest point of the guest paving.
- Aggregate guests at scale: distant or numerous guests are simulated as flows, and individuals are shown near the camera. Gated on: profiling.
- Local knowledge and wayfinding: a guest knows only what it has noticed. Each attraction publishes a noticeability that fades with route distance. A guest follows route distance's next steps with some uncertainty, and a park map lowers that uncertainty. Guests without maps wander, and can get lost in a large park. Builds on fuzzy choice.
- Fuzzy choice: a guest weighs only a few options it happens to notice, such as nearby or recently seen shops and rides. It doesn't score every reachable option at every junction. With this, a choice's cost doesn't grow with the number of attractions, and decisions feel human rather than exhaustive.

## Open questions

- Tuning values: arrival rate, stay length, hunger rates, walking speed, softmax temperature, the carry-on utility, and the commitment bonus. They are set while planning hungry-guests. Tune the carry-on utility with several shops present, because logit has independence of irrelevant alternatives.
- The player-facing form of guest explanations (docs/open-questions.md). The user decides. Until then, ImGui inspectors from legible-simulation stand in.

## Research notes

- The Sims' motives and advertising objects match hunger and food offers.
- Utility AI's response curves support decision 0020. Summed terms keep choices explainable.
- Softmax is McFadden's logit. In logit, duplicated options attract extra share, so the carry-on option is tuned with several shops.
- RollerCoaster Tycoon's guests get lost or fixated. Routing on the distance field and re-scoring at junctions avoids both.

The details are in RESEARCH.md.
