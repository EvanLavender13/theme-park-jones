# guests

This module holds the park's visitors. It serves the believable-guests capability (plans/believable-guests). Guests arrive, wander, grow hungry, thirsty, and tired, choose shops, benches, and toilet blocks, and go home. They leave hungry footfall on the guest network. The module is part of tpj_sim and follows its contract (src/sim/SPEC.md). A guest's hunger leaves the module only as part of hungry footfall, summed over guests, so the park responds to crowds, not to individual guests (principles 3, 5, and 6). Its other needs never leave the module.

## Boundary

addGuests registers the guest state, guest presence, the ground guests walk on and the resolver that derives it, the departure tally and the resolver that creates it, the systems that step guests and their footfall, and the finishers that carry both across a resolution. addPark calls it after addOperations, so guests step after shops and depots.

A guest reads the park only through the medium and intent:

- the guest network (parkNetwork, previousNetwork);
- guest route distance;
- the need offer fields food-offer, drink-offer, rest-offer, and toilet-offer;
- the best rest seats and best meal seats fields, through bestSeatsAt;
- the seat places field, for the pose of the piece the guest heads to or sits at;
- the seat offers field, for the rest relief of a piece a guest picks to eat at;
- its own stocks of each need's visits and servings: guest-visits and meals, drink-visits and drinks, rest-visits and rests, toilet-visits and toilet-uses, and seat-visits and rests;
- parkEntrances, and the opening hours through parkHours and isOpenAt (decision 0025);
- the park's clock, parkTime;
- the guest paving, from routes' walkedSurface, walkwayPiece, parkSegments, and keyedWalkways (decision 0025);
- guest presence, which only guests publish and sample.

A guest visits facilities, which are shops, benches, and toilet blocks, and receives servings through those flows. A guest also visits pieces of furniture, through seat-visits, and receives rests from them. No other module reads a guest's state.

Other modules call guests.h (parkGuests, addGuest, guestRecord, guestOptions, departureTally, and arrivalCurve) and footfall.h (the HungryFootfall field).

parkGuests can also put the keys in a list the caller keeps, clearing it first, and then allocates nothing once the list has grown.

## Arriving and leaving

Guests arrive at random at each entrance connected to the guest network, at the rate the arrival curve gives for the time of day. In each tick, each such entrance admits at most one guest, by a draw keyed on the entrance and the tick, so the same world and seed always admit the same guests (principle 8). If an entrance has no connection, it admits no one, and that is a legitimate state (principle 2). A guest leaves the park when it walks out of an entrance heading home. It also leaves when no guest paving is left for it to stand on. A guest that leaves drops its entry in guest presence.

arrivalCurve(world, second) gives the arrival curve: the mean number of guests each connected entrance admits per game hour at that second of the day. It follows the park's opening hours. A few guests arrive at opening, the most come in the first third of the hours, fewer follow, and none arrive over the last part of the hours or while the park is closed. If the park is open all day, the curve runs over the whole day from the opening time. It depends only on the opening hours, so on a candidate with other hours it gives the curve those hours would admit guests by (decision 0025). It changes nothing and draws nothing.

addGuest(world, place) is the only way a guest comes into being. Arrivals use it, and so does code outside the module, such as park generators, which can't name any guest component (principle 6). It returns the new guest's key. If the place is not on the guest network, addGuest refuses it. A new guest stands at the place's ground position and records the tick it arrived. A new guest's hunger, thirst, and bladder, the rates its hunger, thirst, and tiredness rise at, its speed, and its share of the width are drawn, so guests differ from one another. Its tiredness starts at 0. The same world and seed always give the same guests (principle 8).

## Needs

A guest has four needs: hunger, thirst, tiredness, and bladder. Each runs from 0 to 1. Hunger and thirst rise each tick at the guest's own rates. Tiredness rises with each meter the guest walks across the ground, at its own rate, so it doesn't rise while the guest waits. Each meal the guest eats fills its bladder by BLADDER_PER_MEAL, and each drink by BLADDER_PER_DRINK. No need rises past 1.

A drink, a rest, or a toilet use relieves the need it answers by the relief the facility or piece offered when the guest chose it: a drink relieves thirst, a rest tiredness, and a toilet use the bladder. A meal relieves hunger when the guest eats it, by a relief that depends on where it eats (Meals). No need falls below 0, and a serving relieves no other need.

## Choosing

A guest chooses what to do by scoring its options and picking one at random, weighted by score (decision 0019). Unless it is carrying food, its options are:

- each offer it can reach: one for each need a facility offers, so a shop that sells both goods gives two;
- each piece that best rest seats gives at its place, other than the pieces it remembers as full (Seats);
- carrying on as it is;
- heading home, whenever an entrance is reachable.

While it carries food, its only options are eating now and each piece that best meal seats gives at its place, other than the pieces it remembers as full (Meals).

An offer scores higher the greater the need it answers. It scores lower the farther away it is and the longer its wait. If the guest is already heading to a facility, that facility's offer for the same need scores a little higher, so guests don't flip between facilities. A seat scores as an offer does, with no wait: a rest seat by tiredness and the piece's rest relief, and a meal seat by hunger and the piece's meal relief. Eating now scores only by hunger, with STANDING_MEAL_RELIEF as its relief. If the guest is heading to a piece, that piece's option scores a little higher. If one option scores higher than another, it is never less likely to be picked. If two options score the same, they are equally likely. While the park is closed, heading home outweighs every other option. A guest carrying food eats before it can head home.

A guest finds offers only through route distance and the four need offer fields. If it can't walk to a facility, or the facility's entry in a need's field doesn't say it can serve, that need's offer there is never an option. An entrance is never an option for any need.

A guest makes its choice for each node of its route a little before it gets there: when its place comes within TURN_LOOKAHEAD of the node along its route, or when it stands on the node. It chooses once for each node it comes to. It makes no choice for a shaping point, a node with exactly two edges and no anchor, which only bends a path, so the number of points the player draws doesn't change how guests choose. A guest heading to a facility or home makes no choice for the node it is heading to. A guest also chooses as soon as its target becomes unreachable, stops offering the need it heads there for, or is a piece the field at its place no longer gives, when its visit comes back, and when the park closes (Going home).

## Going home

Heading home's score is the sum of a base, five reason terms, and a commitment term, added in this order:

- the base, HOME_BASE, which is below 0, so a guest whose needs are met rarely heads home before closing;
- hunger, thirst, tiredness, and the toilet: each is HOME_NEED_WEIGHT times the need curve at its need's level, the bladder's for the toilet;
- closing: 0 until CLOSING_LEAD game seconds before the park closes, then rising linearly to CLOSING_SCORE at closing, and CLOSING_SCORE while the park is closed. If the park is open all day, it is always 0;
- commitment: COMMITMENT_BONUS while the guest is heading home, and otherwise 0.

For each need, an offer answering it has a larger relief term than the need's term in heading home, so a guest prefers a facility close by to heading home for one need. When several needs are high at once, their terms add up, and heading home can win even with facilities in reach. If nothing in reach answers a need, its term keeps growing as the need does.

Each time a guest picks heading home, it records the pick's reason: the largest of its five reason terms, or, when several are largest, the first of them in the order hunger, thirst, tiredness, toilet, closing. When the guest walks out of an entrance, the reason it last recorded is the reason it left.

When the clock passes the closing time, every guest that is walking chooses in that tick. A guest heading home keeps making its choice for each node of its route, so it can still turn aside to a facility it needs on the way. Whether the park is open is judged by its current hours, through isOpenAt.

## Departures

departureTally(world) gives the departure tally: how many guests have walked out of an entrance since the clock last passed the opening time, by the reason they left. When the clock passes the opening time, the tally starts again from 0. If the park is open all day, it never does. A guest that leaves because no guest paving is left isn't counted. The tally is saved state on an entity of its own, which resolving the park creates when it is missing. Before the park is first resolved, departureTally gives 0 for every reason.

## Walking

A guest stands at a ground position of its own on the guest paving, and saves hold it. It walks inside the outline of the paving it is on: the visible edge lines of its own joined paving (sim/park/SPEC.md), and the end of each walkway at its door. Where paths that aren't joined overlap, the other path's edge lines don't bound it, so it stays on its own path.

The network chooses a guest's route. When a guest heads to a facility or home, its route follows route distance there, node after node. When it wanders, its route runs through shaping points to the next node it chooses for. When it makes that choice and carries on wandering, it picks at random among the ways leading out of the node, and its route runs on along the way it picked, through shaping points, to the node after. It never picks the way it arrives by unless there is no other way, as at a path's end. If it picks that way, its route stops at the node, so it walks to the end before it turns.

When a guest heads to a piece of furniture, its route follows the next steps of the field it found the piece in, best rest seats or best meal seats, to the piece's seat place, which may lie inside an edge. Over the route's last stretch, the guest moves toward the piece's side of the paving, as far as its box fits, instead of to its own side. So it stops on the paving's edge beside the piece.

A guest walks straight toward its aim at its own speed. Its aim is the farthest point of its route, moved to the guest's side and looking at most AIM_LOOKAHEAD along it, that a straight line reaches with the guest's box on the paving. The aim never moves back along the route until the route changes. The box is on the paving while the guest keeps at least GUEST_CLEARANCE, half the diagonal of its box, from every line that bounds it. If the guest stands nearer a line than that, as at the entrance's door it arrives through, it never comes nearer that line and never crosses it. So a guest heading for a facility across a wide street walks straight to its door, and at a bend it cuts the inside without leaving the paving. Walking anywhere on its route's paving travels that route, so route distance still measures how far it goes (principle 4, decision 0038). The work of walking a guest depends on the paving near it and its route ahead, not on the size of the park.

Each guest walks at its own speed and keeps to its own side of its route for its whole stay, so faster guests overtake slower ones. Speeds spread evenly by WALK_SPEED_SPREAD either side of WALK_SPEED, so guests walk at WALK_SPEED on average. A guest's side is a place across the width its box can use, measured from the left of the way it walks. Sides lie from SIDE_BIAS of the way across up to the right edge, and fewer guests keep near either end of that range than near its middle. On a wide path, guests walking each way stream along opposite sides, and on a narrow one they walk close together. Each point of a guest's route is moved to its side. If the guest's box wouldn't fit at its side of a point, as at a door, at a path's free end, or around the outside of a junction, that point stays in the middle. So a guest heading for a door walks to the door itself.

Guests step aside from one another. At the end of each tick it walks, a guest sets its entry in guest presence, the ground field guest-presence: where it stands, and its velocity over that tick. A guest that starts waiting at a facility or a piece of furniture drops its entry, and sets it again when it next walks. A guest senses other guests only through guest presence, from the entries within SIDESTEP_RADIUS of where it stands. Those say where each stood and how it was moving at the end of the last tick, so the order guests step in never changes where they walk (principle 10). For each guest ahead of it, a guest predicts where they would come nearest if both kept walking as they were. If that is within SIDESTEP_HORIZON and nearer than SIDESTEP_DISTANCE, it steps aside, away from where the other would be, fast enough to pass at SIDESTEP_DISTANCE, and right if the other would be dead ahead. Its steps aside from each such guest add up, to at most SIDESTEP_SPEED either way. Guests walking side by side at one pace don't step aside, and a guest doesn't step aside from those behind it. A guest steps aside only where its box stays on the paving and it can still walk straight to its aim from where it steps to. Otherwise it walks on toward its aim.

A guest's place on the network is the point of its route, between its place and its aim, whose position on the guest's side lies nearest where it stands. Its place moves only forward along its route, so the guest passes each node of its route in order. A guest reaches the end of its route when it stands on it, or when it has walked as far toward it as its box allows, with no node of its route left between, as in front of a door or at a path's free end. Its speed doesn't depend on how the network is cut into edges.

## Visits and servings

When a guest arrives at its target facility, it sends the facility a visit for the need it chose: guest-visits for hunger, drink-visits for thirst, rest-visits for tiredness, and toilet-visits for the bladder. It then waits there until the visit comes back, whatever else changes. If the visit comes back with that need's serving, a meal, a drink, a rest, or a toilet use, the guest was served. A drink, a rest, or a toilet use relieves its need at once, and the guest carries a meal (Meals). If the visit comes back without a serving, the guest was turned away. Either way, it then chooses what to do next. A guest has at most one visit outstanding. It consumes returned visits with FINISHED_CAUSE, meals with EATEN_CAUSE, drinks with DRUNK_CAUSE, rests with RESTED_CAUSE, and toilet uses with USED_CAUSE, which other modules count.

## Seats

When a guest reaches the seat place of the piece it heads to, it sends the piece one seat visit and waits. If the visit hasn't come back VISIT_DELAY plus RETURN_DELAY ticks after it was sent, the piece has seated the guest, and the guest is seated until the visit comes back. When the visit comes back with a rest, tiredness falls by the rest relief the piece offered when the guest chose it, and a guest that came to eat eats its meal (Meals). If it comes back with nothing, the piece was full. The guest remembers it, with the tick it found it full, and chooses in that same tick, wherever it stands. Either way, the guest chooses again at once. It consumes the returned visit with FINISHED_CAUSE and the rest with RESTED_CAUSE.

A guest remembers a piece as full for FULL_MEMORY_TIME ticks after it found it full, and remembers at most SEAT_CHOICES pieces. If it finds one more piece full while it remembers that many, it forgets the one it found full first. While it remembers a piece, that piece is never one of its options.

## Meals

When a guest's visit to a shop comes back with a meal, the guest holds the meal and is carrying food until it eats it. It chooses in that same tick. If it picks eating now, it eats the meal at once, hunger falls by STANDING_MEAL_RELIEF, and it walks on. If it picks a meal seat, it walks to the piece and visits it as Seats says. When the visit comes back with a rest, the guest eats the meal there. Hunger falls by the meal relief, and tiredness by the rest relief, that the piece offered when the guest chose it, even if an edit has removed the piece since. If the piece was full, the guest still carries its meal when it chooses again.

When a guest eats a meal, it consumes it with EATEN_CAUSE and records it, with whether it ate seated. If a guest leaves the park while it carries food, the ledger consumes the meal as discarded (sim/medium/SPEC.md).

## Carrying across an edit

When the player edits the park, a guest keeps its ground position if its box still fits on guest paving there. It stays on the paving its place is carried to (sim/medium/SPEC.md) if that paving lies under it, so where paths that aren't joined overlap, it stays on its own path. Its place is then the carried place. If the carried place isn't on the paving under it, its place is the nearest place on the carrier of the segment, point, or walkway it stands on. If its box no longer fits where it stands, the guest moves to the nearest point where it fits on the guest paving, and takes its place from the paving there. If no guest paving is left, it leaves the park when it next steps. If the guest is carried onto a carrier that runs the other way there, it turns, so it keeps walking the same way over the ground. A guest keeps the choice it has made for a node ahead if the edit leaves that node where it was. A wandering guest also needs the edge it picked out of the node to be whole and where it was. Otherwise the guest makes that choice again. Carrying changes only where guests stand, their places, which way they walk, and which choices for nodes ahead they keep. If a guest is waiting, it keeps waiting. If it is heading to a facility or a piece of furniture, it keeps its target until its next walk. In a candidate, guests stand where committing the candidate's edit would put them (principle 8).

## Hungry footfall

HungryFootfall is the kept scalar field hungry-footfall on the guest network (decision 0031). It shows where hungry guests spend their time. Each stretch of the network, an edge, holds a moving average of the summed hunger of the guests on it, with time constant FOOTFALL_TIME. Every guest counts, with no threshold, so ten hungry guests weigh ten times as much as one (principle 5). If nobody walks a stretch, its value decays toward 0 as time passes. While the stretch is empty, its value never moves away from 0. The decay uses only the simulation's own math, so it is the same on every build (decision 0022).

fieldValue of hungry footfall anywhere inside a stretch is that stretch's value. At a node, it is the mean of the stretches meeting there. The work of stepping footfall is in proportion to the number of guests, not to the size of the network.

Across an edit:

- if a stretch is untouched, it keeps its value exactly;
- if a stretch is split, its value stays with the part holding its middle, even if that part has a new key;
- if stretches are joined, their values add;
- if a path is deleted, its footfall is dropped, except where another path runs over a stretch's middle.

## Inspection

guestRecord(world, key) returns a guest's inspection record for display (decision 0025). The record holds its activity, its place, the ground position it stands on, its four needs, the tick it arrived, its target and the need it heads there for, the reason it last recorded for heading home, whether it is carrying food, while it is seated and its piece is still laid, the seat place of the piece it sits at, and the pieces it remembers as full, each with the tick it found it full, meals eaten, and last meal, with whether it was eaten seated.

guestOptions(world, key) returns the options the guest would weigh if it chose now. Each option has its kind, the facility and need of an offer, its terms, heading home's reason, its Score, and its Probability. An offer's terms are Relief, Distance, Wait, and Commitment, added in that order. A seat's terms are Relief, Distance, and Commitment, added in that order. Eating now's only term is Relief. Heading home's are Base, a term in Reasons for each of HOME_REASONS, and Commitment, added as Going home gives. Every other term is 0. The Score is exactly the sum of the terms.

Neither function changes the world or draws, and neither adds to a save. guestOptions costs as much as one choice, so call it for a guest being inspected, not for every guest in a frame.
