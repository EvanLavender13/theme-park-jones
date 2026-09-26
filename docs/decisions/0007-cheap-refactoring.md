# 0007. Refactoring the park is mechanically cheap

Status: Accepted, 2026-09-26

## Context

Players should be able to reshape a park as ideas change. Because the saved park is intent plus simulation state (principle 1), derived geometry can always be rebuilt.

## Decision

Moving or adding infrastructure regenerates derived geometry. Refactoring may still be costly in space or money, but never in tedious manual rework.

## Consequences

Derived geometry is never the source of truth and must be reproducible from intent, which is testable by regenerating from a save.
