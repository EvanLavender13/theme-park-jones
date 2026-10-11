# Implementation Plan: Furnished Segments

## Goal

The player furnishes guest segments with a Furnish brush, and the new furniture module lays the pieces along the straight sides of each furnished segment.

## Approach

Furnishing is a separate intent component on the segment's entity, as shop-stock is on a box, so unfurnished parks save exactly as before. parkSegments and SegmentRecord carry it as an optional value. Splits, merges, corner splits, and RestoreIntent copy it through two new store operations, which do nothing on a layout copy. parkSurface also gives each segment's body sides, from the corners it builds the body piece from. A resolver in the new module sim/furniture walks the furnished segments and lays evenly spread pieces on derived entities along those sides. The renderer draws them, and the ghost compares the candidate's pieces with the world's.

## Placement

Decision 0027 places each behavior this feature adds:

- Each segment's body sides: sim/park, in sim/park/surface.h, which builds the paved surface and already holds where each side stops.
- Furnishing intent, SetFurnishing, and how edits carry furnishing: sim/park, in sim/park/intent.h, sim/park/internal/components.h, and sim/park/edits.h. The park module owns all intent and every command that changes it.
- Laying furniture and parkFurniture: a new module, sim/furniture, in sim/furniture/furniture.h. It sits in its own layer between sim/routes and sim/operations. yielding-furniture reads routes' walkways, and seat-places' consumers in operations and guests read it. sim/park can't hold it, because sim/park lies below routes.
- Registering furniture: sim/park_schema.cpp, which composes the schema.
- The Furnish tool and its choice: tools, in tools/tools.h, beside the other tools.
- Furnishing in the history's records: tools/history.cpp, which builds those records.
- Drawing pieces, and their ghost: render, in render/park_mesh.h, beside boxes and walkways.
- The Furnish controls: app/ui/tool_panel.h, which draws the Tools panel. app/input/interaction.h holds the tool, so it passes the choice on. app/ui/tooling_ui.cpp only wires the two together.

## Tasks

### Task 1: Make the before parks

Files:
- Create, scratch: `build/furnished-segments/before/`

Step 1: Build main's tpj_scenarios, and tpj_bench with windows-release, since debug can't step the full park in reasonable time. Make the slice parks and the full park, before any change.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && cmake.exe --build --preset windows-release --target tpj_bench && mkdir -p build/furnished-segments/before && cp tests/parks/fed.park build/furnished-segments/before/fed.park && (cd build/furnished-segments/before && ../../windows-debug/tpj_scenarios.exe --slice-parks fed.park warm.park cut.park ../../../tests/parks/main-street.park warm-main-street.park ../../../tests/parks/facilities.park stocked.park day.park no-toilet.park noon.park evening.park) && build/windows-release/tpj_bench.exe --full-park build/furnished-segments/before/full.park && ls build/furnished-segments/before | wc -l`
Expected: `10`.

### Task 2: Spec the sim and park changes

Files:
- Modify: `src/sim/SPEC.md`, `src/sim/park/SPEC.md`

Step 1: Make the sim and park spec changes FEATURE.md lists, word for word.

### Task 3: Spec the furniture module

Files:
- Create: `src/sim/furniture/SPEC.md`

Step 1: Write the furniture spec FEATURE.md gives.

### Task 4: Spec the tool, the drawing, and the panel

Files:
- Modify: `src/tools/SPEC.md`, `src/render/SPEC.md`, `src/app/SPEC.md`

Step 1: Make the tools, render, and app spec changes FEATURE.md lists, word for word.

### Task 5: Add the public interface with stubs

Files:
- Modify: `src/sim/park/intent.h`, `src/sim/park/surface.h`, `src/sim/park/edits.h`, `src/sim/park/edits.cpp`, `src/sim/CMakeLists.txt`, `cmake/layers.txt`, `tests/checks/layer_check_test.cmake`
- Create: `src/sim/furniture/furniture.h`, `src/sim/furniture/furniture.cpp`

Step 1: Add FurnitureKind, FurnishedSide, Furnishing, and ParkSegment's Furnished to intent.h, as FEATURE.md gives them. Include `<optional>`. Add BodySide and PavedSurface's Sides to surface.h, as FEATURE.md gives them.

Step 2: Add SetFurnishing to edits.h, SegmentRecord's Furnished as its last member, SetFurnishing in ParkEdit before RestoreIntent, and its isAccepted and applyCommand declarations after SetOpeningHours'. Update the comments that say fifteen commands. In edits.cpp, define isAccepted for it as returning false, applyCommand as doing nothing, and planOf as giving no plan, with the box edits' planOf. Register it in addParkEdits before RestoreIntent.

Step 3: Create furniture.h with the declarations FEATURE.md gives, and furniture.cpp with addFurniture doing nothing and parkFurniture giving none. Give furnitureSize its values: a bench 1.8 m wide and 0.6 m deep, and a table 1.8 m wide and 1.6 m deep.

Step 4: Add `furniture/furniture.cpp` to tpj_sim's sources in src/sim/CMakeLists.txt, in name order. In cmake/layers.txt, add a line `sim/furniture` after `sim/routes`. In tests/checks/layer_check_test.cmake, add `sim/furniture` after `sim/routes` in the expected list, which the layer table's ctest case compares the table with.

Step 5: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build ends with no warnings and no errors.

### Task 6: Test pass

Dispatch the `test-writer` agent with FEATURE.md. It writes the test FEATURE.md lists and runs it once.

### Task 7: Hold furnishing as intent

Files:
- Modify: `src/sim/park/internal/components.h`, `src/sim/park/intent.cpp`

Step 1: In components.h, add FurnishingIntent, holding a Furnishing, with visitFields writing `kind`, `spacing`, and `side`, in that order. Comment it as a guest segment's furnishing, held only when it has one.

Step 2: In intent.cpp, register it as `furnishing`, DataKind::Intent, after opening-hours, and update addParkIntent's comment in intent.h. In parkSegments, after segmentsOf, set each segment's Furnished from the FurnishingIntent its key holds, or leave it empty.

### Task 8: Give each segment's body sides

Files:
- Modify: `src/sim/park/surface.cpp`

Step 1: Give Output a method that appends a BodySide to the surface's Sides when it wants the owner, as piece does. In buildSurface, in the loop that adds each shape's body piece, also add its left side, from shape.trimmed(0, LEFT) to shape.trimmed(1, RIGHT), and then its right side, from shape.trimmed(0, RIGHT) to shape.trimmed(1, LEFT). Leave out a side whose end lies no farther along shape.Along than its start. Shapes come in key order, so the sides do too. In hideJoined, move surface.Sides into the visible surface it builds, as it moves Pieces, so parkSurface keeps them.

Step 2: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 9: Apply SetFurnishing

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: In the first anonymous namespace, right after isKnown, add setFurnishingOf(world, segment, furnishing). It removes the segment's FurnishingIntent when the furnishing is empty, and otherwise sets it. After it, add isFurnishingValid(furnishing, kind): true when the furnishing is empty, or when the kind is guest, isKnown holds for its kind and its side, and its spacing is finite and above zero. Both come before the stores and namesValid, which use them.

Step 2: isAccepted for SetFurnishing: the key holds a PathSegmentIntent of kind guest, and isFurnishingValid holds for its To with guest kind. applyCommand: when accepted, setFurnishingOf.

### Task 10: Carry furnishing through splits, merges, and corner splits

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Give both stores two operations. `furnished(key)` gives the segment's furnishing: WorldStore reads its FurnishingIntent, and LayoutStore gives none. `setFurnished(key, furnishing)`: WorldStore calls setFurnishingOf on its world, and LayoutStore does nothing, since layouts never hold furnishing. Add a free function turned(furnishing) that swaps Left and Right and keeps Both, and keeps an empty one empty.

Step 2: In applySplit, set each new part's furnishing to the split segment's, read before the parts are added.

Step 3: In applyBendSplit, read both segments' furnishing before the second segment's end moves, and whether the first segment's From is the corner's point and whether the second segment's To is. Set each new part's furnishing to the first segment's, turned when the first's From is the point. Set the last part's to the second's instead, turned when the second's To is the point.

Step 4: Add `std::optional<Furnishing> Inherits{}` as PlanLine's last member, commented as the furnishing a reshaped segment takes when it has none. In planRemove, when low has no furnishing and high has one, set the line's Inherits to high's, turned unless `(low.To == command.Point) == (high.From == command.Point)`.

Step 5: In applyLine, when the line has a key: before the first part, if the store gives the key no furnishing and the line inherits one, set it. Give every later part the key's furnishing.

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build ends with no warnings and no errors.

### Task 11: Restore furnishing

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: In namesValid's segmentValid, also require isFurnishingValid for the record's Furnished with the record's kind.

Step 2: In applyRestore, after each Add segment is added, set its key's furnishing to the record's, and after each Set segment is set, set its furnishing to the record's, both through the store's setFurnished.

### Task 12: Lay the furniture

Files:
- Modify: `src/sim/furniture/furniture.cpp`, `src/sim/park_schema.cpp`

Step 1: In furniture.cpp, add a private derived component, `FurniturePieceState`, holding the segment, the side, the kind, and the pose, with visitFields over each, x, z, facing-x, and facing-z for the pose. Define the tuning values at the top: VERGE_GAP 0.3 m from the paving to a piece's front, END_GAP 1.0 m from a stretch's end to its nearest piece, and PIECE_GAP 0.4 m, the least room between neighbours. Add PIECE_PURPOSE as hashName("furniture-piece").

Step 2: Write the resolver layFurniture. First list every entity holding FurniturePieceState with keysWith. Then, if any segment in parkSegments is a guest segment with a furnishing and a ground line, take parkSurface(world) once, and lay pieces for each such segment, in key order, on each furnished side, Left then Right. That side's stretch is the BodySide in the surface's Sides with the segment's key and that side, if there is one: from its Start S to its End E, of length L, with d the unit direction from S to E. Its outward direction o is a quarter turn counter-clockwise of d, seen from above, for Left, and a quarter turn clockwise for Right, so it points off the paving. Last, destroy every listed entity this resolution didn't lay, so pieces go even when nothing is furnished any more.

Step 3: For a stretch, let the piece's size be furnitureSize of the kind, its least step be its width plus PIECE_GAP, its step be the larger of the spacing and the least step, and the room be L less twice END_GAP less the width. If the room is below 0, lay nothing. Otherwise let the gaps g be the room over the step, rounded to the nearest whole number, and lowered while g is above 0 and the room over g is below the least step. With g at 0, one piece stands at L/2. Otherwise piece i stands at END_GAP plus half the width plus i times the room over g, for i from 0 to g. Each piece's position is S plus d times that distance plus o times VERGE_GAP plus half the depth, and it faces minus o.

Step 4: Key each piece with createDerivedEntity(segment, PIECE_PURPOSE, index). The index has bit 63 set for Right. Bits 16 to 62 hold the stretch's start: S's distance along the segment's direction from its From point, whose position parkPathPoints gives, plus 512 m, in whole centimeters, rounded down. Bits 0 to 15 hold the piece's place in the stretch. Set the entity's FurniturePieceState with emplaceOrReplace.

Step 5: addFurniture registers FurniturePieceState as `furniture-piece`, DataKind::Derived, and the resolver as `furniture`. parkFurniture gives each entity holding it, in key order, as a FurniturePiece. In park_schema.cpp, include the header and call addFurniture after addRoutes, and update the comment there.

Step 6: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests`
Expected: the build ends with no warnings and no errors.

### Task 13: Record furnishing in the history

Files:
- Modify: `src/tools/history.cpp`

Step 1: In recordsOf, copy each segment's Furnished into its SegmentRecord.

### Task 14: Add the Furnish tool

Files:
- Modify: `src/tools/tools.h`, `src/tools/tools.cpp`

Step 1: Add Furnish as ToolKind's last value, FurnishChoice and setFurnishChoice as FEATURE.md gives them, and ToolState's `FurnishChoice Furnish` with a comment. Add FURNISH_MIN_SPACING 2.0 and FURNISH_MAX_SPACING 30.0 m, the spacing slider's range, with the other tool constants.

Step 2: In tools.cpp, add guestSegmentAt: the first guest segment in key order whose ground line passes within half its width of the point, as segmentAt finds any segment. Add furnishEdit: with the pointer on the ground and a guest segment under it, a SetFurnishing of that segment. Its To is empty when the choice has no kind. Otherwise it is the kind and spacing, on Both when BothSides, and else on Left when the pointer lies on the left of the line from the segment's From point to its To point, a positive dot product with the direction a quarter turn counter-clockwise, seen from above, and on Right otherwise.

Step 3: tentativeEdit gives furnishEdit for Furnish. pressPointer sets Holding for Furnish, as for Delete. highlightedEntity gives the guest segment under the pointer for Furnish while not holding. setFurnishChoice sets the tool's choice.

### Task 15: Draw the furniture and its ghost

Files:
- Modify: `src/render/park_mesh.h`, `src/render/park_mesh.cpp`

Step 1: In park_mesh.h, add furnitureHeight, 0.5 m for a bench and 0.75 m for a table, and furnitureColor, the bench box's color for a bench and {0.36, 0.52, 0.30} for a table. Add appendFurniture(mesh, world, alpha), which adds a box for each piece parkFurniture gives in its kind's color with that alpha, and appendFurnitureChanges(ghost, world, candidate), which adds to Lit each candidate piece whose key the world lacks or whose pose differs, in ghostColor of its kind's color, and to Tints each world piece whose key the candidate lacks, in DELETE_TINT.

Step 2: buildParkMesh calls appendFurniture after the boxes, at alpha 1. buildGhostMesh calls appendFurnitureChanges wherever it adds the candidate's walkways, on both of its paths. Update both comments.

### Task 16: Wire the Furnish controls

Files:
- Modify: `src/app/ui/tool_panel.h`, `src/app/ui/tool_panel.cpp`, `src/app/input/interaction.h`, `src/app/input/interaction.cpp`, `src/app/ui/tooling_ui.cpp`

Step 1: Add `{"Furnish", ToolKind::Furnish}` to TOOL_CHOICES after Place toilet. drawToolPanel takes the tool's FurnishChoice. While Furnish is current, after the tool choices it draws a separator, Kind as radio buttons None, Benches, and Tables, Spacing as a slider of doubles from FURNISH_MIN_SPACING to FURNISH_MAX_SPACING formatted "%.1f m", and Sides as radio buttons One side and Both sides. ToolPanelChoice gains `std::optional<FurnishChoice> Furnish`, set when any of them changes.

Step 2: Interaction gains chooseFurnish(choice), which calls setFurnishChoice on its tool. tooling_ui.cpp passes interaction.tool().Furnish to drawToolPanel and calls chooseFurnish when the panel gives a choice.

Step 3: Build the app.

Run: `cmake.exe --build --preset windows-debug --target ThemeParkJones`
Expected: the build ends with no warnings and no errors.

### Task 17: Confirm the parks are unchanged

Files:
- Create, scratch: `build/furnished-segments/after/`

Step 1: Make the same parks with this feature's build and compare them with Task 1's.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && cmake.exe --build --preset windows-release --target tpj_bench && mkdir -p build/furnished-segments/after && cp tests/parks/fed.park build/furnished-segments/after/fed.park && (cd build/furnished-segments/after && ../../windows-debug/tpj_scenarios.exe --slice-parks fed.park warm.park cut.park ../../../tests/parks/main-street.park warm-main-street.park ../../../tests/parks/facilities.park stocked.park day.park no-toilet.park noon.park evening.park) && build/windows-release/tpj_bench.exe --full-park build/furnished-segments/after/full.park && for f in fed warm cut warm-main-street stocked day no-toilet noon evening full; do cmp build/furnished-segments/before/$f.park build/furnished-segments/after/$f.park && echo "$f same"; done`
Expected: ten lines, from `fed same` to `full same`.

### Task 18: Play the brush and capture it

Files:
- Create, scratch: `build/furnished-segments/furnished.park`, `build/furnished-segments/furnished.bmp`

Step 1: Play a new park.

Run: `build/windows-debug/ThemeParkJones.exe`
Expected: Draw guest paths freehand: one with a sharp bend, one crossing it, and one ending on another's middle, and a backstage path. Choose Furnish. Hovering a guest segment highlights it and shows benches along both sides as a ghost. Hovering the backstage path or bare ground shows nothing. Click, and the benches stay. Set One side and hover each side of another segment: the ghost follows the pointer's side. Set Tables at 4 m, then None, on furnished segments: the ghost shows the new pieces, and the removed ones in the delete tint. Then do each edit in FEATURE.md's criterion 3 and see furniture as it describes, including Ctrl+Z and Ctrl+Y. Pieces stand outside the paving, face it, keep clear of each side's ends, and are evenly spaced. Report what was seen, including where a piece overlaps other paving at a sharp bend, which yielding-furniture fixes.

Step 2: Save the park as `build/furnished-segments/furnished.park` with Save park, then capture it.

Run: `build/windows-debug/ThemeParkJones.exe --park build/furnished-segments/furnished.park --capture build/furnished-segments/furnished.bmp && python3 -c "from PIL import Image; Image.open('build/furnished-segments/furnished.bmp').save('build/furnished-segments/furnished.png')"`
Expected: the capture shows the furnished paths with benches and tables in their colors, as played.

### Task 19: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 20: Commit and merge

Files:
- Modify: `plans/effortless-building/furnished-paths/MILESTONE.md`

Step 1: In MILESTONE.md's Features, move "Any edit's ghost shows the pieces it moves and removes." from yielding-furniture's entry to furnished-segments', since this feature draws the ghost for every edit.

Step 2: Stage only this feature's paths: src/sim/SPEC.md, src/sim/CMakeLists.txt, src/sim/park/, src/sim/furniture/, src/sim/park_schema.cpp, cmake/layers.txt, tests/checks/layer_check_test.cmake, src/tools/, src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/SPEC.md, src/app/ui/tool_panel.h, src/app/ui/tool_panel.cpp, src/app/ui/tooling_ui.cpp, src/app/input/interaction.h, src/app/input/interaction.cpp, src/app/SPEC.md, tests/sim/park_world_checks_test.cpp, plans/effortless-building/furnished-paths/MILESTONE.md, and plans/effortless-building/furnished-paths/furnished-segments/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Furnish guest paths with benches and tables`. If the format hook rewraps a file after the commit, add it and amend the commit before merging. Merge into main as implementing-features says, and rebuild windows-release.
