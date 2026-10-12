# Implementation Plan: Quiet Scene

## Goal

Make SceneSync::syncWorld free no block it allocated in steady play on full.park, and run it in the allocation check, with the scene and the simulation unchanged.

## Approach

ParkSession counts the edits, undos, and redos it applies, beside its generation. SceneSync compares that count instead of building a DrawnIntent, so steady play does no work to find that intent didn't change. It keeps the seated guests' working lists, and builds the guest mesh into lists it keeps, which uploadWorldMeshes then uploads. The check names the work running when a block is allocated and freed, and calls syncWorld after each tick.

## Placement

Decision 0027 places each behavior this feature adds:

- Counting applied edits: app, ParkSession in app/session/park_session.h. It is the one place commands, undos, and redos apply to the app's world, and it already gives the scene the generation.
- parkGuests into a list: guests, sim/guests/guests.h, which owns guest listings.
- The seated guests' kept lists and the guest mesh into kept lists: render, render/guest_mesh.h, which owns SeatedGuests and buildGuestMesh.
- Finding an intent change by the edit count, and keeping the guest mesh: app, SceneSync in app/scene/scene_sync.h (tpj_app_core). SceneSync already decides when the guest mesh is rebuilt and keeps the seated guests it is drawn with. Building the mesh there moves it out of the tpj_app executable, so the check runs it. uploadWorldMeshes in app/scene/scene_uploads.cpp only uploads it. src/app/application.cpp changes one argument, composition only.
- Naming the work in the check's report, and running the scene's sync: tests/checks/allocations. watch.h and watch.cpp note the work, and allocation_check.cpp runs it.

## Tasks

### Task 1: Record the simulation and a capture before the change

Files: none.

Step 1: On main, build tpj_scenarios and the app, and record.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `mkdir -p build/quiet-scene && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-scene/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-scene/full-before.txt`
Expected: exit status 0.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 600 --speed 0 --capture build/quiet-scene/capture-before.bmp`
Expected: prints `GPU driver: vulkan` and exits with status 0.

### Task 2: Update the specs

Files:
- Modify: `src/sim/guests/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`

Step 1: Make each edit FEATURE.md's Spec changes lists, with its sentences as written there.

### Task 3: Count the applied edits

Files:
- Modify: `src/app/session/park_session.h`, `src/app/session/park_session.cpp`

Step 1: Give ParkSession a member `uint64_t Edits = 0` and, after generation(), the accessor:

    // Counts the cycles and commits that applied queued commands, an undo, or a redo. It only
    // grows, and replacing the world keeps it.
    [[nodiscard]] uint64_t edits() const { return Edits; }

Step 2: Increment Edits at each of the four places step and applyWithoutTick call noteEdits.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 4: parkGuests into a list

Files:
- Modify: `src/sim/guests/guests.h`, `src/sim/guests/guests.cpp`

Step 1: After parkGuests, declare:

    // As parkGuests, into a list the caller keeps, which it clears first. Allocates nothing once
    // the list has grown.
    void parkGuests(const World &world, std::vector<EntityKey> &guests);

Step 2: Define it with `world.keysWith<Guest>(guests)`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 5: Keep the seated guests' working lists

Files:
- Modify: `src/render/guest_mesh.h`, `src/render/guest_mesh.cpp`

Step 1: Give SeatedGuests three private members, used only during update: `std::vector<EntityKey> Guests`, `std::vector<Taken> Next`, and `std::vector<std::pair<EntityKey, SeatPlace>> Newcomers`. Add to update's comment: "Allocates nothing once its lists have grown."

Step 2: In update, clear Next and Newcomers instead of declaring local lists, fill Guests with parkGuests(world, Guests), and end with `std::swap(Seats, Next)` instead of moving Next into Seats.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 6: Build the guest mesh into kept lists

Files:
- Modify: `src/render/guest_mesh.h`, `src/render/guest_mesh.cpp`

Step 1: After buildGuestMesh, declare:

    // The lists building the guest mesh reuses: the guests' keys and the mesh.
    struct GuestMeshLists {
      std::vector<EntityKey> Guests;
      ParkMesh Mesh;
    };
    // As buildGuestMesh, into lists the caller keeps, which it clears first. Allocates nothing
    // once they have grown.
    void buildGuestMesh(const World &world, const SeatedGuests &seated, GuestMeshLists &lists);

Step 2: Define it: clear the mesh's vertices and indices, fill lists.Guests with parkGuests, and append each guest's box as the returning form does. Make the returning form call it with lists of its own and return the mesh.

Run: `cmake.exe --build --preset windows-debug --target tpj_render tpj_bench`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 7: Keep the edit count and the guest mesh in SceneSync

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`, `src/app/application.cpp`

Step 1: Change syncWorld to take `uint64_t edits` after the generation: ParkSession::edits(). In SceneSync, replace DrawnIntent and `std::optional<DrawnIntent> Intent` with `std::optional<uint64_t> Edits`, commented as the edit count the park mesh was last built at.

Step 2: In syncWorld, on a new generation reset Edits instead of Intent. Replace the DrawnIntent comparison with `if (Edits != edits)`, which sets Edits and does what the intent branch does today. In syncWorld's comment, replace "when the world's intent differs from the last call's" and "When the intent differs" with the edit count differing from the last call's.

Step 3: Add the member `GuestMeshLists GuestLists` and the accessor:

    // The guests as the last syncWorld that needed them drew them.
    [[nodiscard]] const ParkMesh &guestMesh() const { return GuestLists.Mesh; }

In syncWorld, after `Seated.update(world)`, call `buildGuestMesh(world, Seated, GuestLists)`. Add to syncWorld's comment: "Whenever the guest mesh is needed, it is built from them. Once its lists have grown, a call frees no memory it allocated." On a new generation, leave GuestLists as it is; the call rebuilds it.

Step 4: Change uploadWorldMeshes to take `const ParkMesh &guests` in place of `const SeatedGuests &seated`, and upload it with setGuestMesh when meshes.Guests is set. Update its comment: it builds the park mesh and gives it and the guest mesh to the renderer. In src/app/application.cpp, pass `Session.edits()` to syncWorld, and `Scene.guestMesh()` in place of `Scene.seated()` to uploadWorldMeshes.

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 8: Run the scene's sync in the check

Files:
- Modify: `tests/checks/allocations/watch.h`, `tests/checks/allocations/watch.cpp`, `tests/checks/allocations/allocation_check.cpp`, `tests/checks/allocations/CMakeLists.txt`

Step 1: In watch.h, give FreedBlock `const char *AllocatedBy = ""` and `const char *FreedBy = ""`, and declare:

    // The work the next allocations and frees are counted against. The name must outlive the
    // watch, as a string literal does.
    void watchWork(const char *work);

Step 2: In watch.cpp, keep the current work beside the current tick. Note it with each block, and fill both names in a FreedBlock.

Step 3: In allocation_check.cpp, make a `tpj::SceneSync scene`. After each warm-up step, call `scene.syncWorld(session.world(), session.generation(), session.edits())`. In each watched tick, call `watchWork("the tick")` before the step, then `watchWork("the scene's sync")`, then syncWorld twice. Print each freed block as `allocation check: <FreedBy> freed a <n>-byte block at tick <f> that <AllocatedBy> allocated at tick <a>`, and on success print `allocation check: 600 ticks of full.park and the scene's sync freed no block they allocated`, with 600 taken from WATCHED_TICKS. Update the file's comments to say the scene's sync runs as the app runs it, once for a frame with a tick and once for a frame without one.

Step 4: In CMakeLists.txt, rename the test in add_test and set_tests_properties to "allocation check: steady play on full.park frees no block it allocated".

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park and the scene's sync freed no block they allocated`.

### Task 9: Show the check sees the scene's sync

Files: none kept.

Step 1: Plant the temporary list criterion 1 gives in SceneSync::syncWorld, rebuild, and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with report lines starting `allocation check: the scene's sync freed`.

Step 2: Revert the plant, rebuild, and run again.

Expected: exit status 0.

### Task 10: Show the simulation unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-scene/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-scene/scenarios-before.txt build/quiet-scene/scenarios-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-scene/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-scene/full-before.txt build/quiet-scene/full-after.txt`
Expected: exit status 0.

### Task 11: Show the scene unchanged

Files: none.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/stress/full.park --ticks 600 --speed 0 --capture build/quiet-scene/capture-after.bmp && cmp build/quiet-scene/capture-before.bmp build/quiet-scene/capture-after.bmp`
Expected: exit status 0, with no output from cmp. Six paused captures of full.park from one build were byte-identical, and a capture one tick later differed across the park, so cmp tells a drawing change from noise. If cmp reports a difference, treat it as a deviation.

### Task 12: Play an edit

Files: none.

Step 1: Do criterion 4's play check in the windows-debug app.

### Task 13: Confirm the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: steady play on full.park frees no block it allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 14: Commit

Step 1: Stage the feature's paths by name and commit once with the commit-hygiene skill, subject `App: Keep the scene's sync memory and check it`.
