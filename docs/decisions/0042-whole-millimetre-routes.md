# 0042. Route distance adds lengths in whole millimetres

Status: Accepted, 2026-10-15.

## Context

changing-previews makes route distance update only the entries an edit changes, instead of searching the whole network from every source again. An exact update must give the same distance however an edge is cut. Sums of doubles don't. When a shop's connection cuts a path in two, the two halves' lengths can sum to a different last bit than the old edge's length. That difference reaches every node beyond the cut, for every source whose routes cross it, on every frame of a shop drag. An update would then have to visit all of those nodes, and a search from nothing could still disagree with it in the last bit.

## Decision

Each edge's length is the distance of its To stop and the distance of its From stop, each rounded to whole millimetres, subtracted. A distance at a node is the sum of those lengths along a shortest route, kept as a whole number of millimetres, and published in metres. Cutting an edge in two then leaves every distance beyond the cut exactly as it was. The user chose this on 2026-10-13.

## Consequences

Distances change by under a millimetre per edge. A tie between two routes of nearly equal length may break the other way. tpj_scenarios output changes once, in the feature that makes this change, and later features compare against its output.

A mover that walks its next steps adds up the rounded lengths exactly. A first stretch from a place inside an edge is added as it is, so a distance sampled inside an edge need not be whole millimetres.

scalable-runtime's CAPABILITY.md lists this among the exceptions to the rule that a speedup never changes the answer.
