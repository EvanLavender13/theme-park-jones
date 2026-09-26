# 0018. Staff and supplies start as aggregate flows

Status: Accepted, 2026-09-26

## Context

Staff and supplies could be simulated as individual agents or as aggregate flows. Principle 9 has the player direct through standards and policies, not individual instructions.

## Decision

Supplies are conserved quantities moving along backstage network edges with travel delay and capacity. Staff are a capacity pool drawn over network distance. Any visible carts or workers are derived from the flows for presentation (principle 1).

## Consequences

Consumers such as shops see only the flow interface, so an agent-based implementation can later replace a flow's internals without changing them. Conservation of each flow is testable directly.
