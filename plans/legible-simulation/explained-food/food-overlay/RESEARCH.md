# Research: food-overlay

## Which colors should the ramp use?

Viridis sampled at quarters of its range is a standard five-color palette: #440154, #3B528B, #21908C, #5DC863, and #FDE725, dark purple through blue, teal, and green to yellow. Interpolating linearly between neighboring stops in RGB stays close to the continuous map, since viridis itself is built from short, nearly straight runs, and keeps its lightness rising steadily, which is what makes it readable in grayscale and with red-green color deficiency.

Zero means no supplied shop can be reached, which the player must tell apart from a little food. A neutral gray, 0.55 in each channel, is never given by the ramp: along the ramp red reaches 0.55 only between green and yellow, where green is above 0.8. Viridis' lowest stop is a dark purple, so the smallest nonzero value is also far from gray.

The ramp's top is fixed at 0.5, one meal's full relief with no walk and no wait, so the same color means the same availability in every park and at every tick, and a place near two supplied shops saturates at yellow.

Rejected: a ramp scaled to each frame's greatest value. Colors would shift as waits change and would not compare between captures. Rejected: zero as the ramp's lowest color. Nearly nothing and nothing would look the same.

Sources: https://ipalettes.com/palette/Viridis-2273, the five-stop viridis palette; plans/legible-simulation/RESEARCH.md, perceptually uniform ramps; src/render/shaders/terrain.frag, the grass color the band covers.

## How high should the band's tent lie?

Guest paths lie 4 cm above the terrain and backstage paths 2 cm, and the band must lie under both, so its line lies at 1.5 cm and its edges at 0.5 cm. The drop of 1 cm over the band's 6 m width makes two lines' surfaces at a point differ by 1 cm for each 6 m of difference in their distances, so they tie only on the line halfway between them, which is the boundary the rule asks for anyway.

The renderer's depth is reversed in a 32-bit float buffer where the device has one, which keeps a relative precision of about 2^-24 at every distance, about 0.02 mm at the camera's 400 m limit. The half centimeter between the band's edge and the terrain, and between its line and the backstage paths, is far above that at every pitch the camera allows.

Rejected: translucent bands blended over the grass. Where bands overlap, whichever is drawn later blends over the other, so the overlap shows a mix, not the nearest line's value. Rejected: one flat height. Overlapping bands at one height tie in depth and flicker.

Sources: src/render/SPEC.md, reversed depth and path lifts; src/render/park_mesh.h, pathLift.

## Where along a line should the overlay sample?

Availability along an edge is piecewise linear: route distance inside an edge is the lesser of its two ends' distances plus the offset, and offers are sampled at nodes. It kinks only at stops, where the nearer end can change, at the point where the two ends' routes are equally long, and where a shop's effective time crosses a point of the discount curve. Sampling each stop and each point of the line, where the band turns, and every OVERLAY_SPACING, 1 m, between them makes the band exact at nodes and bends, and keeps the interpolation within half a meter of each kink. On fed.park's guest paths, about 120 m in all, that is about 130 samples, each a route distance sample and one offer sample per reachable source. That is a few times the route distance samples thirty walking guests take in a tick, which step in about 0.6 ms on windows-debug, so rebuilding and uploading it every frame, which spares the app a cache to keep in step with the world (decision 0027), is expected to cost about a millisecond at most. The implementation measures it in the Debug panel's frame time.

Sources: src/sim/routes/SPEC.md, sampleEdge; src/sim/medium/SPEC.md, sampling costs; tests/parks/fed.park.
