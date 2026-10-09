# Implementation Plan: Seats and Stalls

## Goal

Benches and toilet blocks seat guests in a fixed number of places for a fixed time, queue the rest at the door, and publish exact rest and toilet offers and records.

## Approach

A new operations component, facilities.h and facilities.cpp, steps each bench and toilet block. Its code is a template over the facility's box kind, run for each kind in the list FACILITIES, with a traits entry naming each kind's visit, serving, and offer kinds, relief, places, and time. A facility keeps its queue of guest keys and the tick each taken place frees. When it seats a guest, it sends the visit and serving back with a delay of the time, so it never holds a seated guest's visit. Its offer's wait walks the guests at the door through the earliest free places. The shop's offerEntries moves to a private header both components use. addOperations calls addFacilities last, so the registration order of addPark doesn't change.

## Placement

Decision 0027 places each behavior this feature adds:

- Seating, queueing, serving, offering, and the record for benches and toilet blocks: operations, src/sim/operations/facilities.h and facilities.cpp, a new component. Operations owns the park's services and their flows and offers. Benches and toilet blocks hold no stock, so they get their own component beside the shop code.
- The facility service state: operations, src/sim/operations/internal/facility_service.h, private as the shop's service state is.
- Publishing an offer at each node anchored to a box: operations, src/sim/operations/internal/offer_entries.h, declared there and defined in operations.cpp. Shops and facilities both publish this way.
- Registering the facilities: addOperations calls addFacilities, so the module still registers through one call.
- Synthetic rest and toilet visits: scenarios, src/scenarios/food_shop.cpp, which owns the food-shop scenario's guests.
- Naming the facility service state as unheld: tests/sim/park_world_checks_test.cpp, UNHELD_TYPES, the list the milestone names.

## Tasks

### Task 1: Update the operations spec

Files:
- Modify: `src/sim/operations/SPEC.md`

Step 1: In the opening paragraph, replace "This module holds operations: the shops and depots that keep the park running behind what the player sees (principle 7)." with "This module holds operations: the shops, depots, benches, and toilet blocks that keep the park running behind what the player sees (principle 7)."

Step 2: In Boundary, after the first list, add:

    addOperations then calls addFacilities, which registers:

    - the flow kinds rest-visits, rests, toilet-visits, and toilet-uses;
    - a facility's service state;
    - the fields rest-offer and toilet-offer and the resolvers that publish them;
    - the system that steps benches and toilet blocks, after the depots.

Step 3: Replace "Every shop box of parkBoxes is a shop, and every depot box is a depot." with "Every shop box of parkBoxes is a shop, and every depot box is a depot. Every bench box is a bench, and every toilet block box is a toilet block. Benches and toilet blocks are facilities." In the same paragraph, replace "Shops and depots read" with "Shops, depots, and facilities read".

Step 4: After the list of what other modules call operations.h for, add: "Other modules call facilities.h for the RestVisits and Rests flow kinds, through which guests visit benches and receive rests, ToiletVisits and ToiletUses, for toilet blocks, the RestOffer and ToiletOffer fields, and facilityRecord."

Step 5: Add a section after Inspection:

    ## Benches and toilet blocks

    A bench has BENCH_SEATS seats and a toilet block has TOILET_STALLS stalls. Its seats or stalls are its places. A facility needs no supplies.

    A facility takes the guests who visit it in the order their visits arrived. Whenever one of its places is free, it gives the place to the guest at the front of its queue. A guest keeps its place for SITTING_TIME on a bench and TOILET_TIME in a toilet block. Then its visit comes back with a rest or a toilet use, and the place is free from that tick. Guests beyond the places wait at the door in arrival order, and none is turned away. Guests visit a bench through rest-visits and get rests, and visit a toilet block through toilet-visits and get toilet uses.

    When a facility is deleted, the guests waiting at its door get their visits back with no serving. The guests in its places still get theirs with a serving. If a visit or serving is sent to a guest who has left, it is consumed as abandoned. Every unit is accounted for, so a facility's servings created always equal the guests it has seated.

    Each bench publishes its offer in the entry field rest-offer, and each toilet block in toilet-offer. It publishes one OfferEntry at its guest anchor, holding REST_RELIEF or TOILET_RELIEF, the ticks the next guest to arrive would wait for a place, and that it has places. The wait is exact if no other visit arrives. If a facility is newly placed, or is in a candidate, it offers a wait of 0 at once, before it has stepped. If a facility has no guest connection, it publishes nothing.

    facilityRecord(world, key) returns a facility's inspection record for display (decision 0025). The record holds its places, how many of them are taken, and how many guests wait at its door. It is computed from the facility's state, not stored, so it adds nothing to a save. For a key that is not a bench or toilet block box, it returns none.

### Task 2: Update the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md:27`

Step 1: Replace the list item "shops selling food and drink, whose supply is cut and restored" with "shops selling food and drink, whose supply is cut and restored, and a bench and a toilet block".

### Task 3: Declare the facility kinds, offers, and record

Files:
- Create: `src/sim/operations/facilities.h`
- Modify: `src/sim/operations/operations.h:58-60`

Step 1: In operations.h, make OfferEntry's comment: "An offer of a need: the need a serving relieves, on a scale from 0 to 1, the ticks the next guest to arrive waits to be taken, and whether the offerer can serve it. A starved shop's offer is OfferEntry{}, which says none."

Step 2: Create facilities.h, guarded TPJ_SIM_OPERATIONS_FACILITIES_H, including sim/entity_key.h, sim/medium/field.h, sim/operations/operations.h, <optional>, <stdint.h>, and <string_view>. Forward-declare World and WorldSchema. Declare, each with its comment:

- RestVisits, "rest-visits": "A guest's visit to a bench, addressed to the guest, which the bench returns to it."
- Rests, "rests": "A rest a bench sends the guest it seated, when its time is up."
- ToiletVisits, "toilet-visits": "A guest's visit to a toilet block, addressed to the guest, which the toilet block returns to it."
- ToiletUses, "toilet-uses": "A toilet use a toilet block sends the guest it took in, when its time is up."
- RestOffer: Entry OfferEntry, Name "rest-offer", Kind FieldKind::Entry. "The rest offer field: each bench's offer, at its guest anchor."
- ToiletOffer: Entry OfferEntry, Name "toilet-offer", Kind FieldKind::Entry. "The toilet offer field: each toilet block's offer, at its guest anchor."
- `inline constexpr int64_t BENCH_SEATS = 3;` "The guests a bench seats at once."
- `inline constexpr uint32_t SITTING_TIME = 300;` "Ticks a guest keeps its seat on a bench."
- `inline constexpr int64_t TOILET_STALLS = 4;` "The guests a toilet block takes in at once."
- `inline constexpr uint32_t TOILET_TIME = 90;` "Ticks a guest keeps its stall in a toilet block."
- `inline constexpr double REST_RELIEF = 0.5;` "The tiredness a rest relieves, on a scale from 0 to 1."
- `inline constexpr double TOILET_RELIEF = 1.0;` "The toilet need a toilet use relieves, on a scale from 0 to 1."
- FacilityRecord with `int64_t Places = 0; int64_t Taken = 0; int64_t Queue = 0;` and a defaulted operator==. "What a bench or toilet block publishes about itself for display and tests (decision 0025): its places, how many are taken, and the guests waiting at its door. Nothing in the park reads it."
- `std::optional<FacilityRecord> facilityRecord(const World &world, EntityKey facility);` "The bench or toilet block box's inspection record in the world as it stands, or none when the key is not one."
- `void addFacilities(WorldSchema &schema);` "Registers the flow kinds rest-visits, rests, toilet-visits, and toilet-uses, the facility-service state, the rest-offer and toilet-offer fields and their resolvers, then the system that steps benches and toilet blocks. addOperations calls it last."

### Task 4: Share the offer's placement

Files:
- Create: `src/sim/operations/internal/offer_entries.h`
- Modify: `src/sim/operations/operations.cpp:441-449`

Step 1: Create offer_entries.h, guarded TPJ_SIM_OPERATIONS_INTERNAL_OFFER_ENTRIES_H, including sim/entity_key.h, sim/medium/field.h, sim/operations/operations.h, and <vector>. Forward-declare World. Declare `std::vector<PlacedEntry<OfferEntry>> offerEntries(const World &world, EntityKey source, const OfferEntry &offer);` with the comment "The source's entries of the offer: one at each node anchored to it on the guest network."

Step 2: In operations.cpp, include it, and move offerEntries out of the anonymous namespace, to just after the anonymous namespace's end, renaming its shop parameter source. Its body stays as it is.

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 5: Seat and serve guests at each facility

Files:
- Create: `src/sim/operations/internal/facility_service.h`
- Create: `src/sim/operations/facilities.cpp`
- Modify: `src/sim/CMakeLists.txt:16`

Step 1: Create facility_service.h, guarded TPJ_SIM_OPERATIONS_INTERNAL_FACILITY_SERVICE_H, in the shape of shop_service.h:

    // State, on a bench or toilet block box's entity: one guest key for each visit unit it holds,
    // waiting at its door in the order it took them in, and the tick each taken place frees,
    // ascending.
    struct FacilityService {
      std::vector<EntityKey> Queue;
      std::vector<uint64_t> FreeAt;
    };

Its visitFields names the fields "queue" and "free-at".

Step 2: Add `operations/facilities.cpp` to src/sim/CMakeLists.txt after `operations/operations.cpp`.

Step 3: In facilities.cpp's anonymous namespace, add a traits template FacilityKinds<BoxKind>, with a specialization for each facility kind naming its Visits, Servings, and Offer kinds, its Relief, its Places, and its Time: RestVisits, Rests, RestOffer, REST_RELIEF, BENCH_SEATS, and SITTING_TIME for BoxKind::Bench, and ToiletVisits, ToiletUses, ToiletOffer, TOILET_RELIEF, TOILET_STALLS, and TOILET_TIME for BoxKind::ToiletBlock. Add `inline constexpr std::array<BoxKind, 2> FACILITIES{BoxKind::Bench, BoxKind::ToiletBlock};` with the comment "Every facility kind, in the order they register and step." Add forEachFacility, which calls `action.template operator()<K>()` for each K of FACILITIES, as operations.cpp's forEachGood does. FacilityKinds and FACILITIES are the only places that name each kind.

Step 4: Add `template <BoxKind K> void serveFacility(World &world, EntityKey facility)`, with the comment "The facility abandons what gone guests left, queues the visits it holds in arrival order, and gives each free place to the guest at the front, sending its visit and serving back after the kind's time." It does, in order:

1. Consume as abandoned every serving of the kind it holds, and every visit of the kind it holds for a guest that is not live.
2. Rebuild the queue from the stored FacilityService, or an empty one: keep each guest's earliest entries up to the visit units it holds for that guest, then append one entry for each further unit, guests in ascending key order. This is the shop's reconcileQueue for one kind.
3. Drop the FreeAt entries at or before world.Tick.
4. While the queue is not empty and FreeAt holds fewer than the kind's Places entries, take the front guest: create one serving for it, send the serving and one visit to the guest with the guest as handle and the kind's Time as delay, and append world.Tick plus Time to FreeAt.
5. Store the queue and FreeAt back in the stored FacilityService, or emplace one when neither is empty.

Use `world.isLive` with a NULL_KEY check for liveness, as operations.cpp's isLive does. Keep the lists the step uses in a buffers struct that stepFacilities makes once and passes in, so the loop allocates only while they grow.

Step 5: Add `void stepFacilities(World &world)`: "Each bench and toilet block, in ascending key order, serves its guests and publishes its offer." It loops over parkBoxes and, for each box, calls the step of the kind of FACILITIES that matches its kind, through forEachFacility.

Task 6 registers stepFacilities, so this task's code builds without an unused-function warning only then. Task 6 builds it.

### Task 6: Offer each facility with an exact wait, and register it

Files:
- Modify: `src/sim/operations/facilities.cpp`
- Modify: `src/sim/operations/operations.cpp:728-747`
- Modify: `src/sim/operations/operations.h:178-182`

Step 1: Add `uint64_t facilityWait(uint64_t from, std::span<const uint64_t> freeAt, size_t queued, int64_t places, uint32_t time)`, with the comment "The ticks the next visit waits, from the cycle stepping from, in which the facility first holds it, to the one that seats its guest, behind the queued guests. Each place is free at its tick in freeAt, or at from when it is not taken. Each queued guest takes the earliest free place, no earlier than from, and keeps it for the time, and the arriving guest takes the earliest place left." Keep the places' free ticks in a fixed array of the largest Places among the kinds, and find the earliest with a scan.

Step 2: In stepFacilities, after serving, each facility publishes its offer with publishStepped of the kind's Offer: offerEntries of OfferEntry{.Relief = Relief, .Wait = facilityWait(world.Tick + 1, FreeAt, the queue's size, Places, Time), .Supplied = true}, from the queue and FreeAt as serveFacility left them.

Step 3: Add `template <BoxKind K> void resolveFacilityOffers(World &world)`, "Publishes the offer of each box of the kind, as for a facility that has not stepped: every place free, so the first guest waits 0.", publishing OfferEntry{Relief, 0, true} through publishResolved and offerEntries.

Step 4: Add `std::optional<FacilityRecord> facilityRecord(const World &world, EntityKey facility)`. It returns none unless the key is a bench or toilet block box of parkBoxes. Places is the kind's Places. Taken counts the stored FreeAt entries after world.Tick. Queue sums the visit units of the kind held for live guests.

Step 5: Add `void addFacilities(WorldSchema &schema)`. Through forEachFacility, register each kind's Visits and Servings flows. Register FacilityService as "facility-service", DataKind::State. Through forEachFacility, add each kind's Offer field, and its resolver named for the field, "rest-offer" and "toilet-offer", with resolveFacilityOffers and the dependencies path-networks and "<offer name>-field". Then add the system stepFacilities.

Step 6: In operations.cpp, include sim/operations/facilities.h and call addFacilities(schema) at the end of addOperations. In operations.h, end addOperations' comment with "and last calls addFacilities."

Step 7: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim 2>&1 | grep -c "warning:\|error:"`
Expected: `0`

### Task 7: Name the facility service state as unheld

Files:
- Modify: `tests/sim/park_world_checks_test.cpp:51-55`

Step 1: Add to UNHELD_TYPES:

    UnheldType{"facility-service",
               "no checked park holds a guest that visits a bench or toilet block until "
               "a-day-out's parks do"},

### Task 8: Send synthetic rest and toilet visits

Files:
- Modify: `src/scenarios/food_shop.cpp`

Step 1: In populateFoodShop, after the second shop, add `AddBox{BoxKind::Bench, Pose{-40.0, 45.0, 0.0, 1.0}}` and `AddBox{BoxKind::ToiletBlock, Pose{-60.0, 60.0, 0.0, 1.0}}`. The bench's footprint, x -41 to -39 and z 44.5 to 45.5, and the toilet block's, x -64 to -56 and z 57.5 to 62.5, clear the second shop's, x -44 to -36 and z 57 to 63, and every path. Update its comment to name the bench and toilet block.

Step 2: In stepGuests, when a guest's visit has come back, also count its RestVisits and ToiletVisits. Consume each kind of visit it holds as finished, its rests with a new local cause RESTED_CAUSE, "rested", and its toilet uses with a new local cause USED_CAUSE, "used". Collect every box but depots, not only shops, in key order, and pick among them as now. For a shop, send food or drink as now. For a bench, send RestVisits, and for a toilet block, ToiletVisits. Update the step's comment to say guests also visit benches and toilet blocks.

Step 3: Build and run the scenarios.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios 2>&1 | grep -c "warning:\|error:"; build/windows-debug/tpj_scenarios.exe > /dev/null 2>&1; echo $?`
Expected: `0` and then `0`

### Task 9: Remake the parks

Files:
- Modify: `tests/parks/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`, `stocked.park`, `stress/full.park`

Step 1: Remake the slice parks.

Run: `build/windows-debug/tpj_scenarios.exe --slice-parks tests/parks/fed.park tests/parks/warm.park tests/parks/cut.park tests/parks/main-street.park tests/parks/warm-main-street.park tests/parks/facilities.park tests/parks/stocked.park; echo $?`
Expected: `0`

Step 2: Remake the full park. It takes about four minutes.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench > /dev/null 2>&1; build/windows-debug/tpj_bench.exe --full-park tests/parks/stress/full.park; echo $?`
Expected: `0`

### Task 10: Check the remade parks hold the new sections

Files: none.

Step 1: Show stocked.park's rest and toilet offers.

Run: `grep -A1 "offer-stepped" tests/parks/stocked.park | grep -o "source=[0-9]* \|relief=[0-9.]* wait=[0-9]* supplied=[a-z]*"`
Expected: the six lines it gives today, `source=8`, `relief=0.5 wait=0 supplied=true`, `source=7`, `relief=0.5 wait=375 supplied=true`, `source=8`, `relief=0.5 wait=0 supplied=true`, then `source=5`, `relief=0.5 wait=0 supplied=true`, `source=6`, and `relief=1 wait=0 supplied=true`.

### Task 11: Check that every park simulates as before

Files: none.

Step 1: Count the lines the remade parks add and remove.

Run: `git diff --numstat -- tests/parks`
Expected: `18	0` for each of tests/parks/cut.park, fed.park, stocked.park, stress/full.park, warm-main-street.park, and warm.park.

Step 2: List the added lines, with entity keys replaced.

Run: `git diff -U0 -- tests/parks | grep '^+' | grep -v '^+++' | sed 's/^+[0-9]* /+KEY /' | sed 's/readable=\[{.*/readable=ENTRIES/' | sort | uniq -c`
Expected: 36 blank `+` lines, 6 each of `+[rest-offer-stepped]`, `+[rest-visits-ledger]`, `+[rests-ledger]`, `+[toilet-offer-stepped]`, `+[toilet-uses-ledger]`, and `+[toilet-visits-ledger]`, 24 of `+KEY packets=[] stocks=[] created=0 consumed=[]`, 10 of `+KEY readable=[] pending=[]`, and 2 of `+KEY readable=ENTRIES`.

### Task 12: Check seating, offers, deletion, and conservation

Files:
- Create: `build/predict-check/seats_check.cpp` (a scratch program, not committed)

Step 1: Write a program in the shape of build/predict-check/two_goods_check.cpp. It loads tests/parks/stocked.park with a schema made by addPark plus its own guest component and a system after addPark's. The system sends the visits each check calls for, each with delay 1, and records the cycle each guest first holds its returned visit and whether it holds a serving then. Call the bench F and the toilet block T. Read an offer as guests.cpp does: sourceEntryAtNode at the box's firstAnchoredNode on the guest network. The offer readable in the cycle a facility first holds a visit is the one it published the cycle before. Call the cycle the facility first holds the first visits h.

Each check starts from a fresh load and prints its line only if it passes, and otherwise what it saw:

- `bench seats and queues`: five rest visits reach F in cycle h. The first three come back with a rest at h + SITTING_TIME, the other two at h + 2 * SITTING_TIME, in ascending key order of their guests.
- `toilet block stalls and queues`: six toilet visits reach T in cycle h. The first four come back with a toilet use at h + TOILET_TIME, and the other two at h + 2 * TOILET_TIME.
- `rest wait exact at the door`: as in the bench check, and a sixth rest visit reaches F in cycle h + 1. It comes back at h + 1 + wait + SITTING_TIME, where wait is the rest offer readable at h + 1, and that wait is 299.
- `toilet wait exact at the door`: as in the toilet check, and a seventh visit reaches T in cycle h + 1. It comes back at h + 1 + wait + TOILET_TIME, and the wait is 89.
- `resolved offers at once`: loading tests/parks/facilities.park, which has never stepped, F's rest offer is REST_RELIEF, wait 0, supplied, and T's toilet offer is TOILET_RELIEF, wait 0, supplied.
- `deleted bench returned every visit`: five rest visits reach F in cycle h, and a DeleteBox of F is queued for the next cycle. All five guests get their visits back: the three seated with a rest, the two at the door without one.
- `deleted toilet block returned every visit`: the same with six toilet visits to T: four with a toilet use, two without.
- `conserved`: in every cycle of every check, each of the twelve kinds of operations has units created equal to units in transit plus units held by every entity, counting a just-deleted entity's stock, plus units consumed.
- the records: the rest wait check keeps F's facilityRecord after the step of cycle h + 1, and the toilet wait check keeps T's. After the eight check lines, the program prints F's record, then T's, then whether facilityRecord gives none for stocked.park's shop 8 and depot 10.

Step 2: Build and run it.

Run: `cd build/predict-check && /mnt/c/msys64/ucrt64/bin/g++.exe -I../../src -std=c++20 -ffp-contract=off -O0 seats_check.cpp ../windows-debug/src/sim/libtpj_sim.a -o seats_check.exe && ./seats_check.exe ../../tests/parks/stocked.park ../../tests/parks/facilities.park`
Expected: the eight check lines in the order above, then F's record, places 3, taken 3, queue 3, then T's, places 4, taken 4, queue 3, then `no record for a shop or depot`.

### Task 13: Run the full checks

Files: none.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -1`
Expected: `0`, then `100% tests passed`, then `tidy: clean.`

### Task 14: Commit

Step 1: Stage only this feature's paths: the files listed in FEATURE.md's Files affected and this feature's plans directory. Commit with the commit-hygiene skill, subject "Operations: Seat guests on benches and in toilet blocks".
