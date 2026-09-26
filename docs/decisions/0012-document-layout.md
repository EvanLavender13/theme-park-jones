# 0012. Document layout

Status: Accepted, 2026-09-26

## Context

The project uses spec-anchored development: module specs persist and are kept in sync with code, and an abandoned spec is worse than none.

## Decision

Project-wide documents live in docs/: principles.md, design-notes.md (provisional), exceptions.md, vision.md, open-questions.md, and decisions/. Each module's spec is a SPEC.md in the module's source directory. CLAUDE.md at the root stays lean and points to these.

## Consequences

A diff that changes a module without touching its SPEC.md is easy to spot, and can be flagged mechanically later.
