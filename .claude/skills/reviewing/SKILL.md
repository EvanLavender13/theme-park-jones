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
- If the round had a Blocker or Major finding, dispatch a fresh full review after fixing it, as "Reviewing again" says.

For code reviews, and for reviews the user asks for directly, still follow steps 6 and 7.

## Dispatch hygiene

The dispatch prompt must not contain:

- Your assessment of the artifact's quality.
- Your guess at what the reviewer will find.
- Justification for design choices made in the artifact.
- Reassurance ("this should be fine, just check").
- Any framing that anchors the reviewer toward or away from particular conclusions.

The reviewer needs the artifact, the context paths, and the type. Nothing else.

## Reviewing again

A plan isn't done while it has serious problems. Anything a plan misses turns up later as a surprise during implementation.

- Stage or commit the artifact before each review, so later fixes show up as a diff.
- If a round finds a Blocker or Major, fix it and dispatch a fresh full review. Repeat until a round finds none. No round is the last while a Blocker or Major remains, however many rounds it takes.
- If a round finds only Minor issues, fix them without another review. A reviewer asked to find issues keeps finding smaller ones.
- Before each new round, check the artifact yourself for the same kind of gap the last round found.
- If Majors keep coming after a few rounds, look for the root cause instead of patching each one. A common cause is mechanism written into a brief that should state only requirements. Fix the cause, then review again.

## Dispatch template

```
Review the following artifact.

Artifact: <path>
Type: <plan-document | implementation-plan | code-change | other>
Required context (read these before reviewing): <comma-separated paths>
Focus (optional, only if the user specified): <one short phrase>

Follow your standard procedure. Return findings in the standard format.
```

## After the review

The user chooses one of:

- Accept some or all findings and invoke the appropriate planning or implementation skill to address them.
- Reject findings with reasoning. Don't argue.
- Dispatch a fresh full review after fixes, as "Reviewing again" says.

Don't edit the artifact. Don't invoke another skill on the user's behalf.

## When the reviewer returns "No issues found."

Pass that result through unchanged. Don't add caveats, re-run the review, or suggest the reviewer might have missed something. The reviewer's job is to be thorough, so take the result as it is.