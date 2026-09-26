# 0017. Dear ImGui (docking branch) for tooling UI

Status: Accepted, 2026-09-26

## Context

Decision 0002 calls for an immediate-mode UI for tooling: overlays, inspectors, attribution panels, tuning.

## Decision

Dear ImGui on the docking branch, pinned to a commit, with its official SDL3 and SDL_GPU backends. Its SDL_GPU backend ships precompiled SPIR-V, so it fits decision 0009 unchanged.

## Consequences

Player-facing UI is a separate, open question.
