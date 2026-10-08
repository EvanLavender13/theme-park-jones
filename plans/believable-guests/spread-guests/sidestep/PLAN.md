# Implementation Plan: Sidestep

## Goal

Each walking guest publishes itself in guest presence and steps aside from the guests ahead of it that it would walk into, judged from last tick's entries, without its box leaving the paving.

## Approach

A new internal component, presence, defines the ground field guest-presence, whose entry is a guest's velocity, and the step aside: from the sampled entries, it predicts each nearest approach and gives the sideways offset for one tick. walk samples guest presence once a tick where the guest stands. walkTowardAim adds the offset to the step on the pass that stops short of the aim, and keeps it only where both lines, to the stepped point and from it to the aim, are clear. stepGuests sets each walking guest's entry after it walks, and drops it when the guest starts waiting or leaves. This feature's RESEARCH.md gives the reasons.

## Placement

Decision 0027 places each behavior this feature adds:

- The guest presence field, its entry, and working out the step aside from a sample: sim/guests, a new component src/sim/guests/internal/presence.h with src/sim/guests/presence.cpp. It is internal because nothing outside the guests module reads guest presence, as the milestone's Medium says. It sits apart from the route ahead and the ground because it reads neither.
- Registering guest presence, sampling it, folding the step aside into the walk, and setting and dropping each guest's entry: sim/guests, src/sim/guests/guests.cpp, which registers the guest state and walks and steps guests.
- SIDESTEP_RADIUS, SIDESTEP_HORIZON, SIDESTEP_DISTANCE, and SIDESTEP_SPEED: sim/guests, src/sim/guests/guests.h, beside the other guest tuning values.
- Remaking warm.park and cut.park: tpj_scenarios --slice-parks, run as it is.
- The closeness and presence checks: scratch programs in build/sidestep/, never committed. The lanes, stays, and door checks are reused from build/own-pace-and-side/ and build/paving-walk/.

## Tasks

### Task 1: Record the unchanged tree

Files:
- Create, scratch: `build/sidestep/build.sh`, `build/sidestep/close.cpp`

Step 1: Make a runtime report on the unchanged tree.

Run: `scripts/runtime-report.sh --launches 3 build/sidestep-before.report`
Expected: the script builds both Windows presets, runs, and prints its report against the budget, with full.park's stages listed for windows-release. Keep the report for Task 12.

Step 2: Copy build/own-pace-and-side/build.sh to build/sidestep/build.sh unchanged. It builds a scratch program against the windows-debug sim library.

Step 3: Write the closeness check, build/sidestep/close.cpp, in the style of build/own-pace-and-side/lanes.cpp. It reads only public headers. For each park path given on the command line, it loads the park, resolves it, and steps it 3600 ticks. After each step, it takes the position from guestRecord of each guest in parkGuests that isn't Waiting. It counts each pair of those guests whose positions lie less than 0.6 m apart, once per step, as a close pair-tick. It prints one line per park: the path, the guests seen, and the close pair-ticks.

Step 4: Build it and run it on every park.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools && build/sidestep/build.sh close && build/sidestep/close.exe tests/parks/*.park parks/*.park | tee build/sidestep/close-before.log`
Expected: one line per park. main-street.park and parks/busted-path.park report close pair-ticks above 0. Keep the log for Task 9.

### Task 2: Describe stepping aside in the guests spec and the brief

Files:
- Modify: `src/sim/guests/SPEC.md`
- Modify: `plans/believable-guests/CAPABILITY.md`

Step 1: Make the edits FEATURE.md's Spec changes gives, to src/sim/guests/SPEC.md's Boundary, Arriving and leaving, and Walking sections, and to the capability's principle 4 note.

Step 2: Read the new Walking paragraph against docs/conventions.md's rules for specs. It states behavior in the present tense, names each tuning value as guests.h does, and says nothing of how the walk is coded.

### Task 3: Add the sidestep tuning values

Files:
- Modify: `src/sim/guests/guests.h`

Step 1: After GUEST_CLEARANCE, add four values, each with a one-line comment:

- SIDESTEP_RADIUS = 3.0: meters around where a guest stands within which it senses other guests in guest presence.
- SIDESTEP_HORIZON = 3.0: seconds ahead within which a guest steps aside from a nearest approach.
- SIDESTEP_DISTANCE = 0.9: meters apart, centre to centre, that a guest steps aside to pass another at.
- SIDESTEP_SPEED = 0.6: the fastest a guest steps aside, in meters per second.

Step 2: In addGuests' comment, add guest presence after "Registers the guest state,".

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 4: Add guest presence and the step aside

Files:
- Create: `src/sim/guests/internal/presence.h`
- Create: `src/sim/guests/presence.cpp`
- Modify: `src/sim/CMakeLists.txt` (add `guests/presence.cpp` after `guests/guests.cpp`)

Step 1: In presence.h, declare:

- Stride, with doubles X and Z: how a guest moved over the last tick it walked, in meters per second. Give it a visitFields listing "x" and "z".
- GuestPresence, a ground field definition: `using Entry = Stride;` and Name `"guest-presence"`, as the example in src/sim/medium/ground_field.h shows.
- `GroundPoint sidestep(EntityKey self, GroundPoint at, GroundDirection heading, double speed, std::span<const GroundSample<Stride>> near);` with a comment: the guest's step aside over one tick, as an offset on the ground, from the guests sampled near it. heading is the unit direction it walks in. The offset is square to heading, at most SIDESTEP_SPEED times SIM_TICK_SECONDS long, and zero when nothing ahead would come within SIDESTEP_DISTANCE within SIDESTEP_HORIZON. Allocates nothing.

Step 2: In presence.cpp, define sidestep. The guest's velocity is heading times speed, and its right is (−heading.Z, heading.X). Start a signed sideways speed at 0, positive to the right. For each sample:

- Skip the guest's own key.
- Let r be the sample's position less at. Skip it unless r dotted with heading is above 0, so it is ahead.
- Let w be the sample's Stride less the guest's velocity. Skip it if w dotted with w is 0.
- Skip it unless r dotted with w is below 0, so they are closing.
- Let t be −(r·w)/(w·w). Skip it if t is above SIDESTEP_HORIZON.
- Let c be r + w·t, and its length std::sqrt(c·c). Skip it unless that length is below SIDESTEP_DISTANCE.
- The needed speed is (SIDESTEP_DISTANCE − length)/t. If c dotted with the right is above 0, the other would be on the right, so subtract the needed speed. Otherwise add it.

Clamp the sum to SIDESTEP_SPEED either way, and return the right times the sum times SIM_TICK_SECONDS. Use only + − × ÷ and std::sqrt (decision 0022).

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 5: Register guest presence and publish each guest's entry

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Include sim/guests/internal/presence.h and sim/medium/ground_field.h. In addGuests, call `addGroundField<GuestPresence>(schema);` after `schema.addComponent<Guest>`.

Step 2: In stepGuests, before calling walk, keep where the guest stands as stood. If walk returns false, push the key to leaving as now. Otherwise, if the guest is now Waiting, call dropGroundEntry<GuestPresence> for it. If not, call setGroundEntry<GuestPresence> with guest.Ground and a Stride of (guest.Ground − stood) divided by SIM_TICK_SECONDS. A guest still waiting skips walk, so it changes no entry.

Step 3: In the loop that destroys leaving guests, call dropGroundEntry<GuestPresence> for each before destroyEntity. No guest in leaving has changed its entry this tick, so the drop is never refused.

Step 4: Update stepGuests' comment to say each guest that walks sets its entry in guest presence, and one that starts waiting or leaves drops it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 6: Step aside in the walk

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Add `std::vector<GroundSample<Stride>> Near;` to WalkLists. At the start of walk, before its loop, call `sampleGroundField<GuestPresence>(world, guest.Ground, SIDESTEP_RADIUS, lists.Near);`.

Step 2: Give walkTowardAim and walkPass two more parameters, the guest's key and `std::span<const GroundSample<Stride>> near`, and pass `key` and `lists.Near` from walk through walkPass.

Step 3: In walkTowardAim's branch where left is less than gap, the guest stops short of its aim. Its heading is (gapX/gap, gapZ/gap), and its plain step ends at the point it moves to today, ahead. Call sidestep with the key, guest.Ground, the heading, guest.Speed, and near. If the offset isn't zero, let stepped be ahead plus the offset. If `ground.clearLine(group, guest.Ground, stepped)` and `ground.clearLine(group, stepped, toward)` both hold, the guest moves to stepped. Otherwise it moves to ahead. left becomes 0 either way. The branch where the guest reaches its aim doesn't change.

Step 4: Add to walkTowardAim's comment that on a pass that stops short of the aim, the guest steps aside from the guests near it where both lines stay clear, so the line to its aim stays clear.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 7: Make the slice parks again

Files:
- Modify: `tests/parks/fed.park`
- Modify: `tests/parks/warm.park`
- Modify: `tests/parks/cut.park`

Step 1: Remake the slice parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park; echo "status $?"`
Expected: `status 0`. fed.park changes only by gaining an empty guest-presence-ground section, since resolving it now gives the field its empty entries, and `git diff tests/parks/fed.park` shows nothing else.

Step 2: Run the park world checks and the integration tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests && build/windows-debug/tpj_sim_tests.exe 2>&1 | tail -3 && build/windows-debug/tpj_integration_tests.exe 2>&1 | tail -3`
Expected: both report `All tests passed`. Guests now reach the shop at different times, so a check that needs a guest waiting at the shop, a guest heading to it, or a shipment in transit may fail on warm.park. If one does, find a WARM_TICKS in src/scenarios/slice_parks.h at which all three hold, as early-turns did with build/early-turns/warm.cpp, set it, and remake the parks. Record it as a minor deviation.

full.park and the other stress parks are left as they are. They load, and their guests publish entries from their first step. main-street-guests makes them again.

### Task 8: Check that warm.park and cut.park hold guest presence

Files:
- Create, scratch: `build/sidestep/held.cpp`

Step 1: Write the presence check. It includes sim/guests/internal/presence.h. For each park path given, it loads and resolves the park without stepping, and calls sampleGroundField<GuestPresence> at (0, 0) with a radius of 1e6. It prints the path, the number of entries, and the number of guests in parkGuests that aren't Waiting.

Run: `build/sidestep/build.sh held && build/sidestep/held.exe tests/parks/warm.park tests/parks/cut.park`
Expected: each line reports entries above 0, and no more than the guests that aren't Waiting. Every guest that walked on the last tick set its entry, and every waiting guest dropped its own. A guest admitted on the last tick has no entry until it first walks, so it can make the entries fewer.

### Task 9: Check closeness, lanes, the paving, and doors

Files: none.

Step 1: Rebuild the scratch checks against the new library and run them.

Run: `build/sidestep/build.sh close && build/own-pace-and-side/build.sh lanes && build/paving-walk/build.sh stays && build/paving-walk/build.sh door && build/sidestep/close.exe tests/parks/*.park parks/*.park | tee build/sidestep/close-after.log && build/own-pace-and-side/lanes.exe tests/parks/*.park parks/*.park && build/paving-walk/stays.exe tests/parks/*.park parks/*.park && build/paving-walk/door.exe`
Expected:

- main-street.park and parks/busted-path.park each report fewer than half the close pair-ticks build/sidestep/close-before.log reports for them.
- Every lanes line reports 0 stalled. main-street.park reports more than two thirds of its clear wide steps right of the middle, and a mean offset between 1 and 3 m.
- Every stays line ends in `0 guests off the paving`.
- Every door line that isn't refused reports meals above 0 and `guests stuck heading to the shop 0`.

If any of these fails, that is a deviation: find the cause before going on.

### Task 10: Play the feature

Files: none.

Step 1: Build windows-release, which the user plays.

Run: `cmake.exe --build --preset windows-release --target tpj_app`
Expected: the build succeeds with no warnings.

Step 2: Ask the user to play main-street.park and parks/busted-path.park with build/windows-release/ThemeParkJones.exe and check criteria 1 and 3. Guests walking toward each other should step apart before they meet and pass without overlapping. A faster guest should step out around a slower one. Guests walking side by side at one pace shouldn't swerve. Near an entrance, guests shouldn't step aside from a guest that has just left. Guests should still keep to their sides and reach shop doors. Fix what the user finds in code, as play findings are fixed, and adjust the four SIDESTEP values if the user asks.

Play finding, no change: the user saw guests standing near shops, a little to one side, while the shop showed a queue of 0. A measure with sidestep on and off showed the same approach times, queue sizes, and slow guests either way. A served guest leaves the shop's queue at once but waits SERVICE_INTERVAL for its meal to come back, and it waits where its box came nearest the door. Two backlog items cover it: waiting guests lining up at the door (spread-guests), and the shop's record counting guests being served (plausible-operations).

### Task 11: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 12: Compare the runtime

Files: none.

Step 1: Make the runtime report after the change and compare it with Task 1's.

Run: `scripts/runtime-report.sh --launches 3 build/sidestep-after.report && scripts/runtime-report.sh --compare build/sidestep-before.report build/sidestep-after.report`
Expected: the comparison prints each stage's change. Report the guests stage's time on full.park, and the tick's total, against the 33 ms budget to the user. Nothing is gated on it.

### Task 13: Commit

Files: none.

Step 1: Stage only this feature's paths: src/sim/guests/, src/sim/CMakeLists.txt, src/scenarios/slice_parks.h if Task 7 changed it, tests/parks/fed.park, tests/parks/warm.park, tests/parks/cut.park, plans/believable-guests/CAPABILITY.md, and plans/believable-guests/spread-guests/sidestep/. Commit them by pathspec with the commit-hygiene skill, subject `Guests: Step aside from guests ahead`.
