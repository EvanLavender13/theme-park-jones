# Feature: Paving Overlay

## Summary

The food overlay shades the guest paving itself instead of a band of ground beside each guest path. It draws the guest paving's own pieces, and a shader colors each pixel by the food availability at that pixel's overlay place. A pixel of a segment's paving takes the place on that segment's line nearest it. A pixel of a junction's paving takes the nearest place on the lines of the segments there. Where paving of several owners covers a pixel, the nearest of their places wins. The value at a place is exact: the CPU computes availability at each place along a line where it can bend, and it is linear between them. The pieces and lines are built only when the paths change. Each tick uploads only the values at the bends. The edge lines stay on top. Zero is a near white. The food tooltip shows over shaded paving only, for the same place.

## Acceptance criteria

1. With the Food overlay on, all guest paving is shaded, and nothing else is: not the ground beside paths, door walkways, or backstage paths. The edge lines draw on top of the shading, and boxes, guests, and ghosts over it. Confirmed by a capture of main-street.park with `--ticks 3000 --overlay food`, compared with one without `--overlay food`. Also by play: in main-street.park with the overlay on, widen a side path as far as Edit path allows, and draw a sharp corner between two guest paths 16 m wide. Look close at both, and at a junction, a taper, and a rounded corner. No guest paving is left unshaded.
2. Each pixel shows the availability at its overlay place. Where a wide street's paving runs past a narrow side path's line, the street's paving shows the street's values. Where paving of two unjoined guest paths overlaps, each pixel shows the nearer line's value, steady as the camera moves. Confirmed by play: in main-street.park with the overlay on, look close at the T where the 10 m street meets a 3 m side path, then draw a guest path overlapping the street without joining it, and orbit the camera over the overlap.
3. Along each guest segment, availability is linear between the places availabilityBends gives, so the shading is exact between them. Confirmed by a check the implementer runs: a scratch program in build/paving-overlay/ that loads main-street.park and warm.park, steps 3000 ticks, and for each pair of consecutive bends on a line compares availabilityValues at three places between them with the straight blend of the two bends' values. Expected: it prints the largest difference over all of them, below 1e-9.
4. Zero shows a near white, unlike every ramp color, the tan of guest paving, and the blue gray of backstage paving. Confirmed by a capture of new.park with `--overlay food`, whose one path reaches no shop, beside the main-street.park capture of criterion 1.
5. Hovering shaded paving shows the food tooltip for the pixel's overlay place, and its value is the one the color shows. Hovering the ground beside a path, a door walkway, or a backstage path shows no tooltip. Confirmed by play in main-street.park with the overlay on, including over the T of criterion 2.
6. While a ghost is shown, the shading follows the candidate park. Widening a segment with Edit path shades the new width under the ghost, and a path the Delete tool hovers shows no shading. Confirmed by play in main-street.park with the overlay on.
7. Showing the overlay costs less per tick than the band did. Confirmed by the runtime report's comparison from before and after: on each stress park, the food-overlay stage's median is lower after than before, and the app's frame times with the overlay shown are no worse.
8. No spec describes a band, OVERLAY_BAND, or foodNear. Confirmed by `git grep -n -i "band overlay\|OVERLAY_BAND\|foodNear" -- src`, which prints nothing.

## Tests

None. The overlay is a view. docs/testing.md checks views by play and capture, and the feature adds no state.

## Medium

- Park intent's derived shape (decision 0025): parkSurface(world) without walkways, for its guest pieces and each piece's owner, and parkSegments, for each guest segment's key and points. render reads both to build the overlay's shape.
- Networks: the guest network's segment carriers, through parkNetwork. render copies their line points into the shape. legible reads their stops and the network's nodes to find bends.
- Route distance: legible reads each source's entries at the stops of each guest segment's carrier, through sampleFieldAtPlaces, to find where its better end changes.
- Food offer: legible reads each source's supplied offer, as availabilityValues does, for its relief and wait.
- Candidate worlds: the shape, the shading, and the tooltip read previewedWorld, as today.

The feature registers no state.

## Spec changes

src/render/SPEC.md. In Boundary, "The band overlay shades by values it is given, and views supplies them." becomes "The paving overlay shades by values it is given, and views supplies them." The list entry "band_overlay.h for OVERLAY_BAND, the reach of the food tooltip" becomes "paving_overlay.h for buildOverlayShape, overlayPlace, and OVERLAY_HEIGHT", and after the list add "views calls paving_overlay.h for shadeOverlay." The list entry "picking.h for groundAtCursor and entityAtCursor" becomes "picking.h for groundAtCursor, pointAtCursor, and entityAtCursor".

In the renderer section, "Each of setParkMesh, setGuestMesh, setOverlayMesh, and setGhostMesh replaces the mesh set by its previous call. If a mesh is empty, nothing is drawn for it." becomes "Each of setParkMesh, setGuestMesh, and setGhostMesh replaces the mesh set by its previous call, and setOverlayShape and setOverlayShades replace the paving overlay's shape and shades. If a mesh or the overlay's shape is empty, nothing is drawn for it." In the next paragraph, "It draws the terrain, then the food overlay, then the park mesh, then guests, all lit and depth tested." becomes "It draws the terrain, then the park mesh, then the paving overlay, then guests, all lit and depth tested." and "Paths, boxes, and guests draw over the overlay where they lie above it." becomes "The overlay draws over the guest paving and under its edge lines, and boxes and guests draw over it." The section "Band overlay" becomes:

    ## Paving overlay

    buildOverlayShape(world) gives the guest paving's overlay shape: each piece of the paved surface that belongs to a guest segment or to a point where guest segments meet, without door walkways, the lines of the guest segments, and which pieces of different owners overlap. It depends only on the park's path points and segments, so it changes only when they do.

    Each spot of the shape has an overlay place, which overlayPlace(shape, point) gives, or none when the shape holds no piece there. A spot of a segment's paving takes the place on that segment's line nearest it. A spot of a point's paving takes the nearest place on the lines of the guest segments at that point. Where paving of several owners holds a spot, the nearest of their places wins, and ties go to the lower segment key.

    shadeOverlay(shape, places, values, full) gives the shades: for each of the shape's lines, the values at the given places on it, in ascending distance. Between two consecutive places on a line, a value blends linearly. The renderer colors each pixel of the shape by the value at its overlay place: a near white for none, and above none a perceptually uniform, colorblind-safe ramp from dark purple to yellow, reaching yellow at full. For the same `full`, a color means the same value in every park. The ramp keeps every positive value distinguishable from none, and none is distinguishable from both kinds of paving. The renderer finds each pixel's place in single precision, so a pixel within a millimeter of a tie between two lines may take either, and a pixel on the outline of an overlap may take the other piece's place.

    The shading lies OVERLAY_HEIGHT above the ground: over the guest paving and under its edge lines, ghosts, boxes, and guests. The ground beside paths, door walkways, and backstage paths are not shaded.

In Picking, after the paragraph on groundAtCursor, add: "pointAtCursor gives the point at a height above the ground under a cursor, where its ray meets that level. It gives none when the eye is not above that level or the ray does not descend. groundAtCursor is pointAtCursor at height 0."

src/legible/SPEC.md. The app's list entry "food.h for foodNear" becomes "food.h for foodAvailability". views's sentence becomes "views calls availability.h for availabilityValues and availabilityBends." The section "Path places" is removed. After the paragraph on availabilityValues, add: "availabilityBends(world, definition) gives the places along each guest segment's line where its availability can bend: each stop of the line, and inside each edge each place where a supplied source's route turns from one end to the other, or its effective time reaches a point of the curve. They come in ascending segment key, then ascending distance. Between two consecutive bends on a line, availability is linear in distance, so the values at the bends give it exactly everywhere."

src/views/SPEC.md. The section "Availability overlays" becomes: "availabilityShades(world, shape, definition) gives render's shades of an overlay shape: the definition's availability at each of availabilityBends' places, saturating at its Full. The app shades its food overlay with FOOD_AVAILABILITY. tpj_bench times the same call, so what is timed is what is shown. For another need, the shades come from the same call with that need's definition."

src/app/SPEC.md. "The Food overlay checkbox shows food availability on the ground for the previewed park (views/SPEC.md). While the overlay is shown, hovering the ground shows the food value there and, for each shop that contributes, the terms that make up the value." becomes "The Food overlay checkbox shows food availability on the guest paving of the previewed park (views/SPEC.md). Its shape is built again only when the previewed park's path points or segments change. While the overlay is shown, hovering shaded paving shows the food value at the overlay place of the point under the cursor at the overlay's height (render/SPEC.md) and, for each shop that contributes, the terms that make up the value. Hovering anywhere else shows no food tooltip."

src/bench/SPEC.md. In Stages, "the food overlay" becomes two entries, "the food overlay's shape" and "the food overlay's shades". The result sentence becomes "It also gives a result computed from its work: a hash, a vertex count, or a count of values." In tpj_bench, "The result name is hash, vertices, or frames." becomes "The result name is hash, vertices, values, or frames."

## Files affected

- Rename and rewrite: `src/render/band_overlay.h` to `src/render/paving_overlay.h`, and `src/render/band_overlay.cpp` to `src/render/paving_overlay.cpp`
- Create: `src/render/shaders/overlay.vert`, `src/render/shaders/overlay.frag`
- Modify: `src/render/CMakeLists.txt`, `src/render/renderer.h`, `src/render/renderer.cpp`, `src/render/SPEC.md`
- Modify: `src/legible/availability.h`, `src/legible/availability.cpp`, `src/legible/food.h`, `src/legible/food.cpp`, `src/legible/CMakeLists.txt`, `src/legible/SPEC.md`
- Delete: `src/legible/path_place.h`, `src/legible/path_place.cpp`
- Modify: `src/views/availability_overlay.h`, `src/views/availability_overlay.cpp`, `src/views/SPEC.md`
- Modify: `src/render/picking.h`, `src/render/picking.cpp`, `src/app/input/cursor.h`, `src/app/input/cursor.cpp`
- Modify: `src/app/scene/scene_sync.h`, `src/app/scene/scene_sync.cpp`, `src/app/scene/scene_uploads.h`, `src/app/scene/scene_uploads.cpp`, `src/app/application.cpp`, `src/app/ui/food_tooltip.h`, `src/app/ui/food_tooltip.cpp`, `src/app/ui/tooling_ui.cpp`, `src/app/SPEC.md`
- Modify: `src/bench/stages.h`, `src/bench/stages.cpp`, `src/bench/report/internal/lines.cpp`, `src/bench/SPEC.md`

## Dependencies

- navigable-networks/wide-routes: guest paving matches the segments drawn.
- sim/park's parkSurface and each piece's owner. routes' route distance inside an edge.
- SDL 3.4's fragment storage buffers.

## Out of scope

- Shading door walkways. It stays in the milestone's deepening candidates.
- Drawing the paving itself with a shader. The overlay's pipeline is where paving textures could start, but nothing here changes how the paving draws.

## Open questions

None.
