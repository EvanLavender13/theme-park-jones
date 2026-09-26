# 0020. Quality as authored curves combined by geometric mean

Status: Accepted, 2026-09-26

## Context

Principle 5 and decision 0004 make everything but physical validity a gradient. The shape of those gradients decides how context turns into quality and how inputs trade off.

## Decision

Each input maps to a 0..1 score through a piecewise-linear curve authored as data. Scores combine by a weighted geometric mean with a floor, so a weak input drags quality down but never to zero. Capacities such as throughput take the minimum of their limiting inputs.

## Consequences

Curves can be plotted and tuned in the tooling UI. Each input's share of the result is attributable through the log of the geometric mean, which lets attribution sum to the total (principle 8).
