# Implementation Plan: Quiet Commands

## Goal

The command queue keeps each command's memory across queueing and emptying, and the allocation check watches its queue at each release and each undo or redo.

## Approach

CommandQueue keeps, for each command type it has held, a list of values, each behind its own pointer. Queueing copy-assigns the command into the next free value of its type, or makes a new one when none is free. Emptying resets the counts and the queue's order. The queue lists each command by its type's id and a pointer to the kept value, and the schema's apply function takes that pointer instead of a std::any. The allocation check stops pausing around its queue and also queues the second history's taken command, so the same push and clear the session runs are watched.

## Placement

Decision 0027 places each behavior this feature adds:

- Keeping each command's memory: sim, src/sim/command_queue.h, CommandQueue. The queue owns its commands, and it stays a generic sim type that holds any registered command.
- Applying a command from a pointer to its value: sim, src/sim/schema.h, CommandType. The schema already owns the function that applies each registered type.
- Watching the queue at each release and each undo or redo: the allocation check, tests/checks/allocations/allocation_check.cpp.
- No app code changes. ParkSession and Interaction call push and the cycle as before. main.cpp gains nothing.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, before any edit, build tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build finishes with exit status 0 and prints no warning.

Step 2: Record the scenarios.

Run: `mkdir -p build/quiet-commands && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-commands/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-commands/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c quiet-commands`.

### Task 2: Update the specs and docs

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/app/SPEC.md`
- Modify: `docs/decisions/0041-allocation-check.md`
- Modify: `docs/testing.md`
- Modify: `docs/conventions.md`

Step 1: Make the SPEC.md edits FEATURE.md's Spec changes gives.

Step 2: In docs/decisions/0041-allocation-check.md, replace "the history's part, the tool park, the scene's sync, the overlay's shape, a click's pick, and each tool's press and release." with "the history's part, the tool park, the scene's sync, the overlay's shape, a click's pick, each tool's press and release, and queueing each command and emptying the queue."

Step 3: In docs/testing.md, replace "and the app's work after an undo or a redo, at a click's pick, and at each tool's press and release (decision 0041)." with "and the app's work after an undo or a redo, at a click's pick, at each tool's press and release, and when it queues a command (decision 0041)."

Step 4: In docs/conventions.md, replace "and for its work after an undo or a redo, at a click's pick, and at each tool's press and release." with "and for its work after an undo or a redo, at a click's pick, at each tool's press and release, and when it queues a command."

### Task 3: Watch the queue in the allocation check

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Delete dropQueued and its comment. In pressAndRelease and drawPath, call `rounds.Queued.clear();` where they called `dropQueued(rounds);`.

Step 2: In editRound, under `the history`, keep the pointer that takeUndo or takeRedo gives. After the null check, queue the command it points to on `rounds.Queued` with `push`, then call `rounds.Queued.clear();`. This runs the push and the clear that the session runs on its own queue.

Step 3: Update the comments. EditRounds's comment says the queue holds the releases' edits and the history's commands. editRound's comment says the watch pauses while the session takes and applies the command, and watches the second history take and note the same one and queue its command.

Step 4: Build and run the check. The queue still uses std::any, so the check fails.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines starting `allocation check: ` that say blocks `the presses` or `the history` allocated were freed.

### Task 4: Keep each command's memory in the queue

Files:
- Modify: `src/sim/command_queue.h`
- Modify: `src/sim/schema.h:9,91-96,268-277`

Step 1: In src/sim/command_queue.h, give QueuedCommand a `const void *Value = nullptr;` in place of `std::any Value;`. Its comment says Value points at the value the queue keeps, valid until the queue is emptied or destroyed.

Step 2: Give CommandQueue this public interface, keeping its class comment:

```cpp
  CommandQueue() = default;
  CommandQueue(const CommandQueue &) = delete;
  CommandQueue &operator=(const CommandQueue &) = delete;
  CommandQueue(CommandQueue &&) noexcept = default;
  CommandQueue &operator=(CommandQueue &&) noexcept = default;
  ~CommandQueue() = default;

  // Copies the command into the next value kept for commands of its type, which reuses that value's
  // memory, or into a new value when every kept one is in use.
  template <typename T> void push(const T &command);
  [[nodiscard]] const std::vector<QueuedCommand> &commands() const { return Commands; }
  [[nodiscard]] bool empty() const { return Commands.empty(); }
  // Empties the queue, keeping every value for later commands of its type.
  void clear();
```

Step 3: Give it these private members. Each Kept's Values points at a `std::vector<std::unique_ptr<T>>`, deleted by a function that casts back to it, as TickBuffers's Held does. Used counts the values in use.

```cpp
  struct Kept {
    TypeId Id = nullptr;
    std::unique_ptr<void, void (*)(void *)> Values{nullptr, nullptr};
    size_t Used = 0;
  };
  std::vector<Kept> Types;
  std::vector<QueuedCommand> Commands;
```

Step 4: Define push in the header. Keep the static_assert that a command is copy constructible, and add one that it is copy assignable. Find the Kept whose Id is `typeIdOf<T>()`, adding one with an empty list when there is none. If Used is below the list's size, copy-assign the command into the value at Used. Otherwise append `std::make_unique<T>(command)`. Push a QueuedCommand with the id and that value's address, and add 1 to Used.

Step 5: Define clear in the header: set every Kept's Used to 0, and clear Commands.

Step 6: Drop `#include <any>` from the header, and include `<memory>`, `<stddef.h>`, and `<vector>` as the code needs.

Step 7: In src/sim/schema.h, change CommandType's Apply to `void (*Apply)(World &world, const void *command) = nullptr;`, and its comment to say Apply calls the owning module's applyCommand on a pointer to a queued value. In addCommand, make the lambda call `applyCommand(world, *static_cast<const T *>(command));`. Drop `#include <any>`.

Step 8: Build and run the cycle tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe "a cycle*,a queue*,a candidate*"`
Expected: exit status 0, `All tests passed (11 assertions in 4 test cases)`.

### Task 5: Run the allocation check, and prove it sees the queue

Files: none kept.

Step 1: Run the check.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park, the scene's sync, the preview, the overlays, the tools, the panels, the uploads, and the drawing, and 20 undos and redos with the app's work after each, freed no block they allocated`. If a block is freed, fix the code that frees it, then run again.

Step 2: Make the probe in FEATURE.md criterion 1: in push, replace the kept value with a new `std::make_unique<T>(command)` even when one is free.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines starting `allocation check: ` that say blocks `the presses` or `the history` allocated were freed.

Step 3: Revert the probe, and run step 1's command again. Expected: as in step 1.

### Task 6: Confirm the criteria

Files: none.

Step 1: Compare the simulation.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-commands/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-commands/scenarios-before.txt build/quiet-commands/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-commands/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-commands/full-before.txt build/quiet-commands/full-after.txt`
Expected: exit status 0.

Step 2: Run the full checks.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play and edits on full.park free no block they allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

Step 3: Compare the runtime report. The preview stage queues one command on a new queue at each repetition (legible/preview.cpp previewEdit), so a fresh queue's cost can show there. main's code hasn't changed since build/quiet-edits-after.report was written, so it serves as the report before.

Run: `scripts/runtime-report.sh build/quiet-commands-after.report > build/quiet-commands-after.log 2>&1 && scripts/runtime-report.sh --compare build/quiet-edits-after.report build/quiet-commands-after.report`
Expected: exit status 0. The comparison marks no stage result-changed. Report each stage's change. If the preview stage is clearly slower, report it as a deviation; a fresh queue in a preview belongs to changing-previews.

### Task 7: Play

Files: none.

Step 1: Build the whole windows-release preset: `cmake.exe --build --preset windows-release`. Expected: exit status 0 and no warning.

Step 2: Ask the user to play criterion 5's play check in build/windows-release/ThemeParkJones.exe on tests/parks/stress/full.park. Fix what the play finds directly.

### Task 8: Review and commit

Step 1: Stage the feature's paths by name, never with `git add -A`. Dispatch the reviewer on the staged diff with FEATURE.md, src/sim/SPEC.md, src/app/SPEC.md, and docs/principles.md.

Step 2: Commit once with the commit-hygiene skill, subject `Sim: Keep each queued command's memory`.
