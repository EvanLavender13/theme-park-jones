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
3. Identify required context: Parent artifacts the reviewer must read. A `MILESTONE.md` review requires its `CAPABILITY.md`. A `PLAN.md` review requires its `FEATURE.md`. A code change requires FEATURE.md when one exists, the SPEC.md of every module it touches, and docs/principles.md. Do not summarize these for the reviewer; pass paths so the reviewer reads them itself.
4. Dispatch via the Agent tool: Use `subagent_type: reviewer`. The dispatch prompt contains only:
    - The artifact path or paths.
    - The artifact type.
    - The required context paths.
    - Any specific focus area the user requested (optional).
    - Nothing else. See "Dispatch hygiene" below.
5. Receive the findings: A plain-text report from the subagent.
6. Present the findings to the user verbatim: Do not summarize, reorder, soften, or add commentary. The point of dispatching to a fresh subagent is wasted if this session filters its output.
7. Ask the user what to act on: Do not propose fixes yourself. The user decides which findings to address and which skill to invoke next.

## Dispatch hygiene

The dispatch prompt must not contain:

- Your assessment of the artifact's quality.
- Your guess at what the reviewer will find.
- Justification for design choices made in the artifact.
- Reassurance ("this should be fine, just check").
- Any framing that anchors the reviewer toward or away from particular conclusions.

The reviewer needs the artifact, the context paths, and the type. Nothing else.

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
- Reject findings with reasoning. Do not argue.
- Re-dispatch a narrower review on a specific finding.

You do not edit the artifact. You do not invoke another skill on the user's behalf.

## When the reviewer returns "No issues found."

Pass that result through unchanged. Do not add caveats, do not re-run the review, do not suggest the reviewer might have missed something. The reviewer was thorough; that is the contract.