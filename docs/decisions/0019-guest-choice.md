# 0019. Guest choice by scored utility with a softmax pick

Status: Accepted, 2026-09-26

## Context

Guests must choose destinations plausibly, without herding, and explain choices in terms of their actual decision factors (principle 8), while the simulation stays deterministic (principle 10).

## Decision

Each candidate destination is scored as a sum of weighted factor terms, such as need relief, route distance, price, and queue. The guest picks by a softmax draw from a seeded per-guest random stream. The factor breakdown of the choice is kept so the guest can explain it. Weights live in one tunable table; per-guest personality may scale them later.

## Consequences

The random generator and its distributions are project code, not the standard library's distributions, whose outputs differ between implementations. A test can check that the recorded factor terms reproduce the score.
