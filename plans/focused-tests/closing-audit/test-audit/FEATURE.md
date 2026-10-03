# Feature: Test Audit

## Summary

test-audit gives the reviewer agent a test-audit lens and the reviewing skill a template to dispatch it. Given merge commits, the lens reads the test cases those merges added or changed, as they stand now, against docs/testing.md, and reports each case that breaks a rule, citing the rule. For rule 2 it searches all of tests/ for the test or law that already proves the property, which no single feature's review can see. It is run once on following-footfall's tests, which the capability's inventory has already judged, and the report goes to Evan.

The feature changes an agent and a skill and no code. Like a test utility (docs/testing.md rule 6), it gets no tests and is checked by running it.

## Acceptance criteria

- .claude/agents/reviewer.md has a lens named Test audits, for the artifact type test-audit. It reads the TEST_CASEs each given merge added or changed, found by diffing the merge against its first parent and limited to tests/. It reads each case as it stands in the working tree and skips any case no longer present. Each case that breaks a rule of docs/testing.md is one Major finding, which cites the rule by number. A rule 2 finding names the test or law that already proves the property, found by searching all of tests/. The suggested fix states the lasting property the case should assert, or says the case should be deleted. Where an acceptance criterion in one of the milestone's FEATURE.md files, or a SPEC.md statement, produced the case, the finding names it. Its investigation procedure lists the reading a test audit needs: the milestone's MILESTONE.md and its features' FEATURE.md files.
- .claude/skills/reviewing/SKILL.md has a Test audit dispatch template. It passes the milestone's MILESTONE.md path and the merges, each as its abbreviated hash and subject, and nothing else. The skill also says how the merges are found and that a test audit's findings are presented verbatim, under steps 6 and 7.
- Run once with the template on following-footfall's merges, 6429bff (Merge: kept-entries) and c52ce85 (Merge: footfall-on-kept-entries), the report has a finding for each of the five cases the capability's inventory lists as rewrite or held among them: cycle_test's "isFinishing is true while a finisher runs and false otherwise", kept_field_test's "a kept entry reads as its owner's rule of its held value and …" and "a saved world holding kept entries, loaded and resolved, …", and footfall_test's "A stretch some guest lies in after a cycle holds V + (S - V) / FOOTFALL_TIME, …" and "A kept entry of hungry footfall reads, k ticks after its tick, …". The report goes to Evan verbatim, and where he rules it and the inventory disagree, the wrong one is fixed.

## Medium

None. The feature introduces, samples, and emits no fields or flows, and changes no code.

## Spec changes

None. Agents and skills have no SPEC.md; the lens and the template are their own documentation.

## Files affected

- Modify: .claude/agents/reviewer.md
- Modify: .claude/skills/reviewing/SKILL.md
- Modify, only if Evan judges an inventory entry wrong: plans/focused-tests/RESEARCH.md

## Dependencies

- docs/testing.md and decision 0032 on main: met (611b26b).
- The capability's inventory in plans/focused-tests/RESEARCH.md: met.
- following-footfall's merges 6429bff and c52ce85: met.

## Out of scope

- When an audit runs, how its findings are fixed or filed, and the Status line: milestone-closing.
- The test-writer handling audit findings: milestone-closing.
- Auditing criteria and spec statements as artifacts in their own right: the capability's open question. The lens only names the criterion behind a case.

## Open questions

None.
