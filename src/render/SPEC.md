# render

Draws views of the park through SDL_GPU (decision 0009). It reads what it needs to draw and never changes simulation state.

## Contract

World space is right-handed and measured in meters, with +Y up. The terrain is a flat square of a size given at creation, centered on the origin in the XZ plane, drawn with 1 m and 10 m grid lines that fade with distance.

createRenderer creates a Vulkan-backed GPU device, claims the window, uploads the terrain, and initializes Dear ImGui's SDL_GPU backend, which requires an ImGui context to exist already. It logs through SDL and returns false on failure. destroyRenderer releases everything it created, including the ImGui GPU backend, and is safe to call after a failed create. The renderer owns the ImGui GPU backend; beginUiFrame starts its part of each ImGui frame.

setParkMesh uploads a park mesh, replacing the one drawn before; an empty mesh draws nothing. drawFrame draws the terrain, then the park mesh through a lit pipeline that shades it from its normals as the terrain is shaded, with a depth test and depth writes and back faces culled. Depth is reversed, the near plane at 1 and the far plane at 0 in a float depth buffer where the device has one, so precision holds across the camera's range and paths lying just above the terrain never flicker through it.

drawFrame renders one CameraView into an offscreen RGBA8 color target with a depth buffer, draws the ImGui draw data over it when given, then blits the result to the swapchain. When the window is minimized the frame is skipped and counts as success. When a capture path is given, the rendered frame, UI included, is also read back and saved as a BMP before drawFrame returns.

Shaders are GLSL in shaders/, compiled to SPIR-V at build time and loaded from shaders/ next to the executable. Following SDL_GPU's SPIR-V layout, vertex-stage uniform buffers use set 1 and fragment-stage uniform buffers use set 3.

## Park mesh

park_mesh.h builds the park's mesh on the CPU, so it can be tested without a GPU. It reads intent only through sim/park's public queries and geometry (sim/park/SPEC.md), and changes nothing. A ParkMesh is a list of ParkVertex, each a position, a unit normal, and an Rgba color in floats, and a list of 32-bit indices, three per triangle, each triangle wound counter-clockwise seen from the side its vertices' normal points to. Appending never changes what a mesh already holds, and the indices it adds name the vertices it adds. meshBounds gives the least rectangle on the ground, GroundBounds MinX, MinZ, MaxX, and MaxZ, holding every vertex's x and z, or none for a mesh with no vertices.

appendPath adds a path's ribbon: flat, pathWidth wide, PATH_LIFT, 2 cm, above the ground, along its groundLine. For a ground line of n points, n at least 2, it adds 2n vertices, two for each point in order: the left edge, at the point minus the right direction times half the width, and then the right edge, at the point plus it. The right direction of a unit tangent (tx, tz) is (-tz, tx). The tangent at the first point is the direction of the segment after it, and at the last point that of the segment before it. At a point between, it is the normalized sum of the unit directions of the segments before and after it, or the one before when that sum has zero length. The normals point up and the colors are pathColor(kind). Each consecutive pair of points adds two triangles covering the quad between their four vertices. Where the ground line bends tighter than half the width, or turns straight back, the ribbon folds over itself, and triangles there can face down and be culled. An empty ground line adds nothing.

appendBox adds a box of a height and color over a pose's footprint for a size: a top at the height over the footprint's corners, and four sides from the ground to the height over its four edges, with no bottom. Each face has four vertices with the face's unit outward normal and two triangles: the top's normal points up, the front side's is Forward, the back side's -Forward, the right side's Right, and the left side's -Right. The front side, over the footprint's front face, takes the color lightened, each of red, green, and blue c becoming c + (1 - c) * 0.4 and alpha kept; the others take the color. A pose with no footprint adds nothing.

buildParkMesh gives a world's park mesh: each entrance from parkEntrances, with ENTRANCE_SIZE, ENTRANCE_HEIGHT 5 m, and ENTRANCE_COLOR, then each path from parkPaths, then each box from parkBoxes, with boxSize(kind), boxHeight(kind), 4 m for a shop and 6 m for a depot, and boxColor(kind), each in the order the query gives. The colors of guest paths, backstage paths, shops, depots, and the entrance are distinct and opaque.
