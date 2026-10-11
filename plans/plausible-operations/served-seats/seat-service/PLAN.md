# Implementation Plan: Seat Service

## Goal

Each piece of furniture publishes a seat offer and seats or turns away the guests who visit it, and the food-shop scenario exercises it.

## Approach

A new unit, src/sim/operations/seats, registers the seat-visits flow, the seat-offers field, the seat-service state, a resolver, and a system. The resolver reads every seat place through a new medium read, resolvedSlots, and publishes one offer for each piece in key order. The system steps only the endpoints that hold seat visits under another key. Each step seats every visit it can and sends the rest back, so no piece carries a visit into the next tick.

## Placement

Decision 0027 places each behavior this feature adds:

- Reading every source's resolved entries of a field: sim/medium, resolvedSlots in src/sim/medium/field.h. Fields belong to the medium, and best-seats will read seat offers the same way.
- Seat offers, seat visits, the seat state, and seat service: sim/operations, in the new src/sim/operations/seats.h, seats.cpp, and internal/seat_service.h. plausible-operations owns the offers and services in the park, and seats follow facilities.cpp's pattern. A unit of its own keeps facilities.cpp to boxes.
- Calling addSeats: sim/operations, in addOperations in src/sim/operations/operations.cpp, which already registers the rest of operations in order.
- The furnished path and seat visits in the food-shop scenario: scenarios, in src/scenarios/food_shop.cpp, which holds that scenario.
- Naming seat-service as unheld: the park world checks, in tests/sim/park_world_checks_test.cpp, which hold the list.

## Tasks

### Task 1: Spec the change

Files:
- Modify: `src/sim/operations/SPEC.md`, `src/sim/medium/SPEC.md`, `src/scenarios/SPEC.md`

Step 1: Make the spec changes FEATURE.md lists, word for word.

### Task 2: Add resolvedSlots

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Above sampleResolvedField, add resolvedSlots with the declaration and comment FEATURE.md gives. It returns the Slots of the field's ResolvedEntries component on `fieldKey(F::Name)`, or an empty span when the world holds none. Add `#include <span>` if field.h lacks it.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 3: Declare seats

Files:
- Create: `src/sim/operations/seats.h`, `src/sim/operations/seats.cpp`
- Modify: `src/sim/CMakeLists.txt`, `src/sim/operations/operations.h:178-183`, `src/sim/operations/facilities.h:77-79`

Step 1: Write seats.h with the declarations FEATURE.md's Public interface gives for it, in the layout of facilities.h. Include `sim/medium/field.h`, `sim/operations/facilities.h` (REST_RELIEF), `sim/operations/operations.h` (MEAL_RELIEF), `sim/park/intent.h` (FurnitureKind), `<stdint.h>`, and `<string_view>`. Give SeatOffer a visitFields that visits `rest-relief`, `meal-relief`, and `seats`, in that order. Define seatOffer in the header: a switch on the kind that returns `{BENCH_REST_RELIEF, BENCH_MEAL_RELIEF, seats}` for Bench and `{TABLE_REST_RELIEF, TABLE_MEAL_RELIEF, seats}` for Table, with a final return of the bench's offer after the switch, as furnitureSeats has.

Step 2: Write seats.cpp with `#include "sim/operations/seats.h"`, `#include "sim/schema.h"`, and an empty `void addSeats(WorldSchema &schema) {}` in namespace tpj, with the parameter unnamed (`WorldSchema & /*schema*/`) so the stub builds without warnings.

Step 3: Add `operations/seats.cpp` after `operations/operations.cpp` in src/sim/CMakeLists.txt.

Step 4: Change addOperations' comment in operations.h so its last clause reads "then the systems that step shops and then depots, then calls addFacilities, and last calls addSeats."

Step 5: In facilities.h, change the end of addFacilities' comment from "addOperations calls it last." to "addOperations calls it after the shops' registrations."

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 4: Add the seat state

Files:
- Create: `src/sim/operations/internal/seat_service.h`

Step 1: Write the header in the layout of internal/facility_service.h, with the guard `TPJ_SIM_OPERATIONS_INTERNAL_SEAT_SERVICE_H`. It declares `struct SeatService { std::vector<uint64_t> FreeAt; };` with the comment "State, on a piece of furniture's entity: the tick each taken seat frees, ascending." and a visitFields that visits `free-at`.

### Task 5: Publish the seat offers

Files:
- Modify: `src/sim/operations/seats.cpp`

Step 1: In an anonymous namespace, add `void resolveSeatOffers(World &world)`. It reads `resolvedSlots<SeatPlaces>(world)` (sim/furniture/furniture.h). For each entry of each slot, it collects the piece's key and `PlacedEntry<SeatOffer>{entry.At, seatOffer(entry.Value.Kind, entry.Value.Seats)}`. It sorts what it collected by the piece's key, then calls `publishResolved<SeatOffers>(world, piece, {placed})` for each piece in that order, so each insert goes at the end of the field's slots.

### Task 6: Step the pieces holding visits

Files:
- Modify: `src/sim/operations/seats.cpp`

Step 1: In the anonymous namespace, add `void stepSeats(World &world)`. It returns at once if `ledgerOf<SeatVisits>(world)` is null. It collects into a local list each live endpoint of that ledger's Stocks that holds units under a handle other than itself, once each, in the Stocks' ascending endpoint order. An endpoint may be gone: stepWorld swaps before it applies commands and resolves, so a piece can be destroyed in the tick a visit reaches it. Skip it, and the next swap passes its stock to each guest. It makes two more local lists before the loop and reuses them for every piece: the piece's seat-visit holdings and its rest holdings.

Step 2: For each collected piece, in order:

- Its seats are the Seats of the first entry of `sourceSlot<SeatOffers>(world, piece)`, or 0 when it has no slot or no entry.
- Copy the piece's Rests stocks from `ledgerOf<Rests>(world)->Stocks` (an equal_range on the endpoint) into the rest list, and consume each with ABANDONED_CAUSE. Only a seated guest who left sends a rest back to a piece.
- Copy the piece's SeatVisits stocks into the visit list in the same way.
- Take `world.tryGet<SeatService>(piece)`, and erase from its FreeAt every tick at or before world.Tick.
- For each holding, handles ascending: if the handle is the null key or not live, consume its units with ABANDONED_CAUSE. Otherwise, for each unit, if fewer FreeAt ticks remain than the seats, `createUnits<Rests>(world, piece, guest, 1)`, `sendUnits<Rests>(world, piece, guest, guest, 1, SEAT_TIME)`, `sendUnits<SeatVisits>(world, piece, guest, guest, 1, SEAT_TIME)`, and push `world.Tick + SEAT_TIME` onto FreeAt. If not, `sendUnits<SeatVisits>(world, piece, guest, guest, 1, RETURN_DELAY)`.
- Afterwards, if FreeAt is empty, remove SeatService from the piece. If it isn't empty and the piece held none, emplace it.

Use a local FreeAt list, reused across pieces, while the piece has no SeatService. Keep FreeAt ascending: every push is world.Tick + SEAT_TIME, which is never below what it holds.

### Task 7: Register seats

Files:
- Modify: `src/sim/operations/seats.cpp`, `src/sim/operations/operations.cpp:747`

Step 1: Fill in addSeats: `addFlow<SeatVisits>(schema)`, `schema.addComponent<SeatService>("seat-service", DataKind::State)`, `addField<SeatOffers>(schema)`, `schema.addResolver(SeatOffers::Name, &resolveSeatOffers, {"furniture", "seat-offers-field"})`, and `schema.addSystem(&stepSeats)`.

Step 2: In addOperations, call `addSeats(schema);` after `addFacilities(schema);`, and include `sim/operations/seats.h`.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 8: Check each piece's offer

Files:
- Create, scratch: `build/seat-service/build.sh`, `build/seat-service/offercheck.cpp`

Step 1: Copy build/seat-places/build.sh to build/seat-service/build.sh. Add `"$B/scenarios/libtpj_scenarios_lib.a"` before the sim library, so Task 12's check links.

Step 2: Write offercheck.cpp. It loads the park file named by its argument with makeParkSchema and resolves it. For each entry of each slot of `resolvedSlots<SeatPlaces>`, it samples SeatOffers with sampleField on `parkNetwork(world, PathKind::Guest)` at the entry's place. It counts the pieces, those whose key is the source of a sampled entry, and those whose entry equals `seatOffer(kind, seats)` for the seat place. It prints `pieces N offered O matching M`.

Run: `cmake.exe --build --preset windows-release --target tpj_sim tpj_scenarios_lib && build/seat-service/build.sh offercheck && build/seat-service/offercheck.exe tests/parks/furnished-paths.park`
Expected: `pieces 85 offered 85 matching 85`.

### Task 9: Name the seat state as unheld

Files:
- Modify: `tests/sim/park_world_checks_test.cpp:54-58`

Step 1: Add to UNHELD_TYPES: `UnheldType{"seat-service", "no park holds a guest seated at furniture until believable-guests' seated-meals remakes the day parks with guests who sit"}`.

Step 2: Build the sim tests and run the park world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 10: Check seat service

Files:
- Create, scratch: `build/seat-service/servecheck.cpp`

Step 1: Write servecheck.cpp. It builds a schema with addPark and one more system, the script, and loads tests/parks/furnished-paths.park with it. B is the first bench of parkFurniture. T is the first table whose segment shares no path point with B's segment, so the edit below leaves T as it is. Let t0 be world.Tick the first time the script runs. Each guest is a plain entity the script creates. The script, by world.Tick − t0:

- 0: create g1, g2, g3, each sending one seat visit to B, and h1 to h5, each sending one to T, all with delay 1, in that order. A guest sends with createUnits and sendUnits of SeatVisits, from and under its own key.
- 100: create g4, sending to B with delay 1.
- 200: destroy h1.
- 300: create g6, sending to B with delay 1.
- 400: create g7, sending to B with delay 20, so it reaches B in the swap that ends the step at 419.
- 410: create g8, sending to B with delay 50.

In the step that starts at 419, the main loop queues a SetFurnishing of B's segment that keeps the segment's furnishing but clears B's side, before stepping. The resolution after that step's swap removes B while it holds g7's visit. Tables are laid only in lines of tables, so clearing a bench side leaves T's line as it was. After each step, the program records, for each guest that first holds its seat visit, world.Tick − t0 and the rests the guest holds. It checks for SeatVisits and Rests that unitsCreated equals unitsInTransit plus unitsHeld plus unitsConsumed. It stops 700 ticks after t0, prints one line per guest in name order, `<name> back <tick> rest <rests>`, with `gone` for h1, then `abandoned seat-visits A rests R`, from unitsConsumed with ABANDONED_CAUSE, then `identity held at every tick`, or the first tick and kind where it failed.

Run: `cmake.exe --build --preset windows-release --target tpj_sim && build/seat-service/build.sh servecheck && build/seat-service/servecheck.exe tests/parks/furnished-paths.park`
Expected:

    g1 back 301 rest 1
    g2 back 301 rest 1
    g3 back 2 rest 0
    g4 back 102 rest 0
    g6 back 601 rest 1
    g7 back 421 rest 0
    g8 back 510 rest 0
    h1 gone
    h2 back 301 rest 1
    h3 back 301 rest 1
    h4 back 301 rest 1
    h5 back 2 rest 0
    abandoned seat-visits 1 rests 1
    identity held at every tick

These lines confirm several things. B seats two guests and T four, and the guests with the lowest keys go first. A full piece sends a visit back the next tick. A seat is free again from the tick its guest's visit comes back (g6). A visit that reached a piece in the tick it was removed comes back to its guest with no rest, and the tick goes on (g7). So does a visit still on its way to it (g8). A guest seated there still gets its rest (g6). A seated guest who leaves has its visit and rest consumed as abandoned (h1).

### Task 11: Confirm the slice parks are otherwise unchanged

Files:
- Create, scratch: `build/seat-service/after/`

Step 1: Make the slice parks and diff them with the checked-in files.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && A=build/seat-service/after && mkdir -p $A && cp tests/parks/fed.park $A/fed.park && build/windows-debug/tpj_scenarios.exe --slice-parks $A/fed.park $A/warm.park $A/cut.park tests/parks/main-street.park $A/warm-main-street.park tests/parks/facilities.park $A/stocked.park $A/day.park $A/no-toilet.park $A/noon.park $A/evening.park && for f in fed warm cut warm-main-street stocked day no-toilet noon evening; do diff tests/parks/$f.park $A/$f.park; done | grep '^[<>]' | sed 's/^> [0-9][0-9]* /> KEY /' | sort | uniq -c`
Expected: six lines, in some order, and none starting with `<`: `27 > ` (the blank line after each section), `9 > [seat-places-stepped]`, `9 > [seat-visits-ledger]`, `9 > [seat-offers-stepped]`, `18 > KEY readable=[] pending=[]`, and `9 > KEY packets=[] stocks=[] created=0 consumed=[]`.

### Task 12: Seat guests in the food-shop scenario

Files:
- Modify: `src/scenarios/food_shop.cpp`
- Create, scratch: `build/seat-service/scenariocheck.cpp`

Step 1: In populateFoodShop, after the backstage path, add a guest path, `AddPath{PathKind::Guest, {NewPoint{{-40.0, -20.0}}, NewPoint{{0.0, -20.0}}}}`, clear of every box. Then furnish it: `SetFurnishing{key, Furnishing{benches, benches}}`, with `SideFurnishing benches{FurnitureKind::Bench, 10.0}`, where key is the one guest segment of parkSegments. Add the path and its benches to the function's comment.

Step 2: Add `constexpr double SEAT_CHANCE = 0.5;` beside SPAWN_CHANCE, commented "A new guest visits the furnished path's first bench with this chance, and otherwise a drawn box." In stepGuests, after the new guest is emplaced and before `if (visited.empty())`, if parkFurniture(world) isn't empty and `drawUniform(drawKey(world, key, GUEST_PURPOSE, 4)) < SEAT_CHANCE`, the guest sends one seat visit to the first piece, with the same drawn walk, and returns. Draw the walk before this branch so both branches use it.

Step 3: In stepGuests, count a held SeatVisits unit as a returned visit beside the others, and consume it with FINISHED_CAUSE. Rests are already consumed with RESTED_CAUSE. Include `sim/furniture/furniture.h` and `sim/operations/seats.h`.

Step 4: Write scenariocheck.cpp. It finds "food-shop" in registeredScenarios(), makes its world as runScenario does, and steps it 3,000 times with its queued commands. After each step, for each endpoint of the SeatVisits ledger that holds a unit under its own key, it counts the guest as seated if it holds a rest and as turned away if not. It prints `seated S turned-away T`.

Run: `cmake.exe --build --preset windows-release --target tpj_scenarios_lib && build/seat-service/build.sh scenariocheck && build/seat-service/scenariocheck.exe`
Expected: one line, `seated S turned-away T`, with both S and T above 0.

Step 5: Check that debug and release simulate the scenario alike.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && cmake.exe --build --preset windows-release --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe > build/seat-service/debug.txt && build/windows-release/tpj_scenarios.exe > build/seat-service/release.txt && build/windows-debug/tpj_scenarios.exe --compare build/seat-service/debug.txt build/seat-service/release.txt; echo $?`
Expected: `0`.

### Task 13: Compare the runtime report

Files:
- Create, scratch: `build/seat-service/after.report`

Step 1: Make the report on this feature's build and compare it with the report from seat-places. Main has changed only in plans since that report, so it stands as the baseline.

Run: `scripts/runtime-report.sh build/seat-service/after.report && scripts/runtime-report.sh --compare build/seat-places/after.report build/seat-service/after.report`
Expected: the tick stages within their noise (`unclear`), and full.park's resolution and preview up by at most the seat offers' resolver. `result-changed` marks every hash stage, since new registered types change every world's hash. Report the numbers.

### Task 14: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 15: Commit, merge, and close the milestone

Files:
- Modify: `plans/plausible-operations/served-seats/MILESTONE.md`

Step 1: Stage only this feature's paths: src/sim/medium/field.h, src/sim/medium/SPEC.md, src/sim/operations/, src/sim/CMakeLists.txt, src/scenarios/food_shop.cpp, src/scenarios/SPEC.md, tests/sim/park_world_checks_test.cpp, and plans/plausible-operations/served-seats/seat-service/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Serve guests at furniture seats`. Merge into main as implementing-features says, and rebuild windows-release.

Step 2: Close the milestone as planning-milestones' Closing a milestone says. Check each of MILESTONE.md's criteria by its means and report what was seen, set its Status to complete, and commit MILESTONE.md by pathspec with subject `Plans: Close the served-seats milestone`.
