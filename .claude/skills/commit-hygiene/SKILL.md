---
name: commit-hygiene
description: Use whenever writing a git commit message. Enforces the project's commit format (Subsystem: imperative subject, optional short body, Co-Authored-By trailer) so commits pass the commit-msg hook on the first try. Use this even for trivial commits.
---

# Commit Hygiene

## Purpose

Produce commit messages that pass `.githooks/commit-msg`, the git hook that aborts a commit when its message breaks format.

## Format

```
<Subsystem>: <Imperative verb> <description>

<optional body, wrapped at 72 chars, at most 12 lines>

Co-Authored-By: Claude <noreply@anthropic.com>
```

## Subject rules

- Subsystem is one capitalized word followed by colon and a space: `Sim:`, `Render:`, `App:`, `Docs:`, `Plans:`, `Skills:`, `Hooks:`, `Build:`.
- Verb is imperative: `Add`, `Drop`, `Fix`, `Rename`, `Move`, `Refactor`. Not `Added`, `Adding`, or `Fixes`.
- Maximum 65 characters total.
- No trailing period.

## Body rules

Most commits do not need a body. Add one only when the diff cannot explain *why* the change happened.

When present:

- One blank line between subject and body.
- Each line at most 72 characters. Wrap manually.
- At most 12 lines, excluding the trailer and its blank line.
- No first-person narration: avoid `I added`, `I fixed`, and similar.
- No step narration: avoid `First`, `Then`, `Next`, `Finally`, `After that`.
- State the why, not the steps.

## Trailer rules

- Always present, one blank line before it.
- Use the `Co-Authored-By:` line your environment specifies. Absent one, use `Co-Authored-By: Claude <noreply@anthropic.com>`.

## Checklist

1. Read the staged diff. Identify the affected subsystem.
2. Write the subject in imperative mood.
3. Decide whether a body is necessary. Default: no.
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