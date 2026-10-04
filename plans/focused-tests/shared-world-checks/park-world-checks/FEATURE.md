# Feature: Park World Checks

## Summary

park-world-checks proves the world-as-value standards of docs/testing.md rule 2 over the park schema's real types, in the sim's own tests. The checks run over every park file in tests/parks/ and over the worlds three edits of warm.park reach, each edit chosen for a reason the feature states. They also fail when a type makeParkSchema registers is held by no world they checked, unless they name it with the reason no checked world can hold it. A new park file, degenerate-intent.park, carries the degenerate but finite intent that only a hand-written save can hold, so the checks cover it too.

## Acceptance criteria

The checked worlds are each park file directly in tests/parks/, opened as the app opens it (loaded with makeParkSchema and resolved), and the same world stepped some cycles, together with the worlds the edits below reach and their candidates. Over them, the standards of sim/SPEC.md hold:

- A copy of a checked world equals it and hashes equal.
- The save of a checked world loads back, and once resolved equals the world saved; saving the loaded world again gives identical text.
- For each edit, the candidate made with it from a world that has just finished a cycle equals the world that queuing the edit for that cycle gives.
- Two runs from the same park file, given the same edits at the same ticks, end in equal worlds.
- Every component type makeParkSchema registers is held by some checked world, except previous-network, which the checks name with its reason: sim/routes/SPEC.md says no resolution ends holding one, so no world between cycles, copy, save, or candidate does. A type held by no checked world and not named fails the checks, and the failure names the type.

The edits, each applied to warm.park, whose guests are walking, heading to its shop, and waiting there, with supplies and orders moving between its shop and depot:

- Deleting a guest path that guests stand on, some of them heading to the shop: guests are carried off a network that loses a carrier, and route distance and the networks are derived again.
- Adding a shop that touches both a guest path and the backstage path: a new shop is connected, supplied, and offered, in the candidate as in the committed world.
- Deleting the shop: the guests waiting at it and heading to it, and the supplies addressed to it, held and in transit, outlive the box.

cut.park already holds supplies in transit to a shop with no supply route, so no edit makes that state.

tests/parks/degenerate-intent.park holds intent of every kind, both path kinds and both box kinds, with finite values a save spells unusually and degenerate intent: a path of repeated points leaving the park, a path of one point, a path of none, a box and an entrance with a zero facing, and positions outside the park. As a park file in tests/parks/, it is a checked world like the others.

## Medium

None. The feature introduces, samples, and emits no fields or flows. Its checks read every capability's registered state only through the sim root's public interface: the schema's registered types, copy, equality, hash, save, load, candidates, and the park edit commands.

## Spec changes

None. The checks prove what sim/SPEC.md already says.

## Files affected

- Create: tests/parks/degenerate-intent.park
- Create: the checks' test file and any support under tests/sim/, registered in tests/sim/CMakeLists.txt

## Dependencies

- makeParkSchema, the park edit commands, and tests/parks/warm.park and cut.park: met.

## Out of scope

- Deleting the tests these checks replace, tests/integration/park_standards_test.cpp among them: held-tests-retired.
- Running the checks over tests/parks/stress/: those parks hold no type the others lack.
- Applying each edit to every park, or randomized edit sequences: rule 4.

## Open questions

None.
