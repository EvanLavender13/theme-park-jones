# Implementation Plan: Test Audit

## Goal

Give the reviewer a test-audit lens and the reviewing skill its dispatch template, then run it on following-footfall's tests and give Evan the report.

## Approach

The lens is one more section in .claude/agents/reviewer.md beside its other artifact lenses, plus one clause in its investigation procedure. The template is one more section in .claude/skills/reviewing/SKILL.md beside its other two. The audit then runs once on following-footfall's two merges, and its report goes to Evan.

## Placement

Decision 0027 places code. This feature adds no code; each behavior it adds lives in the instructions that run it:

- Reading a milestone's test cases against docs/testing.md and reporting rule breaks: .claude/agents/reviewer.md, the Test audits lens. The reviewer already holds every lens that judges an artifact against the project's rules, and it has read-only tools and the prove-it-or-discard rule the audit needs.
- Dispatching a test audit and presenting its report: .claude/skills/reviewing/SKILL.md, the Test audit dispatch template. The reviewing skill owns every reviewer dispatch and its hygiene.

## Tasks

The feature branch `test-audit` exists, from implementing-features step 2. There is no spec task, no interface task, and no test pass: the feature changes an agent and a skill and no code, and like a test utility it is checked by running it (docs/testing.md rule 6).

### Task 1: Add the Test audits lens

Files:
- Modify: `.claude/agents/reviewer.md`, inserting after the "### Code or diffs" section's last bullet and before "## Investigation procedure"

Step 1: Insert this section.

```markdown
### Test audits (type `test-audit`)

A test audit reads the test cases a milestone added or changed against docs/testing.md. The dispatch names the milestone's MILESTONE.md and the merge commits that make up its scope.

- Scope: for each merge, `git diff <merge>^1 <merge> -- tests/` shows the TEST_CASEs it added or changed. Read each of them as it stands now in the working tree, and skip any that no longer exists. A changed fixture or park file is read only for what the cases that use it assert.
- Each case that breaks a rule of docs/testing.md is one Major finding. Location: the file, the line, and the test name. Evidence: the rule's number, and the assertion or construct that breaks it. Failure scenario: what the break costs. For rule 1, a retuning or refactor that fails the test while the code stays correct. For rule 2, the test or standard that already proves the property, found by searching all of tests/ with `git grep`, not only the cases given. For rules 3 to 7, the compiler guarantee, the listed inputs or entry points, the wiring, the test utility, or the module's own test that the case stands in for. Suggested fix: the lasting property the case should assert, or that it should be deleted when no property remains that another test does not already prove.
- Where an acceptance criterion in one of the milestone's FEATURE.md files, or a statement in a SPEC.md, produced the case, the finding names it, since the fix starts there.
- A case that keeps every rule gets no finding.
```

Step 2: Confirm the insertion.

Run: `grep -n "^### Test audits\|^## Investigation procedure" .claude/agents/reviewer.md`
Expected: two lines, the Test audits heading's line number lower than the Investigation procedure's.

### Task 2: Add a test audit's reading to the reviewer's procedure

Files:
- Modify: `.claude/agents/reviewer.md`, step 2 of "## Investigation procedure"

Step 1: In step 2, replace `a code change requires the files it touches and their callers).` with `a code change requires the files it touches and their callers; a test audit requires its MILESTONE.md and the FEATURE.md of each feature directory under it).`

Run: `grep -c "a test audit requires its MILESTONE.md" .claude/agents/reviewer.md`
Expected: `1`

### Task 3: Add the test audit to the reviewing skill's checklist

Files:
- Modify: `.claude/skills/reviewing/SKILL.md`, checklist steps 2 and 3, and the line after the planning-briefs exception

Step 1: In step 2, replace `Plan document, implementation plan, code change, or other.` with `Plan document, implementation plan, code change, test audit, or other.`

Step 2: At the end of step 3, before `Do not summarize these for the reviewer;`, insert `A test audit requires its milestone's MILESTONE.md, and nothing else.` followed by a space.

Step 3: Replace `Code reviews and reviews the user asks for directly still follow steps 6 and 7.` with `Code reviews, test audits, and reviews the user asks for directly still follow steps 6 and 7.`

Run: `grep -c "test audit" .claude/skills/reviewing/SKILL.md`
Expected: `3`

### Task 4: Add the Test audit dispatch template

Files:
- Modify: `.claude/skills/reviewing/SKILL.md`, inserting after the "## Follow-up dispatch template" section and before "## After the review"

Step 1: Insert this section.

````markdown
## Test audit dispatch template

A test audit's scope is the merges named for the milestone's features. For each feature directory under the milestone, find its merge with `git log --format='%h %s' --grep='^Merge: <feature-slug>$' main`. A feature with no merge contributes nothing. Pass each merge as its abbreviated hash and subject, one per line.

```
Audit the tests of this milestone.

Artifact: the test cases added or changed by these merges:
<hash> Merge: <feature-slug>
<hash> Merge: <feature-slug>
Type: test-audit
Required context (read these before reviewing): plans/<capability-slug>/<milestone-slug>/MILESTONE.md

Follow your test-audit procedure. Return findings in the standard format.
```
````

Step 2: Confirm the insertion.

Run: `grep -n "^## Test audit dispatch template\|^## After the review" .claude/skills/reviewing/SKILL.md`
Expected: two lines, the template's line number lower than After the review's.

### Task 5: Run the audit on following-footfall

Step 1: Dispatch the reviewer through the reviewing skill with the Test audit dispatch template, exactly:

```
Audit the tests of this milestone.

Artifact: the test cases added or changed by these merges:
6429bff Merge: kept-entries
c52ce85 Merge: footfall-on-kept-entries
Type: test-audit
Required context (read these before reviewing): plans/scalable-runtime/following-footfall/MILESTONE.md

Follow your test-audit procedure. Return findings in the standard format.
```

### Task 6: Give Evan the report

Step 1: Present the report to Evan verbatim. Below it, say which of the five cases the FEATURE.md names have no finding, and which findings fall on cases the inventory in plans/focused-tests/RESEARCH.md does not list. Wait for his ruling.

Step 2: Make the fixes he rules: in the Test audits section of `.claude/agents/reviewer.md` where the audit was wrong, or in that case's entry in `plans/focused-tests/RESEARCH.md` where the inventory was wrong.

### Task 7: Commit

Step 1: Stage only this feature's paths.

Run: `git add .claude/agents/reviewer.md .claude/skills/reviewing/SKILL.md plans/focused-tests/closing-audit/test-audit plans/focused-tests/RESEARCH.md && git diff --cached --stat`
Expected: only those paths.

Step 2: Commit via the commit-hygiene skill, subject `Agents: Audit a milestone's tests against the testing rules`.
