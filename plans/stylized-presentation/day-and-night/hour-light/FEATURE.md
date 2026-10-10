# Feature: Hour Light

## Summary

The park is lit by the light of its time of day. Render reads the clock and turns the second of the day into a light. Authored keyframes at hours of the day, which wrap at midnight, give the sun's color and strength, the moon's strength, a hemispheric ambient light's sky and ground colors, the sky's color, and an exposure. The sun rises at 6:00, crosses the sky without ever standing overhead, warms toward evening, and sets at 19:30, fading out at the horizon. From dusk to dawn a dim, cool moon light shines from high in the sky, and the ambient light keeps the night readable. The ground, paths, boxes, and guests are lit by it in linear light, in a float target that a PBR Neutral tonemap brings to the screen. Guests take the light's brightness but not its color. A ghost's preview of the park is lit by the hour as the park is, so its walkways blend into the committed ones. The food overlay and the editing marks draw after the tonemap in their own colors, so no hour changes them. The first values are a starting point. The user tunes them with light-preview, the milestone's next feature.

## Acceptance criteria

1. One light rig lights the ground, paths, boxes, and guests: a sun, a moon, and an ambient light that mixes a sky color from above and a ground color from below by which way a surface faces. The frame's sky and the distance fade take the hour's sky color. (capture: `--park tests/parks/noon.park --speed 0`)
2. In daylight each kind of box and path keeps its own recognizable color, as in a capture of the same park before this feature, and a box's top face and its face turned from the sun stay distinct. (capture: noon.park, before and after)
3. The light is a function of the time of day alone. A capture of day.park stepped to noon and a capture of noon.park, both paused, look the same, apart from the Debug panel's frame rate. (capture: `--park tests/parks/day.park --ticks 5400 --speed 0` and `--park tests/parks/noon.park --speed 0`, compared)
4. The sun moves with the hour and its light warms toward evening. At 18:00 the light is lower and warmer than at noon, and the sky has warmed with it. (capture: `--park tests/parks/day.park --ticks 16200 --speed 0`, against the noon capture)
5. At night no light comes from below the ground. A capture of day.park at 23:00 shows the empty park, with every kind of box, both kinds of path, and the ground distinguishable from each other, under a dim, cool light that is not uniformly darker (the slice's criterion 5). (capture: `--park tests/parks/day.park --ticks 25200 --speed 0`)
6. The light changes smoothly through a whole day, with no jump at sunrise, sunset, or midnight. Guests' blue-to-pink shading keeps its hue from noon into the evening. (play: day.park at 4x in the release app, through midnight)
7. The food overlay shows its own ramp at night, with none still near white. At night, highlights, handles, join marks, and the invalid and delete tints keep their colors. A placement ghost is lit as the park is, so of its walkways only the one it adds stands out. (capture: `--park tests/parks/day.park --ticks 25200 --speed 0 --overlay food`; play: at 23:00, hover a shop placement, a refused placement, a highlight, and a path's handles)
8. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after exits 0. The release app's frames on full.park stay within the 33 ms budget, reported before and after. src/render/SPEC.md and src/app/SPEC.md describe the light. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Tasks 1 and 12)

## Tests

None. The light is checked by captures and play (docs/testing.md), and the scenarios comparison shows the simulation unchanged.

## Medium

No fields or flows. Render reads the park's clock through parkTime (sim/clock.h), which the slice's medium map lets stylized-presentation read. The light is computed in render and nothing flows back to the simulation. The app hands the renderer the light of its world's time of day, as composition.

## Spec changes

src/render/SPEC.md:

- Boundary, render reads: add "the time of day through parkTime".
- Boundary, the app calls: add "day_light.h for DayLight, dayLightAt, and dayLightOf".
- Boundary, the app calls: park_mesh.h's list gains GhostMesh.
- The renderer, append to the paragraph that begins "Each of setParkMesh, setGuestMesh, and setGhostMesh": "setGhostMesh takes a GhostMesh and replaces all three of its parts."
- The renderer, the drawFrame paragraph becomes: "drawFrame draws one CameraView under a DayLight. It draws the terrain, the park mesh, and the guests, lit by that light, then the paving overlay, then a ghost's tints, its preview of the park lit as the park is, and its marks, each translucent over the one before, all depth tested. The light never changes the overlay's colors or the marks' colors. The overlay draws over the guest paving and under its edge lines, and boxes and guests draw over it." The rest of the paragraph is unchanged.
- Ghosts and highlights: "buildGhostMesh(world, edit, candidate) gives the edit's ghost:" becomes "buildGhostMesh(world, edit, candidate) gives the edit's ghost in three parts. Its Tints part holds the invalid and delete tints, under the rest. Its Lit part is what the park would look like, drawn in kinds' colors and lit as the park is. Its Marks part holds the marks, over the rest. Tints and marks keep their colors at every hour:". After the list, add: "Whatever the edit adds or changes in a kind's color, the candidate's walkways, and its starved marks are in the Lit part. The invalid and delete tints are in the Tints part, and the join marks and sides in JOIN_MARK_COLOR are in the Marks part."
- A new section after The renderer:

  "## The light

  dayLightAt gives the light at a second of the day, and dayLightOf gives it at a world's time of day (sim/SPEC.md, The clock). The light depends on the second of the day alone. A world at a given tick is lit the same at any speed and after a save and load.

  The light has a sun, a moon, and an ambient light. The ambient light mixes a sky color from above and a ground color from below by the way a surface faces. The frame's sky is the hour's sky color, and the ground, paths, boxes, and guests fade to it with distance.

  The sun rises at SUNRISE_SECOND on the +X side and sets at SUNSET_SECOND on the −X side, passing over the +Z side. It is never straight overhead, so a box's faces stay distinct at noon. Its light warms toward evening and fades out at the horizon. From dusk to dawn a dim, cool moon light shines from high in the sky. No light comes from below the ground. At night the ambient light keeps every kind of box, both kinds of path, and the ground distinguishable. The light changes continuously through the whole day, across midnight too.

  The scene is lit in linear light and tonemapped to the screen. The tonemap keeps the hue of every color it is given. Guests take the light's brightness but not its color, so their need shading keeps its hue at every hour."
- Guests: append "The light dims a guest's box but never changes its hue (The light)."
- Paving overlay, the sentence "The renderer colors each pixel of the shape by the value at its overlay place" gains ", in the same colors at every hour".

src/app/SPEC.md, The scene: append "The scene is lit by the light at the world's time of day (render/SPEC.md, The light)."

The public interface adds src/render/day_light.h:

- `struct Rgb { float R = 0.0f; float G = 0.0f; float B = 0.0f; };`, a linear color.
- `struct DayLight { Vec3 SunDirection; Rgb Sun; Vec3 MoonDirection; Rgb Moon; Rgb AmbientSky; Rgb AmbientGround; Rgb Sky; float Exposure = 1.0f; };`. The directions point toward the light, and the colors are linear and include their strengths.
- `inline constexpr uint64_t SUNRISE_SECOND = 6 * 3600;` and `inline constexpr uint64_t SUNSET_SECOND = 19 * 3600 + 1800;`
- `DayLight dayLightAt(uint64_t second);`
- `DayLight dayLightOf(const World &world);`

drawFrame gains a `const DayLight &light` parameter after the camera.

src/render/park_mesh.h adds `struct GhostMesh { ParkMesh Tints; ParkMesh Lit; ParkMesh Marks; };`, and both buildGhostMesh overloads return a GhostMesh. setGhostMesh takes `const GhostMesh &`.

## Files affected

- Modify: src/render/SPEC.md, src/app/SPEC.md
- Create: src/render/day_light.h, src/render/day_light.cpp
- Create: src/render/shaders/light.glsl, src/render/shaders/tonemap.glsl, src/render/shaders/tonemap.vert, src/render/shaders/tonemap.frag, src/render/shaders/ghost.frag, src/render/shaders/marks.frag
- Modify: src/render/shaders/park.frag, src/render/shaders/terrain.frag, src/render/shaders/overlay.frag
- Modify: src/render/renderer.h, src/render/renderer.cpp, src/render/park_mesh.h, src/render/park_mesh.cpp, src/render/CMakeLists.txt
- Modify: src/app/application.cpp (composition only: pass the world's light to drawFrame)
- Modify: src/app/scene/scene_uploads.cpp (the frame's ghost puts its start and finish marks, highlights, and handles in the Marks part)

## Dependencies

- deterministic-simulation's park-clock: parkTime. Met.
- day.park and noon.park in tests/parks. Met.

## Out of scope

- The Light panel and --light-hour: light-preview, the milestone's next feature. Comparing the overlay's and guests' colors under two hours' light on the same world waits for it.
- Shadows and contact shading: grounded-light.
- A sky gradient, sun and moon discs, and stars: the capability's and milestone's deepening candidates.
- Moving starved marks out of the light. They stay in the park mesh and take the hour's light as boxes do.

## Open questions

- The tuning values. This feature sets first values from the compressed day the user chose, and light-preview lets the user scrub through the day and name any to change.
