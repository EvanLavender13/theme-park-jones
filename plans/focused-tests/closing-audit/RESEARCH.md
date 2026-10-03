# Research: Closing Audit

## Which test cases does a milestone's audit read?

Each feature lands as one commit on its own branch, merged into main as "Merge: <feature-slug>" (implementing-features), and each feature's plans sit in a directory under its milestone named for the slug. A milestone's test cases are therefore the TEST_CASEs added or changed in the diffs of the merges named for its feature directories, each merge diffed against its first parent. That scope is exact even when other capabilities' merges land between them, which a diff over a date or commit range would not be. Fixture and park-file changes in those diffs are read only for what the cases that use them assert.

Following-footfall's two merges, kept-entries and footfall-on-kept-entries, change cases in tests/sim/cycle_test.cpp, tests/sim/medium/kept_field_test.cpp, tests/sim/medium/network_test.cpp, tests/sim/guests/arrivals_test.cpp, and tests/sim/guests/footfall_test.cpp, and the capability's RESEARCH.md inventory already judges several of them as rewrite or held. That makes following-footfall a milestone whose right answers are partly known in advance, so it serves as the audit's first run and its calibration.

Rejected: auditing every test file a milestone touched, whole — it re-reads cases other milestones own and buries the milestone's own findings. A commit range from the MILESTONE.md commit to the close — it sweeps in other capabilities' merges.

## How can an audit by an agent be trusted?

An agent judging against a rubric is trusted only after its judgments are compared with labelled cases and its disagreements are read; agreement on a labelled set is the standard check for an LLM judge, and disagreements usually point at ambiguous rubric text rather than a bad judge. Two labelled sets exist here: the inventory's verdicts on the cases following-footfall added, and a planted case written to break one rule, which the audit must report. Where the audit and the inventory disagree, either may be wrong; Evan decides, and a disagreement that traces to unclear wording in docs/testing.md is fixed there through a decision record. Studies of LLMs detecting test smells find them useful but uneven across smell kinds, which is a further reason the report goes to Evan rather than being acted on unread.

Rejected: trusting the first run without a labelled comparison — nothing would show whether the auditor reads the rules as the 761-case audit did. A second auditor to vote with the first — it shares the first's bias and doubles the cost; Evan's reading is the tiebreak.

Sources: https://deepchecks.com/llm-judge-calibration-automated-issues/ — calibrating a judge against labelled cases and reading disagreements; https://langfuse.com/docs/evaluation/evaluation-methods/llm-as-a-judge — checking a judge against human labels before relying on it; https://arxiv.org/html/2506.07594 — LLMs detecting test smells, uneven by kind.

## Who audits: the reviewer, or a new agent?

The reviewer agent already reads docs/testing.md, has read-only tools, cites a location and evidence for every finding, drops what it cannot prove, and is dispatched through the reviewing skill's hygiene rules, which keep the closing session's opinion out of the prompt. What the audit needs beyond its code lens is a different artifact: test cases from several features read together, with the whole suite searched for where each property is already proven (rule 2), which no single feature's review can see. That is one more lens in reviewer.md and one dispatch template in the reviewing skill.

Rejected: a new test-auditor agent — a second file holding instructions about tests, which is how the rules scattered before decision 0032, with tools and output identical to the reviewer's. Auditing at each feature's review instead of the milestone's close — the per-feature review already checks its tests against docs/testing.md, and it is the cross-feature view it lacks.
