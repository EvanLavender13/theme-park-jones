# Implementation Plan: Early Turns

## Goal

Guests make their choice for each node about 10 m before reaching it, wanderers pick their way out of the node then and route on past it, and guests walk through shaping points without choosing.

## Approach

A guest gains one saved step, Turn. It is the way out of the next node the guest has chosen for, and its carrier is NULL_KEY until the guest has chosen. The route ahead marks each node with its index and whether it is a shaping point. A wanderer's route runs through shaping points, follows Turn out of the first node it chooses at, and ends at the next. Each pass of walk finds the first node of the route ahead that isn't a shaping point. If that node lies within TURN_LOOKAHEAD and Turn doesn't start there, the guest chooses and sets Turn. Turn clears when the guest's place passes its node. Carrying keeps Turn only where the edit left its node and edge where they were, as this feature's RESEARCH.md explains.

## Placement

Decision 0027 places each behavior this feature adds:

- Shaping points, the way back at a node, a wanderer's route through shaping points and its picked turn, the node a guest chooses for next, and passing a route point: sim/guests, the route ahead in src/sim/guests/internal/route_ahead.h with src/sim/guests/route_ahead.cpp. That component already traces a guest's route along the network.
- Choosing for a node ahead, picking a wanderer's turn, clearing the turn once passed, and keeping or forgetting it across an edit: sim/guests, src/sim/guests/guests.cpp, which already chooses, walks, and carries guests.
- The saved turn: sim/guests, the Guest state in src/sim/guests/internal/guest.h.
- TURN_LOOKAHEAD: sim/guests, src/sim/guests/guests.h, beside AIM_LOOKAHEAD.
- Remaking the parks: tpj_scenarios --slice-parks and tpj_bench --full-park, run as they are. Neither changes.
- The stays and door checks: paving-walk's scratch programs in build/paving-walk/, run as they are and never committed.

## Tasks

### Task 1: Record the runtime before the change

Files: none.

Step 1: On the unchanged tree, make a runtime report.

Run: `scripts/runtime-report.sh --launches 3 build/early-turns-before.report`
Expected: the script builds both Windows presets, runs, and prints its report against the budget, with full.park's stages listed for windows-release. Keep the report for Task 13.

### Task 2: Carry the guests in the user's parks into the new save

Files:
- Modify, untracked: `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

The user's parks in parks/ are untracked, and three of them hold guests. After Task 5, a guest's line needs its turn, and a line without one fails to load. A turn whose carrier is 0 means the guest hasn't chosen for a node ahead yet, so it chooses again when it is within 10 m of one. The user approves this conversion by approving this plan.

Step 1: Back up the user's parks, then add an empty turn after each guest's aim. Only guest lines contain ` aim={`.

Run: `mkdir -p build/early-turns/parks-before && cp parks/*.park build/early-turns/parks-before/ && sed -i -E 's/( aim=\{[^}]*\})/\1 turn={carrier=0 from=0 to=0}/' parks/busted-path.park parks/jank.park parks/radii.park; echo "status $?"`
Expected: `status 0`. The three parks hold 35, 37, and 63 guests. Task 11's stays check loads them under the new rules, which shows the conversion worked.

### Task 3: Describe early turns in the guests spec

Files:
- Modify: `src/sim/guests/SPEC.md`

Step 1: Make the three replacements listed under "src/sim/guests/SPEC.md:" in FEATURE.md's Spec changes: one in Choosing, one in Walking's second paragraph, and one in Carrying across an edit. Write the indented paragraphs without their indent.

### Task 4: Add the turn lookahead

Files:
- Modify: `src/sim/guests/guests.h` (after AIM_LOOKAHEAD)

Step 1: Add, after AIM_LOOKAHEAD:

    // Meters before a node, along its route, at which a guest makes its choice for it.
    inline constexpr double TURN_LOOKAHEAD = 10.0;

### Task 5: Give guests a turn

Files:
- Modify: `src/sim/guests/internal/guest.h`

Step 1: Add `RouteStep Turn;` to Guest after `Place Aim;`, and include sim/routes/route_distance.h. In visitFields, add `visitor.field("turn", guest.Turn);` after the "aim" field. Add to the struct's comment, after "the place on its route it walks toward,": "the way it leaves the next node it has chosen for, an empty step at the node when it isn't wandering, and with carrier NULL_KEY until it has chosen,".

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings. warm.park, cut.park, and full.park no longer load until Task 10 makes them again.

### Task 6: Run the test pass

Dispatch the test-writer as the implementing-features skill says, for FEATURE.md's one test. It should fail because the guest heads home at the new point, not because of a build error.

### Task 7: Shaping points and the route through them

Files:
- Modify: `src/sim/guests/internal/route_ahead.h`
- Modify: `src/sim/guests/route_ahead.cpp`

Step 1: RoutePoint gains `uint32_t NodeIndex = 0;` and `bool Shaping = false;` after Node. Its comment says that for a node, NodeIndex is the node's index in the network, and Shaping is whether it is a shaping point. traceRoute sets both for every node point it adds, including the first point when the place is at a node. It takes the index from network.resolve of the point's place, {Carrier, To}.

Step 2: Declare and define these in route_ahead.h and route_ahead.cpp:

- `bool isShapingPoint(const Network &network, uint32_t node);` Whether exactly two edge ends meet the node, by edgeEnds(node).size() == 2, and nodeAnchor(node) is NULL_KEY.
- `bool isWayBack(const RouteStep &step, const Place &at, bool forward);` Whether the step leaves the place back along the place's carrier, against the way forward gives: its carrier is at's, its From is at's distance, and (To > From) differs from forward. Move it here from guests.cpp, where it takes a Guest. guests.cpp calls it with guest.At and guest.Forward.
- `std::optional<RouteStep> onwardStep(const Network &network, uint32_t node, const Place &at, bool forward);` The first step out of the node, in edgeEnds order, that isn't the way back for a guest arriving at the place walking that way. Each step runs along its edge away from the node, from that end's distance to the other end's. It gives none if every step is the way back. Allocates nothing.
- `bool startsAt(const Network &network, const RouteStep &step, uint32_t node);` Whether the step's carrier isn't NULL_KEY and network.resolve of {step.Carrier, step.From} gives a NodePosition with that node.
- `std::optional<size_t> turnNode(const RouteAhead &route, bool headedForEnd);` The index of the first route point that is a node and not a shaping point, or none. Give none if that point lies farther than TURN_LOOKAHEAD along the route from its first point. Also give none if headedForEnd is true, the route ends, and the point is its last. The distance to point i is the sum of |To - From| over points 1 to i.
- `bool passedPoint(const RouteAhead &route, RouteSpot spot, size_t index);` Whether the spot lies past the point at the index and not on it: spot.Point > index + 1, or spot.Point == index + 1 with Along above 0.

Remove passesNode, which nothing calls after Task 8.

Step 3: traceRoute takes `const RouteStep &turn` after sources. With sources, it traces as now. With no sources, the guest is wandering:

- If first's carrier is NULL_KEY, the route is the place alone, and it ends there.
- After each step, at its end node:
    - If the node is a shaping point, the route goes on along onwardStep(network, node, {step.Carrier, step.To}, step.To > step.From). If there is no onward step, the route ends at the node.
    - At the first node that isn't a shaping point, the route goes on along turn if two things hold: startsAt(network, turn, node), and turn isn't isWayBack along the step arriving there. A route that ran on back the way it came would let the aim reach behind the guest, so where the turn goes back, the route stops at the node and the guest walks to the end first.
    - Otherwise, at a node that isn't a shaping point, the route ends.
- The route still stops at AIM_LOOKAHEAD, where it doesn't end. Keep the guard against a step that walks nowhere for the steps through shaping points too.

Rewrite traceRoute's comment to say this.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build fails only in guests.cpp, where traceRoute, passesNode, and isWayBack are called the old way. Task 8 fixes those calls.

### Task 8: Choose ahead of each node

Files:
- Modify: `src/sim/guests/guests.cpp` (wanderStep, walkPass, the Pass enum, walk, and their comments)

Step 1: Replace wanderStep with two functions.

- `firstWanderStep(network, guest, position)` gives a wandering guest's first step and draws nothing. Inside an edge, it gives the step on to the stop ahead, as now. At a node, it gives guest.Turn if startsAt(network, guest.Turn, node). At a shaping point, it gives onwardStep(network, node, guest.At, guest.Forward). At any other node, it gives a step with carrier NULL_KEY, so the route is the place alone.
- `pickTurn(world, key, network, node, at, forward, picks, lists)` gives the step a wanderer picks out of the node. It works as wanderStep's node branch does today: the steps out of the node in edgeEnds order other than isWayBack(step, at, forward), a keyed pick among them on "guest-wander", and the way back if there is no other. It allocates nothing once the lists have grown.

Step 2: In walkPass, remove the passesNode branch, and remove PassedNode from the Pass enum and its comment.

Step 3: Rewrite walk's loop. Up to choosing, it works as now: resolve the place, take the heading, and visit or leave. When the guest must choose and hasn't chosen since its place last moved, it chooses, sets the aim to the place, clears Turn, and goes round again. It no longer chooses because it stands at a node. Then, while distance is left:

- Take the first step and sources as now, with firstWanderStep for a wanderer. Trace the route with guest.Turn.
- Find turnNode(route, the heading has a target or a home entry). If there is one, and guest.Turn doesn't startsAt its NodeIndex, the guest makes its choice for that node:
    - It chooses if it hasn't chosen since its place last moved, and then sets the aim to the place.
    - It then sets Turn for the node at index k. The arriving place is {Points[k].Carrier, Points[k].To}. The arriving way is guest.Forward when k is 0, and otherwise Points[k].To > Points[k].From. A wandering guest takes pickTurn at the node with that place and way. A guest that isn't wandering takes the empty step {Points[k].Carrier, Points[k].To, Points[k].To}.
    - Go round again. The new trace finds Turn starting at the node, so the guest doesn't choose for it twice.
- Walk the pass as now. Then, if there was a turn node, and passedPoint(route, the spot of the guest's place on the route, that index), clear Turn. Use spotOf, and spot 0 when it gives none.
- ReachedEnd and PlaceMoved still let the guest choose again. GroundMoved goes on, and Stayed returns true.

Rewrite walk's comment: the guest makes its choice for each node, other than a shaping point and the node it is heading to, when it comes within TURN_LOOKAHEAD of it or stands on it, and when it must. It chooses at most once until its place moves.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 9: Keep a turn across an edit only where it still holds

Files:
- Modify: `src/sim/guests/guests.cpp` (carryGuests and its comment)

Step 1: Add `carriedTurn(turn, before, after)`, which gives the turn carried across the edit, or none. Carry each end with carryOver. An end holds if three things are true: carryOver gives a place, that place is on the turn's carrier, and its after.groundPoint lies within ON_GROUND_TOLERANCE of the end's before.groundPoint. Compare squared distances, as decision 0022 asks. Carrying compares ground positions, not distances, because an edit that moves a carrier's start point changes every distance along it. carriedTurn gives none unless all of these hold:

- The turn's carrier isn't NULL_KEY.
- Its From end holds, and after.resolve of the carried From gives a NodePosition.
- If From differs from To: its To end holds, and after.resolve of the place halfway between the two carried distances gives an EdgePosition. That edge's FromDistance and ToDistance must be the lesser and the greater of the two carried distances, so no new node splits the edge.

It gives {Carrier, carried From, carried To}. For an empty step it gives {Carrier, carried From, carried From}.

Step 2: In carryGuests, after the guest's place is settled and before its aim is set, set Turn to carriedTurn(guest.Turn, *before, after), or clear it when that gives none. Add to the function's comment that a guest keeps its turn only where the edit left the turn's node, and the edge it picked, where they were.

Step 3: Build and run the listed test.

Run: `cmake.exe --build --preset windows-debug --target tpj_integration_tests && build/windows-debug/tpj_integration_tests.exe "*shaping*" 2>&1 | tail -3`
Expected: `All tests passed`. Use the name the test-writer gave the test case, if the filter matches nothing.

Deviation, from review: an edit can give part of a carrier a new key, so a turn whose node and edge stayed where they were was forgotten. An edit can also move a node's place onto another carrier that stops there. carriedTurn now compares ground positions only. For an empty step, the node's place is carried on whichever carrier passes there, and it must still stand on a node at the same ground position. For a wanderer's step, it carries the middle of the edge, which only that edge's carrier passes, and takes the edge enclosing it there. It keeps the turn when that edge's two ends stand where the turn's did, in either order, and rebuilds the turn on that edge.

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
Expected: both report `All tests passed`. If a check that needs a guest waiting at the shop or heading to it fails on warm.park, that is a Major deviation: stop and report it.

Deviation, minor: at WARM_TICKS 2020, the remade warm.park had a guest waiting at the shop and one heading to it, but no shipment in transit. Meals now happen at slightly different times, so the second shipment leaves at tick 2089. The world checks' deleting-the-shop setup needs all three. A scratch probe (build/early-turns/warm.cpp) showed they all hold from tick 2089 to 2589. src/scenarios/slice_parks.h's WARM_TICKS is now 2200. What the checks assert is unchanged.

### Task 11: Check that guests stay on the paving and reach doors

Files: none. This task runs paving-walk's scratch checks as they are.

Step 1: Rebuild the checks against the new library and run them.

Run: `build/paving-walk/build.sh stays && build/paving-walk/build.sh door && build/paving-walk/stays.exe tests/parks/*.park parks/*.park && build/paving-walk/door.exe`
Expected: every stays line ends in `0 guests off the paving`, and parks/busted-path.park, jank.park, and radii.park load. Every door line that isn't refused reports meals above 0 and `guests stuck heading to the shop 0`. If either fails, that is a deviation: find the cause before going on.

Deviation, minor: the stays check found guests walking into a shop's door and staying there. When a guest's visit came back, it could choose the same shop again. That set the empty step that marks a choice at the door. When the next visit came back, the guest was wandering but still held that empty step, so it took the step as its way out and never chose again. awaitVisit now clears the turn when the visit comes back, so the guest chooses again at the door, as the spec says. After the fix every park reports 0 guests off the paving, and the warm.park window still holds from 2100 to 2580.

### Task 12: Play the feature

Files: none.

Step 1: Build windows-release, which the user plays.

Run: `cmake.exe --build --preset windows-release --target tpj_app`
Expected: the build succeeds with no warnings.

Step 2: Ask the user to play main-street.park and parks/busted-path.park with build/windows-release/ThemeParkJones.exe and check criteria 1, 3, and 4. Wanderers should walk past shop connectors without bending toward the middle of the street, and bend only where they turn. They should turn into side paths before reaching the junction's middle, and walk to a path's free end before turning back. An edit far from them shouldn't change where they go. Fix what the user finds in code, as play findings are fixed.

### Task 13: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

Step 2: Make the runtime report after the change and compare it with Task 1's.

Run: `scripts/runtime-report.sh --launches 3 build/early-turns-after.report && scripts/runtime-report.sh --compare build/early-turns-before.report build/early-turns-after.report`
Expected: the comparison prints each stage's change. Report the guests stage's time on full.park, and the tick's total, against the 33 ms budget to the user. Nothing is gated on it.

### Task 14: Commit

Files: none.

Step 1: Stage only this feature's paths: src/sim/guests/, tests/integration/, tests/parks/warm.park, tests/parks/cut.park, tests/parks/stress/full.park, and plans/believable-guests/spread-guests/early-turns/. Commit them by pathspec with the commit-hygiene skill, subject `Guests: Choose each turn before reaching it`.
