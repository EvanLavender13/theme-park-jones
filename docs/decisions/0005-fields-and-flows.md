# 0005. Fields and flows as the interaction model

Status: Accepted, 2026-09-26

## Context

Cross-cutting mechanics felt wrong in the previous project because each system had one owner and interactions were wired directly between systems. Principle 3 calls for a shared medium instead.

## Decision

Things interact through fields and flows. Fields are non-rival: sampled, never consumed. Flows are rival: conserved, consumed, and moving along networks.

## Consequences

Every interaction between entities goes through a field or a flow, or is listed in docs/exceptions.md with a reason. Flows must conserve their quantities, which is testable. How each field is stored is a per-field implementation choice behind a common sampling interface.
