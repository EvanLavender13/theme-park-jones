# 0033. Rule 1 covers a test's setup

Status: Accepted, 2026-10-03

## Context

Rule 1 of docs/testing.md limited only what a test's expected outcome depends on. The first closing audit, of following-footfall, passed a kept-field case that reaches a later tick by replacing text in a save, since its expected values were sound. That case breaks whenever the save layout changes, though the code is still right, which is the cost rule 1 exists to prevent.

## Decision

Rule 1 also covers a test's setup: a case never reaches its state by depending on an exact text layout, such as editing a save's text.

## Consequences

Closing audits report such setups under rule 1. The kept-field case is already listed for rewriting in plans/focused-tests/RESEARCH.md.
