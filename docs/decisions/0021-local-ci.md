# 0021. Local hooks now, hosted CI later

Status: Accepted, 2026-09-26

## Context

The repository has no remote yet. Checks should be mechanical rather than written guidance.

## Decision

Git hooks in .githooks/: pre-commit formats staged files with clang-format, commit-msg enforces the commit format (ported per 0014), and pre-push builds linux-debug and runs its tests. When a GitHub remote exists, the same checks become an Actions workflow on Linux plus an MSYS2 Windows build.

## Consequences

Each clone activates the hooks once with git config core.hooksPath .githooks.
