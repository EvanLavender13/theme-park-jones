# Feature: Held Tests Retired

## Summary

held-tests-retired deletes the tests that re-prove a world-as-value standard now that park-world-checks proves each one over the park schema's real types. That is 38 of the 39 tests plans/focused-tests/RESEARCH.md lists as held, and three of the sim root's tests of the same standards on synthetic schemas: walk_test's copy test, and cycle_test's candidate and two-runs tests. Any test code and support left with no caller goes with them. The remaining held entry, stepped_field_test's publication-order test, also proves a medium property no standard covers, so the inventory lists it as a rewrite merged into field_test's publication-order rewrite instead. The inventory then holds no held entries.

The feature deletes tests and writes none. Like a test utility (docs/testing.md rule 6), it gets no test pass, and its criteria are checks the implementer runs.

## Acceptance criteria

- None of the 41 test cases PLAN.md lists by file and full name exists anywhere under tests/. Five files that held only such cases are deleted and dropped from their CMakeLists.txt: tests/integration/park_standards_test.cpp, tests/integration/preview_test.cpp, tests/sim/park/sketch_park_test.cpp, tests/sim/routes/routes_park_test.cpp, and tests/sim/routes/network_edits_test.cpp.
- No function, type, constant, or using-declaration left in a changed test file, and no function in a support header those files include, is unused after the deletions. The remaining includes of each changed file are the ones it uses.
- These stay, since only a synthetic schema can show them: save_test's "loading the save of a resolved world and resolving gives the world saved, whose save is the same text", walk_test's other cases, and stepped_field_test's "systems that publish into fields give the same world and the same hash after every tick whatever order they are registered in".
- plans/focused-tests/RESEARCH.md has no held entry. Its stepped_field_test entry for the publication-order test is a rewrite merged into field_test's publication-order rewrite, whose property, that publication order does not change the stored field, covers both layers. Its opening counts and its description of entry kinds say the held tests were deleted when the shared world checks landed.
- The comment at the top of tests/integration/food_loop_test.cpp no longer names a deleted file.
- Nothing under src/ changes, so tpj_scenarios's output is unchanged. The windows-debug build has no warnings, ctest passes with 41 fewer tests than before, park-world-checks' five still among them, and scripts/tidy.sh is clean.

## Medium

None. The feature introduces, samples, and emits no fields or flows.

## Spec changes

None.

## Files affected

- Delete: tests/integration/park_standards_test.cpp, tests/integration/preview_test.cpp, tests/sim/park/sketch_park_test.cpp, tests/sim/routes/routes_park_test.cpp, tests/sim/routes/network_edits_test.cpp
- Modify: tests/integration/CMakeLists.txt, tests/sim/CMakeLists.txt
- Modify: tests/app/session/park_file_test.cpp, tests/sim/cycle_test.cpp, tests/sim/draw_test.cpp, tests/sim/save_test.cpp, tests/sim/walk_test.cpp, tests/sim/guests/add_guest_test.cpp, tests/sim/guests/guest_edits_test.cpp, tests/sim/medium/field_test.cpp, tests/sim/medium/flow_test.cpp, tests/sim/medium/kept_field_test.cpp, tests/sim/medium/network_test.cpp, tests/sim/medium/stepped_field_test.cpp, tests/sim/operations/operations_edits_test.cpp, tests/sim/park/edits_test.cpp, tests/sim/park/geometry_test.cpp, tests/sim/park/intent_test.cpp, tests/sim/routes/route_distance_test.cpp
- Modify: tests/integration/support/park_files.h, tests/integration/food_loop_test.cpp, and any support header under tests/ left with an unused function
- Modify: plans/focused-tests/RESEARCH.md

## Dependencies

- park-world-checks: merged (0c92b61).

## Out of scope

- The inventory's rewrite entries, including the parts of them that re-prove a standard, such as edit_sequences_test's copy and save assertions: reshaped-tests.
- The stepped_field_test publication-order test's rewrite: reshaped-tests.

## Open questions

None.
