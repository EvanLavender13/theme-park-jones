# Implementation Plan: Park World Checks

## Goal

Add the park file of degenerate intent, then have the test-writer write the shared world checks over the park files and three edits of warm.park.

## Approach

The code under test already exists, so the feature is a park file and a test pass. The park file is the save intent_test holds in its INTENT_SAVE string, checked in so every park-file user reads it. A failing check after the test pass is a defect in the simulation or in the checks, and goes through the deviation procedure, never a fix in this plan.

## Placement

Decision 0027 places code. The feature adds no code under src/:

- The shared world checks: tests/sim/, the sim root's tests, since the world-as-value standards are the sim root's (docs/testing.md rule 2) and rule 7 keeps them out of the integration tests.
- Degenerate intent: tests/parks/degenerate-intent.park, beside the other checked-in parks, named for what it holds.

## Tasks

The feature branch `park-world-checks` exists, from implementing-features step 2. There is no spec task and no interface task.

### Task 1: Check in the park file of degenerate intent

Files:
- Create: `tests/parks/degenerate-intent.park`

Step 1: Write exactly this text, each line ending with a line feed and no carriage return:

```
tpj-park 1
seed 99
tick 12
next-key 9

[entrance]
1 x=0 z=126.5 facing-x=0 facing-z=-1
8 x=-0 z=-129 facing-x=0 facing-z=0

[path]
2 kind=guest points=[{x=0.1 z=-3.25} {x=17 z=4} {x=17 z=4} {x=300 z=-128}]
3 kind=backstage points=[{x=5e-324 z=-0}]
4 kind=guest points=[]

[box]
5 kind=shop x=-20.5 z=60 facing-x=3 facing-z=4
6 kind=depot x=1e+300 z=-128 facing-x=0 facing-z=0
7 kind=shop x=-0 z=0 facing-x=-1.7976931348623157e+308 facing-z=1e-310
```

Step 2: Confirm it opens and steps as a park file.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --ticks 60 tests/parks/degenerate-intent.park > /dev/null; echo "exit $?"`
Expected: `exit 0`. A nonzero exit is a defect in how the simulation handles intent a save can hold, and is a Major deviation.

### Task 2: Test pass

Dispatch the test-writer as implementing-features step 4 says, with Feature `plans/focused-tests/shared-world-checks/park-world-checks/FEATURE.md`, Specs `src/sim/SPEC.md, src/sim/park/SPEC.md, src/sim/routes/SPEC.md, src/sim/operations/SPEC.md, src/sim/guests/SPEC.md, src/sim/medium/SPEC.md`, and Public headers `src/sim/world.h, src/sim/schema.h, src/sim/save.h, src/sim/park_schema.h, src/sim/park/edits.h, src/sim/park/intent.h, src/sim/guests/guests.h, src/sim/operations/operations.h`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#<the checks' file name without .cpp>]"`
Expected: the build succeeds and every case passes, since the behavior exists. A failing case goes through the deviation procedure.

### Task 3: Confirm and commit

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, all tests pass, tidy reports no errors.

Step 2: Stage only this feature's paths: `git add tests/parks/degenerate-intent.park tests/sim plans/focused-tests/shared-world-checks/park-world-checks && git diff --cached --stat`, and confirm nothing else is staged.

Step 3: Commit via commit-hygiene, subject `Tests: Check the world standards over the park files`.
