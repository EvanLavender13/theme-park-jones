# Implementation Plan: Watched Resolution

## Goal

The allocation check's drags and commits watch every resolution step except a shrinking exception list.

## Approach

Instead of pausing the watch around unlisted steps, the trace labels the watch's work with each step's name while the step runs. A handler the check gives the watch sees every block freed while watching. It excuses a block whose allocating or freeing work is an excused step, records which excused steps it saw, and keeps the rest as failures.

## Placement

Decision 0027 places each behavior this feature adds:

- Handing each freed block to the check: tests/checks/allocations/watch.h and watch.cpp, which own the watch.
- The exception list, labelling steps, and the report: tests/checks/allocations/allocation_check.cpp, which owns what the check runs and how it reports.

## Tasks

### Task 1: Write the docs

Files:
- Modify: `docs/decisions/0041-allocation-check.md`
- Modify: `docs/testing.md:9`
- Modify: `docs/conventions.md:43`
- Modify: `plans/scalable-runtime/CAPABILITY.md:103`

Step 1: Make the changes FEATURE.md gives for decision 0041, docs/testing.md, and docs/conventions.md, word for word. MILESTONE.md already has its changes. Amend 0041 in place.

Step 2: In CAPABILITY.md's watched-previews entry, replace "watches previews of a changing edit, the preview an overlay keeps at each tick," with "watches the preview an overlay keeps at each tick,". changing-previews' drags and commits already watch previews of a changing edit.

### Task 2: Hand freed blocks to a handler

Files:
- Modify: `tests/checks/allocations/watch.h`
- Modify: `tests/checks/allocations/watch.cpp`

Step 1: Add `void onFreed(void (*handler)(const FreedBlock &block))`: the function called with each noted block freed while watching, after it is counted, or none. Comment that it must not allocate.

Step 2: In forget, call the handler, when there is one, next to blockFreed.

### Task 3: Watch every step but the exceptions

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Replace WATCHED_STEPS with EXCUSED_STEPS: an array of {Step, RemovedBy} pairs of string views. Comment: the resolution steps that still allocate, each with the feature that removes it; the check fails when one no longer allocates, so the list only shrinks.

Step 2: WatchedSteps no longer pauses. While Watching, started(step) keeps the watch's current work and calls watchWork with the step's name, and ended restores the kept work. A step's name comes from the schema's resolver names or a string literal, both outliving the watch, so its data() can be the work. Add `const char *currentWork()` to watch.h for this, if the check has no other way to keep the outer work.

Step 3: Add the check's handler: a block whose AllocatedBy or FreedBy names an excused step marks that step seen and is excused. Any other block is kept, the first REPORTED_BLOCKS of them, and counted. Give it to onFreed for the drags and the commits. Clear the kept blocks at each startWatching, but keep the seen steps across both, so they cover the drags and the commits together.

Step 4: reportFreed, for the drags and for the commits, reports the kept blocks as it reports freed blocks now, and fails when there are any. In main, run checkDrags and checkCommits both, even when the drags fail. After both, fail for each excused step not seen, printing "allocation check: <step> no longer allocates; remove it from EXCUSED_STEPS". main returns 1 when any of these failed.

### Task 4: Find the exception list

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Start EXCUSED_STEPS with every resolution step the drags and commits run that WATCHED_STEPS didn't list, with "finishers", each RemovedBy "to be found". Build windows-debug and run the allocation check.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: it fails, naming every excused step that no longer allocates. Keep the output: it shows criterion 3.

Step 2: Remove each step it names. Give each remaining step the feature that removes it: route-updates for route-distance and the two route-distance fields' resolvers, and "a later feature" for the rest, with the whole-park part MILESTONE.md lists it under. If the table overflows, raise TRACKED_BLOCKS in watch.cpp and note why.

### Task 5: Confirm the feature

Files: none.

Step 1: Build, test, and tidy. Task 4's first run already showed a stale exception failing.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warning, every test passes, tidy prints "tidy: clean.".

Step 2: Dispatch the reviewer on the staged diff as soon as the build passes, in parallel with ctest.

### Task 6: Review and commit

Step 1: Stage only this feature's paths by pathspec, never with `git add -A`, and dispatch the reviewer on the staged diff through the reviewing skill. Present its findings verbatim.

Step 2: Commit with the commit-hygiene skill, subject `Tests: Watch every resolution step but a shrinking list`. Check `git diff --stat` and amend with any files the hook formatted.
