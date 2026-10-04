# guests

Believable guests (plans/believable-guests): the park's visitors, who arrive, wander, grow hungry, choose shops, eat, and go home, and the hungry footfall they leave on the guest network. It is part of tpj_sim and follows its contract (src/sim/SPEC.md). A guest's hunger leaves the module only summed into hungry footfall, so the park responds to crowds, not to individual guests (principles 3, 5, and 6).

## Boundary

addGuests registers the guest state, the systems that step guests and their footfall, and the finishers that carry both across a resolution. addPark calls it after addOperations, so guests step after shops and depots.

A guest reads the park only through the medium and intent: the guest network (parkNetwork, previousNetwork), guest route distance, the food-offer field, its own guest-visits and meals stocks, and parkEntrances (decision 0025). It visits shops and receives meals through the guest-visits and meals flows. No other module reads a guest's state.

Other modules call guests.h and footfall.h: parkGuests, addGuest, guestRecord, and guestOptions, and the HungryFootfall field.

## Arriving and leaving

Guests arrive at regular intervals at each entrance connected to the guest network, one per entrance at a time, and each stays for a drawn length of time. An entrance with no connection admits no one, a legitimate state (principle 2). A guest leaves the park when it walks back to an entrance after its stay, or when the guest network has nowhere left for it to stand.

addGuest(world, place, stayUntil) is the one way a guest comes into being, used by arrivals and by code outside the module such as park generators, which can name no guest component (principle 6). It gives the new guest's key, and refuses a place that is not on the guest network. A new guest's hunger and hunger rate are drawn, so two guests are not alike, and the same world and seed always give the same guests (principle 8).

## Hunger

A guest grows hungrier each tick at its own rate, up to full hunger, and a meal relieves it by the relief the shop offered when the guest chose it.

## Choosing

A guest chooses what to do by scoring its options and picking one at random weighted by score (decision 0019). Its options are each shop it can reach that offers meals, carrying on as it is, and, once its stay is over and an entrance is reachable, heading home. A shop scores higher the hungrier the guest is and lower the farther away it is and the longer its wait, and the shop it is already heading to scores a little higher, so guests do not flip between shops. A higher score is never less likely to be picked, and equal scores are equally likely. Once a stay is over, heading home outweighs any meal.

A guest finds shops only through route distance and the food-offer field, so a shop it cannot walk to, or one that says it has no meals, is never an option. An entrance is never an option for food.

A guest chooses at every node it leaves, as soon as its target becomes unreachable or stops offering meals, and when its visit comes back.

## Walking

A guest walks at a fixed speed along the guest network's carriers, never cutting across (principle 4), and its speed does not depend on how the network is cut into edges. A guest heading to a shop or home follows route distance to it. A wandering guest picks among the ways leading out of each node at random, never turning back unless there is no other way, as at a path's end.

## Visits and meals

A guest arriving at its target shop sends it a visit through the guest-visits flow and waits there, whatever else changes, until the visit comes back. A visit that comes back with a meal means the guest was served: it eats, its hunger falls, and it records the meal. One that comes back without a meal means it was turned away. Either way it then chooses what to do next. A guest has at most one visit outstanding. It consumes returned visits with FINISHED_CAUSE and meals with EATEN_CAUSE, which other modules count.

## Carrying across an edit

When the player edits the park, each guest keeps its ground position if its path survives the edit. A guest whose path or connector is deleted moves to the nearest point of the guest network. Carrying changes only where guests stand: a waiting guest keeps waiting and a guest heading to a shop keeps its target until its next walk. A candidate's guests stand where committing its edit would put them (principle 8).

## Hungry footfall

HungryFootfall, the kept scalar field hungry-footfall on the guest network (decision 0031), shows where hungry guests spend their time. Each stretch of the network, an edge, holds a moving average, with time constant FOOTFALL_TIME, of the summed hunger of the guests on it. Every guest counts, with no threshold, so ten hungry guests weigh ten times one (principle 5). A stretch nobody walks decays toward 0 as time passes, and its value never moves away from 0 while it is empty. The decay uses only the simulation's own math, so it is the same on every build (decision 0022).

fieldValue of hungry footfall anywhere inside a stretch is that stretch's value, and at a node the mean of the stretches meeting it. Stepping footfall costs work in proportion to guests, not to the size of the network.

Across an edit, an untouched stretch keeps its value exactly, a split stretch's value stays with the half holding its middle, joined stretches' values add, and a deleted path's footfall is dropped.

## Inspection

guestRecord(world, key) gives a guest's inspection record for display (decision 0025): its activity, place and ground position, hunger, stay, target, meals eaten, and last meal. guestOptions(world, key) gives the options the guest would weigh if it chose now, each with its terms (Relief, Distance, Wait, Commitment), its Score, which is exactly the sum of its terms, and its Probability. Neither changes the world or draws, and neither adds to a save. guestOptions costs one choice, so it is called for a guest being inspected, never for every guest in a frame.
