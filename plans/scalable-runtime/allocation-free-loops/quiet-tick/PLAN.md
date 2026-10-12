# Implementation Plan: Quiet Tick

## Goal

Build the allocation check of decision 0041 and make a cycle with no commands on full.park free no block it allocated, with the simulation unchanged.

## Approach

Systems and swaps take a TickBuffers that the stepping caller keeps, and each module asks it for a private struct of the lists it reuses. The world's stores and entity map keep extracted map nodes as spares for later adds. Seat service holds its free seats in a FixedList that walks and saves like a vector. Stepped fields keep retired slots. The tick reads intent, anchors, field samples, and stocks through forms that fill kept lists. The check replaces operator new and delete in its own executable and records blocks allocated during watched ticks in a fixed table. It steps full.park through ParkSession.

## Placement

Decision 0027 places each behavior this feature adds:

- Buffers kept from tick to tick: sim, sim/tick_buffers.h (unit sim/*). They are part of the cycle's contract, as the command queue is, and outside code needs them to step.
- Systems and swaps taking buffers, and stepWorld passing them: sim, sim/schema.h and sim/cycle.cpp, which own registration and the cycle.
- A list with a fixed capacity, walked, saved, and loaded as a vector: sim, sim/fixed_list.h. The walk in sim/schema.h and the text in sim/field_text.h own how fields are written.
- Reusing removed nodes, keysWith into a list, and forEachWith: sim, TypedStore in sim/schema.h and World in sim/world.h and sim/world.cpp, which own the storage.
- anchoredNodes into a list: medium, Network in sim/medium/network.h, which owns anchors.
- Stepped fields keeping retired slots, publishStepped giving a list to fill, ordering by place without a temporary buffer, and FieldSample: medium, sim/medium/field.h, which owns fields. sim/medium/kept_field.h follows the slot order's new form.
- The ledger swap's kept lists: medium, sim/medium/flow.h and flow.cpp, which own flows.
- parkEntrances and parkBoxes into a list, and parkHours without allocating: park, sim/park/intent.h and intent.cpp, which own intent listings.
- Each system's kept lists: the system's own module, as a private struct in its .cpp: guests.cpp, footfall.cpp, operations.cpp, facilities.cpp, seats.cpp. Offer entries filled into a list: operations, internal/offer_entries.h, beside offerEntries.
- Seat service's free seats as a FixedList: operations, internal/seat_service.h. The most seats of any furniture kind: furniture, furniture.h, beside furnitureSeats.
- The app keeping its buffers: app/session, ParkSession, the one place the app steps its world.
- tpj_bench, tpj_scenarios, and the park generators keeping their buffers: each runner's own loop in src/bench/stages.cpp and src/scenarios.
- The allocation check: tests/checks/allocations. watch.h and watch.cpp replace operator new and delete and keep the table. allocation_check.cpp holds the work it repeats. Placing tests/checks above every layer: cmake/check_layers.cmake, as it places tests/integration.

## Tasks

### Task 1: Record the simulation before the change

Files: none.

Step 1: Build tpj_scenarios on main and record its output.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-tick-before.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-tick-full-before.txt`
Expected: exit status 0.

### Task 2: Update the specs

Files:
- Modify: `src/sim/SPEC.md`, `src/sim/medium/SPEC.md`, `src/sim/park/SPEC.md`

Step 1: Make each edit FEATURE.md's Spec changes lists, with its sentences as written there.

### Task 3: Add TickBuffers

Files:
- Create: `src/sim/tick_buffers.h`

Step 1: Declare, in namespace tpj:

    // The lists systems and swaps reuse from tick to tick, owned by whoever steps the world. Each is
    // a value of a type a module defines, made on first use and kept. A TickBuffers is never part of
    // a world: walks, hashes, saves, and copies never see it.
    class TickBuffers {
    public:
      TickBuffers() = default;
      TickBuffers(const TickBuffers &) = delete;
      TickBuffers &operator=(const TickBuffers &) = delete;
      TickBuffers(TickBuffers &&) noexcept = default;
      TickBuffers &operator=(TickBuffers &&) noexcept = default;
      ~TickBuffers() = default;

      // The kept T, default constructed on first use. Clear a list before reading it.
      template <typename T> T &get();

    private:
      struct Held {
        TypeId Id = nullptr;
        std::unique_ptr<void, void (*)(void *)> Value{nullptr, nullptr};
      };
      std::vector<Held> Values;
    };

Define get inline below the class: search Values linearly for typeIdOf<T>(); if none holds it, push_back a Held with a new T and a deleter that deletes it as a T. Include sim/type_id.h, memory, and vector.

### Task 4: Pass the buffers to systems and swaps

Files:
- Modify: `src/sim/schema.h`, `src/sim/schema.cpp`, `src/sim/world.h`, `src/sim/cycle.cpp`
- Modify: every system and swap: `src/sim/guests/guests.cpp` (stepGuests), `src/sim/guests/footfall.cpp` (stepFootfall), `src/sim/operations/operations.cpp` (stepShops, stepDepots), `src/sim/operations/facilities.cpp` (stepFacilities), `src/sim/operations/seats.cpp` (stepSeats), `src/sim/medium/field.h` (swapSteppedEntries), `src/sim/medium/ground_field.h` (swapGroundEntries), `src/sim/medium/kept_field.h` (applyKeptChanges), `src/sim/medium/flow.h` (swapLedger)
- Modify: `src/scenarios/beacons.cpp`, `src/scenarios/food_shop.cpp`, `src/scenarios/walkers.cpp`, `src/scenarios/stalls.cpp`, `tests/sim/cycle_test.cpp`, `tests/sim/save_test.cpp`

Step 1: In schema.h, forward-declare TickBuffers. Add `using StepFunction = void (*)(World &world, TickBuffers &buffers);` with the comment "A system or swap. It takes the world and the stepping caller's buffers. It holds no state outside the world, and clears a buffer before reading it, so its result depends on the world alone (principle 10)." Change WorldFunction's comment to name resolvers and finishers only. addSystem and addSwap take a StepFunction, and systems() and swaps() return `const std::vector<StepFunction> &`. Change Systems and Swaps in schema.h and the two functions in schema.cpp to match.

Step 2: In world.h, include sim/tick_buffers.h. Declare `void stepWorld(World &world, CommandQueue &commands, TickBuffers &buffers);` with the comment "One cycle, passing the buffers to every system and swap: resolve if pending, step the systems, advance Tick, run the swaps, apply the queued commands in submission order, and resolve if pending. Empties the queue. Throws std::invalid_argument, changing nothing, if a queued command's type is not registered." Make it the friend in place of the two-argument form. Add `void stepWorld(World &world, TickBuffers &buffers);` ("One cycle with no commands."). Keep the two other forms, with the comment "One cycle with buffers of its own, for callers that step once."

Step 3: In cycle.cpp, move the cycle into the three-argument stepWorld, calling `step(world, buffers)` and `swap(world, buffers)`. Define the others through it: the no-command forms with a local empty CommandQueue, and the bufferless forms with a local TickBuffers.

Step 4: Give every system and swap listed above a second parameter `TickBuffers &buffers`. Name it `/*buffers*/` where the function doesn't use it yet. In the scenarios and tests, add the parameter to each registered function and lambda in the same way, and change nothing else.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 5: Keep the buffers across ticks

Files:
- Modify: `src/app/session/park_session.h`, `src/app/session/park_session.cpp`, `src/bench/stages.cpp`, `src/scenarios/runner.cpp`, `src/scenarios/day_parks.cpp`, `src/scenarios/full_park.cpp`, `src/scenarios/slice_parks.cpp`

Step 1: ParkSession gets a member `TickBuffers Buffers;` after Commands, with the comment "The lists the world's systems reuse from tick to tick. Replacing the world keeps them." ParkSession::step passes it to both of its stepWorld calls. startingPark keeps a local TickBuffers before its loop and steps with `stepWorld(*opened.Park, buffers)`.

Step 2: benchPark's ticks stage (stages.cpp, the loop over ticks) keeps a TickBuffers declared before the loop and steps with `stepWorld(stepped, buffers)`.

Step 3: In runner.cpp, runScenario declares its CommandQueue and a TickBuffers before its loop, and steps with `stepWorld(world, commands, buffers)`. runSave keeps a TickBuffers and steps with `stepWorld(world, buffers)`.

Step 4: Each generator loop keeps one TickBuffers for the whole park it makes: makeDayParks' two loops (day_parks.cpp:188-194) share one, and makeFullPark (full_park.cpp:178), makeSliceParks' warm loop and its cut cycle, makeWarmPark, and makeStockedPark (slice_parks.cpp) each keep one.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 6: Place tests/checks above every layer

Files:
- Modify: `cmake/check_layers.cmake`

Step 1: In check_layers.cmake, place a file under tests/checks as one under tests/integration is placed: label `tests/checks`, layer INTEGRATION_LAYER, and may_include allows it every header. Rename INTEGRATION_LAYER to TOP_LAYER. In the header comment, say that tests/integration and tests/checks sit above every layer and are written as such.

Step 2: Leave the layer check's tests as they are. Task 7's files under tests/checks/allocations include sim and app headers, so "layer check passes on the tree" fails without this change and passes with it; task 7 runs it.

### Task 7: Add the allocation check

Files:
- Create: `tests/checks/allocations/watch.h`, `tests/checks/allocations/watch.cpp`, `tests/checks/allocations/allocation_check.cpp`, `tests/checks/allocations/CMakeLists.txt`
- Modify: `tests/checks/CMakeLists.txt`

Step 1: watch.h declares, in namespace tpj::check:

    // A block allocated and freed while watching, with the ticks the watch was at for each.
    struct FreedBlock {
      size_t Size = 0;
      uint64_t Allocated = 0;
      uint64_t Freed = 0;
    };

    inline constexpr size_t REPORTED_BLOCKS = 20;

    // Starts watching at the tick: every block allocated from now is noted. Forgets earlier findings.
    void startWatching(uint64_t tick);
    // The tick the next allocations and frees are counted at.
    void watchTick(uint64_t tick);
    void stopWatching();
    // How many noted blocks were freed while watching, and the first REPORTED_BLOCKS of them.
    size_t freedCount();
    std::span<const FreedBlock> freedBlocks();
    // True when more blocks were noted at once than the table holds.
    bool tableOverflowed();
    // Called at each noted block freed while watching. Stop here in a debugger to see who frees it.
    void blockFreed(const FreedBlock &block);

Step 2: watch.cpp holds, in static storage only, an open-addressed table of TRACKED_BLOCKS = 1 << 16 entries (address, size, tick), probed linearly from a hash of the address and emptied by backward-shift deletion. It also holds the FreedBlock array, the counts, the watching flag, and the tick. It replaces every global operator new and new[] (plain, nothrow, aligned, aligned nothrow) and every operator delete and delete[] (plain, sized, aligned, sized aligned, nothrow). The new forms allocate with std::malloc, or for aligned forms _aligned_malloc on _WIN32 and std::aligned_alloc elsewhere, rounding the size up to the alignment. The throwing forms throw std::bad_alloc when that fails. While watching, a new form notes the block and its size at the current tick, or sets the overflow flag when the table is full. A delete form looks the address up first. If it was noted, it removes it, counts it, stores it while fewer than REPORTED_BLOCKS are stored, and calls blockFreed. Then it frees with std::free, or _aligned_free for aligned forms on _WIN32. Nothing in watch.cpp allocates. Mark blockFreed `[[gnu::noinline]]` and give it an empty body.

Step 3: allocation_check.cpp's main reads `TPJ_PARKS_DIR "/stress/full.park"` into a string, loads it with loadWorld(makeParkSchema(), text), resolves it, and makes a ParkSession of it. It steps WARM_TICKS = 300 times, then calls startWatching with the world's Tick. It steps WATCHED_TICKS = 600 times, twice SEAT_TIME, so that a seat service can start and end within them when its piece empties, calling watchTick with the world's Tick before each step, then calls stopWatching. If tableOverflowed, it prints `allocation check: the watched ticks noted more blocks than the table holds` and returns 1. If freedCount is above 0, it prints `allocation check: <count> blocks were allocated and freed during the watched ticks`, then one line per stored block, `allocation check: tick freed a <n>-byte block at tick <f> that it allocated at tick <a>`, and returns 1. Otherwise it prints `allocation check: 600 ticks of full.park freed no block they allocated` and returns 0. If reading or loading fails, it prints the reason and returns 2. Comment each constant with what it is for.

Step 4: tests/checks/allocations/CMakeLists.txt:

    # The allocation check (decision 0041). It is an executable of its own because it replaces
    # operator new and delete for the whole program.
    add_executable(tpj_allocation_check allocation_check.cpp watch.cpp)
    target_link_libraries(tpj_allocation_check PRIVATE tpj_app_core tpj_sim)
    target_compile_definitions(tpj_allocation_check PRIVATE
        TPJ_PARKS_DIR="${CMAKE_SOURCE_DIR}/tests/parks")
    tpj_configure_target(tpj_allocation_check)
    add_test(NAME "allocation check: ticks of full.park free no block they allocated"
        COMMAND tpj_allocation_check)
    # A run past TIMEOUT seconds has hung.
    set_tests_properties("allocation check: ticks of full.park free no block they allocated"
        PROPERTIES TIMEOUT 1800)

Add `add_subdirectory(allocations)` at the end of tests/checks/CMakeLists.txt.

Run: `cmake.exe --preset windows-debug && cmake.exe --build --preset windows-debug --target tpj_allocation_check`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines naming freed blocks, since the tick still allocates.

Run: `ctest.exe --preset windows-debug -R "layer check passes on the tree"`
Expected: the test passes.

### Task 8: Hold seat service's free seats in a FixedList

Files:
- Create: `src/sim/fixed_list.h`
- Modify: `src/sim/field_text.h`, `src/sim/schema.h`, `src/sim/furniture/furniture.h`, `src/sim/operations/internal/seat_service.h`, `src/sim/operations/seats.cpp`

Step 1: fixed_list.h declares `template <typename T, size_t N> class FixedList` holding a std::array<T, N> and a size: size, empty, capacity, begin and end, operator[], front, back, push_back, emplace_back, clear, and erase(first, last). push_back and emplace_back throw std::length_error when it is full. Default comparison isn't needed.

Step 2: In field_text.h, add a trait IsList, true for std::vector and FixedList, and use it in writeValue and readValue in place of IsVector. Before readValue's emplace_back, if the list's size has reached a FixedList's capacity, call `cursor.fail("'" + std::string(name) + "' holds more values than it can")`. In schema.h, use IsList in emitField's list branch.

Step 3: In furniture.h, add `inline constexpr uint32_t MOST_FURNITURE_SEATS = 4;` ("The most seats any furniture kind has.") and static_assert that furnitureSeats of each kind is at most it. SeatService's FreeAt becomes `FixedList<uint64_t, MOST_FURNITURE_SEATS>`.

Step 4: In seats.cpp, SeatBuffers.FreeAt becomes the same FixedList, servePiece's freeAt reference takes that type, and its std::erase_if becomes erase with std::remove_if. stepSeats takes `SeatBuffers &kept = buffers.get<SeatBuffers>();` and clears Pieces before filling it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe "[#save_test],[#park_world_checks_test]"`
Expected: all tests pass.

### Task 9: Reuse the world's removed nodes

Files:
- Modify: `src/sim/schema.h`, `src/sim/world.h`, `src/sim/world.cpp`

Step 1: TypedStore gains `std::vector<typename std::map<EntityKey, T>::node_type> Spare;` ("Nodes of removed components, kept for later adds. Never copied, walked, or saved.") and `template <typename... Args> T &add(EntityKey key, Args &&...args)`. add requires the key to hold no T. It takes the last spare if there is one, sets its key and its value to `T(std::forward<Args>(args)...)`, and inserts it. Otherwise it try_emplaces. erase extracts the key's node and keeps it in Spare when there was one.

Step 2: World::emplace adds through add when the key holds no T, as try_emplace did. emplaceOrReplace assigns when the key holds T and adds otherwise. getOrEmplace finds, or adds with no arguments. remove calls the store's erase.

Step 3: World gains `std::vector<std::map<EntityKey, EntityRecord>::node_type> SpareRecords;` with the same comment. addEntity reuses a spare record when there is one, and destroyEntity extracts the record into SpareRecords.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe`
Expected: all tests pass.

### Task 10: Read keys and intent into kept lists

Files:
- Modify: `src/sim/world.h`, `src/sim/park/intent.h`, `src/sim/park/intent.cpp`, `src/sim/medium/network.h`, `src/sim/medium/network.cpp`

Step 1: World gains `template <typename T> void keysWith(std::vector<EntityKey> &keys) const;` and `template <typename T, typename Visit> void forEachWith(Visit &&visit) const;`, with the spec's sentences as comments.

Step 2: intent.h gains `void parkEntrances(const World &world, std::vector<ParkEntrance> &entrances);` and `void parkBoxes(const World &world, std::vector<ParkBox> &boxes);`, each "As the returning form, into a list the caller keeps, which it clears first." intent.cpp fills them through forEachWith, and the returning forms call them. parkHours reads the first OpeningHoursIntent through forEachWith.

Step 3: Network gains `void anchoredNodes(EntityKey entity, std::vector<uint32_t> &nodes) const;` ("The nodes anchored to the entity, ascending, into a list the caller keeps, which it clears first."), and the returning form calls it.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 11: Keep stepped slots and order by place without a temporary buffer

Files:
- Modify: `src/sim/medium/field.h`, `src/sim/medium/kept_field.h`, `src/scenarios/stalls.cpp`, `src/sim/operations/operations.cpp`, `src/sim/operations/facilities.cpp`

Step 1: FieldSlot's and KeptSlot's ByPlace become `mutable std::vector<uint32_t> ByPlace;` and `mutable bool Ordered = false;`, keeping the comment that they are never visited. orderItemsByPlace fills a list it is given, clearing it first, and sorts with std::ranges::sort, comparing by placeBefore and then by position. orderByPlace calls it. entriesByPlace orders the slot when it isn't Ordered and sets Ordered. In kept_field.h, applyKeptChanges inserts into ByPlace only when the slot is Ordered, and replaceKeptEntries fills a local order with orderItemsByPlace, moves it into ByPlace, and sets Ordered.

Step 2: SteppedEntries gains `std::vector<FieldSlot<typename F::Entry>> Retired;` ("Slots a swap took out of Readable, their lists emptied, kept so that the next publishes reuse their memory. Never visited, so walks, saves, hashes, and comparisons never see them."). swapSteppedEntries clears each Readable slot's Entries and ByPlace, clears its Ordered, and moves it to the end of Retired. It then clears Readable and swaps Readable with Pending.

Step 3: publishStepped becomes `template <FieldDefinition F> std::vector<PlacedEntry<typename F::Entry>> &publishStepped(World &world, EntityKey source);`, refusing as it does now. It takes the last Retired slot, or a new one, sets its Source, inserts it in Pending in source order, and returns its Entries, empty. Its comment adds that the list stays valid until the next publish into the field. In stalls.cpp, push each entry into the returned list. In operations.cpp's publishOffer and facilities.cpp's publishFacilityOffer, assign offerEntries' result to the returned list until tasks 16 and 17 fill it in place.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios && build/windows-debug/tpj_sim_tests.exe`
Expected: the build prints no warning, and all tests pass.

### Task 12: Sample a field into kept lists

Files:
- Modify: `src/sim/medium/field.h`

Step 1: Add:

    // The lists sampleField reuses, and the entries it gives. Keep one to sample the same field
    // many times without allocating once its lists have grown.
    template <typename Entry> struct FieldSample {
      std::vector<const FieldSlot<Entry> *> Slots;
      SampleScratch<Entry> Scratch;
      SampleRun<Entry> Run;
      std::vector<SampledEntry<Entry>> Single;
      std::vector<SampledEntry<Entry>> Entries;
    };

and `template <FieldDefinition F> void sampleField(const World &world, const Network &network, const Place &place, FieldSample<typename F::Entry> &sample);`, which static_asserts that F isn't a kept field and fills sample.Entries with what the returning form gives.

Step 2: layeredSlots gains a form that fills a list it clears first, and the returning form calls it. sampleSlotsAt gains a form that takes the scratch, run, and single lists, and the existing form calls it with locals. The new sampleField fills sample.Slots, then samples the one place through them with the sample's lists, appending to sample.Entries after clearing it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 13: Keep the ledger swap's lists

Files:
- Modify: `src/sim/medium/flow.h`, `src/sim/medium/flow.cpp`

Step 1: Declare `struct LedgerSwapLists { std::vector<FlowPacket> Arriving; std::vector<FlowStock> Orphaned; };` ("The lists ledgerSwap reuses."). ledgerSwap takes a `LedgerSwapLists &lists` and assigns the due packets into Arriving and pushes orphaned stocks into Orphaned after clearing each. swapLedger passes `buffers.get<LedgerSwapLists>()`.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 14: Keep the guests' lists

Files:
- Modify: `src/sim/guests/guests.h`, `src/sim/guests/guests.cpp`

Step 1: ScoredOptions moves above WalkLists. WalkLists gains `FieldSample<RouteEntry> Routes;`, `ScoredOptions Scored;`, and `std::vector<double> ChoiceWeights;` (named apart from the wander picks' Weights). Add a private struct GuestTickLists holding `std::vector<EntityKey> Guests;`, `std::vector<EntityKey> Leaving;`, `std::vector<ParkEntrance> EntranceValues;`, `std::vector<EntityKey> Entrances;`, and `WalkLists Walk;`.

Step 2: ParkMoment's Entrances becomes `std::span<const EntityKey>`, and homeEntry, headingOf, and traceGuestRoute take the entrances as that span in place of a `const std::vector<EntityKey> &`. parkMoment takes the two entrance lists and fills them through parkEntrances' filling form. guestOptions keeps local lists for parkMoment, scoreOptions, and softmaxWeights.

Step 3: scoreOptions fills a ScoredOptions it is given, clearing its three lists first. softmaxWeights fills a weights list it clears first. softmaxPick in guests.h takes a `std::vector<double> &weights` third, with its comment saying it fills them. choose samples route distance with the new sampleField into lists.Routes and reads lists.Routes.Entries, and scores into lists.Scored with lists.ChoiceWeights.

Step 4: admitGuests takes the moment's entrance keys and uses network.firstAnchoredNode for each, admitting at its nodePlace when it has one. This is the node anchoredNodes gave first, since anchors are kept in node order.

Step 5: stepGuests takes `GuestTickLists &lists = buffers.get<GuestTickLists>();`. It fills lists.Guests with keysWith<Guest>, clears and fills lists.Leaving, and passes lists.Walk where it passed its local WalkLists.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe "[#park_world_checks_test]"`
Expected: all tests pass.

### Task 15: Keep footfall's list

Files:
- Modify: `src/sim/guests/footfall.cpp`

Step 1: GuestOnStretch gains `size_t Order = 0;`, the guest's position in key order. stepFootfall takes a kept `std::vector<GuestOnStretch>` from the buffers through a private struct, clears it, fills it through `world.forEachWith<Guest>`, and sorts it with std::ranges::sort by Stretch and then Order.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build finishes with exit status 0 and prints no warning.

### Task 16: Keep the shops' and depots' lists

Files:
- Modify: `src/sim/operations/operations.cpp`, `src/sim/operations/internal/offer_entries.h`

Step 1: offer_entries.h gains `void fillOfferEntries(const World &world, EntityKey source, const OfferEntry &offer, std::vector<uint32_t> &nodes, std::vector<PlacedEntry<OfferEntry>> &entries);`, which appends an entry at each node anchored to the source on the guest network, using nodes as its list. offerEntries calls it.

Step 2: Add a private struct RouteLengths holding `std::vector<uint32_t> Nodes;`, `FieldSample<RouteEntry> Sample;`, and `std::vector<std::pair<EntityKey, double>> Lengths;`. Add routeLengthsAt, routeLengthBy, depotRouteBy, and deliveryDelay forms that read both layers into a RouteLengths, with depotRouteBy taking the depots' keys. The existing forms stay for resolvers and the public functions.

Step 3: ShopBuffers gains `std::vector<ParkBox> Boxes;`, `RouteLengths Lengths;`, `std::vector<FlowStock> Held;`, and `std::vector<uint32_t> Nodes;`. stepShops takes `ShopBuffers &kept = buffers.get<ShopBuffers>();`. It fills Boxes with parkBoxes' filling form and Depots from Boxes. It finds each shop's route and delivery delay through the RouteLengths forms. abandonGone copies each stocksAt span into Held before consuming from it. publishOffer fills the list publishStepped gives with fillOfferEntries.

Step 4: Add a private struct DepotBuffers holding Boxes, Shops, Depots, Held, and Lengths. stepDepots takes it from the buffers, and shipGood copies the stocks into Held before consuming, and reads route lengths through Lengths.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe "[#park_world_checks_test]"`
Expected: all tests pass.

### Task 17: Keep the facilities' lists

Files:
- Modify: `src/sim/operations/facilities.cpp`

Step 1: FacilityBuffers gains `std::vector<ParkBox> Boxes;`, `std::vector<FlowStock> Held;`, and `std::vector<uint32_t> Nodes;`. stepFacilities takes it from the buffers and fills Boxes with parkBoxes' filling form. abandonGone copies each stocksAt span into Held before consuming from it. publishFacilityOffer fills the list publishStepped gives with fillOfferEntries.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park freed no block they allocated`.

### Task 18: Make the check pass

Files: whichever the reports name.

Step 1: If the check still reports blocks, run it under gdb.exe with a breakpoint on tpj::check::blockFreed to see where each block is allocated and freed. Fix each one in the same way as tasks 8 to 17, with a list kept in the buffers or in the world's storage. If a reported block is a kept list growing a second time during the watched ticks, raise WARM_TICKS until no list does. Record the warm-up and the check's running time in FEATURE.md's Open questions.

Run: `build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 0, printing `allocation check: 600 ticks of full.park freed no block they allocated`.

### Task 19: Prove the check fails

Files:
- Modify, then revert: `src/sim/guests/guests.cpp`

Step 1: Inside stepGuests' loop over guests, add `const std::vector<EntityKey> held{key};` and `static_cast<void>(held);`.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines of the form `allocation check: tick freed a 8-byte block at tick <f> that it allocated at tick <a>`.

Step 2: Revert the two lines.

Step 3: In src/sim/schema.h, make TypedStore's erase call `Items.erase(key)` instead of keeping the node.

Run: `cmake.exe --build --preset windows-debug --target tpj_allocation_check && build/windows-debug/tpj_allocation_check.exe`
Expected: exit status 1, with lines naming blocks freed in the watched ticks. This failure is the evidence that some seat service starts and ends within the watched ticks. If it exits with status 0, none does: raise WATCHED_TICKS until this step fails, and record the length in FEATURE.md.

Step 4: Revert the change to erase.

### Task 20: Check the runners

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_bench && build/windows-debug/tpj_bench.exe --ticks 30 tests/parks/stress/winding-path.park`
Expected: a park line, then one stage line for each stage, ending with the ticks stage.

### Task 21: Show the simulation unchanged

Files: none.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && build/windows-debug/tpj_scenarios.exe tests/parks/*.park > build/quiet-tick-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-tick-before.txt build/quiet-tick-after.txt`
Expected: exit status 0.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/stress/full.park > build/quiet-tick-full-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/quiet-tick-full-before.txt build/quiet-tick-full-after.txt`
Expected: exit status 0.

### Task 22: Confirm the build, tests, and tidy

Files: none.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build finishes with exit status 0 and prints no warning.

Run: `ctest.exe --preset windows-debug`
Expected: 100% tests passed, including "allocation check: ticks of full.park free no block they allocated".

Run: `scripts/tidy.sh`
Expected: exit status 0 with no warnings.

### Task 23: Commit

Step 1: Stage the feature's paths by name and commit once with the commit-hygiene skill, subject `Sim: Check the tick for allocations and keep its memory`.
