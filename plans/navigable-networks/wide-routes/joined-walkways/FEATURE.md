# Feature: Joined Walkways

## Summary

A door's walkway is drawn as part of the paved surface, joined to the segment it reaches, the way a path drawn onto that segment joins it. Today the renderer lays each walkway over the paving as a separate strip, so the segment's edge line runs straight across it and the join looks like two paths that overlap without joining. After this feature, the segment's edge line opens where the walkway meets it. The walkway's own edge lines run along its sides from the door to the segment's edge and stop there, with sharp corners like any junction drawn with the tools. A walkway that crosses paving it doesn't join still shows each one's edge lines across the other, as unjoined paths do. Routes, connectors, and the simulation don't change. Only what the player sees changes.

## Acceptance criteria

1. In main-street.park, where each shop's front walkway meets the street, no edge line crosses the walkway. The walkway's two side edge lines run from the shop's front face to the street's edge and stop there. The same holds for each back door and the depot where they meet the backstage path, and for the entrance where it meets its path. (check: the scratch plot in PLAN.md, top-down at 20 pixels per meter around the street and the backstage path)
2. A capture of main-street.park in the windows-release build shows each walkway as part of the paving, with no edge line across any walkway at its path. (capture: windows-release, rebuilt after the change)
3. The simulation is unchanged. (tpj_scenarios --compare on output from before and after)
4. In the windows-release build, the user opens main-street.park, zooms in on the street, and sees each shop's walkway join the street the way a side path does. Placing a new shop by frontage beside a path gives a walkway that joins it the same way once placed. (play, by the user)

## Tests

None. The feature changes only what is drawn, which docs/testing.md checks by capture and play.

## Medium

The feature adds no fields or flows and no state.

- Networks: routes reads its own connectors and doors and gives each connector's walkway as a value: its kind, the segment it joins, its door, the direction out of the door's face, where it meets the segment's ground line, and its width. It reads nothing new from intent.
- Paved surface: sim/park adds walkways to the surface it builds from intent, joined to their segments by the rule it already uses for segments that share points. It never reads networks. It takes the walkways as given.
- The renderer composes the two: it asks routes for the walkways and sim/park for the surface with them.

## Spec changes

src/sim/park/SPEC.md, Paved surface. After the paragraph that starts "Edge lines run along each side of each segment", add:

    parkSurface can also add walkways. A walkway is a straight strip of paving of a kind and a width, from a door to a place on a segment's ground line, joined to that segment. Its end at the door lies along the door's face. Its other end is square to its line and lies WALKWAY_OVERLAP, 1 cm, past the place it meets, so a walkway that meets a segment's free end covers that end's edge line. Its piece and its edge lines belong to the segment it joins. Edge lines run along both its sides and across its far end. Its door end has none, since the walkway meets its box there. The walkway joins the segment as a segment joined at a point does: the segment's edge lines are hidden where they lie inside the walkway, and the walkway's edge lines are hidden where they lie inside paving joined to that segment. Where a walkway overlaps paving it isn't joined to, each one's edge lines run across the other. If a walkway's segment has no ground line, its width is not above zero, or its door and the place it meets lie closer together than MIN_SEGMENT_LENGTH, it adds nothing.

src/sim/routes/SPEC.md, Connectors. Add at the end:

    connectorWalkways gives a walkway (sim/park/SPEC.md) for each connector of a kind's network, in key order of the connector. It runs from the connector's door, along the door's face, to where the connector meets its segment, with the connector's width, joined to that segment. If the connector meets the segment at a path point, it is joined to the lowest-keyed segment of the kind that ends there.

src/render/SPEC.md, Boundary: the networks bullet adds connectorWalkways. buildParkMesh: replace "If a door has a connector, the door gets a walkway to the path it reaches, in its kind's width and color." with "If a door has a connector, its walkway is drawn as part of the paved surface, joined to the segment it reaches (connectorWalkways)." Remove "A walkway ends round where it meets its path. A walkway starts flush with its door's face." The ghost's walkways are drawn as before.

Public interface:

    // sim/park/surface.h
    // A strip of paving from a door to a segment, joined to it: its kind, the segment it joins, the
    // door, the unit direction out of the door's face, where it meets the segment's ground line, and
    // its width.
    struct Walkway {
      PathKind Kind = PathKind::Guest;
      EntityKey Joins = NULL_KEY;
      ParkPoint Door;
      ParkPoint Out;
      ParkPoint Meets;
      double Width = 0.0;

      bool operator==(const Walkway &) const = default;
    };

    // How far a walkway's far end lies past the place it meets its segment, in meters.
    inline constexpr double WALKWAY_OVERLAP = 0.01;

    // The park's paved surface with the walkways added, each joined to its segment.
    PavedSurface parkSurface(const World &world, std::span<const Walkway> walkways);

    // sim/routes/networks.h
    // A walkway for each connector of the kind's network, in key order of the connector.
    std::vector<Walkway> connectorWalkways(const World &world, PathKind kind);

## Files affected

- Modify: src/sim/park/SPEC.md
- Modify: src/sim/park/surface.h
- Modify: src/sim/park/surface.cpp
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/render/SPEC.md
- Modify: src/render/park_mesh.cpp
- Modify: plans/navigable-networks/wide-routes/MILESTONE.md

## Dependencies

- edge-reach: doors connect to wide segments, so main-street.park's street shops have walkways to check. Merged.
- effortless-building/path-network: the paved surface and its rule for hiding edge lines inside joined paving. Merged.

## Out of scope

- The ghost's walkways. While a shop is being placed, its new walkway still shows as a tinted strip over the paving. Backlog.
- Rounding a walkway's corners where it meets its segment. They stay sharp, like a junction's default.
- Validity against walkways. A box can still stand on another box's walkway, as today.

## Open questions

None.
