---
name: reviewing
description: Use whenever the user asks to review, check, audit, sanity-check, or sign off on any project artifact — a plan, brief, feature, implementation plan, code change, or diff. Dispatches the work to the read-only `reviewer` subagent so the review runs with fresh context and cannot be biased by the current session. Returns findings to the user without modifying them.
---

# Reviewing

This skill dispatches a review to the `reviewer` subagent and presents the findings. It doesn't review the artifact itself, and it doesn't edit anything.

## Why a subagent

There are two reasons.

- Fresh context. The current session may have written the artifact under review. The subagent has no memory of that conversation and judges only from what is on disk.
- Read-only enforcement. The `reviewer` subagent's tool list leaves out `Edit` and `Write`, and Claude Code enforces that. Its instructions limit Bash to read-only git commands.

## Checklist

1. Identify the artifact: one file, a set of files, a commit range, or the staged diff. If it's unclear, ask the user.
2. Identify the artifact type: plan document, implementation plan, code change, or other. The subagent checks each type differently.
3. Identify required context: the parent artifacts the reviewer must read.
    - For a `MILESTONE.md` review, include its `CAPABILITY.md`. If the milestone is a slice member, also include its `SLICE.md`.
    - For a `SLICE.md` review, include docs/vision.md and the CAPABILITY.md and MILESTONE.md of every existing member.
    - For a `FEATURE.md` and `PLAN.md` review, include its `MILESTONE.md`, the SPEC.md of every module the feature touches, and docs/principles.md.
    - For a code change, include FEATURE.md if one exists, the SPEC.md of every module the change touches, and docs/principles.md.

    Don't summarize these for the reviewer. Pass the paths so the reviewer reads them itself.
4. Dispatch with the Agent tool: Use `subagent_type: reviewer`. Put only these in the dispatch prompt:
    - The artifact path or paths.
    - The artifact type.
    - The required context paths.
    - Any specific focus area the user requested (optional).
    - Nothing else. See "Dispatch hygiene" below.
5. Receive the findings: the subagent returns a plain-text report.
6. Present the findings to the user verbatim: Don't summarize, reorder, soften, or add commentary. If this session filters the output, there was no point in using a fresh subagent.
7. Ask the user what to act on: Don't propose fixes yourself. The user decides which findings to address and which skill to invoke next.

Planning briefs are the exception. If a planning skill dispatches a review of its own brief (CAPABILITY.md, MILESTONE.md, SLICE.md, or FEATURE.md with PLAN.md) before approval, replace steps 6 and 7 with these:

- Fix Minor findings directly.
- Fix Major findings directly when the fix follows from the principles and settled decisions and needs no new design choice.
- Bring the user only the Major findings that need a design choice. Give each one a sentence or two and a recommended option.
- Report the rest in one line, for example "review fixed A, B, and C".
- Dispatch the follow-up review only when a Major fix changed a core definition that other plans rely on. Before you dispatch it, check the fix against every kind of edit or case the definition covers.

For code reviews, and for reviews the user asks for directly, still follow steps 6 and 7.

## Dispatch hygiene

The dispatch prompt must not contain:

- Your assessment of the artifact's quality.
- Your guess at what the reviewer will find.
- Justification for design choices made in the artifact.
- Reassurance ("this should be fine, just check").
- Any framing that anchors the reviewer toward or away from particular conclusions.

The reviewer needs the artifact, the context paths, and the type. Nothing else.

## Review budget

Limit the number of reviews of one artifact, because a reviewer asked to find issues will keep finding smaller ones.

- Run one full review per artifact. Stage or commit the artifact first, so later fixes show up as a diff.
- Run at most one follow-up review. Run it only when the full review had Blocker or Major findings and their fixes changed the design, not just the wording. The follow-up checks that each finding was addressed. It reports only Blocker or Major problems that the fixes introduced. It doesn't re-review untouched parts.
- Don't run a third pass. Fix anything the follow-up finds without another review, or record it as an open question.

Later stages catch what plan reviews miss: milestone and feature reviews against their parents, the separate test pass, and slice closing. You can let a plan move on before it is flawless.

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

Include the full review's report verbatim. It is the reviewer's own output, not your assessment, so it doesn't break dispatch hygiene. Don't describe the fixes. The reviewer reads them from the artifact and the diff.

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
- Reject findings with reasoning. Don't argue.
- Dispatch the one follow-up review the review budget allows, when it applies.

Don't edit the artifact. Don't invoke another skill on the user's behalf.

## When the reviewer returns "No issues found."

Pass that result through unchanged. Don't add caveats, re-run the review, or suggest the reviewer might have missed something. The reviewer's job is to be thorough, so take the result as it is.