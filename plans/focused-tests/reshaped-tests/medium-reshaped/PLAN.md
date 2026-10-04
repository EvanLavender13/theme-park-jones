# Implementation Plan: Medium Reshaped

## Goal

Trim src/sim/medium/SPEC.md's two formulas to properties, have the test-writer rewrite the medium's 19 listed cases, and delete the support and inventory entries the rewrite leaves behind.

## Approach

The spec trim comes first, so the test-writer reads the trimmed spec. The test pass uses implementing-features' rewrite dispatch. Nothing under src/ changes but the spec, so every rewritten test should pass at once, and one that fails is a deviation. tpj_scenarios's output is captured before any change and compared at the end. Its files live under build/, because the Windows executables cannot read WSL's /tmp.

## Placement

This feature adds no behavior to src/. The rewritten cases stay in tests/sim/medium/, the medium's tests, and its fixtures in tests/sim/support/, as docs/conventions.md places them.

## Tasks

### Task 1: Capture tpj_scenarios's output before any change

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/medium-reshaped && build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>build/medium-reshaped/before-err.txt | tr -d '\r' > build/medium-reshaped/before.txt; echo "${PIPESTATUS[0]}"`
Expected: the build succeeds and the last line printed is `0`, tpj_scenarios's own exit status. It exits nonzero when it cannot read or load a park file, and its message is then in before-err.txt.

### Task 2: Trim src/sim/medium/SPEC.md

Files:
- Modify: `src/sim/medium/SPEC.md:29`, `:31`

Step 1: Make the three replacements FEATURE.md's Spec changes gives, exactly as written there. Each old text occurs once in the file.

Run: `grep -c 'a.X + t\|with t 0' src/sim/medium/SPEC.md; grep -c 'fraction of the way' src/sim/medium/SPEC.md`
Expected: `0`, then `2`. Before the change they print `2` and `0`.

### Task 3: Run the test pass

Run implementing-features step 4 with its rewrite dispatch:

- Feature: plans/focused-tests/reshaped-tests/medium-reshaped/FEATURE.md.
- Specs: src/sim/medium/SPEC.md and src/sim/SPEC.md.
- Entries: every entry under the headings sim/medium/carry_over_test.cpp, sim/medium/field_index_test.cpp, sim/medium/field_test.cpp, sim/medium/flow_test.cpp, sim/medium/kept_field_test.cpp, sim/medium/network_test.cpp, and sim/medium/stepped_field_test.cpp in plans/focused-tests/RESEARCH.md. Copy all 19 verbatim, with their headings.

Step 1: Confirm the rewritten tests build and pass.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#carry_over_test],[#field_index_test],[#field_test],[#flow_test],[#kept_field_test],[#network_test],[#stepped_field_test]"`
Expected: a last line beginning `All tests passed`. Before the pass it reads `All tests passed (3037 assertions in 78 test cases)`. A failing test is a deviation, and Evan decides whether the test or the spec is wrong.

Step 2: Check the test-writer's report against FEATURE.md's criteria 1 and 2. Each of the 19 entries must be rewritten, merged, or deleted as the criteria say.

Run: `grep -c 'isResolving is true while a resolver runs' tests/sim/medium/field_test.cpp; grep -c '\\ntick' tests/sim/medium/kept_field_test.cpp`
Expected: `0`, then `0`. Before the pass they print `1` and `2`: the deleted case's name, and the two save-text lines the read-rule case edits.

### Task 4: Delete test support no remaining test uses

Files:
- Modify: `tests/sim/medium/field_index_test.cpp`
- Modify: `tests/sim/support/synthetic_fields.h`, `tests/sim/support/synthetic_flows.h`, `tests/sim/support/synthetic_network.h`

Step 1: Delete field_index_test.cpp's declarations that only cases fe946fc deleted used, if the rewrite left them: the Sampled alias, MixedWorld, the sampled template, ExpectedAlong, and ExpectedCall.

Run: `grep -nw 'MixedWorld\|ExpectedAlong\|ExpectedCall\|Sampled' tests/sim/medium/field_index_test.cpp`
Expected: no output.

Step 2: Write the check by name. For each namespace-scope definition of the three headers, it lists the files that include the header and name it.

```
cat > build/medium-reshaped/support_uses.sh <<'EOF'
check() {
  header=$1; shift
  users=$(git grep -l "support/$header" -- tests)
  for name in "$@"; do
    echo "$name: $(grep -lw "$name" $users | tr '\n' ' ')"
  done
}
check synthetic_fields.h Footfall ReachCall reachCalls Reach Emits PublishOrder publishOrder PublishOrderScope SourceEntries sourcesInPublishOrder publishSources Steps publishSteppedSourcesWhere publishSteppedSources Layout NETWORK_PURPOSE produceNetworks CARRIER_A CARRIER_B CARRIER_C standardLayout addSyntheticNetwork addSyntheticFields makeFieldSchema addStepsComponents makeSteppedFieldSchema LAYOUT_KEY makeFieldWorld networkOf addSource setSteps addStepper entryAt
check synthetic_flows.h Meals Supplies Trader traderKeys issuedKey tradeFor tradeWhere anyKey evenKey oddKey tradeAll Churn makeFlowSchema makeTradeSchema makeTradeWorld stepWithChurn
check synthetic_network.h groundDistance distanceToCarrier SyntheticRandom SyntheticNetwork makeSyntheticNetwork
EOF
```

Step 3: Run: `bash build/medium-reshaped/support_uses.sh | grep ': $'`
Expected: the names no includer uses, one per line. Before the pass these are publishOrder, SourceEntries, sourcesInPublishOrder, publishSources, NETWORK_PURPOSE, produceNetworks, addSyntheticFields, Trader, traderKeys, issuedKey, tradeFor, anyKey, Churn, and makeFlowSchema. Each is referred to by a used definition in its own header. Delete each other listed name that no used definition in its header refers to, along with any include only it needed. Then run the step again, until it lists only names that used definitions refer to.

Step 4: Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests; echo $?`
Expected: a last line of `0`. Warnings are errors, so a warning fails the build.

### Task 5: Remove the medium's entries from the inventory

Files:
- Modify: `plans/focused-tests/RESEARCH.md:209-248`

Step 1: Delete the seven sim/medium/ headings, each with its entries and the blank line after them.

Run: `grep -c '^sim/medium/' plans/focused-tests/RESEARCH.md; grep -c '^- rewrite' plans/focused-tests/RESEARCH.md`
Expected: `0`, then `142`. Before the change they print `7` and `161`.

### Task 6: Confirm the acceptance criteria

Step 1: Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park 2>build/medium-reshaped/after-err.txt | tr -d '\r' > build/medium-reshaped/after.txt; echo "${PIPESTATUS[0]}"; build/windows-debug/tpj_scenarios.exe --compare build/medium-reshaped/before.txt build/medium-reshaped/after.txt; echo $?`
Expected: `0`, then `0`: tpj_scenarios's own exit status, then the compare's. The compare prints nothing when the outputs are identical.

Step 5: Run: `git diff --name-only main -- src`
Expected: `src/sim/medium/SPEC.md` alone.

### Task 7: Review, then commit

Run implementing-features steps 7 to 9. Review the staged diff, then commit once via the `commit-hygiene` skill with the subject `Tests: Reshape the medium's tests and spec`, and merge as `Merge: medium-reshaped`.
