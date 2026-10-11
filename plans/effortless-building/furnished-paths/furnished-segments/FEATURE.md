# Feature: Furnished Segments

## Summary

The player furnishes guest paths with a Furnish brush. They pick benches, tables, or none, a spacing, and one side or both in the Tools panel, hover a guest segment to see the furniture as a ghost, and click to apply it. Furnishing is saved segment intent, set by a new SetFurnishing command. It follows a segment through splits, merges, and corner splits, and undo and redo restore it. A new sim module, furniture, lays the pieces in resolution along the straight sides of each furnished segment's paving, evenly spread and clear of the segment's ends. Each piece is a derived entity, and the park and every ghost draw them. This is the core feature of furnished-paths. yielding-furniture makes the pieces yield to boxes, walkways, other paving, and junction mouths, and follow curves.

## Acceptance criteria

1. The Tools panel has Furnish. With it chosen, the panel shows Kind (None, Benches, Tables), Spacing as a slider in meters, and Sides (One side, Both sides), starting at Benches on both sides. Hovering a guest segment highlights it, and the ghost shows the pieces the candidate adds or moves in their colors, and the pieces it removes in the delete tint. With One side, the side is the one the pointer is on. A click commits that SetFurnishing, and with Kind None it removes the segment's furnishing. Over a backstage segment or bare ground, nothing is highlighted, no ghost shows, and a click commits nothing. (play: on a new park, draw a few guest paths freehand, with a sharp bend and a junction, and furnish them with each kind, spacing, and side)
2. Pieces stand on the verge beside each furnished side of the segment's paving, outside it, facing it, along the straight part of the side. They are evenly spaced, as close to the set spacing as the side's length allows, and keep clear of the side's ends. A side too short for one piece has none. Benches and tables have their own shapes and colors. (play, as in criterion 1, and a capture of the park saved from it)
3. Furnishing is saved and edited as intent. Saving and opening a furnished park gives back the same furniture. Drawing a path across a furnished segment, inserting a point in it, or dragging its point leaves furniture on every part. Removing a point between a furnished and an unfurnished segment leaves the merged segment furnished on the same side of the paving. Inserting a point on a rounded corner's arc keeps furniture on both segments. Deleting a furnished segment and undoing brings its furniture back, and undo and redo restore every furnishing change. (play)
4. The park world checks hold the furnishing intent and the pieces through a new chosen edit of noon.park, and its candidate equals its commit. (listed test, run with ctest)
5. The simulation is unchanged. The parks tpj_scenarios --slice-parks remakes, fed.park loaded and saved again among them, and the full park tpj_bench --full-park writes with windows-release are the same text as from main before this feature. (the check in PLAN.md's Task 17)
6. The build has no warnings, ctest passes, and scripts/tidy.sh is clean. (the check in PLAN.md's Task 19)

## Tests

- The park world checks gain one chosen edit of noon.park, in tests/sim/park_world_checks_test.cpp: furnishing noon.park's longest guest segment with benches on both sides, at a spacing of 10 m, with SetFurnishing, while guests are in the park. It requires that the candidate's parkSegments gives that segment the furnishing, and that the candidate's parkFurniture gives at least one piece. Like the other chosen edits, its candidate must equal the world that commits it, and both are among the checked worlds, so the checks hold the furnishing intent and the furniture-piece type.

## Medium

The feature adds no field or flow. Furnishing is park intent and SetFurnishing tentative intent, which decision 0025 places outside principle 3. The pieces are derived data that display reads through parkFurniture, as it reads parkSurface. seat-places makes them publish into the medium.

sim/furniture reads intent only through parkSegments, parkPathPoints, and parkSurface (sim/park/SPEC.md).

## Spec changes

src/sim/SPEC.md, Schemas and the park: the registration list becomes the medium; park intent and its commands; routes; furniture; operations; guests; last, the routes finisher.

src/sim/park/SPEC.md:

- Boundary: addParkIntent registers the intent types entrance, path-point, path-segment, box, shop-stock, opening-hours, and furnishing. addParkEdits registers the sixteen commands.
- Intent, new bullet after the segment's: "A guest segment can have a furnishing: a kind of furniture, bench or table, a spacing in meters, and a side, left, right, or both, seen walking from its From point to its To point. parkSegments gives each segment's furnishing, or none."
- Save lines, new bullet: "a segment's furnishing, only when it has one, in a furnishing line: its kind, spacing, and side." After the kind names: "Furniture kinds are saved as bench and table, and sides as left, right, and both."
- Commands: "Sixteen commands change intent", with SetFurnishing listed before RestoreIntent. New sentence after SetOpeningHours': "SetFurnishing names a segment and a furnishing, or none." A RestoreIntent segment record holds "a segment's kind, two points, width, edge radii, and furnishing". Acceptance: "For SetFurnishing, it means the key holds a guest segment, and any furnishing it gives has a known kind and side and a spacing that is finite and above zero." RestoreIntent's acceptance adds "every furnishing it holds is on a guest segment, with a known kind and side and a spacing that is finite and above zero".
- Effects: the AddPath split rule reads "The segment keeps its key, kind, width, furnishing, and edge radius at its From end" and "A new segment of the same kind, width, and furnishing runs on from each split point". MovePoint's two bullets keep "furnishing" with the key, kind, width, and edge radii. RemovePoint's bullet: "The one with the lower key keeps its key, kind, and far end, takes the wider of the two widths, and keeps its furnishing, or takes the other's when it has none." The corner split: "The last new segment takes the second segment's width and furnishing, and the rest the first's." New sentence after the effects list: "When a segment takes another's furnishing and the two run opposite ways, its side is turned, left for right." New bullet: "SetFurnishing sets the segment's furnishing, or removes it, and changes nothing else. A segment with no furnishing holds no furnishing line."
- Paved surface, new paragraph after the edge lines': "parkSurface also gives the sides of each segment's body, the part of its paving between the surface at its two ends. For each segment with a ground line, in key order, it gives its left side and then its right side, seen walking from its From point to its To point, each from where it stops nearer the From point to where it stops nearer the To point. A side that stops at the To end no farther along than it stops at the From end is left out."
- editedSegments and editedSurface give none "for edits of boxes, of the opening hours, and of furnishing". A ParkEdit holds "any one of the sixteen commands".

src/sim/furniture/SPEC.md, new:

    # furniture

    This module serves the effortless-building capability (plans/effortless-building). It lays the furniture each guest segment's furnishing asks for, on the verge beside the paving, so a park holds hundreds of seats without the player placing each one (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

    ## Boundary

    furniture reads intent only through parkSegments, parkPathPoints, and parkSurface (sim/park/SPEC.md). addFurniture registers the derived type furniture-piece and its resolver. addPark calls it after addRoutes. Other modules read the furniture through parkFurniture and furnitureSize.

    ## Pieces

    Furniture is derived in each resolution from intent alone, so saves never hold it (principle 1). parkFurniture gives every piece, in key order. A FurniturePiece has its key, its segment, the side of the segment it stands on, its kind, and its pose. furnitureSize gives a kind's footprint, its width along the paving and its depth across it.

    Each piece is an entity, so flows can reach it. Its key is derived from its segment, its side, the stretch of that side it stands in, and its place in that stretch. A piece exists while a resolution lays it, and the resolution that no longer lays it removes it.

    ## Laying

    Pieces stand on each furnished side of a guest segment: the left side, the right side, or both, seen walking from its From point to its To point. They stand beside that side of the segment's body, as parkSurface gives it without walkways, on the verge just outside the paving, each with its front facing it. Each such side is a stretch. Pieces in a stretch keep clear of its ends, and they are evenly spaced, as close to the furnishing's spacing as the stretch allows and never so close that neighbours touch. If a stretch is too short for one piece, it has none.

src/tools/SPEC.md:

- Boundary: the ToolKinds gain Furnish. "setFurnishChoice sets what the Furnish tool lays: a kind of furniture or none, a spacing, and one side or both."
- Tools, new paragraph after Delete's: "Furnish sets the furnishing of the guest segment under the pointer, the first in key order whose width covers the pointer. Its edit is a SetFurnishing of that segment to the tool's choice: no furnishing when the choice has no kind, and otherwise the kind and spacing on both sides, or, for one side, on the side of the segment's line the pointer lies on. The tool marks that segment. Over no guest segment it gives no edit and marks nothing. A click commits the edit."
- History: "A segment's record holds its furnishing."

src/render/SPEC.md:

- Boundary: render also reads the furniture through parkFurniture.
- The park: "Each piece of furniture is drawn as a low box over its footprint, benches and tables each in their own color."
- Ghosts and highlights, with the candidate paragraph: "With the candidate world, the ghost also shows each piece of the candidate's furniture that the edit adds or moves, translucent in its kind's color, in the Lit part, and each piece of the world's furniture that the candidate lacks, in the delete tint, in the Tints part."

src/app/SPEC.md, Tools and the ghost: the tools list gains Furnish, after Place toilet. New paragraph: "While Furnish is chosen, the Tools panel shows Kind, as radio buttons None, Benches, and Tables, Spacing, as a slider in meters, and Sides, as radio buttons One side and Both sides. They set what the tool lays. The choice starts at Benches on both sides and lasts while the app runs."

Public interface:

sim/park/intent.h:

    // A kind of furniture.
    enum class FurnitureKind : uint8_t { Bench, Table };
    constexpr std::array<std::string_view, 2> enumNames(FurnitureKind /*value*/) { return {"bench", "table"}; }

    // A side of a segment, seen walking from its From point to its To point, or both.
    enum class FurnishedSide : uint8_t { Left, Right, Both };
    constexpr std::array<std::string_view, 3> enumNames(FurnishedSide /*value*/) { return {"left", "right", "both"}; }

    // What a guest segment's verge holds: a kind of furniture, the spacing it asks for, in meters, and
    // the side or sides.
    struct Furnishing {
      FurnitureKind Kind = FurnitureKind::Bench;
      double Spacing = 0.0;
      FurnishedSide Side = FurnishedSide::Both;
      bool operator==(const Furnishing &) const = default;
    };

    // ParkSegment gains, last:
    std::optional<Furnishing> Furnished{};

sim/park/surface.h:

    // Where one side of a segment's body runs, seen walking from its From point to its To point:
    // its segment, whether it is the left side, and where it starts, nearer the From point, and
    // ends.
    struct BodySide {
      EntityKey Segment = NULL_KEY;
      bool Left = false;
      ParkPoint Start;
      ParkPoint End;
      bool operator==(const BodySide &) const = default;
    };

    // PavedSurface gains, last:
    std::vector<BodySide> Sides;

sim/park/edits.h:

    // The segment's furnishing, or none to remove it.
    struct SetFurnishing {
      EntityKey Segment = NULL_KEY;
      std::optional<Furnishing> To;
      bool operator==(const SetFurnishing &) const = default;
    };

    // SegmentRecord gains, last:
    std::optional<Furnishing> Furnished{};

    // ParkEdit gains SetFurnishing before RestoreIntent; isAccepted and applyCommand gain overloads.

sim/furniture/furniture.h:

    // A kind's footprint: its width along the paving and its depth across it, in meters.
    constexpr FootprintSize furnitureSize(FurnitureKind kind);

    // A piece of furniture as other modules read it: its key, its segment, the side it stands on,
    // Left or Right, its kind, and its pose, facing the paving.
    struct FurniturePiece {
      EntityKey Key = NULL_KEY;
      EntityKey Segment = NULL_KEY;
      FurnishedSide Side = FurnishedSide::Left;
      FurnitureKind Kind = FurnitureKind::Bench;
      Pose At;
      bool operator==(const FurniturePiece &) const = default;
    };

    // Registers the derived type furniture-piece and the resolver furniture.
    void addFurniture(WorldSchema &schema);
    // Every piece, in ascending key order.
    std::vector<FurniturePiece> parkFurniture(const World &world);

tools/tools.h:

    // ToolKind gains Furnish, last.

    // What the Furnish tool lays: a kind, or none to remove furnishing, the spacing, and whether on
    // both sides or on the side the pointer is on.
    struct FurnishChoice {
      std::optional<FurnitureKind> Kind = FurnitureKind::Bench;
      double Spacing = 10.0;
      bool BothSides = true;
      bool operator==(const FurnishChoice &) const = default;
    };
    // ToolState gains: FurnishChoice Furnish;
    void setFurnishChoice(ToolState &tool, const FurnishChoice &choice);

## Files affected

- Modify: src/sim/SPEC.md, src/sim/park/SPEC.md, src/tools/SPEC.md, src/render/SPEC.md, src/app/SPEC.md
- Create: src/sim/furniture/SPEC.md, src/sim/furniture/furniture.h, src/sim/furniture/furniture.cpp
- Modify: src/sim/park/intent.h, src/sim/park/intent.cpp, src/sim/park/surface.h, src/sim/park/surface.cpp, src/sim/park/internal/components.h, src/sim/park/edits.h, src/sim/park/edits.cpp
- Modify: src/sim/park_schema.cpp, src/sim/CMakeLists.txt, cmake/layers.txt, tests/checks/layer_check_test.cmake
- Modify: src/tools/tools.h, src/tools/tools.cpp, src/tools/history.cpp
- Modify: src/render/park_mesh.h, src/render/park_mesh.cpp
- Modify: src/app/ui/tool_panel.h, src/app/ui/tool_panel.cpp, src/app/ui/tooling_ui.cpp, src/app/input/interaction.h, src/app/input/interaction.cpp
- Modify: tests/sim/park_world_checks_test.cpp (test pass)
- Modify: plans/effortless-building/furnished-paths/MILESTONE.md (feature list)

## Dependencies

- path-network's surface, splits, merges, corner splits, undo, redo, and RestoreIntent. Complete.
- noon.park, for the chosen edit. Present.

## Out of scope

- Yielding to boxes, frontage strips, walkways, other paving, other pieces, junction mouths, and the park's edge, and pieces along curves and tapers: yielding-furniture. Until then, a piece can stand on a box or another path's paving, and only straight sides are furnished.
- Seat places: seat-places.
- Furnishing on furnished-paths.park and full.park: yielding-furniture and seat-places.
- Painting several segments in one drag, and picking up a segment's furnishing: the milestone's deepening candidates.

## Open questions

None. Each kind's footprint, the verge gap, the clearance at a side's ends, the least room between pieces, the spacing slider's range, and the starting spacing are tuning values that PLAN.md sets and playing adjusts.
