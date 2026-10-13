# Feature: Quiet Commands

## Summary

quiet-commands makes the command queue keep its memory. Today CommandQueue holds each command in a std::any. Every command larger than a pointer goes on the heap when it is queued, with its own lists, such as a path's stops or an undo's records. Applying the queue empties it and frees all of it. That happens at every edit, undo, and redo, which breaks docs/conventions.md's Memory in loops rule. After this feature, the queue keeps a list of values for each command type it has held. Queueing copies the command into the next kept value of its type, which reuses that value's memory, and emptying the queue keeps every value. The allocation check watches its queue as each release queues its edit and as each undo or redo queues the history's command. Applying a command still resolves the world and stays with changing-previews. What the player sees doesn't change. This covers criterion 12 of the milestone.

## Acceptance criteria

1. Queueing a command copies it into memory the queue keeps for commands of its type. Emptying the queue, by a cycle, by commitCommands, or by clear, keeps that memory for later commands. Once the queue has grown to hold its commands, queueing and emptying free no block they allocated. (ctest: the allocation check; check: in CommandQueue::push in src/sim/command_queue.h, replace the kept value with a new `std::make_unique<T>(command)` even when one is free, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that say blocks `the presses` or `the history` allocated were freed; then revert)
2. In each watched round, the allocation check queues the released edit of each tool's press and release on its queue and empties the queue, watched as `the presses`. It also queues the command the second history takes for the undo or redo on the same queue and empties it, watched as `the history`. The watch no longer pauses around emptying the queue. It still pauses while the session takes and applies the undo or redo, and for a path tool's press after its first stop. If nothing is freed, the check prints the same line as before and exits with status 0. (check: the command in PLAN.md task 5)
3. Commands still apply in the order they were queued, a queue that holds a command of an unregistered type is still refused before anything changes, and a candidate still equals the world that commits the same commands. (ctest: tests/sim/cycle_test.cpp and the world checks in tests/sim)
4. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md tasks 1 and 6)
5. The app edits, undoes, and redoes as it did before. (play, in the windows-release app on tests/parks/stress/full.park: draw a guest path, place a shop, change the shop's stock in the Inspector, change the opening hours, delete a segment, and undo and redo each; then pause and do the same)
6. src/sim/SPEC.md says the queue keeps its commands' memory. src/app/SPEC.md, docs/testing.md, docs/conventions.md, and decision 0041 say the check watches queueing a command. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: read the five files; the three commands in PLAN.md task 6)

## Tests

None. The allocation check covers the queue, and criterion 1's check proves it sees the queue's memory.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows.

## Spec changes

src/sim/SPEC.md:

- In the Commands section, after "stepWorld empties the queue.", add "A queue keeps the memory of the commands it has held. Queueing a command copies it into memory kept for commands of its type, and emptying the queue keeps that memory for later commands. Once the queue has grown to hold its commands, queueing them and emptying the queue allocate nothing."
- At the end of the Memory in play section, add "Queueing commands and emptying the queue free no memory they allocated once the queue has grown. Applying a command resolves the world, which stays outside this. The allocation check holds the queue to it over each tool's release and each undo and redo on full.park."

src/app/SPEC.md:

- Replace "Taking an undo or redo, noting its change in the history, showing or hiding an overlay, and a click's pick free no memory they allocated either." with "Taking an undo or redo, queueing its command, noting its change in the history, showing or hiding an overlay, and a click's pick free no memory they allocated either."
- Replace "Each tool's press and release frees no memory it allocated either, once the tool's lists have grown." with "Each tool's press and release, with queueing its edit, frees no memory it allocated either, once the tool's lists and the queue have grown."

## Files affected

- Modify: `src/sim/command_queue.h`
- Modify: `src/sim/schema.h`
- Modify: `src/sim/SPEC.md`
- Modify: `src/app/SPEC.md`
- Modify: `docs/decisions/0041-allocation-check.md`
- Modify: `docs/testing.md`
- Modify: `docs/conventions.md`
- Modify: `tests/checks/allocations/allocation_check.cpp`

## Dependencies

- quiet-edits, feature 7: the allocation check's rounds of undos and redos, and its pause.

## Out of scope

- Applying a command, which resolves the world. changing-previews owns it.
- The queues that previews make for a candidate (legible/preview.cpp, render/park_mesh.cpp buildGhostMesh, tools.cpp isAccepted). They resolve a candidate, and changing-previews owns them.
- Watching the session's own queue. The session pushes and applies inside step and applyWithoutTick, which resolve, so the check runs the same push and clear on its own queue.

## Open questions

None.
