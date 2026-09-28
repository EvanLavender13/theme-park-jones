# Feature: Park View

## Summary

park-view puts the park on screen. A new part of tpj_render, park_mesh, builds the park's mesh on the CPU from intent read through the park module's public queries: each path is a flat ribbon its width wide, lying just above the ground along its ground line, and each box and the entrance is a box over its footprint with its front face lighter, with guest and backstage paths and shop and depot boxes told apart by color. The renderer uploads the mesh and draws it through a lit pipeline after the terrain, with reversed depth so ribbons never flicker through the grass. The app starts from the new-park template, or from the park file --park names, steps --ticks N cycles before its first frame, starts its camera framed on what the park holds, and with --hash prints the tick and hash tpj_scenarios would print and exits without a window. tests/parks/sketch.park, with both kinds of path and both kinds of box, is checked in, so the cross-build check runs it and a capture of it shows the view. Ghosts, highlights, and their translucent pass come with box-tools, which has the first ghost to draw.

## Acceptance criteria

1. appendPath lays a path's ribbon along its ground line: for a ground line of n points, n at least 2, it adds 2n vertices and 6(n - 1) indices, and the two vertices for point i are pathWidth apart along the right direction of the tangent the spec gives there, with point i at height PATH_LIFT as their midpoint, within 1e-3 m. Every normal points up, every color is pathColor(kind), and on a ground line that bends nowhere tighter than half the path's width, every triangle's winding faces up. A path whose ground line is empty adds nothing.
2. appendBox builds the open-bottomed box the spec gives over a pose's footprint: 20 vertices and 30 indices, each face's four vertices over its footprint corners at the ground and the height, within 1e-3 m, each with its face's unit outward normal, each triangle wound toward that normal, and the front face's color lightened as the spec gives while the others take the color. A pose with no footprint adds nothing.
3. Appending leaves what the mesh held unchanged, and every index it adds names a vertex it added.
4. meshBounds gives the least rectangle on the ground holding every vertex's x and z, and none for a mesh with no vertices.
5. buildParkMesh gives exactly the mesh made by appending each entrance, then each path, then each box, in key order, with the heights and colors the spec gives, and those five colors are distinct and opaque.
6. For a park file tpj_scenarios loads and a tick count N, `ThemeParkJones --park FILE --ticks N --hash` exits with status 0 and writes one line, `tick <t> hash <h>`, whose t and h equal those of the last line `tpj_scenarios --ticks N FILE` writes for the file. Without --park the line equals the one for tests/parks/new.park.
7. A command line the app spec does not accept, or a park file it cannot read or load, exits with a nonzero status and writes no hash line.
8. tests/parks/sketch.park holds at least one entrance, guest path, backstage path, shop, and depot, loads with makeParkSchema and saves back to identical text, gives every path a non-empty ground line, and is physically valid.
9. `ThemeParkJones --park tests/parks/sketch.park --capture out.bmp`, and the same without --park for the template, frame the whole park and show its paths flat on the ground and smooth along their curves, and its boxes and entrance as boxes with their fronts marked, with each kind told apart. Checked by looking at the capture, which settles the milestone's open question on how smooth paths look.

## Medium

None. The view reads park intent, which sits outside principle 3 (decision 0025), through parkEntrances, parkPaths, parkBoxes, groundLine, and footprintOf, and draws what it derives. The app reads the park file, saveWorld's text, through loadWorld with makeParkSchema, the encoding deterministic-simulation owns. It emits and supplies nothing.

## Principle checks

- Principle 1: the mesh is derived and never saved. buildParkMesh takes the world as const and registers nothing, and a world's save and hash are the same before and after its mesh is built.
- Principle 6: tpj_render reads intent only through the park module's public headers. The private header check covers it.
- Principle 10: the app with the renderer linked computes the same state as the headless runner (criterion 6), and the simulation still links no rendering library.

## Spec changes

src/render/SPEC.md: in the Contract, after the paragraph on createRenderer, add:

```markdown
setParkMesh uploads a park mesh, replacing the one drawn before; an empty mesh draws nothing. drawFrame draws the terrain, then the park mesh through a lit pipeline that shades it from its normals as the terrain is shaded, with a depth test and depth writes and back faces culled. Depth is reversed, the near plane at 1 and the far plane at 0 in a float depth buffer where the device has one, so precision holds across the camera's range and paths lying just above the terrain never flicker through it.
```

and after the Contract, add a section:

```markdown
## Park mesh

park_mesh.h builds the park's mesh on the CPU, so it can be tested without a GPU. It reads intent only through sim/park's public queries and geometry (sim/park/SPEC.md), and changes nothing. A ParkMesh is a list of ParkVertex, each a position, a unit normal, and an Rgba color in floats, and a list of 32-bit indices, three per triangle, each triangle wound counter-clockwise seen from the side its vertices' normal points to. Appending never changes what a mesh already holds, and the indices it adds name the vertices it adds. meshBounds gives the least rectangle on the ground, GroundBounds MinX, MinZ, MaxX, and MaxZ, holding every vertex's x and z, or none for a mesh with no vertices.

appendPath adds a path's ribbon: flat, pathWidth wide, PATH_LIFT, 2 cm, above the ground, along its groundLine. For a ground line of n points, n at least 2, it adds 2n vertices, two for each point in order: the left edge, at the point minus the right direction times half the width, and then the right edge, at the point plus it. The right direction of a unit tangent (tx, tz) is (-tz, tx). The tangent at the first point is the direction of the segment after it, and at the last point that of the segment before it. At a point between, it is the normalized sum of the unit directions of the segments before and after it, or the one before when that sum has zero length. The normals point up and the colors are pathColor(kind). Each consecutive pair of points adds two triangles covering the quad between their four vertices. Where the ground line bends tighter than half the width, or turns straight back, the ribbon folds over itself, and triangles there can face down and be culled. An empty ground line adds nothing.

appendBox adds a box of a height and color over a pose's footprint for a size: a top at the height over the footprint's corners, and four sides from the ground to the height over its four edges, with no bottom. Each face has four vertices with the face's unit outward normal and two triangles: the top's normal points up, the front side's is Forward, the back side's -Forward, the right side's Right, and the left side's -Right. The front side, over the footprint's front face, takes the color lightened, each of red, green, and blue c becoming c + (1 - c) * 0.4 and alpha kept; the others take the color. A pose with no footprint adds nothing.

buildParkMesh gives a world's park mesh: each entrance from parkEntrances, with ENTRANCE_SIZE, ENTRANCE_HEIGHT 5 m, and ENTRANCE_COLOR, then each path from parkPaths, then each box from parkBoxes, with boxSize(kind), boxHeight(kind), 4 m for a shop and 6 m for a depot, and boxColor(kind), each in the order the query gives. The colors of guest paths, backstage paths, shops, depots, and the entrance are distinct and opaque.
```

src/app/SPEC.md: after the Main loop section, add:

```markdown
## Park

The app starts from makeNewPark(1), or from the park file --park names, loaded with makeParkSchema, and resolves it. Before the first frame it steps the world --ticks cycles with no commands. Whenever the world's intent, as parkEntrances, parkPaths, and parkBoxes give it, differs from the intent last drawn, it builds the park mesh and gives it to the renderer. When it builds the first mesh, and the mesh has vertices, it frames the camera on the mesh's bounds: the focus moves to their center, and the distance becomes their half diagonal divided by the sine of half the vertical field of view, so a sphere around them fits the view, within the camera's distance limits.
```

and replace the Command line section's paragraph with:

```markdown
--frames N exits after N frames. --capture PATH writes the last frame to PATH as a BMP and exits after 3 frames unless --frames says otherwise. Together they make the app usable for automated visual checks.

--park PATH starts from that park file. --ticks N, a decimal count, steps N cycles before the first frame, and N is 0 without it. --hash writes `tick <t> hash <h>` to standard output after them, with t the world's tick in decimal and h hashWorld's value as 16 lowercase hexadecimal digits, as tpj_scenarios writes them for the same file and ticks, and exits with status 0 without opening a window.

An unknown option, an option missing its value, a --ticks value that is not a decimal count, or --hash given with --frames or --capture, whatever their values, prints the usage and exits with a nonzero status. A park file that cannot be read or loaded exits with a nonzero status after writing to standard error a message naming the file and, for a load error, LoadError's message.
```

## Files affected

- Modify: src/render/SPEC.md, src/app/SPEC.md
- Create: src/render/park_mesh.h, src/render/park_mesh.cpp
- Create: src/render/shaders/park.vert, src/render/shaders/park.frag
- Modify: src/render/renderer.h, src/render/renderer.cpp, src/render/CMakeLists.txt
- Modify: src/app/main.cpp, src/app/orbit_camera.h, src/app/orbit_camera.cpp
- Create: tests/parks/sketch.park
- Modify: plans/effortless-building/sketch-a-park/MILESTONE.md, moving the translucent pass from park-view to box-tools
- Create, by the test pass: tests/render/ and tests/app/ test files and their CMakeLists.txt, and a test of sketch.park under tests/sim/park/
- Modify, by the test pass: tests/CMakeLists.txt, tests/sim/CMakeLists.txt

## Dependencies

park-intent: the intent queries, pathWidth, boxSize, ENTRANCE_SIZE, groundLine, footprintOf, and makeNewPark. park-edits: isPhysicallyValid, for criterion 8. The renderer, the app's loop, and --capture. tpj_scenarios, for criterion 6.

## Out of scope

- The translucent pass, ghost tints, and hover highlights: box-tools, which has the first ghost to draw.
- Where paths cross or meet: ribbons overlap there, and crossing ribbons of different kinds tie in depth. A deepening candidate in MILESTONE.md.
- Reframing the camera when a later file is opened: park-files. The starting frame is checked only by criterion 9's capture.
- Ribbons that fold where a path bends tighter than half its width or turns back. The player rarely draws one, and it shows as a gap in the ribbon, not a crash.
- Rebuilding only what changed, or caching meshes: the whole mesh is rebuilt when intent changes.
- Paths following terrain as ramps and stairs, which wait for terrain editing.
- Box marks such as a starved shop: plausible-operations.
- A seed other than 1 for the app's new park: park-files' new park.

## Open questions

None.
