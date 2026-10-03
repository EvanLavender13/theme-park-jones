# 0034. Tests speak of standards, not laws

Status: Accepted, 2026-10-03

## Context

docs/testing.md, the agents, the skills, and the focused-tests plans called the properties every world must hold "laws", such as the world-as-value laws. The word reads as more formal than what it names: properties the project holds its code to.

## Decision

docs/testing.md and everything that cites it say standards where they said laws. The meaning of every rule is unchanged.

## Consequences

Earlier decision records keep their wording, since accepted records are not rewritten. tests/integration/park_laws_test.cpp is renamed park_standards_test.cpp.
