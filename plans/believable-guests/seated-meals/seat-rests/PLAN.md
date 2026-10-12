# Implementation Plan: Seat Rests

## Goal

Tired guests choose pieces of furniture from best rest seats, walk to them, sit, and are drawn seated, and a guest turned away from a full piece remembers it and chooses again.

## Approach

Guests read best rest seats through a new allocation-free sampler in sim/wayfinding. It runs the medium's own sampling steps on the field's single slot, with lists the guest's step reuses. A seat becomes a new kind of option. A guest heading to a seat traces its route by the field's entries for the piece, reusing the route tracer's loop. It sends a seat visit at the seat place, infers it is seated from the visit's timing, and remembers full pieces in a fixed array on its entity. The renderer keeps each seated guest's seat between rebuilds in a SeatedGuests object, which the app's SceneSync owns and passes to the mesh, the highlight, and picking.

## Placement

Decision 0027 places each behavior this feature adds:

- Sampling a best seats field without allocating: sim/wayfinding, bestSeatsAt in src/sim/wayfinding/best_seats.h. wayfinding owns the fields and their rule, and any reader of them can use it.
- The poses of a piece's seats: sim/furniture, seatPose in src/sim/furniture/furniture.h. furniture owns each kind's footprint and seat count.
- Seat options, heading to a piece, seat visits, being seated, and remembering full pieces: sim/guests, src/sim/guests/guests.cpp, with the state in src/sim/guests/internal/guest.h. A guest's choices and its walk are the guests module's.
- Tracing a route by a best seats field: sim/guests, traceSeatRoute in src/sim/guests/internal/route_ahead.h, beside traceRoute, which it shares a loop with.
- Keeping each seated guest's seat for drawing and picking: render, SeatedGuests in src/render/guest_mesh.h. It is presentation, and the guest mesh is where guests are placed for drawing.
- Owning the SeatedGuests and updating it when the guest mesh is rebuilt: app/scene, SceneSync in src/app/scene/scene_sync.h, which already decides when the guest mesh is rebuilt. application.cpp and cursor.cpp only pass it on, which is composition.
- Naming pieces and listing full pieces in the Inspector: legible, src/legible/inspect.cpp, which builds every inspector row.
- The guest-mesh stage's SeatedGuests: bench, src/bench/stages.cpp, which builds each measured stage.

## Tasks

### Task 1: Spec the change

Files:
- Modify: `src/sim/guests/SPEC.md`, `src/sim/wayfinding/SPEC.md`, `src/sim/furniture/SPEC.md`, `src/render/SPEC.md`, `src/legible/SPEC.md`, `src/app/SPEC.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

### Task 2: Declare the sim interfaces

Files:
- Modify: `src/sim/guests/guests.h`, `src/sim/wayfinding/best_seats.h`, `src/sim/wayfinding/best_seats.cpp`, `src/sim/furniture/furniture.h`, `src/sim/furniture/furniture.cpp`

Step 1: Add the declarations FEATURE.md's Public interface gives for guests.h, best_seats.h, and furniture.h. guests.h includes `sim/furniture/furniture.h` and `sim/wayfinding/best_seats.h`. Append the new enum values after the existing ones, so saved names keep their meaning. best_seats.h includes `sim/medium/network.h` and declares `class World;`.

Step 2: Add stub definitions: bestSeatsAt clears out, and seatPose returns the piece's pose. Leave unused parameters unnamed.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 3: Sample best seats without allocating

Files:
- Modify: `src/sim/wayfinding/best_seats.cpp`

Step 1: Define bestSeatsAt. Clear out. Take the slot with `sourceSlot<F>(world, fieldKey(F::Name))`, where F is BestRestSeats for SeatUse::Rest and BestMealSeats for SeatUse::Meal. If there is no slot, or the place doesn't resolve, return. At a node, call `positionsAtAny(*slot, network.stopPlaces(node), scratch.Positions)` and append each position's entry value, in that order, as sampleSlotAtNode does. Inside an edge, call `gatherEdgeSample(network, *slot, edge, scratch, read)`, where read gives `slot->Entries[i].Value`. If it returns true, set `scratch.Edge.FromOffset` and `ToOffset` from the position, and call `sampleBestPlaceEdge(scratch.Edge, SEAT_CHOICES, out)`.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 4: Give each seat its pose

Files:
- Modify: `src/sim/furniture/furniture.cpp`

Step 1: Define seatPose as the spec gives it. The piece's facing (FacingX, FacingZ) points to the paving, and a pose holds it unnormalized, so normalize it first. Seen from the paving, a guest looks along the reversed facing, so the piece's left is sim/park's leftOf of the reversed facing: (-FacingZ, FacingX), normalized. Number the seats from that left. A bench's seat 0 is 0.45 m to the left of its middle, and seat 1 is 0.45 m to the right, both facing as the bench faces. A table's seats 0 and 1 are 0.5 m from its middle toward the paving, facing away from the paving, and seats 2 and 3 are 0.5 m away from the paving, facing toward it. Each pair runs left to right. Throw std::out_of_range for a seat not below furnitureSeats(kind).

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 5: Add the guest's seat state

Files:
- Modify: `src/sim/guests/internal/guest.h`

Step 1: Add three fields to Guest after LastMeal:

- `EntityKey TargetOwner = NULL_KEY;`, the segment that lays the piece the guest heads to or sits at.
- `uint64_t Sent = 0;`, the tick it sent its last seat visit.
- `std::array<FullPiece, SEAT_CHOICES> Full{};`, the pieces it remembers as full, where an entry with Piece NULL_KEY is empty.

Update the struct's comment to name them.

Step 2: Visit them last in visitFields, as `target-owner`, `sent`, and `full-0` to `full-3`, after `last-meal`. Name the full entries from a constexpr array of four names, with a static_assert that its size equals SEAT_CHOICES. Include `sim/wayfinding/best_seats.h` and `<array>`.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 6: Score seats as options

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Add to WalkLists a `SampleScratch<BestPlace> SeatScratch` and a `std::vector<BestPlace> Seats`. Give scoreOptions, choose, and chooseFor a `WalkLists &lists` parameter. guestOptions makes a local WalkLists for its call.

Step 2: Add `bool remembersFull(const Guest &guest, EntityKey piece, uint64_t tick)`: some entry of Full holds the piece and `tick < Found + FULL_MEMORY_TIME`.

Step 3: In scoreOptions, after the facility offers and before carrying on, call `bestSeatsAt(world, network, guest.At, SeatUse::Rest, lists.SeatScratch, lists.Seats)`. For each place the guest doesn't remember as full, push an option of kind Seat, with Facility the place's Key, Need Tiredness, and Relief `RELIEF_WEIGHT * needCurve(guest.Tiredness) * place.Value`. Its Distance is `DISTANCE_WEIGHT * place.Distance`, and its Commitment is COMMITMENT_BONUS when the guest's activity is HeadingToSeat and its target is the piece. Its Score is Relief + Distance + Commitment, in that order. Add `std::vector<EntityKey> Owners` to ScoredOptions, holding the place's Owner for a seat and NULL_KEY for every other option. Push it alongside Reliefs, which gets the place's Value.

Step 4: In choose, when the picked option's kind is Seat, set the activity to HeadingToSeat, Target to the piece, TargetNeed to Tiredness, ServingRelief to its relief, and TargetOwner to its owner.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 7: Trace a route to a seat

Files:
- Modify: `src/sim/guests/internal/route_ahead.h`, `src/sim/guests/route_ahead.cpp`

Step 1: Move traceRoute's body, after its first point is added, into a private function template that takes the step chooser as a callable `on(step)` returning `std::optional<RouteStep>`. traceRoute calls it with wanderOn or routeOn, as now.

Step 2: Declare and define `void traceSeatRoute(const World &world, const Network &network, const Place &place, const RouteStep &first, SeatUse use, EntityKey piece, SampleScratch<BestPlace> &scratch, std::vector<BestPlace> &places, RouteAhead &route);`. Its comment: "Fills the route as traceRoute does, going on from each step's end by the piece's entry in the use's best seats field there. It ends where that entry has arrived, and stops where the field doesn't give the piece. Allocates nothing once the lists have grown." Its chooser calls bestSeatsAt at `{step.Carrier, step.To}` and finds the entry whose Key is the piece. If there is none, it returns none. If its Next has carrier NULL_KEY, it sets Ends and returns none. Otherwise it returns its Next.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 8: Head to a seat

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Give Heading a `std::optional<BestPlace> Seat`. In headingOf, while the activity is HeadingToSeat, call bestSeatsAt at the guest's place with SeatUse::Rest and take the entry whose Key is the target. If there is none, set the activity to Wandering, clear Target, and set Dropped. headingOf takes the WalkLists.

Step 2: In walk, after the facility arrival, if `heading.Seat` has a Next with carrier NULL_KEY, send the seat visit: `createUnits<SeatVisits>(world, key, key, 1)`, then `sendUnits<SeatVisits>(world, key, guest.Target, key, 1, VISIT_DELAY)`. Set Sent to world.Tick and the activity to Waiting, and return Stays.

Step 3: In traceGuestRoute, when `heading.Seat`, call traceSeatRoute from the guest's place along `heading.Seat->Next`. Pass `heading.Seat.has_value()` to turnNode with the others.

Step 4: Add `void shiftTowardPiece(const World &world, const Guest &guest, RouteAhead &route)`, called in walk after moveToSide when the heading is a seat and the route ends. It finds the piece's SeatPlace in `sourceSlot<SeatPlaces>(world, guest.TargetOwner)` by its Piece. Its direction is from the route's last point's At to the piece's pose. The last point's Shift becomes that direction, normalized, times `0.5 * width - GUEST_CLEARANCE`, where width is the last point's Width. If the piece isn't found, the route has one point, the direction has no length, or the length is at most 0, it changes nothing.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 9: Sit, take the rest, or remember the piece as full

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Add `void rememberFull(Guest &guest, EntityKey piece, uint64_t tick)`. It writes `{piece, tick}` into the first entry of Full that is empty or expired, or else into the entry with the least Found.

Step 2: Only a seat target sets TargetOwner. Clear TargetOwner wherever Target is cleared: in choose, in headingOf, and in awaitVisit. Add an `awaitSeat`, which stepGuests calls in place of awaitVisit when TargetOwner isn't NULL_KEY. awaitSeat checks `unitsHeld<SeatVisits>(world, key, key)`.

- If it holds none, and the activity is Waiting and `world.Tick >= Sent + VISIT_DELAY + RETURN_DELAY`, set the activity to Seated. Return still waiting.
- If it holds the visit, consume it with FINISHED_CAUSE. If it holds rests, consume them with RESTED_CAUSE and lower tiredness by ServingRelief, not below 0. Otherwise call rememberFull with the target and world.Tick. Set the activity to Wandering, clear Target, TargetOwner, and Turn, and report that the guest must choose.

Step 3: In stepGuests, await while the activity is Waiting or Seated. Pass a `mustChoose` argument to walk, true when a seat visit just came back, and start `progress.MustChoose` from `moment.Closes || mustChoose`.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 10: Publish the seat and the full pieces

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: In guestRecord, while the activity is Seated, set Seat to the piece's SeatPlace from `sourceSlot<SeatPlaces>(world, state->TargetOwner)`, if it is there. Copy each entry of Full into FullPieces that is not empty and not expired at world.Tick, at the same index, and leave the others empty. This allocates nothing.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 11: Draw seated guests on their seats

Files:
- Modify: `src/render/guest_mesh.h`, `src/render/guest_mesh.cpp`, `src/render/picking.h`, `src/render/picking.cpp`

Step 1: Add GUEST_SEATED_HEIGHT and SeatedGuests to guest_mesh.h. SeatedGuests keeps a list of taken seats, `{Guest, Piece, Seat, Pose}`, ascending by guest. update reads every guest's record. A guest whose record has a Seat keeps its old entry if that was at the same piece. The others with a Seat take, in key order, the lowest seat below the piece's Seats that no kept or new entry at the piece holds, posed by seatPose. If none is free, the guest gets no entry. Each kept entry's pose is posed again from the record, so a piece re-laid in place moves its guests with it. seatOf finds the guest's entry by binary search.

Step 2: buildGuestMesh and appendGuestEntity take the SeatedGuests. A guest with a seat is drawn with appendBox at the seat's pose, GUEST_SIZE, and GUEST_SEATED_HEIGHT. Every other guest is drawn as before.

Step 3: entityAtCursor takes the SeatedGuests and tests each guest's box where buildGuestMesh draws it.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_render`
Expected: the build ends with no warnings and no errors.

### Task 12: Keep the seats in the scene

Files:
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`, `src/app/input/cursor.h`, `src/app/input/cursor.cpp`, `src/app/application.cpp`, `src/bench/stages.cpp`

Step 1: SceneSync gains a `SeatedGuests Seated` and a `[[nodiscard]] const SeatedGuests &seated() const`. syncWorld empties it for a new generation and calls `Seated.update(world)` whenever it returns Guests true.

Step 2: uploadWorldMeshes, uploadPreviewMeshes, and entityUnderCursor take a `const SeatedGuests &` and pass it to buildGuestMesh, appendGuestEntity, and entityAtCursor. application.cpp passes `Scene.seated()` to each.

Step 3: In stages.cpp, the guest-mesh stage builds a SeatedGuests, updates it from the park, and builds the mesh with it, inside the timed call.

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build ends with no warnings and no errors.

### Task 13: Name pieces in the Inspector

Files:
- Modify: `src/legible/inspect.cpp`

Step 1: boxText also looks the key up in parkFurniture, and names a piece as its kind's display name and its key, as it names a box.

Step 2: choiceRows counts options of kind Seat with those of kind Offer for each need.

Step 3: guestRows adds a "Full pieces" row after "Last meal". It lists each of the record's FullPieces that isn't empty as its name from boxText and `{} s ago`, using seconds(ticksSince(world, Found)), joined with "; ". If there are none, it says "none".

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug --target ThemeParkJones`
Expected: the build ends with no warnings and no errors.

### Task 14: Remake the parks

Files:
- Modify: `tests/parks/cut.park`, `evening.park`, `noon.park`, `warm.park`, `warm-main-street.park`, `tests/parks/stress/full.park`, `parks/busted-path.park`, `parks/jank.park`, `parks/radii.park`

Step 1: Remake the slice parks.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park tests/parks/day.park tests/parks/no-toilet.park tests/parks/noon.park tests/parks/evening.park; echo $?`
Expected: `0`

Step 2: Confirm criterion 6.

Run: `for f in tests/parks/*.park; do git show HEAD:$f | cmp -s - <(sed -E 's/ target-owner=.*$//' $f) || echo $f; done`
Expected: no output.

Step 3: Remake full.park.

Run: `cmake.exe --build --preset windows-release --target tpj_bench > /dev/null 2>&1; build/windows-release/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

Step 4: Add the new fields to the play parks' guest lines.

Run: `sed -i -E 's/(last-meal=\{[^}]*\})$/\1 target-owner=0 sent=0 full-0={piece=0 found=0} full-1={piece=0 found=0} full-2={piece=0 found=0} full-3={piece=0 found=0}/' parks/busted-path.park parks/jank.park parks/radii.park; grep -c "full-3=" parks/busted-path.park parks/jank.park parks/radii.park`
Expected: `parks/busted-path.park:35`, `parks/jank.park:37`, `parks/radii.park:63`

Step 5: Load them.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 1 parks/busted-path.park parks/jank.park parks/radii.park > /dev/null; echo $?`
Expected: `0`

If the saved form of an entry differs from the sed's text, copy it from a guest line of the remade noon.park and run step 4 again.

### Task 15: Check the guests' seats

Files:
- Create: `build/seat-rests/build.sh` (copied from build/best-seats/build.sh), `build/seat-rests/seatcheck.cpp`

Step 1: Write seatcheck.cpp. It loads tests/parks/stress/full.park and checks bestSeatsAt against sampleField for both uses, at every node's nodePlace and at the middle of every edge. Then it copies the park, applies SetFurnishing with no furnishing on every guest segment except the five with the lowest keys, and resolves it. It steps the copy 1800 ticks. At each tick, it does four things:

- For each guest, it calls guestOptions. Each seat option must name a piece that bestSeatsAt gives at the guest's place with the terms criterion 1 states, and no piece in the record's FullPieces.
- It follows each guest's record from tick to tick. A guest becoming Waiting with a piece as its target counts as `reached`. A guest that becomes Seated exactly at the visit's send tick plus 2, and later stands with tiredness lowered by its seat's rest relief, counts as `seated`. A guest that leaves Waiting at the send tick plus 2 with a new choice made in that tick, and the piece in its FullPieces, counts as `turned away`. Each other outcome counts as wrong.
- It checks the ledger identity of seat-visits and rests.
- It updates one SeatedGuests and checks that no two guests share a seat pose, and that no guest's seat changes while it stays Seated at the same piece.

It prints the lines criteria 1, 3, 4, and 7 give.

Step 2: Build and run it.

Run: `sh build/seat-rests/build.sh seatcheck && build/seat-rests/seatcheck.exe`
Expected: `bestSeatsAt: N places, 0 differ`, `seat options: N checked, 0 wrong`, `seated: N wrong: 0`, `turned away: N wrong: 0`, `reached: N`, and `seats: N checked, 0 shared, 0 moved`, each N above 0.

### Task 16: Play and measure

Step 1: Rebuild windows-release in full.

Run: `cmake.exe --build --preset windows-release`
Expected: the build ends with no warnings and no errors.

Step 2: Play full.park in build/windows-release/ThemeParkJones.exe with the park file. Do the play checks of criteria 2, 4, and 5.

Step 3: Make the runtime report and compare it with build/best-seats/after.report.

Run: `scripts/runtime-report.sh build/seat-rests/after.report > build/seat-rests/report.log 2>&1; tail -1 build/seat-rests/report.log; scripts/runtime-report.sh --compare build/best-seats/after.report build/seat-rests/after.report | tail -2`
Expected: `over-budget 0 of 25` in the report, and the comparison's lines, with full.park's ticks changed.

### Task 17: Confirm the criteria

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports nothing.

### Task 18: Commit

Step 1: Commit the feature with commit-hygiene. Stage only this feature's paths, by pathspec.
