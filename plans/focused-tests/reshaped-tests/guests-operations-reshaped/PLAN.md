# Implementation Plan: Guests Operations Reshaped

## Goal

Trim src/sim/guests/SPEC.md and src/sim/operations/SPEC.md to properties, have the test-writer rewrite the 34 listed entries and the 12 added ones, and delete the support and inventory entries the rewrite leaves behind.

## Approach

The spec trims come first, so the test-writer reads the trimmed specs. The 12 added entries and the corrected scoring entry go into the inventory before the dispatch, which copies entries from it. The test pass uses implementing-features' rewrite dispatch. Nothing under src/ changes but the specs, so every rewritten test should pass at once, and one that fails is a deviation. tpj_scenarios's output is captured before any change and compared at the end, under build/, because the Windows executables cannot read WSL's /tmp.

## Placement

This feature adds no behavior to src/. The rewritten cases stay in tests/sim/guests/, tests/sim/operations/, and tests/integration/, the modules' tests, and their fixtures in tests/sim/support/ and tests/integration/support/, as docs/conventions.md places them.

## Tasks

### Task 1: Capture tpj_scenarios's output before any change

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/guests-operations-reshaped && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>build/guests-operations-reshaped/before-err.txt | tr -d '\r' > build/guests-operations-reshaped/before.txt; echo "${PIPESTATUS[0]}"`
Expected: the build succeeds and the last line printed is `0`, tpj_scenarios's own exit status. It exits nonzero when it cannot read or load a park file, and its message is then in before-err.txt.

### Task 2: Trim src/sim/guests/SPEC.md

Files:
- Modify: `src/sim/guests/SPEC.md:15`, `:21`, `:31`, `:33`, `:35`, `:41`, `:51`, `:55`, `:69`, `:71`

Step 1: Make the fourteen guests changes FEATURE.md's Spec changes gives, exactly as written there. Each old text occurs once in the file.

Run: `grep -c '1/2700\|45 to 90\|, 0\.4\.\|60 ticks\|1800\|floor(\|is 4,\|0\.01\|0\.02\|0\.3\. \|, 0\.5, \|, 10, \|2\.3\|(0\.3, 0\.1)\|a\.Y + \|0\.25\|simExp((Score\|1\.3 m/s\|1 tick\|300 ticks\|(S - V)\|simLog\|{carrier, FromDistance\|weights of 1' src/sim/guests/SPEC.md; grep -c 'HEAD_HOME_SCORE exceeds\|never falls as hunger rises' src/sim/guests/SPEC.md`
Expected: `0`, then `2`. Before the change they print `10` and `0`.

### Task 3: Trim src/sim/operations/SPEC.md

Files:
- Modify: `src/sim/operations/SPEC.md:13`, `:19`, `:37`, `:38`, `:46`, `:48`, `:50`

Step 1: Make the eight operations changes FEATURE.md's Spec changes gives, exactly as written there. Each old text occurs once in the file.

Run: `grep -c '2 m/s\|, 8,\|, 24,\|30 ticks\|1 tick\|90 ticks\|MEAL_RELIEF 0\.5\|ceil(\|T_0\|T_n\|ORDER_DELAY + D' src/sim/operations/SPEC.md; grep -c 'REORDER_POINT is below ORDER_UP_TO\|never falls as the distance grows' src/sim/operations/SPEC.md`
Expected: `0`, then `2`. Before the change they print `7` and `0`.

### Task 4: Add the 12 entries to the inventory and correct two entries

Files:
- Modify: `plans/focused-tests/RESEARCH.md:162-227`

Step 1: In the entry under the heading `sim/guests/guest_options_test.cpp` that begins `- rewrite: "guestOptions gives the options Choice lists at the guest's pl…"`, replace `Property: An offer scores higher when the guest is hungrier or the offer nearer or quicker, and heading home appears only past StayUntil.` with `Property: An offer never scores lower when the guest is hungrier, and scores lower when it is farther or its wait longer; heading home appears only at or after StayUntil, and then outscores every offer.`

Step 2: At the end of the entry under the heading `sim/guests/walks_test.cpp` that begins `- rewrite: "A guest's Hunger rises each cycle by its own rate, drawn when…"`, after `Restates min(1, h + drawn rate) through the draw formula (S1).`, add ` Its path starts at a literal z = 123, against northGate's door at z = 125; lay it from northGate's line start.`

Step 3: Under the heading `sim/guests/add_guest_test.cpp`, after its entry, add:

```
- rewrite: "gatesIntent…" Property: unchanged, as its comment states. Its guest path runs at a literal z = 123, against northGate's door at z = 125; lay it from northGate's line start.
```

Step 4: Under the heading `sim/guests/arrivals_test.cpp`, after its entries, add:

```
- rewrite: "gatesWorld…" Property: unchanged, as its comment states. Its guest path runs at a literal z = 123, against northGate's door at z = 125; lay it from northGate's line start. The loose entrance stays far from any path.
```

Step 5: Under the heading `sim/guests/footfall_test.cpp`, after its entries, add:

```
- rewrite: "Hungry footfall holds at most one kept entry in each stretch…" Property: unchanged, as the case's name states. Its crossing path runs at a literal z = 121.5, inside the gate path's 123 to 119; take its place from eatingGatePoints, off the gate path's midpoint.
- rewrite: "A stretch that exactly one footfall entry is carried into ho…" Property: unchanged, as the case's name states. Same crossing path.
```

Step 6: Under the heading `sim/guests/walks_test.cpp`, after its entries, add:

```
- rewrite: "crossingWorld…" Property: unchanged, as its comment states. Its spine starts at a literal z = 123, against northGate's door at z = 125; lay it from northGate's line start.
```

Step 7: Under the heading `sim/operations/food_offer_test.cpp`, after its entry, add:

```
- rewrite: "guestLine…" Property: unchanged, as its comment states. It runs at a literal z = -12, 3 m from every line-park shop's front door, from a 6 m shop depth and a 4 m reach; place it from boxSize, pathWidth, and CONNECTION_REACH by name, within reach of shopAt's front door.
```

Step 8: Under the heading `sim/operations/service_test.cpp`, after its entries, add:

```
- rewrite: "A shop's queue holds an entry for each visit unit it holds, …" Property: unchanged, as the case's name states. It expects the second guest's returned visit held by that guest one tick later, which holds only while RETURN_DELAY is 1; take it as addressed to the guest, in transit or held.
- rewrite: "A starved shop keeps the visits that reached it earliest and…" Property: unchanged, as the case's name states. It expects the later visit held by its guest one tick after its return, which holds only while RETURN_DELAY is 1; take it as addressed to the guest, in transit or held.
```

Step 9: After the service_test entries and the blank line after them, before the heading `tools/path_tool_test.cpp`, add these two headings, each followed by a blank line, its entries, and a blank line:

```
sim/support/guest_parks.h

- rewrite: "northGate…" Property: unchanged, as its comment states: the entrance's back lies on the park's north edge, facing into the park. It places the entrance at a literal z = 126.5, its door at 125, and lines from the door start at 123, from a 256 m park, a 3 m entrance depth, and a 4 m reach; place it from PARK_SIZE and ENTRANCE_SIZE by name, and give the z where lines leading in from its door start, within CONNECTION_REACH of the door and clear of its face by more than half pathWidth(PathKind::Guest).
- rewrite: "legsWorld…" Property: unchanged, as its comment states. Its near leg runs from a literal z = 123 to 119, and its far leg starts 6 m from the door, beyond a 4 m reach; start the near leg at northGate's line start and the far leg beyond CONNECTION_REACH of the door, by name.
- rewrite: "eatingWorld…" Property: unchanged, as its comment states. Its gate path starts at a literal z = 123, its shops' doors lie 3.5 m from the spine's middle and 2.5 m from the backstage path's, and its depot's door 3 m from the backstage path's end, from a 6 m shop depth, an 8 m depot depth, and a 4 m reach; place each from northGate's line start, boxSize, pathWidth, and CONNECTION_REACH by name, keeping the spine and the cross where they are.

sim/support/line_park.h

- rewrite: "shopAt, depotAt, and depotAcross…" Property: unchanged, as their comments state. They put every door a literal 3 m from the line, from a 6 m shop depth, an 8 m depot depth, and a 4 m reach; place each from boxSize, pathWidth(PathKind::Backstage), and CONNECTION_REACH by name, within reach of the line, with depotAcross's door meeting the line where depotAt's does.
```

Run: `grep -c '^- rewrite' plans/focused-tests/RESEARCH.md; grep -c '^integration/\|^sim/guests/\|^sim/operations/\|^sim/support/' plans/focused-tests/RESEARCH.md; grep -c 'An offer never scores lower\|drawn rate) through the draw formula (S1)\. Its path starts' plans/focused-tests/RESEARCH.md`
Expected: `132`, `14`, then `2`. Before the change they print `120`, `12`, and `0`.

### Task 5: Run the test pass

Run implementing-features step 4 with its rewrite dispatch:

- Feature: plans/focused-tests/reshaped-tests/guests-operations-reshaped/FEATURE.md.
- Specs: src/sim/guests/SPEC.md, src/sim/operations/SPEC.md, src/sim/routes/SPEC.md, src/sim/park/SPEC.md, src/sim/medium/SPEC.md, and src/sim/SPEC.md.
- Entries: every entry under the headings that begin `integration/`, `sim/guests/`, `sim/operations/`, and `sim/support/` in plans/focused-tests/RESEARCH.md, fourteen headings and 46 entries. Copy them verbatim, with their headings.

Step 1: Confirm the rewritten tests build and pass.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests && build/windows-debug/tpj_sim_tests.exe -# "[#add_guest_test],[#arrivals_test],[#choice_test],[#footfall_test],[#guest_edits_test],[#guest_options_test],[#visits_test],[#walks_test],[#food_offer_test],[#operations_test],[#service_test],[#shop_records_test]" | tail -2; build/windows-debug/tpj_integration_tests.exe | tail -2`
Expected: two lines beginning `All tests passed`, each followed by a blank line. Before the pass they read `All tests passed (506411 assertions in 75 test cases)` and `All tests passed (88 assertions in 1 test case)`. A failing test is a deviation, and Evan decides whether the test or the spec is wrong.

Step 2: Check the test-writer's report against FEATURE.md's criteria 1 and 2. Each of the 46 entries must be rewritten or merged as they say.

Run: `grep -rn 'RETURN_DELAY == 1\|VISIT_DELAY == 1\|SERVICE_INTERVAL == 90\|MEAL_RELIEF == 0\.5\|REORDER_POINT == 8\|ORDER_UP_TO == 24\|ORDER_DELAY == 30\|drawnStayUntil\|drawnStartingHunger\|drawnHungerRate\|checkPick\|choiceOptions(' tests/sim/guests/*.cpp tests/sim/operations/*.cpp | wc -l; grep -c '126\.5\|123\.0\|6\.5,' tests/sim/support/guest_parks.h; grep -c '\-6\.0\|7\.0' tests/sim/support/line_park.h; grep -c '121\.5' tests/sim/guests/footfall_test.cpp; grep -c '\-12\.0' tests/sim/operations/food_offer_test.cpp; grep -c '123\.0' tests/sim/guests/add_guest_test.cpp tests/sim/guests/arrivals_test.cpp tests/sim/guests/walks_test.cpp`
Expected: `0`, `0`, `0`, `0`, `0`, then `tests/sim/guests/add_guest_test.cpp:0`, `tests/sim/guests/arrivals_test.cpp:0`, and `tests/sim/guests/walks_test.cpp:0`. Before the pass they print `18`, `6`, `3`, `2`, `1`, then `:1`, `:1`, and `:2`: the pinned values and copied formulas, and the literal placements.

### Task 6: Delete test support no remaining test uses

Files:
- Modify: `tests/sim/support/guest_parks.h`, `tests/sim/support/line_park.h`, `tests/sim/support/ledger_writes.h`, `tests/integration/support/park_files.h`

Step 1: Write the check by name. For each namespace-scope definition of the four headers, it lists the files that include the header and name it. Add to its lists any definition the test pass added to a header.

```
cat > build/guests-operations-reshaped/support_uses.sh <<'EOF'
check() {
  header=$1; shift
  users=$(git grep -l "support/$header" -- tests)
  for name in "$@"; do
    echo "$name: $(grep -lw "$name" $users | tr '\n' ' ')"
  done
}
check guest_parks.h WALK_STEP northGate guestPath resolvedWorld guestNetwork gatePlace homeDistance recordOf isLive guestDraw drawnStayUntil drawnHungerRate drawnStartingHunger stepUntil FIRST_ARRIVAL LEGS_GATE NEAR_LEG FAR_LEG nearLegPoints legsWorld walkOntoFarLeg stepWith strandPastStay GuestStep traceGuest EATING_GATE EATING_SPINE EATING_CROSS EATING_BACKSTAGE NEAR_SHOP FAR_SHOP STARVED_SHOP EATING_DEPOT EATING_GATE_PATH eatingGatePoints eatingBackstagePoints eatingWorld eatingGatePath routesAt routeFrom anchorPlace offerOf ParkOffers parkOffers offerAmong isOfferReachable isEntrance homeEntry isAtNode midpointOf footfallAt choiceOptions checkFollowsOffer checkFollows checkPick
check line_park.h LINE SHOP FAR_DEPOT DEPOT TWIN_DEPOT OTHER_SHOP LOOSE_SHOP LOOSE_DEPOT GONE shopAt depotAt depotAcross looseShop looseDepot linePath lineWorld
check ledger_writes.h ledgerIn hold carry packetsFrom conserved
check park_files.h ParkFile readParkText parkFile openPark recordOf
EOF
```

Step 2: Run: `bash build/guests-operations-reshaped/support_uses.sh | grep ': $'`
Expected: the names no includer uses, one per line. Before the pass these are guestDraw, LEGS_GATE, STARVED_SHOP, EATING_DEPOT, offerOf, checkFollowsOffer, checkFollows, ledgerIn, conserved, ParkFile, and readParkText. All but conserved are referred to by a used definition in their own header. Delete conserved, and each other listed name that no used definition in its header refers to, along with any include only it needed. Then run the step again until it lists only names that used definitions refer to.

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_integration_tests; echo $?`
Expected: a last line of `0`. Warnings are errors, so a warning fails the build.

### Task 7: Remove the guests', operations', and food loop's entries from the inventory

Files:
- Modify: `plans/focused-tests/RESEARCH.md`

Step 1: Delete the fourteen headings that begin `integration/`, `sim/guests/`, `sim/operations/`, and `sim/support/`, each with its entries and the blank line after them.

Run: `grep -c '^integration/\|^sim/guests/\|^sim/operations/\|^sim/support/' plans/focused-tests/RESEARCH.md; grep -c '^- rewrite' plans/focused-tests/RESEARCH.md`
Expected: `0`, then `86`.

### Task 8: Confirm the acceptance criteria

Step 1: Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>build/guests-operations-reshaped/after-err.txt | tr -d '\r' > build/guests-operations-reshaped/after.txt; echo "${PIPESTATUS[0]}"; build/windows-debug/tpj_scenarios.exe --compare build/guests-operations-reshaped/before.txt build/guests-operations-reshaped/after.txt; echo $?`
Expected: `0`, then `0`: tpj_scenarios's own exit status, then the compare's. The compare prints nothing when the outputs are identical.

Step 5: Run: `git diff --name-only main -- src`
Expected: `src/sim/guests/SPEC.md` and `src/sim/operations/SPEC.md`, and nothing else.

### Task 9: Review, then commit

Run implementing-features steps 7 to 9. Review the staged diff, then commit once via the `commit-hygiene` skill with the subject `Tests: Reshape the guests' and operations' tests and specs`, and merge as `Merge: guests-operations-reshaped`.
