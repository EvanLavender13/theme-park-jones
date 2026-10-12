# Feature: Quiet Scene

## Summary

quiet-scene makes the scene's sync reuse its memory and adds it to the allocation check. The app calls SceneSync::syncWorld once a frame, after the frame's ticks. Today each call derives the park's whole paving to find out whether intent changed. At every tick it also makes new lists for the seated guests and a new guest mesh. After this feature, ParkSession counts the edits it applies, and the scene finds an intent change by comparing that count, so steady play does no work to find that nothing changed. The seated guests keep their working lists. SceneSync builds the guest mesh into a mesh it keeps, and the app uploads that mesh. The allocation check runs syncWorld after each tick, and once more as a frame without a tick, and names the work that freed a block. What the scene draws doesn't change. This covers criterion 4 of the milestone.

## Acceptance criteria

1. The allocation check runs the scene's sync as the app does. After each tick it calls SceneSync::syncWorld twice, once for the frame with the tick and once for a frame without one. A block freed by either the tick or the sync fails the check, and the report line names both pieces of work: `allocation check: <work> freed a <n>-byte block at tick <f> that <work> allocated at tick <a>`, where each work is `the tick` or `the scene's sync`. If nothing is freed, it prints `allocation check: 600 ticks of full.park and the scene's sync freed no block they allocated` and exits with status 0. ctest runs it as "allocation check: steady play on full.park frees no block it allocated". (check: in SceneSync::syncWorld, in src/app/scene/scene_sync.cpp, add `const std::vector<uint64_t> held{world.Tick};` and `static_cast<void>(held);`, build tpj_allocation_check, run `build/windows-debug/tpj_allocation_check.exe`, and see it exit with status 1, printing lines that start `allocation check: the scene's sync freed`; then revert)
2. Finding whether intent changed, bringing the seated guests up to date, and building the guest mesh free no block they allocated, over the check's watched ticks of full.park. (ctest)
3. The scene draws what it drew before. A capture of full.park, paused after 600 ticks, is byte-identical before and after the feature. Paused captures of one build are byte-identical from run to run, and one tick changes the capture, so the comparison shows a drawing change. (check: the capture commands in PLAN.md tasks 1 and 11)
4. An edit, an undo, and a redo still redraw the park and the guests at once, paused or running. (play: open tests/parks/stress/full.park in the windows-debug app, pause, place a guest path, and see it drawn at once with the guests standing where they were; undo it and see it go; redo it and see it return; unpause and see guests walk and sit on benches)
5. The simulation is unchanged. tpj_scenarios writes the same output on windows-debug before and after the feature, for its scenarios and every park in tests/parks, and for full.park with 300 ticks. (check: tpj_scenarios --compare, commands in PLAN.md task 10)
6. The windows-debug build has no warnings, ctest passes, and scripts/tidy.sh is clean. (check: the three commands in PLAN.md task 13)

## Tests

None. The allocation check covers the sync, and criterion 1's check proves it sees the sync.

## Medium

None changes. The feature adds, samples, and publishes no fields or flows. The scene reads the world through parkGuests, guestRecord, and the seat places guests publish, as it does today.

## Spec changes

src/sim/guests/SPEC.md: after the line "Other modules call guests.h (parkGuests, addGuest, guestRecord, guestOptions, departureTally, and arrivalCurve) and footfall.h (the HungryFootfall field).", add:

"parkGuests can also put the keys in a list the caller keeps, clearing it first, and then allocates nothing once the list has grown."

src/render/SPEC.md, Guests: after the SeatedGuests paragraph, add:

"buildGuestMesh can also fill a GuestMeshLists that the caller keeps. It clears the lists first, and allocates nothing once they have grown. Bringing SeatedGuests up to date allocates nothing once its own lists have grown."

src/app/SPEC.md, The scene: replace "The park's walkways and shop marks are rebuilt whenever the park's intent changes or a park is opened. The guests are rebuilt whenever the world ticks or the park's intent changes, as an edit applied while paused changes it." with "The park's walkways and shop marks are rebuilt whenever an edit, an undo, or a redo applies, or a park is opened. The guests are rebuilt whenever the world ticks or an edit, an undo, or a redo applies, as one applied while paused may move them." After the sentence "Each seated guest keeps its seat from one rebuild to the next, and a click picks guests where they are drawn.", add:

"Once its lists have grown, keeping the scene up to date frees no memory it allocated, whether or not the world ticked. That covers finding whether intent changed, the seated guests, and the guest mesh. The allocation check holds a stretch of full.park's ticks to it."

## Files affected

Modify:

- src/sim/guests/SPEC.md, src/render/SPEC.md, src/app/SPEC.md
- src/app/session/park_session.h, src/app/session/park_session.cpp
- src/sim/guests/guests.h, src/sim/guests/guests.cpp
- src/render/guest_mesh.h, src/render/guest_mesh.cpp
- src/app/scene/scene_sync.h, src/app/scene/scene_sync.cpp
- src/app/scene/scene_uploads.h, src/app/scene/scene_uploads.cpp, src/app/application.cpp
- tests/checks/allocations/watch.h, tests/checks/allocations/watch.cpp, tests/checks/allocations/allocation_check.cpp, tests/checks/allocations/CMakeLists.txt

## Dependencies

- quiet-tick: the allocation check and ParkSession's kept buffers. Merged.
- tests/parks/stress/full.park.

## Out of scope

- Uploading the guest mesh to the GPU, and the park mesh, which steady play never rebuilds.
- The preview, the overlay, the ghost, picking, the tools, and the panels. kept-preview, quiet-tools, and quiet-panels take them.
- Drawing guests from one shared mesh instead of rebuilding it at every tick. It stays in the capability's backlog as "Instanced guests".

## Open questions

None.
