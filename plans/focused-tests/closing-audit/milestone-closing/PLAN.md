# Implementation Plan: Milestone Closing

## Goal

Add the milestone closing procedure, the Status line, the routing, the test-writer's rewrite mode, and the rule 1 change, then close following-footfall with them.

## Approach

Each piece is a text edit to the skill, agent, or doc that owns it. The close of following-footfall then follows the new section step by step. Its one code change is splitting a test helper so tidy passes. The feature commits once, close included.

## Placement

Decision 0027 places code. The only code change is in a test helper. The other behaviors live in the instructions that run them:

- Closing a milestone, and the Status line: .claude/skills/planning-milestones/SKILL.md, which owns a milestone's brief from planning to close, as planning-slices owns a slice's.
- Routing a close: .claude/skills/planning-overview/SKILL.md, which routes every planning intent.
- Rewriting cases a finding names: .claude/agents/test-writer.md, the only writer of tests.
- The rule 1 change: docs/testing.md, through docs/decisions/0033-test-setup-in-rule-1.md (decision 0032).
- requireNodeSample's split: tests/sim/medium/kept_field_test.cpp, where the helper lives.

## Tasks

The feature branch `milestone-closing` exists, from implementing-features step 2. There is no spec task, no interface task, and no test pass.

### Task 1: Add the Status line to MILESTONE.md's format

Files:
- Modify: `.claude/skills/planning-milestones/SKILL.md`, the Output format block

Step 1: In the Output format block, replace

```
Slice: <slice-slug>, or none
```

with

```
Slice: <slice-slug>, or none

Status: planned
```

Run: `grep -c "^Status: planned" .claude/skills/planning-milestones/SKILL.md`
Expected: `1`

### Task 2: Add the closing section and its gate

Files:
- Modify: `.claude/skills/planning-milestones/SKILL.md`, the Hard gate section, and a new section inserted before "## Process notes"

Step 1: In the Hard gate section, after the paragraph that begins `` `researching`, `maintaining-backlog`, and `reviewing` are the only callable skills from here``, add this paragraph:

```markdown
While closing a milestone, `commit-hygiene` is also callable and the `test-writer` agent may be dispatched. The acceptance criterion or SPEC.md statement a finding names may be rewritten. Test cases may be deleted, and test code edited only to fix a build, test, or tidy failure without changing what a case asserts.
```

Step 2: Insert this section before "## Process notes":

````markdown
## Closing a milestone

When every feature in the milestone's Features list has merged into main. A MILESTONE.md with no Status line predates milestone closing; decision 0032's audit read the tests those milestones added.

1. Find the milestone's merges: for each feature directory under the milestone, `git log --format='%h %s' --grep='^Merge: <feature-slug>$' main`.
2. Dispatch the test audit via the `reviewing` skill's Test audit dispatch template, and present the report to Evan verbatim.
3. Settle each finding with Evan. It is fixed, filed, or rejected.
    - A case that should not exist is deleted.
    - A case asserting a real property in the wrong shape is rewritten. When the finding names the criterion or spec statement behind the case, rewrite that first. Then dispatch the `test-writer` with the template below.
    - A fix that needs a design choice, or belongs to planned work, is filed in plans/BACKLOG.md via `maintaining-backlog`, naming the test, the rule, and the property. A finding plans/focused-tests/RESEARCH.md already lists counts as filed.
    - A rejected finding needs nothing more, unless Evan says the rule is wrong or incomplete. Then a decision record changes docs/testing.md.
4. Confirm that the windows-debug build has no warnings, `ctest.exe --preset windows-debug` passes, and `scripts/tidy.sh` is clean.
5. Set the MILESTONE.md's Status to complete, and commit via `commit-hygiene`.

Rewrite dispatch:

```
Rewrite the test cases these closing-audit findings name.

Features: <FEATURE.md path of each feature whose cases are named>
Specs: <SPEC.md paths those features change or depend on>
Findings:
<the findings, verbatim>

Follow your rewrite mode and return your standard report.
```
````

Run: `grep -n "^## Closing a milestone\|^## Process notes\|While closing a milestone" .claude/skills/planning-milestones/SKILL.md`
Expected: three lines: the gate paragraph, then Closing a milestone, then Process notes.

### Task 3: Route closing a milestone

Files:
- Modify: `.claude/skills/planning-overview/SKILL.md`, the Skill selection table

Step 1: Replace `| Plan a milestone inside an existing capability | \`planning-milestones\` |` with `| Plan or close a milestone inside an existing capability | \`planning-milestones\` |`.

Run: `grep -c "Plan or close a milestone" .claude/skills/planning-overview/SKILL.md`
Expected: `1`

### Task 4: Give the test-writer a rewrite mode

Files:
- Modify: `.claude/agents/test-writer.md`, inserting a section after "## Slice mode"'s paragraph and before "## Procedure"

Step 1: Insert:

```markdown
## Rewrite mode

When dispatched with closing-audit findings, rewrite only the cases they name, each to assert the property its finding's suggested fix states, following the procedure below with those properties as its list. When the specs do not determine a property, report it as a gap. A case a finding says to delete is not yours; it is deleted at the close.
```

Run: `grep -n "^## Slice mode\|^## Rewrite mode\|^## Procedure" .claude/agents/test-writer.md`
Expected: three lines in that order.

### Task 5: Extend rule 1 to setups

Files:
- Modify: `docs/testing.md`, rule 1
- Create: `docs/decisions/0033-test-setup-in-rule-1.md`

Step 1: In rule 1, after the sentence ending `or a formula copied from the code.`, insert ` Its setup never depends on an exact text layout either: a case that reaches its state by editing a save's text breaks when the save layout changes.`

Step 2: Create docs/decisions/0033-test-setup-in-rule-1.md:

```markdown
# 0033. Rule 1 covers a test's setup

Status: Accepted, 2026-10-03

## Context

Rule 1 of docs/testing.md limited only what a test's expected outcome depends on. The first closing audit, of following-footfall, passed a kept-field case that reaches a later tick by replacing text in a save, since its expected values were sound. That case breaks whenever the save layout changes, though the code is still right, which is the cost rule 1 exists to prevent.

## Decision

Rule 1 also covers a test's setup: a case never reaches its state by depending on an exact text layout, such as editing a save's text.

## Consequences

Closing audits report such setups under rule 1. The kept-field case is already listed for rewriting in plans/focused-tests/RESEARCH.md.
```

Run: `grep -c "editing a save's text" docs/testing.md`
Expected: `1`

### Task 6: Split requireNodeSample

Files:
- Modify: `tests/sim/medium/kept_field_test.cpp`, the requireNodeSample helper

Step 1: Move the per-end checks into their own helper, so that each function falls under tidy's size limits. Insert above requireNodeSample:

```cpp
void requireNodeEnd(const NodeEnd &end, const NetworkEdge &edge, const ExpectedEnd &expected) {
  REQUIRE(end.Edge.Carrier == edge.Carrier);
  REQUIRE(end.Edge.From == edge.From);
  REQUIRE(end.Edge.To == edge.To);
  REQUIRE(end.Edge.FromDistance == edge.FromDistance);
  REQUIRE(end.Edge.ToDistance == edge.ToDistance);
  REQUIRE(end.AtFrom == expected.AtFrom);
  REQUIRE(end.Along.size() == expected.Along.size());
  for (size_t j = 0; j < expected.Along.size(); ++j) {
    CAPTURE(j);
    REQUIRE(end.Along[j].FromOffset == expected.Along[j].FromOffset);
    REQUIRE(end.Along[j].ToOffset == expected.Along[j].ToOffset);
    REQUIRE(end.Along[j].Value == expected.Along[j].Value);
  }
}
```

and make requireNodeSample's loop body:

```cpp
    CAPTURE(i);
    requireNodeEnd(sample.Ends[i], network.edges()[ends[i].Edge], ends[i]);
```

Step 2: Build and run the file's cases.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#kept_field_test]"`
Expected: the build succeeds, and all test cases pass.

### Task 7: Close following-footfall

Step 1: Follow the new Closing a milestone section for plans/scalable-runtime/following-footfall. Its merges are `6429bff Merge: kept-entries` and `c52ce85 Merge: footfall-on-kept-entries`. Dispatch the test audit and present its report to Evan verbatim, under each finding naming whether plans/focused-tests/RESEARCH.md already lists the case. Settle each finding as Evan rules.

Step 2: Confirm the checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, all tests pass, and tidy reports no errors.

Step 3: In plans/scalable-runtime/following-footfall/MILESTONE.md, after the line `Slice: none`, add a blank line and `Status: complete`.

### Task 8: Commit

Step 1: Stage only this feature's paths.

Run: `git add .claude/skills/planning-milestones/SKILL.md .claude/skills/planning-overview/SKILL.md .claude/agents/test-writer.md docs/testing.md docs/decisions/0033-test-setup-in-rule-1.md tests/sim/medium/kept_field_test.cpp plans/scalable-runtime/following-footfall/MILESTONE.md plans/focused-tests/closing-audit/milestone-closing && git diff --cached --stat`
Expected: only those paths, plus plans/BACKLOG.md if task 7 filed anything there (stage it too).

Step 2: Commit via commit-hygiene, subject `Skills: Close a milestone with an audit of its tests`.
