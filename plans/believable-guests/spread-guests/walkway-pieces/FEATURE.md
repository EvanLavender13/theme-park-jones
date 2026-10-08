# Feature: Walkway Pieces

## Summary

Prepares the paving for guests to walk on. sim/park gains walkwayPiece, which gives the piece parkSurface adds for one walkway, so a reader can tell which piece is which walkway's. routes gains keyedWalkways, which gives each connector's walkway with the connector's key, so a reader knows which connector a walkway belongs to. paving-walk uses both to build the ground guests walk on. A one-time check, a scratch program that is never committed, then shows that the outline guests will walk inside closes, over every park in tests/parks, tests/parks/stress, and the user's parks in parks/. Nothing the player sees or the simulation does changes.

## Acceptance criteria

1. walkwayPiece(segments, walkway), given the park's segments as parkSegments gives them, gives exactly the piece parkSurface(world, walkways) adds for that walkway, or nothing when parkSurface adds nothing for it. parkSurface builds each walkway's piece with it, so the two can't differ. Confirmed by review, and by criterion 4's capture.
2. keyedWalkways(world, kind) gives the same walkways as connectorWalkways(world, kind), in the same order, each with the key of the connector it was made for. connectorWalkways gives each walkway keyedWalkways gives. Confirmed by review, and by criterion 4's capture.
3. The outline guests will walk inside closes. In every park in tests/parks, tests/parks/stress, and parks/, take the visible guest edge lines of parkSurface with each guest connector's walkway, and add each walkway's door end, from its piece's last corner to its first. Each of these lines starts and ends within 1e-5 m of a point on another of them. Confirmed by a check: the scratch program build/paving-holes/closure.exe prints `0 open ends` for every park. An earlier version of this check paired piece edges and found unpaired ones in main-street.park and parks/busted-path.park, so guests walk inside the outline instead of across pieces (the milestone's RESEARCH.md).
4. Nothing the player sees or the simulation does changes. `tpj_scenarios --compare` of windows-debug output over the scenarios and tests/parks/*.park before and after the feature reports no difference. A `--capture` of tests/parks/main-street.park before and after is the same file, byte for byte. Confirmed by both checks.
5. src/sim/park/SPEC.md and src/sim/routes/SPEC.md describe walkwayPiece and keyedWalkways. Confirmed by review.

## Tests

None. The functions are derived data with no state, the check is a one-time scratch program, and paving-walk's guests are their first consumer, checked in play (docs/testing.md).

## Medium

The feature adds no field or flow. walkwayPiece is part of park intent's derived shape that sim/park publishes (decision 0025). keyedWalkways is part of what routes publishes about connectors. believable-guests' paving-walk reads both.

## Spec changes

src/sim/park/SPEC.md:

- In the list of public functions under Boundary, after pieceDistance, add walkwayPiece.
- At the end of the walkway paragraph under Paved surface, add: "walkwayPiece gives the piece parkSurface adds for one walkway, given the park's segments as parkSegments gives them, or nothing when parkSurface adds nothing for it."

src/sim/routes/SPEC.md:

- In Boundary, "and their walkways through connectorWalkways" becomes "and their walkways through connectorWalkways and keyedWalkways".
- At the end of Connectors, add: "keyedWalkways gives the same walkways as connectorWalkways, in the same order, each with the key of the connector it was made for."

Public interface:

- src/sim/park/surface.h, after the Walkway struct and WALKWAY_OVERLAP:

      // The piece parkSurface adds for the walkway, owned by the segment it joins, given the park's
      // segments as parkSegments gives them, or none when it adds nothing for it.
      std::optional<SurfacePiece> walkwayPiece(std::span<const ParkSegment> segments,
                                               const Walkway &walkway);

- src/sim/routes/networks.h, after connectorWalkways:

      // A connector's walkway: the connector's key, and the walkway made for it.
      struct KeyedWalkway {
        EntityKey Connector = NULL_KEY;
        Walkway Way;

        bool operator==(const KeyedWalkway &) const = default;
      };

      // Each walkway connectorWalkways gives, in the same order, with its connector's key.
      std::vector<KeyedWalkway> keyedWalkways(const World &world, PathKind kind);

## Files affected

- Modify: src/sim/park/surface.h
- Modify: src/sim/park/surface.cpp
- Modify: src/sim/park/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/routes/SPEC.md
- Create, never committed: build/paving-holes/build.sh and build/paving-holes/closure.cpp (build/ is ignored by git)

## Dependencies

effortless-building's path-network and navigable-networks' wide-routes: the paved surface, walkways, and connectors. No sibling feature.

## Out of scope

- The guests' walkable paving, joining walkways to the paving they overlap, and anything guests do. paving-walk does these.
- Cutting joined paving into pieces that don't overlap. Guests don't need it, and the texturing item in plans/BACKLOG.md holds it.
- A permanent test of the paving's shape. docs/testing.md allows no test of one module's internals.

## Open questions

None.
