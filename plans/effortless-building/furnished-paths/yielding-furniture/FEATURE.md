# Feature: Yielding Furniture

## Summary

Furniture stops standing on things. Each furnished side's line now follows the paving's outline around bends, rounded corners, and tapers, and runs on into the next segment's side, so furniture follows curves without a gap where a point splits a path. The line stops at a junction or a free end. Along the line, a piece can't stand where it would overlap paving of either kind, a walkway, a box, the entrance, the frontage strip in front of a door, a piece laid before it, or the park's edge. The places where it can stand make the side's stretches, and the pieces spread evenly within each stretch. Placing a shop beside furniture removes the pieces it covers and re-spreads the rest of that stretch, and pieces elsewhere don't move. The ghost shows this before the commit, through furnished-segments' furniture ghost. A new park file, tests/parks/furnished-paths.park, holds every case, and the park world checks cover it.

## Acceptance criteria

1. Furniture follows curves. On tests/parks/furnished-paths.park, pieces stand along the outside and the inside of each rounded corner, tight and wide, and along the taper's curve, each square to the paving's edge where it stands and facing it. Where two segments meet at a bend or a split point, both furnished on that side with one spacing, furniture runs on from one to the other with no gap: the two pieces nearest the point stand as far apart as neighbours. At a junction, pieces stop short of the junction's mouth, and at a free end, short of the end. (capture of furnished-paths.park, and the plot in PLAN.md's Task 9)
2. Furniture yields. On furnished-paths.park, no piece overlaps paving of either kind, a walkway, a box, the entrance, the frontage strip in front of a door that has a walkway, or another piece, and none reaches outside the park. Within each stretch of free verge, the pieces are evenly spaced. (the check in PLAN.md's Task 9, which counts each kind of overlap and expects 0 of each, and its plot)
3. Placing a shop beside a furnished segment removes the pieces under it and its frontage strip, and re-spreads the pieces left in that stretch. Every piece in any other stretch keeps its key and its pose. The shop is accepted wherever it would be accepted without furniture. (the check in PLAN.md's Task 10)
4. In play, furniture yields as criteria 1 to 3 say, with freehand input. Draw guest paths with bends, a junction, a rounded corner, and two widths, and furnish them. Then place a shop beside furniture, draw a guest path and a backstage path across it, and drag a point of a furnished path. Before each commit, the ghost shows the pieces removed in the delete tint and the re-spread pieces in their colors. After the commit, the park shows what the ghost showed. Undo and redo restore the furniture. (play, PLAN.md's Task 11)
5. tests/parks/furnished-paths.park holds guest paths at MIN_PATH_WIDTH, the default width, and MAX_PATH_WIDTH, a taper, a tight and a wide rounded corner, junctions, shops fronting furnished segments, connectors crossing their verges, and a backstage path. Every guest segment has furnishing, both kinds appear, and one side and both sides each appear. The park world checks cover it. (ctest)
6. The simulation is unchanged for parks without furnishing. The parks tpj_scenarios --slice-parks remakes, fed.park loaded and saved again among them, and the full park that windows-release tpj_bench --full-park writes are the same text as from main before this feature. (the check in PLAN.md's Task 12)
7. The build has no warnings, ctest passes, and scripts/tidy.sh is clean. (PLAN.md's Task 13)

## Tests

None. furnished-paths.park joins the park world checks, which cover the furnishing intent and the pieces it lays.

## Medium

The feature adds no field or flow. The pieces stay derived data that display reads through parkFurniture (decision 0025). seat-places makes them publish into the medium.

sim/furniture now also reads park intent through parkBoxes, parkEntrances, and footprintOf, and reads doors and walkways through routes' parkDoors and keyedWalkways. It reads each side's line through parkSurface's sides. It reads nothing private to another module (principle 6).

## Spec changes

src/sim/park/SPEC.md, Paved surface: replace the paragraph on body sides with:

    parkSurface also gives each side of each segment: the line along the paving's outline beside it. For each segment with a ground line, in key order, it gives its left side and then its right side, seen walking from its From point to its To point. A side runs along the segment's body, from where the body stops nearer the From point to where it stops nearer the To point. Suppose only one other segment with a ground line uses an end's point, and the two don't leave the point in the same direction. There the outline turns from this side to the other segment's side, around the edge, the taper's curve, or the step between them. The side runs on along that turn as far as halfway along it, and it marks that end as running on. At every other end, the side stops where the body stops. sidePoints gives the positions a side runs through, from its From end to its To end, and the side says which of them is where its body starts. If a side's body ends behind where it starts, or the side has no length, the side is left out.

src/sim/furniture/SPEC.md:

- Boundary, first sentence: "furniture reads intent only through parkSegments, parkPathPoints, parkBoxes, parkEntrances, footprintOf, and parkSurface (sim/park/SPEC.md), and doors and walkways only through routes' parkDoors and keyedWalkways (sim/routes/SPEC.md)."
- Pieces, second paragraph, second sentence: "Its key is derived from its segment, its side, where its stretch starts along the segment, and its place in that stretch."
- Laying, replaced with:

      Pieces stand on each side of a guest segment that has furnishing, its left or its right, seen walking from its From point to its To point. Each side has its own kind and spacing. The pieces stand along that side's line, as parkSurface gives it, on the verge just outside the paving. Each piece is square to the line where it stands and faces the paving. The line follows the paving around a bend, a rounded corner, or a taper, as far as halfway to the next segment's side, so furniture follows curves. It stops where the segment's body meets a junction or ends.

      A piece can't stand where it, with a margin around it, would overlap paving of either kind, a walkway, a box, the entrance, a door's frontage strip, or a piece laid before it. It also can't stand where it would reach outside the park. A door's frontage strip runs straight out from the face the door is on, as wide as that face, to where the door's walkway ends. A door without a walkway has none. Segments are laid in key order, each one's left side first.

      The places along a side where a piece can stand make its stretches. Pieces in a stretch are evenly spaced, as close to the side's spacing as the stretch allows and never so close that neighbours touch. Where the side stops, they keep clear of its end. Where it runs on, the nearest piece stands half the spacing from its end, so furniture runs on around a bend at the spacing. If a stretch is too short for one piece, it has none. If a piece would overlap one laid before it, as on the inside of a tight bend, it is left out.

      If an edit leaves a stretch and everything that blocks it as they were, the stretch's pieces keep their keys and poses, wherever else the edit changes the side. Furniture never makes an edit refused. Where a box or a path takes the place of pieces, they yield (principle 5).

Public interface, sim/park/surface.h. BodySide becomes SegmentSide:

    // One side of a segment, seen walking from its From point to its To point: its segment, whether
    // it is the left side, the Count positions of its surface's SidePoints from First that it runs
    // through, from its From end to its To end, the index among them of where its body starts,
    // and whether it runs on into the next segment's side at its From end and at its To end.
    struct SegmentSide {
      EntityKey Segment = NULL_KEY;
      bool Left = false;
      size_t First = 0;
      size_t Count = 0;
      size_t Body = 0;
      std::array<bool, 2> RunsOn{};

      bool operator==(const SegmentSide &) const = default;
    };

    // PavedSurface's Sides holds SegmentSides, and it gains, last:
    std::vector<ParkPoint> SidePoints;

    // The positions the side of the surface runs through.
    std::span<const ParkPoint> sidePoints(const PavedSurface &surface, const SegmentSide &side);

## Files affected

- Modify: src/sim/park/SPEC.md, src/sim/park/surface.h, src/sim/park/surface.cpp
- Modify: src/sim/furniture/SPEC.md, src/sim/furniture/furniture.cpp
- Create: tests/parks/furnished-paths.park
- Modify: plans/effortless-building/furnished-paths/MILESTONE.md (the Medium section's list of what furniture reads)

## Dependencies

- furnished-segments: furnishing intent, the furniture module, and the furniture ghost. Merged.
- routes' parkDoors and keyedWalkways. Present.

## Out of scope

- Seat places, and furnishing full.park: seat-places.
- Pieces filling the inside of a bend more densely than the spacing allows, or packing around obstacles by moving neighbours: a piece that would overlap one laid before it is left out.
- Furniture along a junction's own edges, between two of its arms: the line stops where each body meets the junction.
- Player control of the verge gap and junction clearance: the milestone's deepening candidate.

## Open questions

None. The margin around pieces, the clearance where a side stops, and how finely the line is tested are tuning values that PLAN.md sets and playing adjusts.
