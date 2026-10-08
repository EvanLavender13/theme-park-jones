# Implementation Plan: Own Pace and Side

## Goal

Each guest walks at its own drawn speed and aims along its route moved to its own drawn side, and the route point stays in the middle wherever the guest's box wouldn't fit at its side.

## Approach

A guest gains two saved values drawn in addGuest: Speed, which replaces WALK_STEP as the distance it walks each tick, and Share, the mean of two uniform draws. Each point of the route ahead gains the width of its carrier and a Side position. traceRoute sets Side to the point's own position, and moveToSide then moves each one to the right of the stretch running into it, by the guest's side across the usable width. It keeps the move only where GuestGround::fits says the box fits. pointAt reads Side, so advanceAim, walkTowardAim, and reachedEnd aim along the moved route, and nearestSpot keeps reading each point's position on the network, as this feature's RESEARCH.md explains.

## Placement

Decision 0027 places each behavior this feature adds:

- Each route point's width and side position, moving a route to a side, and reading spots on the moved route: sim/guests, the route ahead in src/sim/guests/internal/route_ahead.h with src/sim/guests/route_ahead.cpp. That component already traces the route and gives its spots' positions.
- Drawing a guest's speed and share, walking at its speed, and moving its route to its side before it aims: sim/guests, src/sim/guests/guests.cpp, which already draws a guest's hunger rate and walks it.
- The saved speed and share: sim/guests, the Guest state in src/sim/guests/internal/guest.h.
- WALK_SPEED_SPREAD and SIDE_BIAS: sim/guests, src/sim/guests/guests.h, beside WALK_SPEED and the other guest tuning values.
- Remaking the parks: tpj_scenarios --slice-parks and tpj_bench --full-park, run as they are. Neither changes.
- The lanes check, and the stays and door checks: scratch programs in build/own-pace-and-side/ and build/paving-walk/, never committed.

## Tasks

### Task 1: Record the unchanged tree

Files:
- Create, scratch: `build/own-pace-and-side/lanes.cpp`, `build/own-pace-and-side/build.sh`

Step 1: Make a runtime report on the unchanged tree.

Run: `scripts/runtime-report.sh --launches 3 build/own-pace-and-side-before.report`
Expected: the script builds both Windows presets, runs, and prints its report against the budget, with full.park's stages listed for windows-release. Keep the report for Task 13.

Step 2: Copy build/paving-walk/build.sh to build/own-pace-and-side/build.sh unchanged. It builds a scratch program against the windows-debug sim library.

Step 3: Write the lanes check, build/own-pace-and-side/lanes.cpp, in the style of build/paving-walk/stays.cpp. It reads only public headers. For each park path given on the command line, it loads the park, resolves it, and steps it 3600 ticks. After each step, for each guest in parkGuests, it reads guestRecord and keeps the guest's position from the step before.

- A guest is stalled if it isn't Waiting and its position hasn't changed for 30 steps in a row. Count each stalled guest once.
- A guest is walking on a wide path if it isn't Waiting, it moved since the step before, and parkNetwork(world, PathKind::Guest).widthAt(record.At) is at least 9. For each such step, take the unit direction it moved in, d, and its right, (−d.Z, d.X). Its offset is (position − groundPoint(record.At)) dotted with that right. Count the steps, and sum their offsets. A step is clear of the middle if its offset is more than 0.1 m either way. Count the clear steps, and the clear steps right of the middle. A guest walking along the middle of a straight street has an offset of exactly 0, so only clear steps say which side it keeps.

For each park, print one line: `<path>: <guests> guests, <stalled> stalled, <steps> wide steps, <clear> clear of the middle, <percent>% of those right, mean offset <meters> m`. Print 0% when there are no clear steps, and 0 m when there are no wide steps.

Step 4: Build the lanes check and run it on the unchanged tree.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim tpj_tools && build/own-pace-and-side/build.sh lanes && build/own-pace-and-side/lanes.exe tests/parks/*.park parks/*.park | tee build/own-pace-and-side/lanes-before.log`
Expected: every line reports 0 stalled. main-street.park reports a mean offset near 0 m. Keep the log to compare with Task 11's. If a park reports a stalled guest before any change, report it to the user before going on, since the stall is older than this feature.

### Task 2: Carry the guests in the user's parks into the new save

Files:
- Modify, untracked: `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

The user's parks in parks/ are untracked, and three of them hold guests. After Task 5, a guest's line needs its speed and share, and a line without them fails to load. Each guest gets a speed and share from awk's generator, drawn the way addGuest draws them. The user approves this conversion by approving this plan.

Step 1: Back up the user's parks, then add a speed and share after each guest's hunger rate. Only guest lines contain ` hunger-rate=`.

Run: `mkdir -p build/own-pace-and-side/parks-before && cp parks/*.park build/own-pace-and-side/parks-before/ && for f in parks/busted-path.park parks/jank.park parks/radii.park; do awk 'BEGIN { srand(7) } / hunger-rate=/ { sub(/ hunger-rate=[^ ]+/, "& speed=" sprintf("%.6f", 1.0 + 0.6 * rand()) " share=" sprintf("%.6f", (rand() + rand()) / 2)) } { print }' "$f" > "$f.new" && mv "$f.new" "$f" || exit 1; done; echo "status $?"; grep -c ' speed=' parks/busted-path.park parks/jank.park parks/radii.park`
Expected: `status 0`, then `parks/busted-path.park:35`, `parks/jank.park:37`, and `parks/radii.park:63`. Task 11's checks load them under the new rules, which shows the conversion worked.

### Task 3: Describe own pace and side in the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md`

Step 1: Make the two replacements and the one added paragraph listed under "src/sim/guests/SPEC.md:" in FEATURE.md's Spec changes. Write the indented paragraphs without their indent.

### Task 4: Add the speed spread and the side bias

Files:
- Modify: `src/sim/guests/guests.h` (WALK_SPEED and the lines after it)

Step 1: Change WALK_SPEED's comment to `// The mean speed guests walk at, in meters per second.` Add after it:

    // A guest's speed is drawn uniformly from WALK_SPEED less this up to WALK_SPEED plus this.
    inline constexpr double WALK_SPEED_SPREAD = 0.3;
    // Where every guest's side starts, across the width its box can use seen the way it walks, from
    // 0 at the left to 1 at the right. Its share of the rest puts it between here and the right.
    inline constexpr double SIDE_BIAS = 0.5;

### Task 5: Save each guest's speed and share

Files:
- Modify: `src/sim/guests/internal/guest.h`

Step 1: Add `double Speed = WALK_SPEED;` and `double Share = 0.5;` after HungerRate in Guest. In the struct's comment, after "how hungry it is and how fast that rises", add "how fast it walks in meters per second, its share of the width on its side".

Step 2: In visitFields, add `visitor.field("speed", guest.Speed);` and `visitor.field("share", guest.Share);` after "hunger-rate".

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings. warm.park, cut.park, and full.park no longer load until Task 10 makes them again.

### Task 6: Draw each guest's speed and share

Files:
- Modify: `src/sim/guests/guests.cpp` (guestDraw, addGuest)

Step 1: Give guestDraw a draw index: `double guestDraw(const World &world, EntityKey guest, const char *purpose, uint64_t index = 0)`, passing index to drawKey in place of 0. Update its comment to name the index.

Step 2: In addGuest, after the hunger draws:

- `guest.Speed = WALK_SPEED - WALK_SPEED_SPREAD + guestDraw(world, key, "guest-speed") * 2.0 * WALK_SPEED_SPREAD;`
- `guest.Share = 0.5 * (guestDraw(world, key, "guest-share", 0) + guestDraw(world, key, "guest-share", 1));`

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 7: Walk each guest at its own speed

Files:
- Modify: `src/sim/guests/guests.cpp` (WALK_STEP, WalkProgress, walk)

Step 1: Remove WALK_STEP and its comment. In WalkProgress, change `double Left = WALK_STEP;` to `double Left = 0.0;`.

Step 2: In walk, start with `progress.Left = guest.Speed * SIM_TICK_SECONDS;` after declaring progress. In walk's comment, change "Walks the guest WALK_STEP across the guest paving" to "Walks the guest as far as its speed takes it in a tick across the guest paving".

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 8: Give each route point its width and a side position

Files:
- Modify: `src/sim/guests/internal/route_ahead.h` (RoutePoint, pointAt's comment)
- Modify: `src/sim/guests/route_ahead.cpp` (walkStep, traceRoute, pointAt)
- Modify: `src/sim/guests/guests.cpp` (reachedEnd)

Step 1: Add to RoutePoint, after Shaping, `double Width = 0.0;` and `GroundPoint Side;`. Put them last, because walkStep and traceRoute build RoutePoints by position, and a field added earlier would take the value meant for Node. In its comment, add that Width is the width of the stretch's carrier, and Side is where the guest aims for the point, on its own side of the route.

Step 2: In walkStep's add lambda, set both points it pushes with Width = carrier->Width and Side equal to their At. In traceRoute, set the first point's Side to its At, and leave its Width 0. The first point has no stretch running into it, so moveToSide never reads its Width.

Step 3: Make pointAt read Side in place of At, for both the point and the one before it. Change its comment in route_ahead.h to "The spot's ground position on the guest's side of the route."

Step 4: In reachedEnd, compare guest.Ground with `pointAt(route, {last, 1.0})` in place of `route.Points[last].At`, and say in its comment that it stands on the route's end on its side.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings. Every Side still equals At, so guests aim as they did before this feature.

### Task 9: Move each guest's route to its side

Files:
- Modify: `src/sim/guests/internal/route_ahead.h` (after traceRoute)
- Modify: `src/sim/guests/route_ahead.cpp`
- Modify: `src/sim/guests/guests.cpp` (walk)

Step 1: Declare in route_ahead.h after traceRoute:

    // Moves each point's Side to the guest's side, from 0 at the left to 1 at the right of the
    // width a guest's box can use there, seen the way the route runs into the point. The first
    // point is seen the way the route runs out of it, and a stretch with no length on the ground
    // the way the nearest one that has length runs. The usable width is that stretch's carrier's
    // width less twice GUEST_CLEARANCE. Side
    // stays at the point's own position where the guest's box wouldn't fit at the moved one, on the
    // group's paving with door ends counted, or where no stretch has length. Allocates nothing.
    void moveToSide(const GuestGround &ground, uint32_t group, double side, RouteAhead &route);

Step 2: Define it in route_ahead.cpp.

- A stretch i, for i from 1, runs from Points[i − 1].At to Points[i].At. Point i takes stretch i's direction, and point 0 takes stretch 1's. If that stretch has no length on the ground, search back from it for the nearest earlier stretch with length, and failing that forward for the nearest later one. Stretches without length are rare, at the start of a route and at empty steps, so the search is short. Leave a point's Side as At if no stretch has length.
- A point takes its width from the same stretch it takes its direction from: the Width of the point that stretch ends at. A guest standing on a junction's node has its place on the carrier it arrived by, but its route runs out along another, so point 0 must take that other stretch's width with its direction.
- Normalize a direction (x, z) with std::sqrt of x² + z². Its right is (−z, x), as sim/park's leftOf turns the other way.
- The offset is (side − 0.5) × max(0, width − 2 × GUEST_CLEARANCE), with the width the point takes. If it is 0, leave Side as At. Otherwise, the moved position is At plus right × offset. Set Side to it if ground.fits(group, moved, true), and leave Side as At otherwise.

Step 3: In walk, after traceGuestRoute, call `moveToSide(ground, *group, SIDE_BIAS + ((1.0 - SIDE_BIAS) * guest.Share), lists.Route);`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

Deviation, minor: Task 11's stays check found guests coming nearer the paving's edge than GUEST_CLEARANCE in most parks. The aim is saved as a place, and its position is worked out again each pass. With Side stored per point, a spot between two points blended their Sides, and the first point is the guest's place, which moves every tick. Where one end of a stretch fell back to the middle and the other didn't, the aim's position moved between passes, so the guest walked toward a point it had never checked a clear line to. A probe counted that in about one pass in ten on main-street.park. Each point now holds Shift, the offset for the stretch running into it, and moveToSide sets it without testing fit. pointAt takes the ground and group, adds the stretch's Shift to the spot's position on the network, and keeps it only where the box fits there. A spot's position then depends only on the spot and its stretch, so it is the same on every pass, and the probe found no such walk. The spec's wording is unchanged.

### Task 10: Make the parks that hold guests again

Files:
- Modify: `tests/parks/warm.park`
- Modify: `tests/parks/cut.park`
- Modify: `tests/parks/stress/full.park`

Step 1: Remake the slice parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park; echo "status $?"`
Expected: `status 0`, and fed.park unchanged in `git status --short tests/parks`.

Step 2: Remake the full park.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench && build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park; echo "status $?"`
Expected: `status 0`.

Step 3: Run the park world checks and the integration tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests && build/windows-debug/tpj_sim_tests.exe 2>&1 | tail -3 && build/windows-debug/tpj_integration_tests.exe 2>&1 | tail -3`
Expected: both report `All tests passed`. Guests now reach the shop at different times, so a check that needs a guest waiting at the shop, a guest heading to it, or a shipment in transit may fail on warm.park. If one does, find a WARM_TICKS in src/scenarios/slice_parks.h at which all three hold, as early-turns did with build/early-turns/warm.cpp, set it, and remake the parks. Record it as a minor deviation.

### Task 11: Check lanes, stalls, the paving, and doors

Files: none.

Step 1: Rebuild the scratch checks against the new library and run them.

Run: `build/own-pace-and-side/build.sh lanes && build/paving-walk/build.sh stays && build/paving-walk/build.sh door && build/own-pace-and-side/lanes.exe tests/parks/*.park parks/*.park | tee build/own-pace-and-side/lanes-after.log && build/paving-walk/stays.exe tests/parks/*.park parks/*.park && build/paving-walk/door.exe`
Expected:

- Every lanes line reports 0 stalled. main-street.park reports most of its wide steps clear of the middle, more than two thirds of those right, and a mean offset between 1 and 3 m.
- Every stays line ends in `0 guests off the paving`, and parks/busted-path.park, jank.park, and radii.park load.
- Every door line that isn't refused reports meals above 0 and `guests stuck heading to the shop 0`.

If any of these fails, that is a deviation: find the cause before going on.

Deviation, major, for the user: main-street.park reports 60.9% of clear wide steps right of the middle and a mean offset of 0.45 m, short of the two thirds and 1 to 3 m expected. Split by activity, guests heading to a shop average 0.83 m right, wanderers 0.36 m, and guests heading home 1.6 m left. The 10 m street narrows into the 3 m path to the entrance at its north end. A guest's aim reaches up to AIM_LOOKAHEAD ahead, and on the narrow path or at a door the side shift is small or none. So a guest heading home from a shop on the east side walks diagonally across the street toward the narrow path, and guests heading to a shop walk straight to its door. Only guests whose aim stays on the street keep to their side. The expectation came from the side alone and missed this. Stalls, the stays check, and the door check pass. Whether the street reads as two streams is left to the user's play.

Deviation, from play: the user played parks/busted-path.park and found no lanes on its wide plaza ("meh. not really"). Two changes, each measured with the lanes check split by activity:

- AIM_LOOKAHEAD is now 10 m, down from 30 m. A guest holds its side until a door or a narrow path's mouth lies within 10 m along its route, and then cuts across to it, instead of crossing from 30 m away.
- nearestSpot measures each stretch moved by its Shift, so a guest's place is the spot whose position on its side lies nearest where it stands. With the 10 m lookahead alone, 6 guests stalled in busted-path.park at a U-turn between two parallel carriers 0.7 m apart. A guest stood at its aim at its side of the route's end, but its place on the middle line stayed on the other carrier, so the route never grew. The guests spec's sentence on the place changes to match.

With both, main-street.park reports 79.7% of clear wide steps right of the middle with a mean offset of 1.21 m, and busted-path.park 77.0% and 1.11 m. With the place change alone at 30 m, busted-path.park reports 57.8% and 0.62 m. Every park reports 0 stalled and 0 guests off the paving, and the door check reports no guest stuck.

The user then found the streams close together, and SIDE_BIAS went from 0.4 to 0.5, so every guest's side lies right of the middle. main-street.park then reports 80.9% right and 1.27 m, and busted-path.park 88.0% and 1.61 m. Every park still reports 0 stalled and 0 guests off the paving, and the door check reports no guest stuck.

### Task 12: Play the feature

Files: none.

Step 1: Build windows-release, which the user plays.

Run: `cmake.exe --build --preset windows-release --target tpj_app`
Expected: the build succeeds with no warnings.

Step 2: Ask the user to play main-street.park and parks/busted-path.park with build/windows-release/ThemeParkJones.exe and check criteria 1 and 2. Guests should walk at different speeds and pass one another. On the 10 m street, guests walking each way should keep to opposite sides, with fewer near the edges. On the 3 m side paths, they should walk close together. Guests heading to a shop should still walk to its door. Fix what the user finds in code, as play findings are fixed.

### Task 13: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

Step 2: Make the runtime report after the change and compare it with Task 1's.

Run: `scripts/runtime-report.sh --launches 3 build/own-pace-and-side-after.report && scripts/runtime-report.sh --compare build/own-pace-and-side-before.report build/own-pace-and-side-after.report`
Expected: the comparison prints each stage's change. Report the guests stage's time on full.park, and the tick's total, against the 33 ms budget to the user. Nothing is gated on it.

### Task 14: Commit

Files: none.

Step 1: Stage only this feature's paths: src/sim/guests/, src/scenarios/slice_parks.h if Task 10 changed it, tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park, and plans/believable-guests/spread-guests/own-pace-and-side/. Commit them by pathspec with the commit-hygiene skill, subject `Guests: Walk at their own pace on their own side`.
