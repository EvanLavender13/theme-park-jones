# guests

This module holds the park's visitors. It serves the believable-guests capability (plans/believable-guests). Guests arrive, wander, grow hungry, choose shops, eat, and go home. They leave hungry footfall on the guest network. The module is part of tpj_sim and follows its contract (src/sim/SPEC.md). A guest's hunger leaves the module only as part of hungry footfall, summed over guests, so the park responds to crowds, not to individual guests (principles 3, 5, and 6).

## Boundary

addGuests registers the guest state, the ground guests walk on and the resolver that derives it, the systems that step guests and their footfall, and the finishers that carry both across a resolution. addPark calls it after addOperations, so guests step after shops and depots.

A guest reads the park only through the medium and intent:

- the guest network (parkNetwork, previousNetwork);
- guest route distance;
- the food-offer field;
- its own guest-visits and meals stocks;
- parkEntrances (decision 0025);
- the guest paving, from parkSurface with the guest walkways, walkwayPiece, parkSegments, and keyedWalkways (decision 0025).

A guest visits shops and receives meals through the guest-visits and meals flows. No other module reads a guest's state.

Other modules call guests.h (parkGuests, addGuest, guestRecord, and guestOptions) and footfall.h (the HungryFootfall field).

## Arriving and leaving

Guests arrive at regular intervals at each entrance connected to the guest network, one per entrance at a time. Each guest stays for a drawn length of time. If an entrance has no connection, it admits no one, and that is a legitimate state (principle 2). A guest leaves the park when it walks back to an entrance after its stay. It also leaves when no guest paving is left for it to stand on.

addGuest(world, place, stayUntil) is the only way a guest comes into being. Arrivals use it, and so does code outside the module, such as park generators, which can't name any guest component (principle 6). It returns the new guest's key. If the place is not on the guest network, addGuest refuses it. A new guest stands at the place's ground position. A new guest's hunger and hunger rate are drawn, so guests differ from one another. The same world and seed always give the same guests (principle 8).

## Hunger

A guest grows hungrier each tick at its own rate, up to full hunger. A meal relieves its hunger by the relief the shop offered when the guest chose it.

## Choosing

A guest chooses what to do by scoring its options and picking one at random, weighted by score (decision 0019). Its options are:

- each shop it can reach that offers meals;
- carrying on as it is;
- heading home, once its stay is over and an entrance is reachable.

A shop scores higher the hungrier the guest is. It scores lower the farther away it is and the longer its wait. If the guest is already heading to a shop, that shop scores a little higher, so guests don't flip between shops. If one option scores higher than another, it is never less likely to be picked. If two options score the same, they are equally likely. Once a stay is over, heading home outweighs any meal.

A guest finds shops only through route distance and the food-offer field. If it can't walk to a shop, or the shop's food-offer shows no meals, that shop is never an option. An entrance is never an option for food.

A guest chooses at every node its place reaches or passes. It also chooses as soon as its target becomes unreachable or stops offering meals, and when its visit comes back.

## Walking

A guest stands at a ground position of its own on the guest paving, and saves hold it. It walks inside the outline of the paving it is on: the visible edge lines of its own joined paving (sim/park/SPEC.md), and the end of each walkway at its door. Where paths that aren't joined overlap, the other path's edge lines don't bound it, so it stays on its own path.

The network chooses a guest's route. When a guest heads to a shop or home, its route follows route distance there, node after node. When it wanders, its route runs to the next node, and at each node it picks at random among the ways leading out of it. It never turns back unless there is no other way, as at a path's end.

A guest walks straight toward its aim at a fixed speed. Its aim is the farthest point of its route, looking at most AIM_LOOKAHEAD along it, that a straight line reaches with the guest's box on the paving. The aim never moves back along the route until the route changes. The box is on the paving while the guest keeps at least GUEST_CLEARANCE, half the diagonal of its box, from every line that bounds it. If the guest stands nearer a line than that, as at the entrance's door it arrives through, it never comes nearer that line and never crosses it. So a guest heading for a shop across a wide street walks straight to its door, and at a bend it cuts the inside without leaving the paving. Walking anywhere on its route's paving travels that route, so route distance still measures how far it goes (principle 4, decision 0038). The work of walking a guest depends on the paving near it and its route ahead, not on the size of the park.

A guest's place on the network is the point of its route, between its place and its aim, nearest where it stands. Its place moves only forward along its route, so the guest passes each node of its route in order. A guest reaches the end of its route when it stands on it, or when it has walked as far toward it as its box allows, with no node of its route left between, as in front of a door or at a path's free end. Its speed doesn't depend on how the network is cut into edges.

## Visits and meals

When a guest arrives at its target shop, it sends the shop a visit through the guest-visits flow. It then waits there until the visit comes back, whatever else changes. If the visit comes back with a meal, the guest was served: it eats, its hunger falls, and it records the meal. If the visit comes back without a meal, the guest was turned away. Either way, it then chooses what to do next. A guest has at most one visit outstanding. It consumes returned visits with FINISHED_CAUSE and meals with EATEN_CAUSE, which other modules count.

## Carrying across an edit

When the player edits the park, a guest keeps its ground position if its box still fits on guest paving there. It stays on the paving its place is carried to (sim/medium/SPEC.md) if that paving lies under it, so where paths that aren't joined overlap, it stays on its own path. Its place is then the carried place. If the carried place isn't on the paving under it, its place is the nearest place on the carrier of the segment, point, or walkway it stands on. If its box no longer fits where it stands, the guest moves to the nearest point where it fits on the guest paving, and takes its place from the paving there. If no guest paving is left, it leaves the park when it next steps. If the guest is carried onto a carrier that runs the other way there, it turns, so it keeps walking the same way over the ground. Carrying changes only where guests stand, their places, and which way they walk. If a guest is waiting, it keeps waiting. If it is heading to a shop, it keeps its target until its next walk. In a candidate, guests stand where committing the candidate's edit would put them (principle 8).

## Hungry footfall

HungryFootfall is the kept scalar field hungry-footfall on the guest network (decision 0031). It shows where hungry guests spend their time. Each stretch of the network, an edge, holds a moving average of the summed hunger of the guests on it, with time constant FOOTFALL_TIME. Every guest counts, with no threshold, so ten hungry guests weigh ten times as much as one (principle 5). If nobody walks a stretch, its value decays toward 0 as time passes. While the stretch is empty, its value never moves away from 0. The decay uses only the simulation's own math, so it is the same on every build (decision 0022).

fieldValue of hungry footfall anywhere inside a stretch is that stretch's value. At a node, it is the mean of the stretches meeting there. The work of stepping footfall is in proportion to the number of guests, not to the size of the network.

Across an edit:

- if a stretch is untouched, it keeps its value exactly;
- if a stretch is split, its value stays with the part holding its middle, even if that part has a new key;
- if stretches are joined, their values add;
- if a path is deleted, its footfall is dropped, except where another path runs over a stretch's middle.

## Inspection

guestRecord(world, key) returns a guest's inspection record for display (decision 0025). The record holds its activity, its place, the ground position it stands on, hunger, stay, target, meals eaten, and last meal.

guestOptions(world, key) returns the options the guest would weigh if it chose now. Each option has its terms (Relief, Distance, Wait, Commitment), its Score, and its Probability. The Score is exactly the sum of the terms.

Neither function changes the world or draws, and neither adds to a save. guestOptions costs as much as one choice, so call it for a guest being inspected, not for every guest in a frame.
