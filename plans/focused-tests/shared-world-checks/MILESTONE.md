# Milestone: Shared World Checks

Slice: none

Status: complete

## Summary

shared-world-checks proves the world-as-value standards of docs/testing.md rule 2 once, over real park state, and deletes the tests that proved them again for each feature's state. Today the sim root proves them only on synthetic schemas, so features re-proved them over their own state, and 39 such tests are held in the capability's inventory until something proves them over the park schema. This milestone adds that proof to the sim root's tests: copy, save, hash, candidate, and two runs, checked over every park file in tests/parks/ and the worlds a few edits reach from them, each edit chosen for a stated reason, with a check that fails when a registered type is held by no world it checked. Then the held tests go, and so do the sim root's synthetic tests of the same standards. It comes before reshaped-tests because a feature's tests can only stop re-proving a standard once the standard is proven for its state.

## Acceptance criteria

- tpj_sim_tests holds the shared world checks. Over every park file directly in tests/parks/, and over the worlds a few park edits reach from one named park each, it checks each world-as-value standard of rule 2 once: a copy equals its world and hashes equal; a save loads back, resolves equal, and saves again to identical text; a candidate equals the world that commits the same commands; and two runs give the same state. Each edit states why it was chosen, such as deleting the path a guest of warm.park stands on, and no edit is applied to every park.
- The checks fail, naming the type, when a component type makeParkSchema registers is held by no world they checked and is not named in the checks with the reason no checked world can hold it. Only previous-network is named: sim/routes/SPEC.md says no resolution ends holding one, so no world between cycles, copy, save, or candidate does. Every other registered type is held.
- tests/parks/ gains a park file holding degenerate but finite intent of every kind, the state only a hand-written save can hold (sim/park/SPEC.md), such as a path of one point, a zero facing, and a position outside the park, so the checks cover it once intent_test's held tests are deleted. The cross-build check compares it too, as it does every park file there.
- The tests plans/focused-tests/RESEARCH.md lists as held are deleted, tests/integration/park_standards_test.cpp with them, and the inventory's held entries are removed. stepped_field_test's publication-order test is the exception: it also proves a medium property no standard covers, so the inventory lists it as a rewrite instead.
- The sim root's tests of the same standards on synthetic schemas are deleted. Those only a synthetic schema can show stay, as the milestone's RESEARCH.md lists: the walk's refusals, a change to each kind of thing the walk covers changing equality and the hash, a copy's independence, equal worlds giving identical saves however built, a save holding no derived data, the load's refusals, and the save format's round trip of every kind of field at its bounds, bare entities, and state on a derived entity.
- Test support no remaining test uses is deleted.
- The tests the inventory lists as rewrite still re-prove a standard in part, such as edit_sequences_test's copy and save assertions. Those parts go with their rewrites in reshaped-tests, not here.
- Nothing under src/ changes, and tpj_scenarios's windows-debug output is identical before and after.

## Medium

This milestone introduces, samples, and emits no fields or flows. It changes tests only, and the shared world checks read every capability's registered state through the sim root's public interface: the schema's registered types, copy, equality, hash, save, load, and candidates.

## Dependencies

- closing-audit: complete.
- makeParkSchema, the park files in tests/parks/, and the park edit commands: met.

## Core feature

`park-world-checks`, because it is the proof the rest waits on. Once it lands, each world-as-value standard holds over every registered type for the first time, and a new type is caught if nothing covers it and the checks do not name it, even before any held test is deleted.

## Features

1. `park-world-checks`: the shared world checks over the park files and chosen edits, with the type coverage check, in tpj_sim_tests, and the park file of degenerate intent. Depends on: none.
2. `held-tests-retired`: deletes the held tests, the sim root's synthetic tests the checks replace, and the test support they leave unused, and removes the held entries from the inventory. Depends on: feature 1.

## Deepening candidates

None beyond the capability's.

## Open questions

None.

## Research notes

warm.park holds every saved registered type and resolution derives the rest, except previous-network, which no resolution ends holding and the checks name. The held tests' reasons map to a few edits, each on one park, and to one park file of degenerate intent, which no command can make. The randomized edit sequences go with the tests that used them. Depth is in RESEARCH.md.
