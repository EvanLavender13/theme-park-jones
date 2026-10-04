# Implementation Plan: Park Routes Reshaped

## Goal

Trim src/sim/park/SPEC.md and src/sim/routes/SPEC.md to properties, have the test-writer rewrite the 22 listed cases and the 12 added ones, and delete the file, support, and inventory entries the rewrite leaves behind.

## Approach

The spec trims come first, so the test-writer reads the trimmed specs, and the 12 added entries go into the inventory before the dispatch, which copies entries from it. The test pass uses implementing-features' rewrite dispatch. Nothing under src/ changes but the specs, so every rewritten test should pass at once, and one that fails is a deviation. tpj_scenarios's output is captured before any change and compared at the end, under build/, because the Windows executables cannot read WSL's /tmp.

## Placement

This feature adds no behavior to src/. The rewritten cases stay in tests/sim/park/ and tests/sim/routes/, the modules' tests, and their fixtures in tests/sim/support/, as docs/conventions.md places them.

## Tasks

### Task 1: Capture tpj_scenarios's output before any change

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/park-routes-reshaped && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>build/park-routes-reshaped/before-err.txt | tr -d '\r' > build/park-routes-reshaped/before.txt; echo "${PIPESTATUS[0]}"`
Expected: the build succeeds and the last line printed is `0`, tpj_scenarios's own exit status. It exits nonzero when it cannot read or load a park file, and its message is then in before-err.txt.

### Task 2: Trim src/sim/park/SPEC.md

Files:
- Modify: `src/sim/park/SPEC.md:7`, `:13`, `:21`, `:23-32`, `:34`, `:38`, `:42`, `:46`, `:47`, `:68`

Step 1: Make the ten park replacements FEATURE.md's Spec changes gives, exactly as written there. Each old text occurs once in the file.

Run: `grep -c '256\|1 cm\|1 mm\|3 m\|Barry\|sqrt\|z = 128\|larger of their\|axis\.X\|own frame\|MIN_SEGMENT_SAMPLES\| = (t' src/sim/park/SPEC.md; grep -c 'Catmull-Rom\|unit vector' src/sim/park/SPEC.md`
Expected: `0`, then `2`. Before the change they print `17` and `1`.

### Task 3: Trim src/sim/routes/SPEC.md

Files:
- Modify: `src/sim/routes/SPEC.md:15`, `:17`, `:19`, `:25`, `:27`

Step 1: Make the seven routes changes FEATURE.md's Spec changes gives, exactly as written there. Each old text occurs once in the file.

Run: `grep -c '4 m\|1 mm\|sqrt\|(first + second)\|s_a\|Distance + u\|t = ((p' src/sim/routes/SPEC.md; grep -c 'CONNECTION_REACH exceeds half' src/sim/routes/SPEC.md`
Expected: `0`, then `1`. Before the change they print `4` and `0`.

### Task 4: Add the 12 unlisted cases to the inventory

Files:
- Modify: `plans/focused-tests/RESEARCH.md:255-262`

Step 1: Under the heading `sim/routes/connections_test.cpp`, after its two entries, add these five lines:

```
- rewrite: "Each kind's network has its paths as carriers and, besides t…" Property: unchanged, as the case's name states. Its connected parks place doors and lines from literal box and entrance depths, lines 2 m and 3 m beyond doors inside a 4 m reach, and shop 7's door 0.5 mm from a junction; place each from boxSize, ENTRANCE_SIZE, CONNECTION_REACH, and JUNCTION_TOLERANCE by name. The parks also hold a door flush against a line of a kind it serves, its face touching the line's edge, which has a connector, since CONNECTION_REACH exceeds half of every pathWidth.
- rewrite: "A connector has two points, its door at distance 0 and its c…" Property: unchanged, as the case's name states. Same connected parks. The door and the reach are compared within rounding, since the spec no longer gives the midpoint's or the reach's formula.
- rewrite: "A connector's first stop is at a node no other stop of its n…" Property: unchanged, as the case's name states. Same connected parks.
- rewrite: "A connector stops only at its door and its connection, whose…" Property: unchanged, as the case's name states. Same connected parks.
- rewrite: "A network's anchors are exactly its connectors' door nodes, …" Property: unchanged, as the case's name states. Same connected parks.
```

Step 2: Under the heading `sim/routes/networks_test.cpp`, after its entry, add these three lines:

```
- rewrite: "Where two segments meet, each carrier has a stop within the …" Property: unchanged, as the case's name states. Its meeting parks put lines through another's line points at whole meters, which assumes the ground line's sample spacing; take those points from groundLine itself, as the self-crossing park does.
- rewrite: "Consecutive stops of a carrier lie more than the tolerance a…" Property: unchanged, as the case's name states. Its clustered park places lines 0.5 mm and 0.8 mm apart against a 1 mm tolerance; place them from JUNCTION_TOLERANCE by name.
- rewrite: "Nodes are numbered in order of first stop, walking carriers …" Property: unchanged, as the case's name states. Same clustered park.
```

Step 3: After the networks_test entries and the blank line after them, before the heading `sim/routes/route_distance_test.cpp`, add this heading, a blank line, the entry, and a blank line:

```
sim/routes/previous_networks_test.cpp

- rewrite: "A finisher registered between addRoutes and addDropPreviousN…" Property: unchanged, as the case's name states. It requires that a shop pushed at (6.5, 80) anchors the guest network, which holds only for the current shop depth and connection reach; place the shop from boxSize, pathWidth, and CONNECTION_REACH by name so its doors reach both lines.
```

Step 4: Under the heading `sim/routes/route_distance_test.cpp`, after its four entries, add these three lines:

```
- rewrite: "sampleField of a kind's route distance field at a node gives…" Property: unchanged, as the case's name states. Its separated park places each line a literal 2 m from its door and requires the doors to anchor, which holds only for the current connection reach; place each line from pathWidth and CONNECTION_REACH by name, within reach of its door.
- rewrite: "Following Next from any place with a source's entry, samplin…" Property: unchanged, as the case's name states. Same separated park.
- rewrite: "Every world a random edit sequence reaches resolves route di…" Property: unchanged, as the case's name states. Its opening shop's door lies a literal 1 m beyond its line's edge, and the sequence must route through it; place the shop from pathWidth, boxSize, and CONNECTION_REACH by name, within reach of the line.
```

Run: `grep -c '^- rewrite' plans/focused-tests/RESEARCH.md; grep -c '^sim/park/\|^sim/routes/' plans/focused-tests/RESEARCH.md`
Expected: `154`, then `8`. Before the change they print `142` and `7`.

### Task 5: Run the test pass

Run implementing-features step 4 with its rewrite dispatch:

- Feature: plans/focused-tests/reshaped-tests/park-routes-reshaped/FEATURE.md.
- Specs: src/sim/park/SPEC.md, src/sim/routes/SPEC.md, src/sim/medium/SPEC.md, and src/sim/SPEC.md.
- Entries: every entry under the headings that begin `sim/park/` and `sim/routes/` in plans/focused-tests/RESEARCH.md, eight headings and 34 entries. Copy them verbatim, with their headings.

Step 1: Confirm the rewritten tests build and pass.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#edits_test],[#geometry_test],[#validity_test],[#connections_test],[#networks_test],[#route_distance_test],[#previous_networks_test]"`
Expected: a last line beginning `All tests passed`. Before the pass, with `[#edit_sequences_test]` added to the filter, it reads `All tests passed (3483 assertions in 54 test cases)`. A failing test is a deviation, and Evan decides whether the test or the spec is wrong.

Step 2: Check the test-writer's report against FEATURE.md's criteria 1 to 3. Each of the 34 entries must be rewritten or merged as they say.

Run: `grep -c 'TEST_CASE' tests/sim/park/edit_sequences_test.cpp; grep -c '7\.01\|-3\.0005\|0\.3008' tests/sim/routes/connections_test.cpp tests/sim/routes/networks_test.cpp`
Expected: either `0` or a missing-file error from the first grep, then `tests/sim/routes/connections_test.cpp:0` and `tests/sim/routes/networks_test.cpp:0`. Before the pass they print `1`, then `:2` and `:1`: the random case, and the literals placed against CONNECTION_REACH and JUNCTION_TOLERANCE.

### Task 6: Delete edit_sequences_test.cpp

Files:
- Delete: `tests/sim/park/edit_sequences_test.cpp`
- Modify: `tests/sim/CMakeLists.txt:29`

Step 1: If the file still exists, delete it with `git rm -q tests/sim/park/edit_sequences_test.cpp`, and delete the line `    park/edit_sequences_test.cpp` from tests/sim/CMakeLists.txt.

Run: `test -e tests/sim/park/edit_sequences_test.cpp; echo $?; grep -c 'edit_sequences' tests/sim/CMakeLists.txt`
Expected: `1`, then `0`.

### Task 7: Delete test support no remaining test uses

Files:
- Modify: `tests/sim/support/park_worlds.h`, `tests/sim/support/same_networks.h`, `tests/sim/support/route_edits.h`

Step 1: Write the check by name. For each namespace-scope definition of the three headers, it lists the files that include the header and name it.

```
cat > build/park-routes-reshaped/support_uses.sh <<'EOF'
check() {
  header=$1; shift
  users=$(git grep -l "support/$header" -- tests)
  for name in "$@"; do
    echo "$name: $(grep -lw "$name" $users | tr '\n' ' ')"
  done
}
check park_worlds.h ParkIntent intentOf writePose writeKey worldOf sameBits samePose samePoints
check same_networks.h sameNetwork sameNetworks
check route_edits.h RouteEditDraws ROUTE_EDIT_REGION snappedPoint routeEditPath routeEditPose besideLine routeEdit
EOF
```

Step 2: Run: `bash build/park-routes-reshaped/support_uses.sh | grep ': $'`
Expected: the names no includer uses, one per line. Before the pass these are writePose, writeKey, ROUTE_EDIT_REGION, snappedPoint, routeEditPath, routeEditPose, and besideLine, each referred to by a used definition in its own header. Delete each other listed name that no used definition in its header refers to, along with any include only it needed, and run the step again until it lists only names that used definitions refer to. same_networks.h includes park_worlds.h and names a local variable samePoints, so samePoints counts as unused when tests/sim/support/same_networks.h is the only file its line lists.

Step 3: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests; echo $?`
Expected: a last line of `0`. Warnings are errors, so a warning fails the build.

### Task 8: Remove the park's and routes' entries from the inventory

Files:
- Modify: `plans/focused-tests/RESEARCH.md`

Step 1: Delete the eight headings that begin `sim/park/` and `sim/routes/`, each with its entries and the blank line after them.

Run: `grep -c '^sim/park/\|^sim/routes/' plans/focused-tests/RESEARCH.md; grep -c '^- rewrite' plans/focused-tests/RESEARCH.md`
Expected: `0`, then `120`.

### Task 9: Confirm the acceptance criteria

Step 1: Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>build/park-routes-reshaped/after-err.txt | tr -d '\r' > build/park-routes-reshaped/after.txt; echo "${PIPESTATUS[0]}"; build/windows-debug/tpj_scenarios.exe --compare build/park-routes-reshaped/before.txt build/park-routes-reshaped/after.txt; echo $?`
Expected: `0`, then `0`: tpj_scenarios's own exit status, then the compare's. The compare prints nothing when the outputs are identical.

Step 5: Run: `git diff --name-only main -- src`
Expected: `src/sim/park/SPEC.md` and `src/sim/routes/SPEC.md`, and nothing else.

### Task 10: Review, then commit

Run implementing-features steps 7 to 9. Review the staged diff, then commit once via the `commit-hygiene` skill with the subject `Tests: Reshape the park's and routes' tests and specs`, and merge as `Merge: park-routes-reshaped`.
