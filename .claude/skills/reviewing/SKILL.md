---
name: reviewing
description: Use whenever the user asks to review, check, audit, sanity-check, or sign off on any project artifact — a plan, brief, feature, implementation plan, code change, or diff. Dispatches the work to the read-only `reviewer` subagent so the review runs with fresh context and cannot be biased by the current session. Returns findings to the user without modifying them.
---

# Reviewing

This skill dispatches a review to the `reviewer` subagent and presents the findings. It does not review the artifact itself. It does not edit anything.

## Why a subagent

Two reasons.

- Fresh context. The current session may have written the artifact under review. The subagent has no memory of that conversation and judges only from what is on disk.
- Read-only enforcement. The `reviewer` subagent's tool list excludes `Edit` and `Write`, which Claude Code enforces. Its instructions limit Bash to read-only git commands.

## Checklist

1. Identify the artifact: One file, a set of files, a commit range, or the staged diff. Ask the user if ambiguous.
2. Identify the artifact type: Plan document, implementation plan, code change, or other. The subagent applies a different lens to each.
3. Identify required context: Parent artifacts the reviewer must read. A `MILESTONE.md` review requires its `CAPABILITY.md`, plus its `SLICE.md` when it is a slice member. A `SLICE.md` review requires docs/vision.md and the CAPABILITY.md and MILESTONE.md of every existing member. A `PLAN.md` review requires its `FEATURE.md`. A code change requires FEATURE.md when one exists, the SPEC.md of every module it touches, and docs/principles.md. Do not summarize these for the reviewer; pass paths so the reviewer reads them itself.
4. Dispatch via the Agent tool: Use `subagent_type: reviewer`. The dispatch prompt contains only:
    - The artifact path or paths.
    - The artifact type.
    - The required context paths.
    - Any specific focus area the user requested (optional).
    - Nothing else. See "Dispatch hygiene" below.
5. Receive the findings: A plain-text report from the subagent.
6. Present the findings to the user verbatim: Do not summarize, reorder, soften, or add commentary. The point of dispatching to a fresh subagent is wasted if this session filters its output.
7. Ask the user what to act on: Do not propose fixes yourself. The user decides which findings to address and which skill to invoke next.

Planning briefs are the exception. When a planning skill dispatches the review of its own brief (CAPABILITY.md, MILESTONE.md, or SLICE.md) before approval, steps 6 and 7 are replaced:

- Fix Minor findings directly.
- Fix Major findings directly when the fix follows from the principles and settled decisions without a new design choice.
- Bring the user only the Major findings that need a design choice. Give each one a sentence or two and a recommended option.
- Report the rest in one line, for example "review fixed A, B, and C".
- Dispatch the follow-up review only when a Major fix changed a core definition that other plans rely on. Before dispatching it, check the fix against every kind of edit or case the definition covers.

Code reviews and reviews the user asks for directly still follow steps 6 and 7.

## Dispatch hygiene

The dispatch prompt must not contain:

- Your assessment of the artifact's quality.
- Your guess at what the reviewer will find.
- Justification for design choices made in the artifact.
- Reassurance ("this should be fine, just check").
- Any framing that anchors the reviewer toward or away from particular conclusions.

The reviewer needs the artifact, the context paths, and the type. Nothing else.

## Review budget

A reviewer asked to find issues will keep finding smaller ones, so reviews of one artifact are capped.

- One full review per artifact. Stage or commit the artifact first, so later fixes show up as a diff.
- At most one follow-up review, and only when the full review had Blocker or Major findings whose fixes changed the design, not just wording. The follow-up checks that each finding was addressed and reports only Blocker or Major problems the fixes introduced. It does not re-review untouched parts.
- No third pass. Anything the follow-up finds is fixed without another review, or recorded as an open question.

Later stages catch what plan reviews miss: milestone and feature reviews against their parents, the separate test pass, and slice closing. A plan does not need to be flawless before it moves on.

## Dispatch template

```
Review the following artifact.

Artifact: <path>
Type: <plan-document | implementation-plan | code-change | other>
Required context (read these before reviewing): <comma-separated paths>
Focus (optional, only if the user specified): <one short phrase>

Follow your standard procedure. Return findings in the standard format.
```

## Follow-up dispatch template

Include the full review's report verbatim. It is the reviewer's own output, not your assessment, so it does not break dispatch hygiene. Do not describe the fixes; the reviewer reads them from the artifact and the diff.

```
Follow-up review of the following artifact.

Artifact: <path>
Changes since the full review: <git command that shows them, or "the artifact was revised throughout">
Required context (read these before reviewing): <comma-separated paths>

Full review report:
<verbatim report>

Follow your follow-up procedure. Return findings in the standard format.
```

## After the review

The user chooses one of:

- Accept some or all findings and invoke the appropriate planning or implementation skill to address them.
- Reject findings with reasoning. Do not argue.
- Dispatch the one follow-up review the review budget allows, when it applies.

You do not edit the artifact. You do not invoke another skill on the user's behalf.

## When the reviewer returns "No issues found."

Pass that result through unchanged. Do not add caveats, do not re-run the review, do not suggest the reviewer might have missed something. The reviewer was thorough; that is the contract.