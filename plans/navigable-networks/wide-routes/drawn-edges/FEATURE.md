# Feature: Drawn Edges

## Summary

Shops and depots snap to the paving as it is drawn, and doors connect by their distance from that paving. Today the frontage snap and the connection test both work from a segment's center line plus half its width. That matches the drawing only beside a plain straight side. Beside a taper, at a free end, around a corner with an edge radius, at a sharp outside corner, at an inside corner, and at a junction, the snap puts the box on the paving or gives no pose, and the connection test misses doors the player sees right beside the paving. After this feature, the frontage snap fronts the nearest place on the drawn edge lines. At a sharp outside corner, the box lines up with one of the two sides. Along a curve, it turns smoothly with the pointer. If the box doesn't fit there, as in an inside corner, it fronts the nearest place along the paving where it does, with 1 m clear around its front and sides. A door connects to the segment whose drawn paving lies nearest, within 3 m of that paving. The connector runs straight out of the door to the drawn paving, and on from there to the segment's center line, so its walkway leaves the door straight even beside a curve. On the inside of a curve, the box backs off the paving just far enough to clear it, and its facing turns smoothly along the curve. A first version, played by the user, slanted the walkway toward the center line on the outside of a curve and made the box jump to the curve's ends on the inside. Both are fixed here.

## Acceptance criteria

1. In main-street.park with its boxes removed, and in a new park with one wide guest L whose inside corner curves and whose outside corner stays sharp, the frontage snap gives an accepted pose with a front-door connector at every shape the paving takes. The shapes are a straight side, a free end, the taper, the outside and inside of a rounded corner, the outside and inside of the block's corners, a junction pocket, and the L's sharp outside corner and curved inside corner. Depots get the same beside the backstage path's side, its rounded outside corner, and its free end. Each shape is tried with three pointers placed by hand, not on exact lines. Every part of each front face lies at least 1 m from the paving. A shop placed by hand 2.5 m beyond the L's sharp outside corner also connects. (check: the probe in PLAN.md prints a table, every row ending `accepted connected`)
2. In main-street.park with every box removed, every pointer on a 2 m grid near guest or backstage paving gets a frontage pose. Each pose the world accepts has a front-door connector. Each pose it refuses is listed with whether it still overlaps paving. The report gives the count of each. In the windows-release build, frontagePose takes under 5 ms on average over a grid of pointers in tests/parks/stress/full.park. (check: the sweep in PLAN.md)
3. The listed test passes.
4. The simulation of main-street.park, warm.park, fed.park, cut.park, supply.park, routes.park, sketch.park, and new.park is unchanged. Each door there either faces a straight side or a square end within reach, or lies far from any paving of its kind. degenerate-intent.park is left out: its shop at the origin stands on a free end's paving, which only a hand-written save allows, so its front door's connection may change. The world checks still cover it. (tpj_scenarios --compare on output from before and after)
5. The runtime report's resolution and preview stages on the stress parks are reported before and after, in the windows-release build. (check: tpj_bench, reported, not gated)
6. In the windows-release build, rebuilt after the change, the user opens main-street.park and places shops by hand beside the taper, around the bend at the street's east end, at the block's inside and outside corners, and in the pockets where the street meets the cross street. They also draw a wide path with a rounded corner and place a shop at its sharp outside corner. At each place, the ghost faces the paving as drawn, sits 1 m clear of it, shows valid wherever there's room, and the placed shop gets a walkway joined to the paving. (play, by the user)
8. Moving the pointer in small steps along the inside and the outside of a rounded corner moves the fronting box smoothly, with no jump in its place or its facing. This holds at main-street.park's bend at point 9 and block corners at points 7 and 14, inside and outside, and at the inside of the L park's corner. (check: the slide in PLAN.md, every sweep's largest step under 0.5 m and 5°)
9. Every walkway leaves its door straight out of the door's face, including beside curves and corners. (check: the probe in PLAN.md prints each placed box's walkway direction, every one within 1° of its door's facing)
7. src/tools/SPEC.md describes frontage against the drawn paving. src/sim/routes/SPEC.md measures reach to the drawn paving. src/sim/park/SPEC.md lists pieceDistance. The milestone, this capability's brief, and the slice say reach is measured to the drawn paving. (check: read each beside the code)

## Tests

One bug test, in tests/integration/, since the bug crosses the tools, sim/park's validity, and routes:

- Frontage beside a sharp outside corner. Setup: load tests/parks/new.park and resolve it. Add a guest path through three new points at (-60, -60), (-30, -60), and (-30, -30). Set both of the path's new segments to width 10, and set the middle point's corner radius to 12. The corner's inside then curves, and its outside stays a sharp corner near (-25, -65). Change: ask frontagePose for a shop with the pointer at (-22.5, -67.0), a few meters beyond that sharp corner, and apply AddBox with the pose it gives. Expected: frontagePose gives a pose, isAccepted accepts the AddBox, and after applying it and resolving, the guest network has a carrier keyed connectorKey(shop, Face::Front) for the new shop. Before this feature, frontagePose gives no pose there, because the pointer lies more than half the width, plus the setback, plus the shop's depth from both segments' center lines.

The test calls frontagePose, so tpj_integration_tests links tpj_tools as well as tpj_sim. tpj_tools needs no window, so the integration tests still run headless. Update the comment above its link line in tests/integration/CMakeLists.txt to say so.

## Medium

The feature adds no fields or flows and no state.

- Paved surface: sim/park gives the drawn paving through parkSurface, and pieceDistance measures how far a point lies from one of its pieces. It reads nothing new.
- Networks: routes reads the paved surface, without walkways, to measure each door's distance from each segment's paving. It reads it from intent alone, so loads and candidates derive the same networks. Consumers read the networks unchanged.
- Frontage: the tools read the paved surface's edge lines and pieces, and box sizes. Frontage state stays tool state and is never saved (principle 1).

## Spec changes

src/sim/park/SPEC.md:

- Boundary, in the paragraph starting "segmentLine, cornerRadiusFor": after "editedSurface,", add " pieceDistance,".
- Paved surface, at the end of the paragraph starting "parkSurface gives the paved surface", add: "pieceDistance gives how far a point lies from a piece, in meters, and 0 when it lies on the piece."
- Paved surface, in the walkway paragraph: replace "from a door to a place on a segment's ground line, joined to that segment." with "from a door to a place on or inside a segment's paving, joined to that segment."

src/sim/routes/SPEC.md:

- Boundary: replace "routes reads intent only through parkSegments, parkEntrances, parkBoxes, and footprintOf (sim/park/SPEC.md, decision 0025)." with "routes reads intent only through parkSegments, parkEntrances, parkBoxes, footprintOf, parkSurface, and pieceDistance (sim/park/SPEC.md, decision 0025)."
- Connectors: replace the two sentences "A door's distance from a segment is its distance from the segment's ground line, less half the segment's width: how far it stands from the segment's edge. If a segment of the kind a door serves lies within CONNECTION_REACH of it, the door gets a connector to the nearest such segment, the lower key winning a tie." with:

      A door's distance from a segment is how far it stands from the segment's paving, as parkSurface draws it without walkways. A segment's paving is the pieces it owns and the pieces of the points it ends at. A door on that paving is at distance 0. If a segment of the kind a door serves lies within CONNECTION_REACH of it, the door gets a connector to the nearest such segment. If several lie equally near, within JUNCTION_TOLERANCE, the one whose ground line lies nearest the door wins, then the lower key.

- Connectors: replace "A connector is a short straight carrier keyed connectorKey(entity, face). It runs from the door to the nearest point on that segment's ground line, and stops that segment's carrier there at a node." with:

      A connector is a short carrier keyed connectorKey(entity, face). It runs from the door straight out of the door's face to where that line first meets paving of its kind, and on from there to the nearest point on that segment's ground line, and stops that segment's carrier there at a node. Its length counts both parts. If that line meets no paving of its kind within CONNECTION_REACH, if the door already stands on paving, or if the nearest point on the ground line from the door lies straight out of its face, the connector runs straight from the door to the nearest point on the ground line from the door.

- Connectors: in the connectorWalkways paragraph, replace "It runs from the connector's door, along the door's face, to where the connector meets its segment," with "It runs from the connector's door, along the door's face, straight along the connector's first part. If the connector runs straight to the ground line, it ends there. Otherwise it ends straight out of the door, past where the connector meets the paving by half its width, or by the length of the connector's part from there to the ground line if that is less, so its square end lies inside the paving."

src/tools/SPEC.md:

- Boundary: replace "frontagePose gives the pose of a box that fronts a segment." with "frontagePose gives the pose of a box that fronts paving."
- Tools: replace the paragraph starting "Near a segment of the kind its front door serves" with:

      Near paving of the kind its front door serves, the box fronts that paving instead, in the pose frontagePose gives. A shop's front door serves guest paving, and a depot's serves backstage paving. The box fronts the paving as parkSurface draws it without walkways, at the place on its edge lines nearest the pointer. It faces the paving square to the edge line there, and the middle of its front face lies FRONTAGE_SETBACK outside the paving. Where that place is a corner of the edge lines, the box faces square to the corner's side whose outward direction lies nearer the direction to the pointer, or to the first side along the edge line on a tie. If the corner turns by FRONTAGE_CORNER_TURN or less, as between the short pieces of a curve, and the pointer lies between the outward directions of its two sides, the box faces the corner along the line from the pointer instead, so it turns smoothly around a curve. Along the inside of a curve, where the edge lines turn away from the paving by FRONTAGE_CORNER_TURN or less at each corner, the box fronts a blend of the nearby pieces of that curve, each weighted by how near the pointer it lies, so its place and facing change smoothly there too. A box fits where its footprint overlaps no paving of either kind, every part of its front face lies at least FRONTAGE_SETBACK from all paving, and its front door lies within CONNECTION_REACH of paving of the kind it serves, so it connects. If the box doesn't fit, it keeps its facing and moves the least distance that makes it fit: straight out, away from the paving, by up to FRONTAGE_PUSH, as on the inside of a curve, or along the edge by up to its own width, as beside a junction. If neither fits, it fronts instead the place on the edge lines nearest the pointer where it fits, moved out the same way. It tries places every FRONTAGE_STEP along the edge lines, within twice its own width of the first place, on edges whose outward direction lies within a quarter turn of the first place's. Each place first faces as a pointer the same distance straight out from it would make it face, so the box turns smoothly between places along a curve, and otherwise faces square to its edge. If none fits, it stays at the first place, which the world refuses if the box overlaps paving there. Paving is near when the pointer lies on paving of that kind, or within FRONTAGE_SETBACK plus the box's depth of one of its edge lines. If several edge lines are equally near, the box fronts the one that belongs to the lower key. If the box fronts paving when the button goes down, dragging doesn't turn it. While snapping is off, no box fronts paving.

Public interface:

    // sim/park/surface.h
    // How far the point lies from the piece, in meters: 0 when it lies on the piece.
    double pieceDistance(const SurfacePiece &piece, ParkPoint point);

    // tools/tools.h
    // Where a box fronts a corner of the paving's edge lines that turns by this much or less, in
    // radians, it turns with the pointer, as along a curve. At a sharper corner it lines up with one
    // side.
    inline constexpr double FRONTAGE_CORNER_TURN = std::numbers::pi / 6.0;
    // A fronting box that doesn't fit where it first fronts tries places this far apart along the
    // edge lines, in meters.
    inline constexpr double FRONTAGE_STEP = 0.25;
    // A fronting box that doesn't fit moves straight out by at most this, in meters, so its front
    // door stays within CONNECTION_REACH of the paving it fronts.
    inline constexpr double FRONTAGE_PUSH = CONNECTION_REACH - FRONTAGE_SETBACK;
    // The pose of a box of the kind that fronts the paving near the point, as tools/SPEC.md's Place
    // shop and Place depot state, or none when no paving of the kind its front door serves is near.
    std::optional<Pose> frontagePose(const World &world, BoxKind kind, ParkPoint point);

    // sim/routes/networks.h, comments only
    // Connectors that meet a carrier this close to each other, or to its end, stop there at one
    // node. Segments whose paving lies this close to equally near a door tie for it.
    inline constexpr double JUNCTION_TOLERANCE = 0.001;
    // A door farther than this from the paving of every segment of its kind has no connector.
    inline constexpr double CONNECTION_REACH = 3.0;

## Files affected

- Modify: src/sim/park/SPEC.md
- Modify: src/sim/park/surface.h
- Modify: src/sim/park/surface.cpp
- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/tools/SPEC.md
- Modify: src/tools/tools.h
- Modify: src/tools/frontage.cpp
- Modify: plans/navigable-networks/wide-routes/MILESTONE.md
- Modify: plans/navigable-networks/CAPABILITY.md
- Modify: plans/slices/good-paths/SLICE.md
- Modify: plans/effortless-building/path-network/MILESTONE.md, dropping the candidate this feature delivers: a box on the inside of a tight curve finds a place where its whole front face clears the paving by the setback.
- The test pass adds the bug test under tests/integration/ and links tpj_tools in tests/integration/CMakeLists.txt.

## Dependencies

- joined-walkways: walkways drawn joined to their segments, so a connected shop shows its walkway as paving. Merged.
- effortless-building/path-network: the paved surface with its edge lines, and the frontage snap. Complete.

## Out of scope

- Connectors that join a wide segment at its drawn edge, so routes don't count the walk to the center line. It stays a milestone candidate.
- Fronting a walkway, or keeping a box off another box's walkway. The world allows a box on a walkway, as today.
- Moving a fronting box clear of other boxes. The world refuses the overlap, and the ghost shows it.
- Frontage when moving a box. It stays a candidate of path-network.

## Open questions

None.
