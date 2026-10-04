# Implementation Plan: Sim Root Reshaped

## Goal

Trim src/sim/SPEC.md to properties, let the test pass dispatch the test-writer with inventory entries, have it rewrite the sim root's 42 listed cases, and delete the support and inventory entries that leaves behind.

## Approach

The spec trim and the two process edits come first, so the test-writer reads the trimmed spec and the implementer has the rewrite dispatch. The test pass rewrites the cases; nothing under src/ changes except the spec, so every rewritten test is expected to pass at once, and one that fails is a deviation. tpj_scenarios's output is captured before any change and compared at the end.

## Placement

This feature adds no behavior to src/. Its changes have these owners:

- The rewrite dispatch: .claude/skills/implementing-features/SKILL.md, whose test pass dispatches the test-writer, and .claude/agents/test-writer.md, whose rewrite mode does the rewriting.
- The rewritten cases: tests/sim/, the sim root's tests, as docs/conventions.md places a module's tests.
- The draw keys tpj_scenarios writes lines for: tests/sim/support/expected_draws.h, where src/scenarios/SPEC.md already says they are.

## Tasks

### Task 1: Capture tpj_scenarios's output before any change

Files: none

Step 1: Build and run.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/sim-root-reshaped && build/windows-debug/tpj_scenarios.exe tests/parks/*.park | tr -d '\r' > build/sim-root-reshaped/before.txt; echo $?`
Expected: the build succeeds and the last line printed is `0`.

### Task 2: Trim src/sim/SPEC.md

Files:
- Modify: `src/sim/SPEC.md:9`, `:15`, `:23`, `:25`

Step 1: Make each replacement FEATURE.md's Spec changes lists, exactly as written there. Each old text occurs once in the file.

Step 2: Confirm.

Run: `grep -c '1/30 s\|SplitMix64\|drawBits folds\|top 53 bits\|high 64 bits\|rounding the target' src/sim/SPEC.md`
Expected: `0`

### Task 3: Widen the test-writer's rewrite mode

Files:
- Modify: `.claude/agents/test-writer.md:39`

Step 1: Replace

```
When dispatched with closing-audit findings, rewrite only the cases they name, each to assert the property its finding's suggested fix states, following the procedure below with those properties as its list.
```

with

```
When dispatched with closing-audit findings, or with entries of the inventory of tests to rewrite, rewrite only the cases they name, each to assert the property its finding's suggested fix or its entry states, merging cases where an entry says to, following the procedure below with those properties as its list.
```

### Task 4: Add the rewrite dispatch to implementing-features' test pass

Files:
- Modify: `.claude/skills/implementing-features/SKILL.md:25`, and the Test pass dispatch section after line 46

Step 1: In step 4, replace "Then dispatch the `test-writer` agent with the Agent tool, using the template below." with "Then dispatch the `test-writer` agent with the Agent tool, using the first template below, or the rewrite template for a feature that rewrites cases plans/focused-tests/RESEARCH.md lists, with the feature's entries copied verbatim under their file names."

Step 2: After the closing fence of the existing template, add:

````
Rewrite dispatch:

```
Rewrite the test cases these inventory entries name.

Feature: plans/<capability-slug>/<milestone-slug>/<feature-slug>/FEATURE.md
Specs: <SPEC.md paths the feature changes or depends on>
Entries:
<the entries, verbatim, each under its file name>

Follow your rewrite mode and return your standard report.
```
````

### Task 5: Run the test pass

Run implementing-features step 4 with the rewrite dispatch. Feature: plans/focused-tests/reshaped-tests/sim-root-reshaped/FEATURE.md. Specs: src/sim/SPEC.md. Entries: every entry under the headings sim/cycle_test.cpp, sim/draw_test.cpp, sim/fp_environment_test.cpp, sim/save_test.cpp, sim/schema_test.cpp, sim/sim_math_test.cpp, sim/walk_test.cpp, and sim/world_checks_test.cpp in plans/focused-tests/RESEARCH.md, 42 in all, copied with their headings.

Step 1: Confirm the rewritten tests build and pass.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#cycle_test],[#draw_test],[#fp_environment_test],[#save_test],[#schema_test],[#sim_math_test],[#walk_test],[#world_checks_test]"`
Expected: `All tests passed`. Nothing under src/ changed, so a failing test is a deviation: either the test or the spec is wrong, and Evan decides which.

Step 2: Confirm none of the 42 old names remains.

Run: `python3 - <<'EOF'
import re
lines = open('plans/focused-tests/RESEARCH.md').read().split('\n')
files = {'sim/cycle_test.cpp', 'sim/draw_test.cpp', 'sim/fp_environment_test.cpp', 'sim/save_test.cpp', 'sim/schema_test.cpp', 'sim/sim_math_test.cpp', 'sim/walk_test.cpp', 'sim/world_checks_test.cpp'}
current = None
for line in lines:
    if line in files or re.match(r'^[a-z].*\.cpp$', line):
        current = line
    m = re.match(r'^- rewrite: "(.*?)…?"', line)
    if m and current in files and m.group(1).rstrip('…') in open('tests/' + current).read():
        print(current, m.group(1))
EOF`
Expected: no output, unless the test-writer kept an old name for a merged test whose name already states its property; any line printed is checked against its entry's property.

### Task 6: Reduce expected_draws.h to its keys and weights

Files:
- Modify: `tests/sim/support/expected_draws.h`
- Delete: `tests/sim/support/expected_draws.py`

Step 1: Remove the expected values, which no test reads.

Run: `python3 - <<'EOF'
import re
path = 'tests/sim/support/expected_draws.h'
text = open(path).read()
text = re.sub(r'\},\n +\.Bits = [^\n]*\n +\.Uniform = [^\n]*\n +\.IntegerPick = [^\n]*\n +\.DoublePick = \d+\},', '}},', text)
text = re.sub(r'  uint64_t Bits = 0;\n  double Uniform = 0.0;\n  size_t IntegerPick = 0;\n  size_t DoublePick = 0;\n', '', text)
open(path, 'w').write(text)
EOF
grep -c 'Bits\|Uniform\|IntegerPick\|DoublePick' tests/sim/support/expected_draws.h`
Expected: `0`

Step 2: Replace the first three comment lines of the header with:

```
// Draw keys and weights tpj_scenarios writes its draw lines for, so the cross-build check compares
// keyed draws between builds. Each key is chosen to vary one word of a draw's key, or all of them.
```

Step 3: Delete the generator: `git rm -q tests/sim/support/expected_draws.py`.

Step 4: Remove `#include <stddef.h>` from the header if nothing in it uses size_t any more.

Step 5: In src/scenarios/SPEC.md, make the replacement FEATURE.md's Spec changes lists for it.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 7: Delete test support no remaining test uses

Files:
- Modify: `tests/sim/support/synthetic_types.h`

Of the support headers, the eight rewritten files include only synthetic_types.h and sim_math_reference.h, and sim_math_reference.h is a generated table tpj_scenarios also reads. So only synthetic_types.h can be left with unused definitions.

Step 1: List its top-level definitions: every struct, enum, constant, function, and function template at namespace scope.

Run: `grep -nE '^(struct|enum|inline|constexpr|template|[A-Za-z_:<>, ]+ [A-Za-z_][A-Za-z0-9_]*\()' tests/sim/support/synthetic_types.h`
Expected: the definitions' first lines. Read the header to catch any the pattern misses, such as a return type spread over two lines.

Step 2: For each name, find its uses outside the header.

Run: `git grep -lw <name> -- tests src ':!tests/sim/support/synthetic_types.h'`
Expected: a name with output is used. A name with no output is used only if a used definition in the header refers to it; otherwise delete it. After each round of deletions, repeat this step for the names left, since a definition used only by a deleted one is now unused too.

Step 3: Confirm.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests tpj_scenarios`
Expected: the build succeeds with no warnings.

### Task 8: Remove the sim root's entries from the inventory

Files:
- Modify: `plans/focused-tests/RESEARCH.md`

Step 1: Delete the headings sim/cycle_test.cpp, sim/draw_test.cpp, sim/fp_environment_test.cpp, sim/save_test.cpp, sim/schema_test.cpp, sim/sim_math_test.cpp, sim/walk_test.cpp, and sim/world_checks_test.cpp, each with its entries and the blank line after them.

Run: `grep -c '^sim/[a-z_]*\.cpp$' plans/focused-tests/RESEARCH.md`
Expected: the count before the edit minus 8.

### Task 9: Confirm the acceptance criteria

Step 1: Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

Step 2: Run: `ctest.exe --preset windows-debug`
Expected: `100% tests passed`.

Step 3: Run: `scripts/tidy.sh`
Expected: `tidy: clean.`

Step 4: Compare tpj_scenarios's output with Task 1's.

Run: `build/windows-debug/tpj_scenarios.exe tests/parks/*.park | tr -d '\r' > build/sim-root-reshaped/after.txt && build/windows-debug/tpj_scenarios.exe --compare build/sim-root-reshaped/before.txt build/sim-root-reshaped/after.txt; echo $?`
Expected: `0`

Step 5: Run: `git diff --name-only main -- src`
Expected: `src/scenarios/SPEC.md` and `src/sim/SPEC.md` alone.

### Task 10: Review, then commit

Run implementing-features steps 7 to 9: review the staged diff, then commit once via the `commit-hygiene` skill with the subject `Tests: Reshape the sim root's tests and spec`, and merge as `Merge: sim-root-reshaped`.
