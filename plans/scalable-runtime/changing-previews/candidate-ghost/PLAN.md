# Implementation Plan: Candidate Ghost

## Goal

With a candidate, the ghost reads the surface an edit changes from the units the candidate's resolution derived, and shows exactly what it shows today.

## Approach

editedParts gains a form that takes the candidate. It lists the point-paving, segment-paving, hidden-edges, and ground-lines units the candidate's last resolution derived, with derivedUnits. For each listed owner, it compares the owner's run in the candidate's resolved surface with the world's, as changedOwners compares two surfaces. For each listed segment, it compares the candidate's ground line with the world's. The candidate's units equalled the world's before that resolution, so every owner and segment that differs is listed, and the comparison makes the parts exact. buildGhostMesh calls this form whenever it has a candidate.

## Placement

Decision 0027 places each behavior this feature adds:

- The edited parts read from a candidate: park, src/sim/park/edits.h and edits.cpp, next to the form that derives them. Park owns the paved surface, the ground lines, and what an edit changes in them.
- The ghost choosing that form when it has a candidate: render, src/render/park_mesh.cpp, which builds the ghost.

## Tasks

### Task 1: Write the specs and the milestone entry

Files:
- Modify: `src/sim/park/SPEC.md`
- Modify: `src/render/SPEC.md`
- Modify: `plans/scalable-runtime/changing-previews/MILESTONE.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

Step 2: In MILESTONE.md's Features list, insert before thread-rules, renumbering it 10: "9. `candidate-ghost`: with a candidate, the ghost reads the surface an edit changes from the units the candidate's resolution derived, instead of deriving the paving around the edit before and after it. Depends on: feature 8."

Step 3: In MILESTONE.md's whole-park work left, replace the item that begins "Planning a path edit, its acceptance check, and the ghost's edited surface" with two items:

    - The ghost's edited surface: read from the candidate (candidate-ghost). A refused edit has no candidate, so its ghost still derives the paving around it.
    - Planning a path edit and its acceptance check: they copy the layout, derive the whole park's ground (worldGround, keptCurves, groundOf), compare every segment before and after (pointsChanged, withFarEnds), and derive the paving around the edit, before and after (surfaceAround).

### Task 2: Record the scenarios before the change

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/candidate-ghost-baseline-scenarios.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/candidate-ghost-baseline-full.txt`

Expected: both files written, exit 0.

### Task 3: Compare one owner's paving and copy one segment

Files:
- Modify: `src/sim/park/edits.cpp:1463-1485` (changedOwners)
- Modify: `src/sim/park/edits.cpp:2379-2397` (editedSegmentsOf)

Step 1: Split the loop body of changedOwners into a function in the same anonymous namespace, `void addIfChanged(const OwnerPaving *was, EntityKey owner, const OwnerPaving &paving, PavedSurface &changed)`. It does what the body does now: it works out piecesDiffer and edgesDiffer from was and paving, and adds the owner's pieces when they differ and its edges and edge points when they differ. Its comment: "Adds to changed the owner's pieces when they differ from its paving in was, and its edge lines when they do. was is null when the owner had no paving." changedOwners calls it for each owner of after, with pavingOf(before, owner).

Step 2: Split the body of editedSegmentsOf's loop into `void copySegment(const ParkSegment &from, ParkSegment &segment)`, which copies the key, kind, ends, width, line, and edge radii, keeping the line's memory with copyGrowing, and empties the furnishing. Its comment: "Copies the segment into one the caller keeps, keeping its line's memory, with no furnishing." editedSegmentsOf calls it for each index.

### Task 4: Read the edited parts from the candidate

Files:
- Modify: `src/sim/park/edits.h:318-326`
- Modify: `src/sim/park/edits.cpp:878-912` (EditLists)
- Modify: `src/sim/park/edits.cpp:2511-2536` (after editedParts)

Step 1: In edits.h, after the declaration of editedParts, declare:

    // As above, from the edit's candidate: a copy of the world, which is resolved, with the edit
    // applied and then resolved once, as makeCandidate and the preview make it, and not resolved
    // again since. It compares the paving and ground lines the candidate's last resolution derived
    // with the world's, and derives none.
    void editedParts(const World &world, const PlannedEdit &planned, const World &candidate,
                     EditedParts &parts, TickBuffers &buffers);

Step 2: In edits.cpp, include `sim/park/internal/surface_units.h`. Add to EditLists two lists, `std::vector<EntityKey> Derived;` and `std::vector<EntityKey> Owners;`, and add to its comment's list "the units a candidate's resolution derived, and the owners among them".

Step 3: Define the new form after the other editedParts. It reads, in this order:

1. It empties parts as the other form does: beginRefill and endRefill on parts.Surface, and resizeKeeping(parts.Segments, 0, lists.SpareSegments). If kept.HasPlan is false, it returns.
2. The segments. derivedUnits(candidate, GroundLines::Name, lists.Derived). lists.Changed is cleared. For each key, now is unitValue<GroundLines>(candidate, key) and was is unitValue<GroundLines>(world, key). If now is null, skip the key. If was is null, or its Segment.Line or Segment.Width differs from now's, the key is pushed on lists.Changed, and if now's line is empty, the function returns with parts empty, since an edit whose added or reshaped segment has no ground line shows nothing. Then resizeKeeping(parts.Segments, lists.Changed.size(), lists.SpareSegments), and copySegment from each listed key's candidate ground line into parts.Segments in order. derivedUnits gives the keys ascending, so the segments are in ascending key order, as editedSegments gives them.
3. The owners. lists.Owners is cleared. For each of PointPavingUnits::Name, SegmentPavingUnits::Name, and HiddenEdges::Name, call derivedUnits(candidate, name, lists.Derived) and append lists.Derived to lists.Owners. Then sortUnique(lists.Owners).
4. beginRefill(parts.Surface). For each owner, paving is pavingOf(resolvedSurface(candidate), owner). If it is null, skip the owner: an owner the edit leaves with no paving is not part of the edited surface, as the other form leaves it out. Otherwise addIfChanged(pavingOf(resolvedSurface(world), owner), owner, *paving, parts.Surface). Then endRefill(parts.Surface).

Its comment in edits.cpp: "Every paving unit and ground line the edit changes was derived by the candidate's resolution, since the candidate's units equalled the world's before it. The comparison drops those that came out the same."

### Task 5: Build the ghost from the candidate

Files:
- Modify: `src/render/park_mesh.cpp:711` (buildGhostMesh)

Step 1: In buildGhostMesh, replace `editedParts(world, planned, lists.Parts, lists.Buffers);` with a call to the candidate form when candidate holds a world, `editedParts(world, planned, *candidate, lists.Parts, lists.Buffers)`, and the present call otherwise.

### Task 6: Confirm the feature

Files: none.

Task 2's outputs are the before outputs for the scenarios. build/furniture-units.report is the before report.

Step 1: Build both presets, run the tests, tidy, and the scenarios.

Run: `cmake.exe --build --preset windows-debug && cmake.exe --build --preset windows-release && ctest.exe --preset windows-debug && scripts/tidy.sh && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/candidate-ghost-scenarios.txt && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/candidate-ghost-full.txt && build/windows-debug/tpj_scenarios.exe --compare build/candidate-ghost-baseline-scenarios.txt build/candidate-ghost-scenarios.txt && build/windows-debug/tpj_scenarios.exe --compare build/candidate-ghost-baseline-full.txt build/candidate-ghost-full.txt`

Expected: no warning, every test passes, tidy prints "tidy: clean.", and both comparisons exit 0.

Step 2: As soon as the build passes, stage only this feature's paths by pathspec, never with `git add -A`, and dispatch the reviewer on the staged diff through the reviewing skill, in parallel with the checks. Present its findings verbatim.

Step 3: Run `scripts/runtime-report.sh build/candidate-ghost.report && scripts/runtime-report.sh --compare build/furniture-units.report build/candidate-ghost.report`. Expected: path-drag faster, its median at least 1 ms below; no stage marked result-changed.

Step 4: Profile with Very Sleepy four times into build/perf/candidate-ghost-1.sleepy to -4, as the milestone's research notes do, and confirm that editedParts is under 1% of the path drag's timed frame and that no stack under it holds editedBetween or surfaceAround.

Step 5: Read the candidate form of editedParts and everything it calls for criterion 2, and confirm nothing allocates once its lists have grown.

Step 6: Ask the user to play criterion 3 on the windows-release app.

### Task 7: Commit

Step 1: Commit with the commit-hygiene skill, subject `Sim: Read an edit's ghost surface from its candidate`. Check `git diff --stat` and amend with any files the hook formatted.
