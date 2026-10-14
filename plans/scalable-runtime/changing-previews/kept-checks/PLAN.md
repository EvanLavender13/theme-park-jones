# Implementation Plan: Kept Checks

## Goal

A changed frame plans its edit once into lists the caller keeps. The acceptance check, the candidate, the shop context, the ghost, and a cycle's commands all work in kept lists, so none of them allocates once its lists have grown.

## Approach

The edit's plan and its results move into a PlannedEdit the caller keeps. The working lists move into a struct each file keeps in the caller's TickBuffers, and the surface and curve builders refill storage they are given. The preview plans once and makes its candidate with a makeCandidate form that applies a change instead of a queued command. The ghost reads that plan, and the cycle passes its buffers to applyCommand. Resolution tells a trace each step, so the allocation check can watch the drags and commits while pausing inside the resolvers no feature has moved yet.

## Placement

Decision 0027 places each behavior this feature adds:

- Applying a command with the cycle's buffers, makeCandidate's change form, and the resolve trace: sim, src/sim/schema.h, src/sim/world.h, and src/sim/cycle.cpp. The schema owns how commands are registered and applied, and the cycle owns resolution and candidates.
- PlannedEdit, planEdit, applyPlanned, editedParts, and the buffered applyCommand forms: park, src/sim/park/edits.h and src/sim/park/edits.cpp. The park owns edits and their acceptance.
- The plan's storage, EditPlan and its parts, and PlanLists: park, a new private header src/sim/park/internal/plan.h. PlannedEdit names PlanLists in a public header, so the plan's types can't stay in edits.cpp's anonymous namespace: GCC refuses a field whose type is there (-Wsubobject-linkage, an error here). The internal directory keeps them private to the park.
- The surface builders' kept lists: park, src/sim/park/surface.cpp, declared in src/sim/park/internal/layout.h. keptCurves' kept lists: park, src/sim/park/curves.cpp, declared in src/sim/park/internal/curves.h. They own those builders.
- Planning once in the preview and the shop context's kept form: legible, src/legible/preview.h and preview.cpp. The preview owns the candidate and the shop context.
- The arrival day's kept preview: legible, src/legible/arrivals.h and arrivals.cpp, which own the arrival day.
- The ghost from a plan: render, src/render/park_mesh.h and park_mesh.cpp, which own the ghost.
- The path tool's press in kept lists: tools, src/tools/tools.cpp and src/tools/internal/tool_lists.h.
- Passing the preview's plan to the ghost: src/app/scene/scene_sync.cpp and src/bench/stages.cpp, composition only.
- The drags' and commits' watch: tests/checks/allocations/allocation_check.cpp. The check owns what it watches.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: On main, build tpj_scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: exit status 0 and no warning.

Step 2: Record the scenarios.

Run: `mkdir -p build/kept-checks && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/kept-checks/scenarios-before.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/kept-checks/full-before.txt`
Expected: exit status 0.

Step 3: Create the branch: `git switch -c kept-checks`.

The runtime baseline is build/flat-ground.report. Main hasn't changed since it was written, so no before report is made.

### Task 2: Write the specs and the rule's text

Files:
- Modify: `src/sim/SPEC.md`, `src/sim/park/SPEC.md`, `src/legible/SPEC.md`, `src/render/SPEC.md`, `src/tools/SPEC.md`
- Modify: `docs/conventions.md`, `docs/testing.md`
- Modify: `docs/decisions/0041-allocation-check.md`

Step 1: Make each change FEATURE.md's Spec changes gives, in the place it names. Decision 0041 keeps its Status line.

### Task 3: Add the interfaces

Files:
- Modify: `src/sim/world.h`, `src/sim/schema.h`, `src/sim/cycle.cpp`
- Create: `src/sim/park/internal/plan.h`
- Modify: `src/sim/park/edits.h`, `src/sim/park/edits.cpp`

Step 1: In world.h, declare ResolveTrace, ResolveTraceSlot, and makeCandidate's change form before resolveWorld, as FEATURE.md gives them. Add the form to World's friends next to the other makeCandidate forms. In cycle.cpp, define it like makeCandidate(world, commands, buffers, into): copy into, call change(into, context), set ResolvePending, and resolve.

Step 2: In schema.h, give CommandType.Apply a TickBuffers & parameter. In addCommand, call `applyCommand(world, command, buffers)` when `requires(World &w, const T &c, TickBuffers &b) { applyCommand(w, c, b); }` holds, and `applyCommand(world, command)` otherwise. HasApplyCommand accepts either form. In cycle.cpp, applyCommands takes the buffers and passes them. stepWorld, both refilling and returning makeCandidate forms, and commitCommands pass theirs.

Step 3: Create internal/plan.h. Move these out of edits.cpp's anonymous namespace into it, in namespace tpj, unchanged: Solid, Line, PlanPoint, PlacedPoint, Split, BendSplit, PlanLine, and EditPlan. Add PlanLists:
- Plan, the EditPlan.
- HasPlan, whether planning gave a plan.
- Accepted, whether the world accepts the edit.
- Crossings, the crossings' positions.
- SpareSplits, SpareLines, and SpareBends, the spare elements resizeKeeping keeps for the plan's lists.
- World, the GroundLists worldGround fills from the world, whose Layout and Ground are the before side.
- After, the layout the plan makes, and AfterGround, the GroundLists its ground is derived into. editedParts reads these, so the ghost derives no ground planEdit already derived.

plan.h includes sim/park/internal/layout.h for PathLayout and GroundLists.

Step 4: In edits.h, add PlannedEdit, planEdit, isAccepted and pathCrossings on a plan, applyPlanned, EditedParts, editedParts, and the sixteen buffered applyCommand forms, as FEATURE.md gives them. Include `<memory>` and `<span>`, and sim/tick_buffers.h.

Step 5: In edits.cpp, include internal/plan.h. Define PlannedEdit's members out of line: the constructor makes Lists, and the rest are defaulted. Add stubs: planEdit does nothing, isAccepted on a plan returns Lists->Accepted, pathCrossings returns Lists->Crossings, applyPlanned does nothing, editedParts clears the parts, and each buffered applyCommand calls the form without buffers.

Run: `cmake.exe --build --preset windows-debug`
Expected: exit status 0 and no warning.

### Task 4: Run the test pass

Dispatch the test-writer as the implementing-features skill says, with FEATURE.md, src/sim/SPEC.md and src/sim/park/SPEC.md, and the headers src/sim/world.h and src/sim/park/edits.h. Confirm the changed candidate check builds and fails on the drags, because the stubbed planEdit and applyPlanned leave the kept candidate unchanged.

### Task 5: Tell the trace each step

Files:
- Modify: `src/sim/cycle.cpp`

Step 1: In resolveWorld(world, buffers), read buffers.get<ResolveTraceSlot>().Trace once. When it is set, call started(resolver.Name) before each resolver and ended after it, and started("finishers") before the finishers and ended after them. Tell ended on the way out of an exception too.

Step 2: In both makeCandidate forms that refill into, call started("copy-world") and ended("copy-world") around copyWorld(world, into).

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 6: Refill the surface builders' storage

Files:
- Modify: `src/sim/park/surface.cpp`
- Modify: `src/sim/park/internal/layout.h`

Step 1: In surface.cpp, add PavingLists in the anonymous namespace. It holds the wanted, built, and asked lists, the shapes, the ends, the Scratch, the Joins, the hider bounds, the near and hidden lists, a Visible surface, a Hiders list, a Near surface, the PointReaches, the bounds, and the near points. surfaceOf, surfaceAround, and pointsAffected take it from the TickBuffers their callers pass, with get<PavingLists>().

Step 2: Make the builders refill:
- buildSurface takes the surface to fill and the PavingLists. It clears the surface's five lists and fills them. focusOf and farEndsOf fill the kept lists, and shapesOf and endsOf use their kept forms. Scratch.Turns is sized with resizeKeeping.
- Joins gets reset(ground), which clears and refills its three lists, in place of its constructor.
- hideJoined takes a span of hiders and the PavingLists. It swaps the surface's pieces and sides into the Visible surface and writes the hidden edge lines there. Then it swaps every list back, so neither surface frees a list.
- PointReaches gets reset(layout, ground) in place of its constructor. pointsNear fills a list it is given.

Step 3: Change the declarations in internal/layout.h:
- `void surfaceOf(const PathLayout &layout, const PathGround &ground, PavedSurface &surface, TickBuffers &scratch)`
- `void surfaceAround(const PathLayout &layout, const PathGround &ground, std::span<const EntityKey> focus, bool hide, PavedSurface &surface, TickBuffers &scratch)`
- `void pointsAffected(const PathLayout &layout, const PathGround &ground, std::span<const EntityKey> points, std::vector<EntityKey> &affected, TickBuffers &scratch)`

Each comment adds "refilling the surface (or list) it is given, and keeping the lists it works with in the scratch. Allocates nothing once they have grown." The returning parkSurface forms build into a new surface with these. Those forms are for callers outside frames, and for the two surface resolvers, which unit-surface changes. walkedSurfaceOf uses the PavingLists too.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the park module builds. edits.cpp fails to build until Task 9.

### Task 7: Refill keptCurves' storage

Files:
- Modify: `src/sim/park/curves.cpp`
- Modify: `src/sim/park/internal/curves.h`

Step 1: Change keptCurves to `void keptCurves(const PathLayout &before, const PathGround &groundBefore, const PathLayout &after, const std::vector<SegmentEnd> &added, CurveWrites &writes, TickBuffers &scratch)`. It clears writes and refills them. It takes the before ground from its caller, which reads it from the world's units, so only the after ground is derived.

Step 2: Keep its lists in a CurveLists in curves.cpp's anonymous namespace, taken from the scratch:
- the after ground's GroundLists;
- the two Ends;
- the Now, Previous, and Two lists of PointEnd;
- the list of points.

Ends gets reset(layout) in place of its constructor. at(point, found) and points(keys) fill lists they are given. keepInsides, landAdded, and closeGaps read the lists they are given.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: curves.cpp builds. edits.cpp fails to build until Task 9.

### Task 8: Plan in kept lists

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Add EditLists to edits.cpp's anonymous namespace, taken from the planning caller's TickBuffers. It holds the working lists:
- the segments and corners;
- the meeting corners;
- the stop positions;
- the drawn lines, kept with resizeKeeping;
- the plan points' keys;
- the added ends;
- the chain applyBendSplit builds;
- the layout withCurves applies the plan to before its curves are known;
- the solids;
- the point lists of editedBetween;
- the before, after, and changed surfaces;
- the edited segments' indexes;
- RestoreIntent's names, added keys, extra keys, and restored solids.

Step 2: Planning fills the plan in place:
- planPath, planMove, planInsert, planRemove, planDeleteSegment, planRadius, planTaper, planEdge, and planWidth each take the PlanLists, the EditLists, and the TickBuffers. They return whether they gave a plan.
- They start by clearing the plan: resizeKeeping takes Splits, Lines, and BendSplits to 0, keeping their elements' lists in the spares. They also reset every other field.
- splitOf, bendSplitOf, and planStops add an element with resizeKeeping and clear its inner lists.
- finishPlan works on the plan in place and returns whether it kept it.

Step 3: Read the world into the kept lists:
- parkSegments and parkCorners use their kept forms with the TickBuffers. Fill the corners once at the start of each path command's planning.
- roundedCorner, isBendOfKind's stopPosition, and stopPositions read the corners list, with appendStopPosition and a finder over it.

Step 4: Replace segmentsAt and segmentsNotAt, which copy segments with their lines:
- A helper gives pointers to the two segments at a point, for isBendOfKind, bendSplitOf, meetCorner, and mergingPair. The callers have checked that exactly two meet there.
- countAt counts the segments at a point.
- planMove walks the segments at its point in place.
- addMeetings takes the point whose segments it skips, NULL_KEY for none, in place of a list of kept segments. planMove and planRemove pass their point.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: edits.cpp fails to build only in the functions Task 9 changes.

### Task 9: Check the plan in kept lists

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: applyPlan takes its keys list, and applyBendSplit its chain list, from the caller.

Step 2: Planning fills PlanLists' World with worldGround at its start, so World.Layout is the before layout and World.Ground the before ground. withCurves copies that layout into EditLists' curves layout by assignment, applies the plan through a LayoutStore, and calls keptCurves into Plan.Curves, with World.Ground as the before ground.

Step 3: Check the plan in the kept lists:
- solidsOf fills the solids list from parkEntrances' and parkBoxes' kept forms.
- editedBy copies World.Layout into PlanLists' After by assignment and applies the plan with its curves. editedBetween takes World for the before side and derives the after ground into AfterGround with groundOf's kept form. It runs once per plan, in planEdit.
- pointsChanged and withFarEnds fill the kept point lists. withFarEnds reads the changed points from one list and appends to another.
- pointsAffected and surfaceAround take the kept lists and surfaces and the TickBuffers.
- changedOwners refills the changed surface it is given.
- The edited segments are kept as indexes into the after ground's segments.
- acceptedPlan returns whether the plan fits, and crossingsOf fills PlanLists' Crossings.

Step 4: The box checks for AddBox and MoveBox use PlanLists' World, the solids list, and a piecesNear that fills a kept focus list and a kept surface.

Step 5: isRestoreAccepted uses the kept lists:
- namesValid's names list.
- applyRestore's added keys: a list of pairs sorted by the record's key, looked up by binary search, in place of the std::map.
- restoredSolids' list and the extra keys.
- The after layout and ground, in PlanLists' After and AfterGround, derived once.

Step 6: setHours finds the holder with keysWith<OpeningHoursIntent> into a kept list, in place of world.keys().

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: exit status 0 and no warning.

### Task 10: Plan once through the public forms

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: planEdit visits the edit. For a path command, it plans into planned.Lists, then sets Accepted from acceptedPlan and fills the crossings for AddPath, MovePoint, and RemovePoint when accepted. For a box command and RestoreIntent, it sets Accepted from their checks. For SetShopStock, SetOpeningHours, SetFurnishing, and DeleteBox, it sets Accepted as isAccepted does today.

Step 2: applyPlanned takes the TickBuffers FEATURE.md gives it, and keeps applyPlan's keys and chain lists in the EditLists from them. It does nothing when the plan is refused. Otherwise it applies the plan with a WorldStore for a path command. For any other edit, it makes the change the command's applyCommand makes after its check.

Step 3: editedParts reads PlanLists' World, After, and AfterGround, which planEdit left. It finds the points and the drawing surfaces as editedBetween does for drawing, with the EditLists from the buffers, and derives no layout or ground. It refills parts.Surface from the changed surface, and parts.Segments from the edited segments' indexes with resizeKeeping and copyGrowing, as parkSegments' kept form refills. It leaves both empty when the edit gave no plan or an edited segment has no ground line.

Step 4: Each buffered applyCommand plans into buffers.get<PlannedEdit>() and applies the plan. The forms without buffers, and the returning isAccepted, pathCrossings, editedSegments, and editedSurface, make a PlannedEdit and TickBuffers of their own and call the kept forms.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: every test passes, the save checks and the changed candidate check among them.

Step 5: Run the integration tests that apply edits directly.

Run: `cmake.exe --build --preset windows-debug --target tpj_integration_tests && build/windows-debug/tpj_integration_tests.exe`
Expected: every test passes.

### Task 11: Plan once in the preview

Files:
- Modify: `src/legible/preview.h`, `src/legible/preview.cpp`
- Modify: `src/legible/arrivals.h`, `src/legible/arrivals.cpp`

Step 1: Add Preview's Planned and drop KeptPreview's Queue, as FEATURE.md gives. makePreview takes no queue. It plans the edit with planEdit into preview.Planned, with the buffers. It returns before making a candidate when the plan is refused. Otherwise, when spare holds no world on the world's schema, it makes one with copyWorld. Then it calls makeCandidate's change form into spare, with a change whose context holds the edit, the plan, and the kept preview's buffers, and which calls applyPlanned with them.

Step 2: Add shopContext's kept form. It keeps its two box lists and its guest segment keys in a ShopLists from the buffers:
- ghostShop reads the boxes with parkBoxes' kept form.
- connectionPlace asks whether a place's carrier is a guest segment by binary search in parkSegmentKeys(candidate, PathKind::Guest, keys).
- The footfall comes from the kept-field fieldValue with the sample it is given.
- The depot comes from nearestDepot with the buffers.

makePreview calls it with the kept preview's Footfall and Buffers. The returning form calls it with a sample and buffers of its own. previewEdit keeps its own lists as it does now.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: exit status 0 and no warning.

Step 3: Keep a preview in the arrival day. KeptArrivalDay in src/legible/arrivals.h gains a KeptPreview, Preview. When keepArrivalDay makes the day again for hours that differ from the park's, it clears Preview.Tick, so the preview is made again for the world as it is now. Then it calls keepPreview with a ParkEdit holding SetOpeningHours with the hours, and without following ticks. It reads the samples from previewedWorld of the kept preview's Made. It no longer calls previewEdit.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: exit status 0 and no warning.

### Task 12: Build the ghost from the plan

Files:
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`
- Modify: `src/app/scene/scene_sync.cpp`, `src/bench/stages.cpp`

Step 1: Change the kept buildGhostMesh to take the plan, and add GhostLists' Parts, as FEATURE.md gives:
- accepted comes from isAccepted on the plan.
- It calls editedParts once into lists.Parts. reshapedBy and the edited surface read it.
- appendJoinMarks takes its crossings from pathCrossings on the plan.
- A MovePoint's or InsertPoint's place comes from appendStopPosition into lists.Positions, with the corners from parkCorners' kept form in lists.Corners.

The two returning forms make a PlannedEdit and TickBuffers of their own and plan the edit.

Step 2: scene_sync.cpp passes the preview's Planned. stages.cpp's timeDrag passes kept.Made.Planned. It drops the isAccepted call before each frame, which would plan the edit a second time. After the frame's timing, it throws the same error when isAccepted on kept.Made.Planned is false. previewStages' ghost stage keeps calling the returning form.

Run: `cmake.exe --build --preset windows-debug`
Expected: exit status 0 and no warning, apart from tools.cpp, which Task 13 changes if it fails to build.

### Task 13: Ask about a path tool's press in kept lists

Files:
- Modify: `src/tools/tools.cpp`, `src/tools/internal/tool_lists.h`

Step 1: ToolLists gains an AddPath, Pressed, and a PlannedEdit. acceptsStop takes the ToolPark. It assigns the drawn stops into Pressed's stops, which keeps their memory, and appends the next stop. It sets the kind, plans it with planEdit using the lists' Buffers, and returns isAccepted on the plan. Building one ParkEdit from it would copy the stops, so keep a ParkEdit in ToolLists. Hold the AddPath in it and refill it in place.

Run: `cmake.exe --build --preset windows-debug`
Expected: exit status 0 and no warning.

### Task 14: Watch the drags and the commits

Files:
- Modify: `tests/checks/allocations/allocation_check.cpp`

Step 1: Add a ResolveTrace, WatchedSteps, that holds the names of the steps the milestone has moved: copy-world, segment-ends, straight-lines, point-arms, corners, and ground-lines. When a step starts that it doesn't hold, it pauses the watch, and it resumes it when that step ends. Finishers are one step, which it never holds. No step starts inside another, so it nests nothing. It acts only while its Watching flag is set, which each check sets after starting the watch and clears before stopping it.

Step 2: Add checkDrags, run after the edit rounds on the session's world. Each drag has its own copy of the world, TickBuffers for stepping it, KeptPreview, and GhostLists. Its kept preview's ResolveTraceSlot holds the WatchedSteps. Each frame steps the drag's world one tick first. The two drags are:
- The lowest-keyed shop moved along its face, 0.5 m a frame, out to 2 m and back, as tpj_bench's shop drag moves it.
- A guest path from the middle of the first guest segment the world accepts one from, as tpj_bench's spurChoice finds it. Its end lies 4 m beyond the segment's side, plus 0.5 m a frame, out to 2 m and back.

Each frame calls keepPreview, then throws std::runtime_error when isAccepted on the kept preview's plan is false, then calls buildGhostMesh with that plan. Asking the returning isAccepted would plan the edit again in new lists. Run WARM_FRAMES, 8, of each drag. Then start the watch and run WATCHED_FRAMES, 16, of each drag. Stop the watch after the last frame. checkDrags reports with reportFreed("the watched drags").

Step 3: Add checkCommits, run after checkDrags, on a copy of the session's world. It uses its own TickBuffers, with the WatchedSteps in their ResolveTraceSlot, and its own CommandQueue. Each round commits, with commitCommands:
- the lowest-keyed shop moved 0.5 m along its face, then back;
- the first guest path point, in key order, that the world lets move 0.5 m along +x and back with no crossings either way, moved there, then back;
- the first guest segment's width set 0.5 m wider by a RestoreIntent, whose Set holds the segment's record from segmentRecords with the new width, then back to its record.

Find the point and the records before the warm-up, and throw when there is none. Run WARM_ROUNDS rounds, then start the watch, run WATCHED_ROUNDS rounds, and stop it. Report with reportFreed("the watched commits").

Step 4: In drawPath, drop the pause and resume around the second stop. Update the comment there, the check's header comment, and its success message, to name the drags and the commits.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, and the message names the watched drags and commits. If watched work frees a block, fix the code that allocates it, then run again.

### Task 15: Confirm the feature

Files: none.

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warning, every test passes, and tidy prints no finding.

Step 2: Check the simulation is unchanged.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/kept-checks/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/kept-checks/full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/kept-checks/scenarios-before.txt build/kept-checks/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/kept-checks/full-before.txt build/kept-checks/full-after.txt`
Expected: both comparisons exit with status 0.

Step 3: Write the after report and compare it with the baseline.

Run: `cmake.exe --build --preset windows-release && scripts/runtime-report.sh build/kept-checks.report && scripts/runtime-report.sh --compare build/flat-ground.report build/kept-checks.report`
Expected: no stage is marked result-changed. On full.park, shop-drag and path-drag are marked faster. path-drag's median is at least 1.5 ms below 13.33 ms, and shop-drag's at least 0.15 ms below 9.48 ms. No other stage is marked slower.

Step 4: Profile the drags. Run this four times, with NAME from kept-checks-1 to kept-checks-4:

    W=$(wslpath -w .)
    "/mnt/c/Program Files/Very Sleepy/sleepy.exe" "/r:$W\build\windows-release\tpj_bench.exe --ticks 1 $W\tests\parks\stress\full.park" "/o:$W\build\perf\NAME.sleepy"

Over the four profiles, look at the samples whose stacks hold timeShopDrag or timePathDrag.
Expected:
- No sample holds applyCommand.
- No sample holds both buildGhostMesh and planEdit.
- Every sample that holds planEdit also holds keepPreview.
- Under timePathDrag, some sample holds planEdit.

Step 5: Read planEdit, applyPlanned, editedParts, the buffered applyCommand forms, shopContext's kept form, the kept buildGhostMesh, keepArrivalDay, and acceptsStop, and every function they call. Confirm that each allocates nothing once its lists have grown. Then grep src for callers of the returning forms FEATURE.md's criterion 5 names. Confirm that each caller is a scenario builder, a tpj_bench stage, or a returning form itself.

### Task 16: Commit

Stage only this feature's paths. Commit with the commit-hygiene skill, subject `Sim: Plan each changed frame's edit once in kept lists`. Then check `git diff --stat` and amend if the hook formatted files.
