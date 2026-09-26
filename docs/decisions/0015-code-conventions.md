# 0015. Code conventions: naming and layout only

Status: Accepted, 2026-09-26

## Context

SteelJones's CONVENTIONS.md combines naming and file rules with an orthodox C++ language subset (no exceptions, no RTTI, no allocating STL containers, C headers, a five-year wait on new features).

## Decision

Adopt the naming and file-layout rules only, in docs/conventions.md. The language-subset rules are not adopted; modern C++ is fine where it helps.

## Consequences

Determinism (principle 10) is enforced by tests, not by a language subset.
