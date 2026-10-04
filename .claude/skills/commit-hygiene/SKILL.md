---
name: commit-hygiene
description: Use whenever writing a git commit message. Enforces the project's commit format (Subsystem: imperative subject, optional short body, Co-Authored-By trailer), so commits pass the commit-msg hook on the first try. Use this even for trivial commits.
---

# Commit Hygiene

## Purpose

Write commit messages that pass `.githooks/commit-msg`. That git hook aborts a commit if its message breaks the format.

## Format

```
<Subsystem>: <Imperative verb> <description>

<optional body, wrapped at 72 chars, at most 12 lines>

Co-Authored-By: Claude <noreply@anthropic.com>
```

## Subject rules

- Start with the subsystem, written as one capitalized word followed by a colon and a space: `Sim:`, `Render:`, `App:`, `Docs:`, `Plans:`, `Skills:`, `Hooks:`, `Build:`.
- Use an imperative verb: `Add`, `Drop`, `Fix`, `Rename`, `Move`, `Refactor`. Don't use `Added`, `Adding`, or `Fixes`.
- Keep the subject to at most 65 characters in total.
- Don't end the subject with a period.

## Body rules

Most commits don't need a body. Add one only when the diff can't explain *why* the change happened.

If there is a body:

- Put one blank line between subject and body.
- Keep each line at most 72 characters. Wrap lines by hand.
- Keep it to at most 12 lines, not counting the trailer and its blank line.
- Don't narrate in the first person, such as `I added` or `I fixed`.
- Don't narrate steps with `First`, `Then`, `Next`, `Finally`, or `After that`.
- State the why, not the steps.

## Trailer rules

- Always include it, with one blank line before it.
- Use the `Co-Authored-By:` line your environment specifies. If it specifies none, use `Co-Authored-By: Claude <noreply@anthropic.com>`.

## Checklist

1. Read the staged diff and identify the affected subsystem.
2. Write the subject in imperative mood.
3. Decide whether a body is needed. Usually it isn't.
4. If yes, write the body explaining why, not what.
5. Add the trailer with a blank line before it.
6. Commit.

## Examples

Subject only (most common):

```
Skills: Drop status frontmatter from briefs

Co-Authored-By: Claude <noreply@anthropic.com>
```

Subject and body:

```
Hooks: Strip uniform leading indent from heredoc bodies

Templates indent the heredoc body for readability, which made the
linter reject otherwise-valid messages. Stripping the common indent
preserves relative indentation so code blocks inside the body still
render correctly.

Co-Authored-By: Claude <noreply@anthropic.com>
```