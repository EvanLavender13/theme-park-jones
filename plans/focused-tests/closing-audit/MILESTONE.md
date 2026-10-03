# Milestone: Closing Audit

Slice: none

Status: planned

## Summary

closing-audit makes closing a milestone a step with a check in it. Today a milestone has no close: its last feature merges and the next plan starts, so no one ever reads its tests together, and no feature's review can see that two of its features proved the same property or that a property it proved is already proven elsewhere. The milestone gives the reviewer a test-audit lens, which reads the test cases a milestone added or changed against docs/testing.md and searches the whole suite for where each property is already proven, and gives planning-milestones a closing procedure built on it: the audit's report goes to Evan verbatim, and the milestone closes only when each finding is fixed, filed, or rejected by him. It comes first in the capability because it is small and it checks the two milestones after it. Its first close is following-footfall, the last milestone to land, whose test cases the capability's inventory has already judged, so the first run is also a check that the audit reads the rules as that inventory did.

## Acceptance criteria

- The reviewer agent has a test-audit lens. Given the test cases a milestone added or changed, it reports each case that breaks a rule of docs/testing.md, citing the rule; for rule 2 it also names the test or law that already proves the property, found by searching the whole suite rather than only the cases given; and where an acceptance criterion or spec statement produced the case, the finding names it. The reviewing skill has the lens's dispatch template, which passes the milestone's MILESTONE.md and the merges that define its scope, and nothing else.
- The lens, run on following-footfall's test cases, reports every case among them that plans/focused-tests/RESEARCH.md lists as rewrite or held. Run on a planted case, it reports the case under rule 4: a case asserting a property no other case asserts, shown over a list of inputs chosen for no stated reason, merged on a scratch branch that is never pushed or merged into main, so that its merge enters the lens's scope through the dispatch template like any feature's. Each case where the lens and the inventory disagree goes to Evan, and whichever he judges wrong is fixed: the lens, or the inventory.
- planning-milestones has a section, Closing a milestone, that runs once every feature in the milestone's Features list has merged. It finds the milestone's test cases as the TEST_CASEs added or changed by the merges named "Merge: <feature-slug>" for its feature directories, each diffed against its first parent, read as they stand at the close, skipping any a later change has deleted; dispatches the test-audit lens through the reviewing skill; and presents the report to Evan verbatim. Each finding is then fixed, filed, or rejected by Evan. A case that should not exist is deleted. A case asserting a real property in the wrong shape is rewritten by the test-writer, after the criterion behind it is rewritten when the finding names one. A fix that needs a design choice, or belongs to work already planned, is filed in plans/BACKLOG.md naming the test, the rule, and the property. A rejection that shows a rule is wrong goes to a decision record changing docs/testing.md. The close then confirms that windows-debug builds without warnings, its tests pass, and scripts/tidy.sh is clean, sets the MILESTONE.md's Status to complete, and commits via commit-hygiene. planning-milestones' hard gate allows, while closing, deleting and editing test code, dispatching the test-writer, and calling commit-hygiene, as planning-slices' gate does for a slice's close.
- MILESTONE.md's format in planning-milestones has a Status line, planned when written and complete when closed. A MILESTONE.md with no Status line predates milestone closing: decision 0032's audit read every test case those milestones added, and its findings are the capability's inventory. planning-overview routes closing a milestone to planning-milestones.
- The test-writer accepts a dispatch carrying a closing audit's findings and rewrites only the cases they name, each to assert the property the finding states, under its usual rules.
- following-footfall is closed through the procedure: its audit report has gone to Evan, each finding is fixed, filed, or rejected, its Status is complete, and the close is committed. Findings the capability's inventory already lists count as filed: its rewrite entries in reshaped-tests, and its held entries in registered-laws, which deletes them. The close leaves scripts/tidy.sh clean, which today fails on the size of requireNodeSample in tests/sim/medium/kept_field_test.cpp, so the close shortens that helper without changing what any case asserts.
- Nothing under src/ changes, and tpj_scenarios's windows-debug output is identical before and after.

## Medium

This milestone introduces, samples, and emits no fields or flows. It changes skills and agents, and the tests of one closed milestone.

## Dependencies

- plans/focused-tests/CAPABILITY.md: approved.
- docs/testing.md and decision 0032, on the test-discipline branch: unmet until that branch merges into main, since each feature branches from main.
- The capability's inventory in plans/focused-tests/RESEARCH.md, which the calibration compares against: met.
- following-footfall's two features, merged into main: met.

## Core feature

`test-audit`, because it is the check itself: once it exists and has been calibrated, it can be run on any milestone's tests by hand, and it already tells Evan where following-footfall's tests break the rules. The closing procedure only fixes when it runs and what happens to its findings.

## Features

1. `test-audit`: the reviewer's test-audit lens and its dispatch template in the reviewing skill, calibrated on following-footfall's test cases against the capability's inventory and on one planted case. Depends on: none.
2. `milestone-closing`: the Closing a milestone section in planning-milestones, the Status line in MILESTONE.md's format, planning-overview's routing, the test-writer's handling of audit findings, and following-footfall's close through it. Depends on: feature 1.

Both features change skills and agents, not code, so their acceptance criteria are checks the implementer runs and reports, as for a test utility, and they have no test pass.

## Deepening candidates

None.

## Open questions

None. Whether the audit should also read criteria and spec statements is the capability's open question; this milestone's findings name the criterion behind a case, which is the evidence that question needs.

## Research notes

A milestone's test cases are those changed by its features' merges, each against its first parent, which stays exact while other work interleaves. The auditor is the reviewer with one more lens, since a separate agent would be a second file of instructions about tests. It is trusted only after its first run is compared with cases already judged, following-footfall's in the inventory, and with a planted violation. Depth is in RESEARCH.md.
