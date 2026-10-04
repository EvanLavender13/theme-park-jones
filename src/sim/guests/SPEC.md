# guests

This module holds the park's visitors. It serves the believable-guests capability (plans/believable-guests). Guests arrive, wander, grow hungry, choose shops, eat, and go home. They leave hungry footfall on the guest network. The module is part of tpj_sim and follows its contract (src/sim/SPEC.md). A guest's hunger leaves the module only as part of hungry footfall, summed over guests. So the park responds to crowds, not to individual guests (principles 3, 5, and 6).

## Boundary

addGuests registers the guest state, the systems that step guests and their footfall, and the finishers that carry both across a resolution. addPark calls it after addOperations, so guests step after shops and depots.

A guest reads the park only through the medium and intent:

- the guest network (parkNetwork, previousNetwork);
- guest route distance;
- the food-offer field;
- its own guest-visits and meals stocks;
- parkEntrances (decision 0025).

A guest visits shops and receives meals through the guest-visits and meals flows. No other module reads a guest's state.

Other modules call guests.h (parkGuests, addGuest, guestRecord, and guestOptions) and footfall.h (the HungryFootfall field).

## Arriving and leaving

Guests arrive at regular intervals at each entrance connected to the guest network, one per entrance at a time. Each guest stays for a drawn length of time. An entrance with no connection admits no one, and that is a legitimate state (principle 2). A guest leaves the park when it walks back to an entrance after its stay. It also leaves when the guest network has nowhere left for it to stand.

addGuest(world, place, stayUntil) is the only way a guest comes into being. Arrivals use it, and so does code outside the module, such as park generators, which can't name any guest component (principle 6). It returns the new guest's key. It refuses a place that is not on the guest network. A new guest's hunger and hunger rate are drawn, so guests differ from one another. The same world and seed always give the same guests (principle 8).

## Hunger

A guest grows hungrier each tick at its own rate, up to full hunger. A meal relieves its hunger by the relief the shop offered when the guest chose it.

## Choosing

A guest chooses what to do by scoring its options and picking one at random, weighted by score (decision 0019). Its options are:

- each shop it can reach that offers meals;
- carrying on as it is;
- heading home, once its stay is over and an entrance is reachable.

A shop scores higher the hungrier the guest is. It scores lower the farther away it is and the longer its wait. The shop the guest is already heading to scores a little higher, so guests don't flip between shops. A higher score is never less likely to be picked, and equal scores are equally likely. Once a stay is over, heading home outweighs any meal.

A guest finds shops only through route distance and the food-offer field. So a shop it can't walk to, or one that says it has no meals, is never an option. An entrance is never an option for food.

A guest chooses at every node it leaves. It also chooses as soon as its target becomes unreachable or stops offering meals, and when its visit comes back.

## Walking

A guest walks at a fixed speed along the guest network's carriers and never cuts across (principle 4). Its speed doesn't depend on how the network is cut into edges. A guest heading to a shop or home follows route distance to it. A wandering guest picks at random among the ways leading out of each node. It never turns back unless there is no other way, as at a path's end.

## Visits and meals

When a guest arrives at its target shop, it sends the shop a visit through the guest-visits flow. It then waits there until the visit comes back, whatever else changes. If the visit comes back with a meal, the guest was served: it eats, its hunger falls, and it records the meal. If the visit comes back without a meal, the guest was turned away. Either way, it then chooses what to do next. A guest has at most one visit outstanding. It consumes returned visits with FINISHED_CAUSE and meals with EATEN_CAUSE, which other modules count.

## Carrying across an edit

When the player edits the park, each guest keeps its ground position if its path survives the edit. A guest whose path or connector is deleted moves to the nearest point of the guest network. Carrying changes only where guests stand. A waiting guest keeps waiting, and a guest heading to a shop keeps its target until its next walk. A candidate's guests stand where committing its edit would put them (principle 8).

## Hungry footfall

HungryFootfall is the kept scalar field hungry-footfall on the guest network (decision 0031). It shows where hungry guests spend their time. Each stretch of the network, an edge, holds a moving average of the summed hunger of the guests on it, with time constant FOOTFALL_TIME. Every guest counts, with no threshold, so ten hungry guests weigh ten times as much as one (principle 5). A stretch nobody walks decays toward 0 as time passes. While it is empty, its value never moves away from 0. The decay uses only the simulation's own math, so it is the same on every build (decision 0022).

fieldValue of hungry footfall anywhere inside a stretch is that stretch's value. At a node, it is the mean of the stretches meeting there. The work of stepping footfall is in proportion to the number of guests, not to the size of the network.

Across an edit:

- an untouched stretch keeps its value exactly;
- a split stretch's value stays with the half holding its middle;
- joined stretches' values add;
- a deleted path's footfall is dropped.

## Inspection

guestRecord(world, key) returns a guest's inspection record for display (decision 0025). The record holds its activity, place and ground position, hunger, stay, target, meals eaten, and last meal.

guestOptions(world, key) returns the options the guest would weigh if it chose now. Each option has its terms (Relief, Distance, Wait, Commitment), its Score, and its Probability. The Score is exactly the sum of the terms.

Neither function changes the world or draws, and neither adds to a save. guestOptions costs as much as one choice, so call it for a guest being inspected, not for every guest in a frame.
