# Implementation Plan: Held Tests Retired

## Goal

Delete the held tests and the sim root's synthetic tests of the same standards, with whatever test code they leave unused, and clear the held entries from the inventory.

## Approach

Every change is a deletion under tests/, plus the inventory's text. Five files hold nothing else and go whole; the other cases are cut from their files, along with any helper the compiler or a search shows nothing else uses. There is no test pass, since nothing is written, and the checks at the end confirm the suite still builds, passes, and is tidy.

## Placement

Decision 0027 places code. The feature adds no behavior and no code; it removes test cases from tests/ and entries from plans/focused-tests/RESEARCH.md.

## Tasks

The feature branch `held-tests-retired` exists, from implementing-features step 2. There is no spec task, no interface task, and no test pass.

Deleting a case means deleting the `TEST_CASE` with its body and the comment directly above it, if that comment speaks only of that case.

### Task 1: Delete the files that hold only held tests

Files:
- Delete: `tests/integration/park_standards_test.cpp`, `tests/integration/preview_test.cpp`, `tests/sim/park/sketch_park_test.cpp`, `tests/sim/routes/routes_park_test.cpp`, `tests/sim/routes/network_edits_test.cpp`
- Modify: `tests/integration/CMakeLists.txt`, `tests/sim/CMakeLists.txt`

Step 1: `git rm -q` the five files.

Step 2: In tests/integration/CMakeLists.txt, remove `park_standards_test.cpp` and `preview_test.cpp` from `add_executable(tpj_integration_tests ...)`, and replace the comment `# The standards run over every park file in tests/parks/, and the other tests open parks by name.` with `# The tests open the parks of tests/parks/ by name.` In tests/sim/CMakeLists.txt, remove `park/sketch_park_test.cpp`, `routes/network_edits_test.cpp`, and `routes/routes_park_test.cpp` from `add_executable(tpj_sim_tests ...)`.

Run: `git grep -n -E "park_standards_test|integration/preview_test|[( ]preview_test\.cpp|sketch_park_test|routes_park_test|network_edits_test" -- tests ":!tests/legible"`
Expected: one line, tests/integration/food_loop_test.cpp's top comment, which task 4 changes.

### Task 2: Delete the held and synthetic cases from the remaining files

Files:
- Modify: each file listed below

Step 1: Delete these cases, matched by their full names:

tests/app/session/park_file_test.cpp:
- A park saved with saveParkFile and opened with openParkFile is unchanged

tests/sim/draw_test.cpp:
- a copy of a world draws the same values as the world
- a candidate draws the same values as the world its commands would give

tests/sim/guests/add_guest_test.cpp:
- addGuest with the same arguments on two equal worlds leaves them equal

tests/sim/guests/guest_edits_test.cpp:
- Every world randomized park edits with guests walking, waiting, and eating reach equals its copy
- Every world randomized park edits with guests walking, waiting, and eating reach equals its save loaded and resolved
- A candidate made with an edit from a world of randomized park edits with guests walking, waiting, and eating, once it has stepped a cycle, equals the world that queues the edit for that cycle

tests/sim/medium/field_test.cpp:
- resolved entries never appear in a save, and loading the save of a resolved world and resolving gives the world saved
- a candidate made with makeCandidate samples every field as the world that commits the same commands and resolves

tests/sim/medium/flow_test.cpp:
- a save holds ledgers, and loading the save of a world with packets in transit and stocks held and resolving gives the world saved
- changing any packet, stock, created count, or consumed count changes the world's hash
- a copy of a world with flows stepped forward equals the original stepped forward

tests/sim/medium/kept_field_test.cpp:
- a saved world holding kept entries, loaded and resolved, reads exactly as the world saved, and reading it never changes its save

tests/sim/medium/network_test.cpp:
- a world holding networks copies equal and hashes equal
- changing a carrier point, stop, or anchor of a held network changes the world's hash
- a save holds no network
- a place in a registered state component saves and loads back equal

tests/sim/medium/stepped_field_test.cpp:
- a save holds stepped entries, and loading the save of a resolved world with both layers in flight and resolving gives the world saved
- changing any stepped entry changes the world's hash
- a copy stepped forward equals the original stepped forward
- a candidate made with makeCandidate samples every field, both layers, as the world that commits the same commands

tests/sim/operations/operations_edits_test.cpp:
- Every world a randomized park edit sequence with synthetic guests reaches equals its save loaded and resolved, and the two stay equal as they step on
- A candidate made with an edit from a world of a randomized park edit sequence with synthetic guests, once it has stepped a cycle, equals the world that queues the edit for that cycle

tests/sim/park/edits_test.cpp:
- Commands given to makeCandidate are each applied exactly when isAccepted is true on the candidate as it is when they apply

tests/sim/park/geometry_test.cpp:
- Ground lines and footprints computed twice from the same input are identical bit for bit

tests/sim/park/intent_test.cpp:
- A save of finite intent of every kind loads, saves to identical text, and loads equal
- A world of degenerate finite intent copies and hashes as a legitimate world

tests/sim/routes/route_distance_test.cpp:
- Every world a random edit sequence reaches equals its save loaded and resolved, route distance included
- A candidate made with an edit has the route distance of the world after a cycle applies it

tests/sim/save_test.cpp:
- a loaded and resolved world steps in lockstep with the world saved

tests/sim/walk_test.cpp:
- a copy equals its original, hashes the same, and keeps its keys

tests/sim/cycle_test.cpp:
- a candidate made just after a cycle equals the world that queuing its commands for that cycle gives, and leaves its source unchanged
- worlds built by the same calls and cycled with the same commands at the same ticks stay equal after every cycle

Do not delete stepped_field_test's "systems that publish into fields give the same world and the same hash after every tick whatever order they are registered in", or save_test's "loading the save of a resolved world and resolving gives the world saved, whose save is the same text".

Step 2: In each file changed in step 1, remove each helper function, type, constant, and using-declaration that no code left in the file uses, deciding each by searching the file for its name. Then, for each `#include` of a project header, search the file for the functions and types that header declares; remove the include when none of them is used. Change nothing else.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_app_tests tpj_integration_tests`
Expected: builds with no warnings. An unused-function or unused-variable error names a helper step 2 missed; remove it and build again.

### Task 3: Delete support functions left with no caller

Files:
- Modify: `tests/integration/support/park_files.h`, and any header under `tests/*/support/` that the step finds

Step 1: Remove `parkFiles` and its comment from tests/integration/support/park_files.h, with any `#include` only it used.

Step 2: For each support header included by a file tasks 1 and 2 changed, search each inline function it defines across tests/, excluding the header itself: `git grep -n -w <name> -- tests ':!<header path>'`. Remove each with no match, and any include only it used.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_app_tests tpj_integration_tests`
Expected: builds with no warnings.

### Task 4: Fix food_loop_test's comment

Files:
- Modify: `tests/integration/food_loop_test.cpp:3-4`

Step 1: Replace `These name parks and compare runs, so` / `// they are not standards that park_standards_test.cpp could check over every park file.` with `These name parks and compare runs, so` / `// they are not standards the sim's park world checks prove over every park file.`, rewrapped at 100 columns.

### Task 5: Clear the held entries from the inventory

Files:
- Modify: `plans/focused-tests/RESEARCH.md`

Step 1: Delete every line beginning `- held:`, except stepped_field_test's `- held: "systems that publish into fields give the same world and the…"` entry, which becomes:

```
- rewrite: "systems that publish into fields give the same world and the…" Property: Publication order does not change the stored field. Merged into field_test's "publishing the same sources in different orders gives the sa…", which covers both layers; its hash half re-proves a standard (S2).
```

Step 2: In the first section's opening paragraph, replace `and 40 more re-prove a world-as-value standard over real park state and wait for the shared world checks.` with `and 40 more re-proved a world-as-value standard over real park state. Those the inventory held were deleted once the shared world checks proved the standards over the park schema, except one that also proved a medium property and became a rewrite.`

Step 3: In "Which existing tests are in the wrong shape?", replace `A held entry re-proves a world-as-value standard over real park state, and is deleted once the shared world checks covers that state.` with `The entries the audit held, which re-proved a world-as-value standard over real park state, were deleted when the shared world checks landed.`

Step 4: Remove any file heading in the inventory, such as `sim/routes/network_edits_test.cpp`, left with no entry under it.

Run: `grep -c "^- held" plans/focused-tests/RESEARCH.md`
Expected: `0`

### Task 6: Confirm

Step 1: Confirm no listed case survives and src/ is untouched.

Run: `for n in "draws the same values as the" "saves back to identical text" "copies equal and hashes equal" "a save holds no network" "steps in lockstep with the world saved" "a copy equals its original, hashes the same" "a candidate made just after a cycle equals" "worlds built by the same calls and cycled" "randomized park edits with guests walking, waiting, and eating reach equals" "random edit sequence reaches equals its save" "random edit sequence reaches has the networks" "A candidate made with an edit has the"; do git grep -c -F "$n" -- tests; done; git diff --stat main -- src`
Expected: no output.

Step 2: Run the full checks, and compare the test count with main's 592.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings; `100% tests passed, 0 tests failed out of 551`; tidy reports no errors.

### Task 7: Commit

Step 1: Stage only this feature's paths: `git add -u tests plans/focused-tests && git add plans/focused-tests/shared-world-checks/held-tests-retired && git diff --cached --stat`, and confirm nothing else is staged.

Step 2: Commit via commit-hygiene, subject `Tests: Delete tests the park world checks replace`.
