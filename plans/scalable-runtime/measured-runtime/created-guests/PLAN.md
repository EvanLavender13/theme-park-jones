# Implementation Plan: Created Guests

## Goal

Add addGuest to the guests module, make arrivals create their guests through it with every draw unchanged, and show with a runtime report before and after that no stage's result changed.

## Approach

addGuest checks the place against the guest network, creates the entity, and draws the hunger rate and starting hunger on the new key with the same expressions admitGuests uses today. admitGuests draws the stay on world.nextKey(), the key addGuest is about to create, and calls addGuest, so every arriving guest is bit for bit what it was (created-guests/RESEARCH.md). A runtime report made before any edit and one made after are compared to show the work each stage does is unchanged.

## Placement

Decision 0027 places each behavior this feature adds:

- Creating a guest: sim, the guests module, src/sim/guests/guests.h and guests.cpp. Guest is private to the module (guests/internal/guest.h, principle 6), so only the module can create one. addGuest is the public way in for full-park's generator.
- Arrivals through addGuest: sim, the guests module, admitGuests in src/sim/guests/guests.cpp, unchanged in role. It now draws only the stay and leaves the rest to addGuest.
- The before and after reports: build/runtime-report/, written by scripts/runtime-report.sh. They are evidence for the feature's report, and build/ is not committed.

## Tasks

### Task 1: Make the report before any change

Files: none in the tree. Writes build/runtime-report/created-guests-before.txt.

Step 1: On the feature branch, before any edit, make the report.

Run: `scripts/runtime-report.sh build/runtime-report/created-guests-before.txt`
Expected: the script builds both presets, runs ten rounds, and ends with `runtime-report: wrote build/runtime-report/created-guests-before.txt in <m>m <ss>s`. The file has a `park windows-release tests/parks/stress/winding-path.park launches 10 ...` line and a `park windows-debug ...` line, each followed by six stage lines.

### Task 2: Add the guests spec's Adding a guest section

Files:
- Modify: `src/sim/guests/SPEC.md`, between the Stepping and Arrivals sections

Step 1: Insert the text under "src/sim/guests/SPEC.md, a new section between Stepping and Arrivals:" in plans/scalable-runtime/measured-runtime/created-guests/FEATURE.md, without the leading "> " of each line.

### Task 3: Rewrite the guests spec's Arrivals section

Files:
- Modify: `src/sim/guests/SPEC.md`, the Arrivals section

Step 1: Replace the whole Arrivals section, from `## Arrivals` to the paragraph's end, with the text under "src/sim/guests/SPEC.md, the Arrivals section becomes:" in FEATURE.md, without the leading "> " of each line.

### Task 4: Declare addGuest with a stub

Files:
- Modify: `src/sim/guests/guests.h`, after the declaration of guestRecord
- Modify: `src/sim/guests/guests.cpp`, after the definition of guestRecord

Step 1: In guests.h, after `std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest);`, add the declaration under "src/sim/guests/guests.h gains, after guestRecord:" in FEATURE.md, with its comment.

Step 2: In guests.cpp, after guestRecord's definition, add the stub:

```cpp
EntityKey addGuest(World & /*world*/, const Place & /*place*/, uint64_t /*stayUntil*/) {
  throw std::logic_error("addGuest is not implemented");
}
```

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build succeeds with no warnings.

### Task 5: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/sim/guests/SPEC.md and src/sim/SPEC.md as the specs, and src/sim/guests/guests.h as the public header.

### Task 6: Implement addGuest

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: In the anonymous namespace, before admitGuests, add the draw helper:

```cpp
// The guest's uniform draw for the purpose, on its key and the world's seed and tick.
double guestDraw(const World &world, EntityKey guest, const char *purpose) {
  return drawUniform(drawKey(world, guest, hashName(purpose), 0));
}
```

Step 2: Replace the stub of Task 4 with:

```cpp
EntityKey addGuest(World &world, const Place &place, uint64_t stayUntil) {
  if (!parkNetwork(world, PathKind::Guest).resolve(place)) {
    throw std::invalid_argument("addGuest: the place is not on the guest network");
  }
  const EntityKey key = world.createEntity();
  Guest guest;
  guest.At = place;
  guest.StayUntil = stayUntil;
  guest.HungerRate = HUNGER_RATE_MIN +
                     guestDraw(world, key, "guest-hunger-rate") * (HUNGER_RATE_MAX - HUNGER_RATE_MIN);
  guest.Hunger = guestDraw(world, key, "guest-starting-hunger") * STARTING_HUNGER_MAX;
  world.Registry.emplace<Guest>(world.findEntity(key), guest);
  return key;
}
```

The expressions for HungerRate and Hunger are the ones admitGuests uses today, in the same order, so an arriving guest's values do not change by a bit.

Step 3: Build and run the test pass's addGuest tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#<file the test pass reports>]"`
Expected: the build has no warnings. Every test for criteria 1 to 3 passes. The test for criterion 4 may pass already, since arrivals' draws are unchanged.

### Task 7: Make arrivals create their guests through addGuest

Files:
- Modify: `src/sim/guests/guests.cpp`, admitGuests

Step 1: Replace admitGuests's comment and body after the `anchored.empty()` check, so the function reads:

```cpp
// Each entrance with a guest connector admits a guest at the end of every ARRIVAL_INTERVAL
// ticks, through addGuest, with its stay drawn on the key addGuest gives it.
void admitGuests(World &world, const Network &network) {
  if ((world.Tick + 1) % ARRIVAL_INTERVAL != 0) {
    return;
  }
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    const std::vector<uint32_t> anchored = network.anchoredNodes(entrance.Key);
    if (anchored.empty()) {
      continue;
    }
    const EntityKey key{world.nextKey()};
    const uint64_t stayUntil =
        world.Tick + STAY_MIN +
        static_cast<uint64_t>(guestDraw(world, key, "guest-stay") *
                              static_cast<double>(STAY_MAX - STAY_MIN));
    addGuest(world, network.nodePlace(anchored.front()), stayUntil);
  }
}
```

addGuest is defined after the anonymous namespace and declared in guests.h, which guests.cpp includes, so admitGuests can call it.

Step 2: Build and run the guests tests.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#arrivals_test],[#walks_test],[#choice_test],[#visits_test],[#footfall_test],[#guest_edits_test],[#<file the test pass reports>]"`
Expected: no warnings, and every test passes, the existing guests tests unchanged among them.

### Task 8: Confirm the acceptance criteria

Step 1: Windows.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug`
Expected: no warnings; every test passes, "private header check passes on the tree" among them.

Step 2: Linux and tidy.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, tidy is clean.

Step 3: The cross-build check (criterion 5).

Run: `scripts/cross-build-check.sh`
Expected: it ends with `cross-build-check: both builds wrote the same <n> lines; passed.`

Step 4: The report after the change, and the comparison (criterion 6).

Run: `scripts/runtime-report.sh build/runtime-report/created-guests-after.txt && scripts/runtime-report.sh --compare build/runtime-report/created-guests-before.txt build/runtime-report/created-guests-after.txt`
Expected: twelve stage lines, none ending in `result-changed`, then `clear <n> of 12`. Put the comparison in the feature's report as it is, whatever it says of time.

### Task 9: Commit

Stage everything with `git add -A && git reset -q parks/`, so the untracked parks/routes.park and parks/sketch.park stay out. Commit once via the commit-hygiene skill, subject `Sim: Add addGuest and create arriving guests through it`.
