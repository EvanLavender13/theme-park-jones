# Research: Paving Overlay

## How can the paving be shaded exactly without costing more than the band?

The overlay's rule is per spot: a spot of a segment's paving shows the value at the place on that segment's line nearest it, a spot of a junction's paving the nearest place on the lines of the segments there, and where paving of several owners covers a spot, the nearest of their places wins, ties to the lower segment key. Two things make a color exact: the place, and the value at the place.

The place can be found per pixel. A fragment shader can take, for each pixel of a guest piece, the nearest place on the piece's own lines, the same polylines the network and the tooltip use. A piece that overlaps another owner's piece also checks the lines of each overlapping piece that holds the pixel, and drops the pixel if one is nearer. So one piece draws each pixel, with the place the rule gives, and no heights or depth tricks are needed. The pieces are only those the paving already has, about 9,000 vertices on full.park. Their lines and overlaps change only when the paths do.

The value can be exact between a few samples, because availability is linear between known bends. Inside an edge, a source's route distance is the better of the two ends' distances plus the distance along the carrier to that end (src/sim/routes/SPEC.md), so it is linear except where the better end changes. A source's term is its relief times the discount curve at its effective time, which is distance over a fixed speed plus a wait that is fixed within a tick, and the curve is linear between its points (decision 0020). Availability is the sum of the terms, so along an edge it is linear between the places where some source's better end changes or some source's time reaches a point of the curve. Sampling at those bends and at each stop, and blending linearly between them, gives the exact value everywhere. On paths that branch like a tree, a source's better end rarely changes inside an edge, and its time crosses a curve point only every 78 m of walking, so there are far fewer bends than one per meter.

So each tick, the CPU computes the values at the bends, as it computes values at 1 m samples today, and uploads only those numbers. The band mesh is no longer rebuilt every tick. The shader reads the two bends around a pixel's place and blends their values. It then maps the value to the ramp, so the colors between two bends follow the ramp rather than a straight blend of two colors.

The shader works in single precision. Across a park a few hundred meters wide, it finds places to well under a millimeter. A pixel within that distance of an exact tie between two lines may take either line. Everywhere else, it picks the same place as the CPU. Rasterization gives each pixel on the shared outline of two pieces to one of them, so a pixel on the outline of an overlap may show the other piece's place.

SDL 3.4 lets a fragment shader read storage buffers, in resource set 2 after its sampled and storage textures, and binds them with SDL_BindGPUFragmentStorageBuffers (.cpm-cache/sdl3/45cb/include/SDL3/SDL_gpu.h). The renderer's shaders are GLSL 450 compiled to SPIR-V by glslang, which supports read-only storage buffers.

Rejected: cutting the paving into triangles of at most 1 m on the CPU and coloring their corners. It is exact only at the corners, about 100,000 vertices on full.park, rebuilt and uploaded every tick. Rejected: values only at the paving's own corners. A body runs a whole segment, and junction corners would need a guessed line. Rejected: heights that fall with distance from a spot's line, so the depth test picks the nearest. It needs a height budget between the paving and its edge lines and is exact only at vertices. Rejected: values at 1 m samples, blended. The blend misses the bends of availability.

Sources: src/sim/routes/SPEC.md — a source's entry inside an edge; src/legible/availability.cpp — terms and the discount curve; docs/decisions/0020-gradient-curves.md — piecewise-linear curves; .cpm-cache/sdl3/45cb/include/SDL3/SDL_gpu.h — fragment storage buffers and their resource set; build/drawn-edges/bench-after.txt — today's food-overlay stage, 3.5 ms and 20,787 vertices on full.park, and its park mesh, 8,914 vertices.

## What color is zero?

Plotting tools give a ramp's empty value its own color, or leave it transparent. Transparent would show unshaded paving, which the milestone rules out. The ramp runs from dark purple to yellow, guest paving is tan, and backstage paving is a blue gray, so a near white is far from all of them. Gray is close to backstage paving, and near black is close to the ramp's dark purple.

Sources: src/render/park_mesh.h — the paving colors; https://matplotlib.org/stable/users/explain/colors/colormap-manipulation.html — giving a colormap's empty value its own color or transparency.
