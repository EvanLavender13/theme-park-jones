# 0006. Operational infrastructure is optional but its absence has a cost

Status: Accepted, 2026-09-26

## Context

Real parks vary: not every real coaster has a transfer track. Requiring such infrastructure would be less realistic, not more, and would gate the player (principle 5).

## Decision

Operational infrastructure such as transfer tracks and maintenance bays may be absent, but absence has a cost. For example, a ride without storage track must close for maintenance.

## Consequences

Infrastructure is modelled as capacity for a flow (such as maintenance), so its absence degrades the flow instead of blocking construction.
