# Feature: Milestone Closing

## Summary

milestone-closing gives planning-milestones a Closing a milestone section that runs the test audit when a milestone's last feature has merged. Evan gets the audit's report verbatim, and each finding is fixed, filed, or rejected before the milestone is marked complete. MILESTONE.md gains a Status line, planning-overview routes closing to planning-milestones, and the test-writer gets a mode for rewriting the cases a finding names. Rule 1 of docs/testing.md is extended, through decision 0033, to cover a setup that edits save text, the gap following-footfall's audit showed. Then following-footfall is closed through the new section, which leaves scripts/tidy.sh clean again.

The feature changes skills, an agent, and docs, plus one test helper for tidy. Like a test utility (docs/testing.md rule 6), it gets no tests and is checked by running it.

## Acceptance criteria

- .claude/skills/planning-milestones/SKILL.md has a section, Closing a milestone, that runs once every feature in the milestone's Features list has merged into main.
  - It finds the milestone's merges, dispatches the reviewing skill's Test audit dispatch template, and presents the report to Evan verbatim.
  - Each finding is then fixed, filed, or rejected by Evan. A case that should not exist is deleted. A case asserting a real property in the wrong shape is rewritten by the test-writer, after the criterion or spec statement the finding names is rewritten. A fix that needs a design choice, or belongs to planned work, is filed in plans/BACKLOG.md naming the test, the rule, and the property; a finding plans/focused-tests/RESEARCH.md already lists counts as filed. A rejection that shows a rule wrong or incomplete goes to a decision record changing docs/testing.md.
  - The close then confirms the windows-debug build, its tests, and scripts/tidy.sh, sets Status to complete, and commits.
  - The skill's hard gate allows, while closing: rewriting the acceptance criterion or SPEC.md statement a finding names, deleting test cases, editing test code only to fix a build, test, or tidy failure without changing what a case asserts, dispatching the test-writer, and calling maintaining-backlog and commit-hygiene.
- MILESTONE.md's format in that skill has a Status line after the Slice line, planned when written and complete when closed. The section says a MILESTONE.md with no Status line predates milestone closing.
- .claude/skills/planning-overview/SKILL.md routes closing a milestone to planning-milestones.
- .claude/agents/test-writer.md has a Rewrite mode. Dispatched with closing-audit findings, it rewrites only the cases they name, each to assert the property the finding's suggested fix states, and reports a gap when the specs do not determine that property. Cases a finding says to delete are not its work.
- docs/testing.md rule 1 also covers a test's setup: a case that reaches its state by editing a save's text breaks when the save layout changes. docs/decisions/0033-test-setup-in-rule-1.md records the change.
- following-footfall is closed through the section. Its report has gone to Evan, and each finding is fixed, filed, or rejected. scripts/tidy.sh is clean, which takes splitting requireNodeSample in tests/sim/medium/kept_field_test.cpp without changing what any case asserts. ctest passes. Its MILESTONE.md has Status: complete, and the close is committed.
- Nothing under src/ changes.

## Medium

None. The feature introduces, samples, and emits no fields or flows.

## Spec changes

None. Skills, agents, and docs/testing.md have no SPEC.md.

## Files affected

- Modify: .claude/skills/planning-milestones/SKILL.md
- Modify: .claude/skills/planning-overview/SKILL.md
- Modify: .claude/agents/test-writer.md
- Modify: docs/testing.md
- Create: docs/decisions/0033-test-setup-in-rule-1.md
- Modify: tests/sim/medium/kept_field_test.cpp (requireNodeSample only)
- Modify: plans/scalable-runtime/following-footfall/MILESTONE.md (Status line)
- Modify, only if a finding is filed there: plans/BACKLOG.md

## Dependencies

- test-audit: merged (611bbfd).

## Out of scope

- Rewriting the cases following-footfall's findings name. Each is already in the inventory, so it is filed in reshaped-tests or registered-laws.
- Pushing main, which this feature makes possible once tidy is clean.

## Open questions

None.
