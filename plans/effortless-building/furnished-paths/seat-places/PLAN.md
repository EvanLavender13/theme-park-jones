# Implementation Plan: Seat Places

## Goal

Furniture publishes a seat places entry for each piece, the full park carries furniture on every guest segment, and the runtime report compares it with the park before the milestone.

## Approach

sim/furniture registers the seat places field in addFurniture, before its resolver. After layFurniture has laid or kept every chain, it groups the pieces by segment and publishes one entry per piece from each segment, at the place on the segment's guest carrier nearest the piece. makeFullPark furnishes every guest segment after laying out its boxes, so full.park carries several hundred pieces.

## Placement

Decision 0027 places each behavior this feature adds:

- The seat places field, its entry, and furnitureSeats: sim/furniture, in src/sim/furniture/furniture.h. The pieces are furniture's, and the slice makes their segments the field's sources.
- Publishing the entries: sim/furniture, in layFurniture in src/sim/furniture/furniture.cpp, which knows every piece once it has laid them.
- Furnishing the full park: scenarios, in makeFullPark in src/scenarios/full_park.cpp, which lays out everything else in that park.

## Tasks

### Task 1: Make the before parks

Files:
- Create, scratch: `build/seat-places/before/`

Step 1: Before any change, build tpj_scenarios with windows-debug and make the slice parks.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && B=build/seat-places/before && mkdir -p $B && cp tests/parks/fed.park $B/fed.park && build/windows-debug/tpj_scenarios.exe --slice-parks $B/fed.park $B/warm.park $B/cut.park tests/parks/main-street.park $B/warm-main-street.park tests/parks/facilities.park $B/stocked.park $B/day.park $B/no-toilet.park $B/noon.park $B/evening.park && ls $B | wc -l`
Expected: `9`.

### Task 2: Make the runtime report from before the milestone

Files:
- Create, scratch: `build/seat-places/before-milestone.report`

Step 1: Check that the other session's staged files and the stress parks are the same at 81f2ad1, the commit before furnished-segments, as on main, so a detached checkout leaves the index alone.

Run: `git diff --stat 81f2ad1 HEAD -- plans/sound-architecture tests/parks/stress | wc -l`
Expected: `0`.

Step 2: Check out 81f2ad1, make the report, and come back to the seat-places branch. The report takes several minutes.

Run: `git switch --detach 81f2ad1 && mkdir -p build/seat-places && scripts/runtime-report.sh build/seat-places/before-milestone.report; git switch seat-places && git branch --show-current && git status --short`
Expected: the report's table printed, then `seat-places`, then the status shows only the other session's staged plans/sound-architecture files and image.png.

### Task 3: Spec seat places and the furnished full park

Files:
- Modify: `src/sim/furniture/SPEC.md`, `src/scenarios/SPEC.md`, `src/bench/SPEC.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

### Task 4: Declare the field

Files:
- Modify: `src/sim/furniture/furniture.h`

Step 1: Include `sim/medium/field.h`, `<stdint.h>`, and `<string_view>`. After furnitureSize, add furnitureSeats, returning 2 for Bench and 4 for Table, with a final `return 2;` after the switch as furnitureSize has. Then add SeatPlace and SeatPlaces as FEATURE.md gives them. SeatPlace's visitFields visits, in order: `piece`, `x`, `z`, `facing-x`, `facing-z` (the four parts of At), `kind`, `seats`, `segment`.

Step 2: Change addFurniture's comment to: "Registers the derived types furniture-piece and laid-furniture, the field seat places, and the resolver furniture."

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 5: Publish the entries

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: In addFurniture, call `addField<SeatPlaces>(schema)` after the two addComponent calls and before addResolver, so the field's resolver empties it before furniture publishes.

Step 2: Add `void publishSeatPlaces(World &world)` above layFurniture. It reads `parkFurniture(world)`, which is in key order, and `parkNetwork(world, PathKind::Guest)`. It collects, for each piece, the segment and a `PlacedEntry<SeatPlace>`: the place is `network.nearestPlaceOn(piece.Segment, GroundPoint{piece.At.X, piece.At.Z})`, and a piece whose place is none is left out. It stable-sorts the collected entries by segment, so each segment's entries stay in key order of their pieces, and calls `publishResolved<SeatPlaces>(world, segment, entries)` once for each segment with at least one entry.

Step 3: Call publishSeatPlaces at the end of layFurniture, after the pieces it no longer lays are destroyed.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 6: Check each piece's entry

Files:
- Create, scratch: `build/seat-places/seatcheck.cpp`, `build/seat-places/build.sh`

Step 1: Copy build/yielding-furniture/perf/build.sh to build/seat-places/build.sh. It builds `<name>.cpp` against the windows-release libraries.

Step 2: Write seatcheck.cpp. It loads the park file named by its argument and resolves it. For each piece of parkFurniture, it finds the place nearestPlaceOn gives on the piece's segment in the guest network, samples SeatPlaces there with sampleField, and looks for an entry from the piece's segment whose Piece is the piece's key. It counts the pieces, those it finds an entry for, and those whose entry's At, Kind, Seats, and Segment match the piece and furnitureSeats. It prints `pieces N found F matching M`.

Run: `cmake.exe --build --preset windows-release --target tpj_sim && build/seat-places/build.sh seatcheck && build/seat-places/seatcheck.exe tests/parks/furnished-paths.park`
Expected: `pieces 85 found 85 matching 85`.

### Task 7: Confirm the simulation is unchanged

Files:
- Create, scratch: `build/seat-places/after/`

Step 1: Make the slice parks with this feature's build and compare them with Task 1's.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && A=build/seat-places/after && mkdir -p $A && cp tests/parks/fed.park $A/fed.park && build/windows-debug/tpj_scenarios.exe --slice-parks $A/fed.park $A/warm.park $A/cut.park tests/parks/main-street.park $A/warm-main-street.park tests/parks/facilities.park $A/stocked.park $A/day.park $A/no-toilet.park $A/noon.park $A/evening.park && for f in fed warm cut warm-main-street stocked day no-toilet noon evening; do diff build/seat-places/before/$f.park $A/$f.park; done | grep '^[<>]' | sort | uniq -c`
Expected: three lines, each counted 9: `> ` (the blank line after a section), `> <key> readable=[] pending=[]`, with the same key in every park, and `> [seat-places-stepped]`. No line starts with `<`.

### Task 8: Furnish the full park

Files:
- Modify: `src/scenarios/full_park.cpp`, `tests/parks/stress/full.park`

Step 1: In full_park.cpp, add `void furnish(World &world)` after layOut. For each segment of parkSegments(world) whose Kind is PathKind::Guest, it calls `applyAccepted(world, SetFurnishing{segment.Key, Furnishing{SideFurnishing{FurnitureKind::Bench, FURNITURE_SPACING}, SideFurnishing{FurnitureKind::Table, FURNITURE_SPACING}}}, "furnishing segment " + std::to_string(static_cast<uint64_t>(segment.Key)))`, with `constexpr double FURNITURE_SPACING = 10.0;` beside the file's other constants and the comment "How far apart each guest segment's furniture stands, in meters." Update layOut's comment if it lists what the park holds. In makeFullPark, call furnish(world) right after layOut(world).

Step 2: Write the full park and count its furniture. Copy seatcheck.cpp to build/seat-places/count.cpp and make it also print, after its line, `bench B table T furnished S of G`: the pieces of each kind, the guest segments with furnishing, and the guest segments in parkSegments.

Run: `cmake.exe --build --preset windows-release && build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park && build/seat-places/build.sh count && build/seat-places/count.exe tests/parks/stress/full.park`
Expected: found and matching equal pieces, pieces between 300 and 1,500, both kinds above 0, and S equal to G.

Step 3: Confirm the full park still holds what it held.

Run: `for s in guest box; do awk -v s="[$s]" '/^\[/ {h=$0; next} h==s && NF' tests/parks/stress/full.park | wc -l; done`
Expected: `2008`, then `93`, as on main: the 2,000 guests placed and those who arrived in the warm minute, and 30 shops, 3 depots, 15 toilet blocks, and 45 benches.

### Task 9: Compare the runtime report

Files:
- Create, scratch: `build/seat-places/after.report`

Step 1: Make the report on this feature's build and compare it with Task 2's.

Run: `scripts/runtime-report.sh build/seat-places/after.report && scripts/runtime-report.sh --compare build/seat-places/before-milestone.report build/seat-places/after.report`
Expected: the comparison's tick stages on full.park within their noise, and its resolution stages higher by the cost of laying furniture. `result-changed` marks every park's hash stages, since the milestone registers new types and every world's hash covers them, and full.park's park mesh, since it now draws furniture. Report the numbers.

### Task 10: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 11: Commit, merge, and close the milestone

Files:
- Modify: `plans/effortless-building/furnished-paths/MILESTONE.md`

Step 1: Stage only this feature's paths: src/sim/furniture/, src/scenarios/SPEC.md, src/scenarios/full_park.cpp, src/bench/SPEC.md, tests/parks/stress/full.park, and plans/effortless-building/furnished-paths/seat-places/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Publish seat places and furnish the full park`. Merge into main as implementing-features says, and rebuild windows-release.

Step 2: Close the milestone as planning-milestones' Closing a milestone says: confirm the build, tests, and tidy (Task 10), check each of MILESTONE.md's criteria by its means and report what was seen, set its Status to complete, and commit MILESTONE.md by pathspec with subject `Plans: Close the furnished-paths milestone`.
