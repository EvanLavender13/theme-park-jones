# Feature: Inspectors

## Summary

Benches and toilet blocks get inspectors, and the guest Inspector's choice table shrinks to six fixed rows. With the Look tool, clicking a bench or a toilet block opens the Inspector on it, showing its places, how many are taken, and how many guests wait at its door. The guest's choice table has one row for each of its four needs, then carrying on and heading home. A need's row shows the guest's best offer of that need and the chance the guest picks any offer of that need. The table no longer grows with the park.

## Acceptance criteria

1. With the Look tool, clicking a bench or a toilet block opens the Inspector on it and highlights it. The Inspector's title is "Bench" or "Toilet block" and the key, and its rows are Places, Taken, and Queue. The rows follow every tick. After the facility is removed, the Inspector says "No longer in the park". Clicking a depot or an entrance still leaves the Inspector as it was. (play: noon.park at 1x, clicking the bench and the toilet block while guests use them, then removing the bench)
2. The guest Inspector's choice table has six rows in this order: Hunger, Thirst, Tiredness, Toilet, Carry on, and Head home. A need's row shows the facility of the guest's highest-scoring offer of that need, that offer's relief, distance, wait, and commitment terms, and its score. Its chance is the sum of the probabilities of all the guest's offers of that need. If the guest has no offer of that need, the row says "no offer". Carry on's row shows its score and chance. Head home's row shows "home for" and its reason, its commitment, its score, and its chance, or "no route home" when heading home is not an option. Heading home's base and reason terms stay below the table. (play: noon.park, clicking a guest; the six chances add to 1.00, give or take rounding)
3. The table keeps its six rows in a park with many facilities. (play: tests/parks/stress/full.park, clicking a guest)
4. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after exits 0. src/legible/SPEC.md and src/app/SPEC.md describe the new inspectors and the table. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Tasks 1 and 9)

## Tests

None. The inspectors are checked by play, and the scenarios comparison shows the simulation unchanged (docs/testing.md).

## Medium

No fields or flows are added, and the simulation uses nothing this feature produces.

- Inspection records (plausible-operations, believable-guests, decision 0025): facilityRecord for benches and toilet blocks, and guestOptions for the guest's choice table.
- Park intent (effortless-building): bench and toilet block boxes and their poses, which picking already reads, and each box's kind, for the title.

Inspections are built for display and never enter the world.

## Spec changes

src/legible/SPEC.md, Inspectors:

- The first paragraph's first two sentences become: "An inspector explains one guest, shop, bench, or toilet block from what it publishes about itself (decision 0025). pickSubject replaces the subject when the player picks one of these."
- The guest's second bullet becomes: "a table of six rows, one for each of hunger, thirst, tiredness, and the toilet, then carrying on and heading home (principle 8). A need's row shows the facility of the guest's highest-scoring offer of that need, each term of that offer's score, and its score, and the probability that the guest picks any of its offers of that need. If the guest has no offer of that need, the row says so. Heading home's row shows the reason it would head home for, its commitment, its score, and its probability, or says that there is no route home. Carrying on's row shows its score and its probability. Below the table is a table of heading home's base and reason terms when heading home is an option."
- After the shop's sentence, add: "For a bench or a toilet block it shows its places, how many are taken, and how many guests wait at its door."

src/app/SPEC.md:

- Tools and the ghost: "clicking a guest or a shop opens the Inspector on it" becomes "clicking a guest, a shop, a bench, or a toilet block opens the Inspector on it".
- Tooling UI: "For a guest, it also shows the options the guest is choosing between, with their chances, and heading home's terms" becomes "For a guest, it also shows the guest's choices, one row for each need and one each for carrying on and heading home, with their chances, and heading home's terms".

The public interface, src/legible/inspect.h:

- `enum class SubjectKind : uint8_t { Guest, Shop, Bench, ToiletBlock };`, with its comment "What an inspector explains: a guest, a shop, a bench, or a toilet block."
- ChoiceRow's `Option` and `Need` become `Choice` and `Target`. Its comment becomes: "One row of a guest's choice table, as text: the choice, where it leads, the terms of its score, its score, and its probability. A need's row holds its best offer's facility and terms and the probability of any offer of that need. Carrying on has no target and no terms. Heading home's target is home and its reason, and its only term is its commitment."
- inspectorSubject's comment becomes: "The subject for the key: a guest when the key holds one, a shop, bench, or toilet block when it holds that box, and none otherwise."

## Files affected

- Modify: src/legible/SPEC.md
- Modify: src/app/SPEC.md
- Modify: src/legible/inspect.h
- Modify: src/legible/inspect.cpp
- Modify: src/app/ui/inspector_window.cpp
- Modify: src/app/ui/inspector_window.h
- Modify: src/app/scene/scene_uploads.h

## Dependencies

- plausible-operations' served-needs: facilityRecord. Met.
- believable-guests' a-day-out: guestOptions with four needs and heading home. Met.
- The Look tool's picking, which already meets bench and toilet block boxes (src/render/picking.cpp). Met.

## Out of scope

- Expanding a need's row to show every offer of that need. It is in the milestone's deepening candidates.

## Open questions

None.
