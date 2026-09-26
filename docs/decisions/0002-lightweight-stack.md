# 0002. A lightweight stack instead of an engine

Status: Accepted, 2026-09-26

## Context

The game's distinguishing systems (intent resolved into geometry, fields and flows, a deterministic simulation separate from rendering) do not map onto a heavyweight engine's object model, and fighting an engine costs more than building the few pieces actually needed.

## Decision

No heavyweight engine. The stack is a rendering layer, an ECS, and an immediate-mode UI for tooling, each chosen as a separate library decision.

## Consequences

The rendering layer is chosen in 0009. The ECS and the UI library are still open questions.
