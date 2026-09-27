# 0025. Intent, candidate resolution, and inspection sit outside principle 3

Status: Accepted, 2026-09-26

## Context

Principle 3 says things interact only through fields and flows. Planning the boxes-and-tubes slice found three cross-capability dependencies that are neither: capabilities deriving their part of the world from the player's park intent, resolving tentative intent into a candidate world for a preview, and tooling reading what entities publish about themselves for display. The principles gate requires such dependencies to be ruled on rather than planned around.

## Decision

All three sit outside principle 3, which governs interaction between things in the park, and none is an exception.

Park intent is the player's expression that the world is derived from (principle 1). Deriving from it is not an interaction between entities.

Candidate resolution is the same derivation applied to tentative intent in a copy of the world, so a preview can be exact (principle 8). The capability that shows a preview samples the candidate world's fields through the normal interface and never calls another capability's derivation directly.

Inspection records are what an entity publishes about itself for display and tests. Only tooling and tests read them; no park entity consumes them, so they cannot carry interaction.

## Consequences

Reviews cite this record rather than flagging these dependencies under principles 3 and 6. An inspection record that a park entity starts reading is an interaction and must become a field or flow. docs/exceptions.md stays empty for these.
