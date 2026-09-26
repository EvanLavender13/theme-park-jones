# 0004. Building requirements are a gradient, not a gate

Status: Accepted, 2026-09-26

## Context

Urbek City Builder shapes buildings by their surroundings, but gates them on hard requirements. Hard gating fights the dabbling feel this game wants.

## Decision

A building always appears. Its form, quality, and success reflect how well its context is satisfied.

## Consequences

This is the building-specific form of principle 5. Resolvers need defined behavior across the whole range of context, including an empty one. The shape of the quality curves is an open question.
