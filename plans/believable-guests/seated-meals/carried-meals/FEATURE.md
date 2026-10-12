# Feature: Carried Meals

## Summary

Guests carry the food they buy and choose where to eat it. When a shop serves a guest, the meal stays in the guest's hands, and the guest chooses at once between eating now and each piece that best meal seats gives where it stands. Eating now relieves hunger by STANDING_MEAL_RELIEF in that tick, and the guest walks on. A guest that picks a meal seat walks to it and sits as a tired guest does. When its seat visit comes back with a rest, it eats there: hunger falls by the piece's meal relief and tiredness by its rest relief. A guest turned away from a full piece still carries its meal and chooses again. While it carries food, a guest weighs nothing else. The last meal records whether it was eaten seated, and a guest that leaves carrying food has its meal discarded by the ledger. The Inspector says when a guest is carrying food and counts eating now under hunger. Every park with guests changes, because a meal eaten standing relieves less than a shop's offer did. full.park, whose guest segments all carry furniture, is where meal seats are played until seat-parks.

## Acceptance criteria

1. When a guest's visit to a shop comes back with a meal, the guest holds the meal, and its record says it is carrying food. It chooses in that tick. While it carries food, its options are eating now and each piece that best meal seats gives at its place, other than the pieces it remembers as full, and nothing else. Eating now has kind eat-now, need hunger, and only a Relief term, RELIEF_WEIGHT times hunger's curve times STANDING_MEAL_RELIEF. A meal seat has kind seat, the piece as its facility, need hunger, and the terms of a rest seat with hunger's curve and the field's meal relief. A guest that isn't carrying food has no eat-now option and no hunger seat option. (check: the scratch program of criterion 3 calls guestOptions for every guest at every tick it steps and checks each option of a carrying guest, and that a guest not carrying has none of these. Expected: `meal options: N checked, 0 wrong` and `carrying: N`, each N above 0)
2. A guest that picks eating now eats in that tick. Its hunger falls by STANDING_MEAL_RELIEF, not below 0, and its bladder fills by BLADDER_PER_MEAL. Its last meal records the tick, hunger before and after, and that it was not seated. It is no longer carrying food, and it walks on. (check: the scratch program counts `standing meals: N wrong: 0`, N above 0)
3. A guest that picks a meal seat follows best meal seats' next steps to the piece, stops at the paving's edge beside it, and is seated as a tired guest is. When its seat visit comes back with a rest, it eats there. Hunger falls by the piece's meal relief and tiredness by its rest relief, as the piece offered them when the guest chose it, even if an edit has removed the piece since. The last meal records that it was seated. If the visit comes back with nothing, the guest remembers the piece as full, still carries its meal, and chooses again in that tick. (play full.park: follow a guest from a shop to a table and see it sit and eat; clear the furnishing of a segment whose tables have guests seated and see each still eat. Check: a scratch program steps a copy of tests/parks/stress/full.park for 1800 ticks, with furnishing cleared from every guest segment but the five with the lowest keys, so pieces fill and some carrying guests have no seat near. It prints `seated meals: N wrong: 0` and `meal turned away: N wrong: 0`, each N above 0)
4. Meals conserve. Each meal a guest eats is consumed with EATEN_CAUSE in the tick its record counts it. If a guest leaves the park carrying food, the ledger consumes its meal as discarded. (check: the scratch program also steps tests/parks/day.park from 9:00 to 9:00 the next day, and the full.park copy of criterion 3. At every tick of each, meals consumed as eaten rise by exactly the sum of the rise in meals eaten over the guests, and units created equal units held plus in transit plus consumed. Expected: `day.park: meals eaten N, ledger 0 wrong` and `full.park: meals eaten N, ledger 0 wrong`, each N above 0)
5. The guest Inspector has a "Carrying" row that says "food" or "nothing". Hunger's row counts eating now among the guest's hunger options and names it "eat now" when it scores highest. While the guest carries food, the thirst, tiredness, toilet, carrying on, and heading home rows say "carrying food". (play full.park with the Inspector open on a guest from the moment a shop serves it until it eats)
6. tpj_scenarios --slice-parks and tpj_bench --full-park remake the parks, and every park in tests/parks that holds guests changes. The play parks in parks/ gain the new saved fields and load. On full.park, the runtime report's over-budget count stays 0 of 25. (check: the remake commands exit 0, `tpj_scenarios --ticks 1` loads the play parks, and the runtime report compared with build/seat-rests/after.report)
7. src/sim/guests/SPEC.md and src/legible/SPEC.md describe the changes as built. (review)

## Tests

None. No park in tests/parks holds furniture with guests until seat-parks, so none holds a carrying guest or a seated meal yet. The new fields are part of the guest state already checked in every park with guests, so copies, saves, and hashes cover their format now. seat-parks makes noon.park hold a carrying guest. The conserved meals of criterion 4 are a property of the existing flow ledger, which its own checks cover.

## Medium

- Draws meals (flow, from plausible-operations' shops): a served guest now holds its meal until it eats, and consumes it with EATEN_CAUSE then. If the guest leaves first, the ledger consumes it as discarded.
- Samples best meal seats (resolved entry field, from navigable-networks' best-seats) at the guest's place for choice and heading, and at each node of its route ahead for the next step.
- Reads seat offers (resolved entry field, from plausible-operations' served-seats) from the piece's own slot when the guest picks a meal seat, for the rest relief a seated meal also gives.
- Reads seat places (resolved entry field, from effortless-building's furnished-paths) for the piece's pose, as seat-rests does.
- Supplies seat visits and draws rests (flows, with served-seats), as seat-rests does.
- Guests (inspection record): the record adds whether the guest is carrying food, and the last meal adds whether it was eaten seated. The Inspector reads them.

The guest learns it carries food only from its own ledger stock. It reads the piece only through the fields and the flows (principle 6). The new state is part of the guest entity and its ledger stock, which every park with guests holds.

## Spec changes

src/sim/guests/SPEC.md:

- Boundary, the read list: "the best rest seats field, through bestSeatsAt;" becomes "the best rest seats and best meal seats fields, through bestSeatsAt;". After the seat places line, add "the seat offers field, for the rest relief of a piece a guest picks to eat at;".
- Needs: "A serving relieves the need it answers by the relief the facility offered when the guest chose it: a meal relieves hunger, a drink thirst, a rest tiredness, and a toilet use the bladder." becomes "A drink, a rest, or a toilet use relieves the need it answers by the relief the facility or piece offered when the guest chose it: a drink relieves thirst, a rest tiredness, and a toilet use the bladder. A meal relieves hunger when the guest eats it, by a relief that depends on where it eats (Meals)."
- Choosing: "Its options are:" becomes "Unless it is carrying food, its options are:". After the list: "While it carries food, its only options are eating now and each piece that best meal seats gives at its place, other than the pieces it remembers as full (Meals)."
- Choosing: "A seat scores as an offer does, with the piece's rest relief and no wait." becomes "A seat scores as an offer does, with no wait: a rest seat by tiredness and the piece's rest relief, and a meal seat by hunger and the piece's meal relief. Eating now scores only by hunger, with STANDING_MEAL_RELIEF as its relief."
- Choosing: after "While the park is closed, heading home outweighs every other option.": "A guest carrying food eats before it can head home."
- Walking: "its route follows the best rest seats field's next steps to the piece's seat place" becomes "its route follows the next steps of the field it found the piece in, best rest seats or best meal seats, to the piece's seat place".
- Visits and servings: "If the visit comes back with that need's serving, a meal, a drink, a rest, or a toilet use, the guest was served, and that need falls. A guest that eats a meal also records it." becomes "If the visit comes back with that need's serving, a meal, a drink, a rest, or a toilet use, the guest was served. A drink, a rest, or a toilet use relieves its need at once, and the guest carries a meal (Meals)."
- Seats: "When the visit comes back with a rest, tiredness falls by the rest relief the piece offered when the guest chose it." becomes "When the visit comes back with a rest, tiredness falls by the rest relief the piece offered when the guest chose it, and a guest that came to eat eats its meal (Meals)."
- A new section after Seats:

      ## Meals

      When a guest's visit to a shop comes back with a meal, the guest holds the meal and is carrying food until it eats it. It chooses in that same tick. If it picks eating now, it eats the meal at once, hunger falls by STANDING_MEAL_RELIEF, and it walks on. If it picks a meal seat, it walks to the piece and visits it as Seats says. When the visit comes back with a rest, the guest eats the meal there. Hunger falls by the meal relief, and tiredness by the rest relief, that the piece offered when the guest chose it, even if an edit has removed the piece since. If the piece was full, the guest still carries its meal when it chooses again.

      When a guest eats a meal, it consumes it with EATEN_CAUSE and records it, with whether it ate seated. If a guest leaves the park while it carries food, the ledger consumes the meal as discarded (sim/medium/SPEC.md).

- Inspection: the record sentence adds "whether it is carrying food," before "while it is seated", and "meals eaten, and last meal" becomes "meals eaten, and last meal, with whether it was eaten seated". After "A seat's terms are Relief, Distance, and Commitment, added in that order.": "Eating now's only term is Relief."

src/legible/SPEC.md, Inspectors: the guest's first bullet adds "whether it is carrying food," after "the reason it heads home for,". After "A need's row counts seat options as offers of that need.": "Hunger's row also counts eating now, and names it "eat now" when it scores highest. While the guest carries food, every other row says that it is carrying food."

## Public interface

sim/guests/guests.h:

- ChoiceKind gains EatNow after Seat, with name "eat-now". The enum's comment adds "or eat the meal it carries now". The ChoiceOption comment adds "Eating now's only term is Relief."
- GuestMeal gains `bool Seated = false;` after After, visited last as `seated`. Its comment becomes "A meal a guest ate: the tick, its hunger before and after, and whether it ate seated."
- GuestRecord gains `bool Carrying = false;` after Reason. Its comment adds "whether it is carrying food" after "the reason it last recorded for heading home".
- `inline constexpr double STANDING_MEAL_RELIEF = 0.25;` after CHOICE_TEMPERATURE, with the comment "The hunger a meal relieves when a guest eats it now, standing."

## Files affected

- Modify: src/sim/guests/guests.h, src/sim/guests/guests.cpp, src/sim/guests/internal/guest.h, src/sim/guests/SPEC.md
- Modify: src/legible/inspect.cpp, src/legible/SPEC.md
- Modify: tests/parks/cut.park, evening.park, noon.park, warm.park, warm-main-street.park, tests/parks/stress/full.park, parks/busted-path.park, parks/jank.park, parks/radii.park

## Dependencies

- seat-rests: seat options, heading to a piece, seat visits, being seated, full pieces, and seated drawing. Merged.
- navigable-networks/best-seats: the best meal seats field. Merged.
- plausible-operations/served-seats: seat offers with both reliefs, and rests sent to guests whose piece is gone. Merged.

## Out of scope

- Furniture in the day parks, far-tables.park, unfurnished.park, the world checks' changes, criterion 8's comparisons, and tuning STANDING_MEAL_RELIEF: seat-parks.
- The Inspector's seated or standing meal line and the relief lost: legible-simulation's explained-seating.
- Counting a seated meal's rest in a meal seat's score, drinks carried to a seat, and food that loses relief while carried: the milestone's deepening candidates.

## Open questions

None.
