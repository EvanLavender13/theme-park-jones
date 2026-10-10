# Milestone: Day and Night

Slice: day-at-the-park

Status: planned

## Summary

The park's light follows its clock. A sun crosses the sky by day and warms toward evening. At dusk a dim, cool moon light and a hemispheric ambient light take over, and the night stays readable. The scene is lit in linear light and tonemapped, which starts the post-processing chain that later milestones extend. The day is compressed: sunrise near 6:00, golden light from about 17:00, sunset near 19:30, and full dark by about 20:30. So the last open hour is dusk, and guests walk home after closing at night. Overlays and editing marks keep their exact colors at every hour, and guests keep their need shading. A Light panel previews the light at any hour without touching the park's clock, so the user can tune the day by scrubbing through it. This milestone is the foundation of stylized-presentation and the day-at-the-park slice's member 6, which the slice's captures of noon.park and of day.park at night need.

## Acceptance criteria

1. One light rig lights the ground, paths, boxes, and guests. It holds a sun, a moon, and a hemispheric ambient light that mixes a sky color and a ground color by which way a surface faces. The frame's sky and the distance fade take the same hour's sky color. (capture: noon.park)
2. The scene is lit in linear light and tonemapped to the screen by a tonemapper that keeps the hue of base colors. Each kind of box and path keeps its own recognizable color in daylight. (capture: noon.park)
3. Every part of the light is a function of the time of day alone, read from the park's clock. A capture of day.park stepped 5400 ticks and a capture of noon.park, both paused with --speed 0, look the same, apart from the Debug panel's frame rate. (capture: both, compared)
4. By day, the sun moves with the hour, its light warms toward evening, and it is never straight overhead, so the faces of a box stay distinct at noon. The slice's criterion 4 holds: a capture of noon.park shows the park in daylight, with guests on the paving and at the facilities' doors. (capture: noon.park with --speed 0, and day.park with --ticks 16200 --speed 0 for 18:00)
5. At night, no light comes from below the ground. A dim, cool moon light and the ambient light keep every kind of box, both kinds of path, and the ground distinguishable from each other. The slice's criterion 5 holds: a capture of day.park at 23:00 shows the empty park at night, with every box and path still distinguishable from the ground. (capture: day.park with --ticks 25200 --speed 0)
6. The light changes smoothly through the whole day, with no jump at sunrise, sunset, or midnight. (play: watch day.park through a day at 4x)
7. The colors that carry meaning keep it at every hour:
   - The food overlay's colors are the same under every hour's light. (capture: noon.park with --overlay food --speed 0, with --light-hour 12 and with --light-hour 23, compared)
   - Guests take the light's brightness but not its color, so a guest's blue-to-pink need shading keeps its hue at every hour. (capture: noon.park with --speed 0, with --light-hour 12 and with --light-hour 18, compared)
   - Ghosts, highlights, handles, and join marks keep their colors at night. (play: at 23:00, hover a placement ghost, a highlight, and a path's handles)
8. The Light panel previews the light at any hour of the day. While a preview hour is set, the scene shows that hour's light, and the park's clock, ticks, and hash are unchanged. Turning the preview off returns the light to the clock's hour. The --light-hour H option starts the app with the preview set to hour H, so a capture shows any hour's light on the same world. (play, and the captures of criterion 7)
9. The simulation is unchanged: a tpj_scenarios --compare of windows-debug output from before and after shows no difference. The release app's frames on tests/parks/stress/full.park stay within the 33 ms budget. (check: the comparison, and the frames stage of scripts/runtime-report.sh before and after)
10. src/render/SPEC.md describes the light, the tonemap, and the clock read. src/app/SPEC.md describes the Light panel and --light-hour. The windows-debug build has no warnings, ctest passes, and tidy is clean.

## Medium

The milestone introduces no fields or flows, and the simulation uses nothing it produces.

- hour-light reads the park's clock through parkTime (sim/clock.h), as the slice's medium map assigns stylized-presentation. The clock is part of the tick cycle, not an interaction between entities. The light is a function of the second of the day, computed in render.
- light-preview gives the renderer a chosen second of the day in place of the clock's, from the Light panel or the --light-hour option. It changes only what is drawn. It queues no edit, and it changes neither the world nor its tick.
- Both draw what the renderer already reads: intent, the networks, and guests' and shops' inspection records (src/render/SPEC.md).

## Dependencies

- deterministic-simulation's park-clock: parkTime and the speed controls. Met.
- believable-guests' a-day-out: day.park, noon.park, and evening.park in tests/parks. Met.
- legible-simulation's food overlay, whose colors must keep their meaning (src/render/SPEC.md, Paving overlay). Met.

## Core feature

hour-light is the core. On its own it delivers every capture the slice asks of this milestone, and the player sees the day pass. light-preview only makes the hour-light's values quicker to tune.

## Features

1. `hour-light`: Render reads the time of day, and authored ramps keyed on it drive a sun, a moon, a hemispheric ambient light, and the sky. The scene renders in linear light into a float target and is tonemapped to the screen. The overlay and editing marks draw after the tonemap in their own colors, and guests are lit by brightness only. Depends on: none.
2. `light-preview`: A Light panel with an hour slider previews the light at any hour of the day without changing the park's clock, and the --light-hour option sets the preview from the command line for captures. Depends on: feature 1.

## Deepening candidates

Unordered pool this milestone draws later features from.

- A moon and stars drawn in the night sky.
- Moon phases that change the night's light from day to day.

## Open questions

- The tuning values: the keyframes' hours and colors, the sun's peak elevation, the twilight band, and how dark night gets. hour-light sets first values from the compressed day. The user adjusts them by scrubbing the Light panel and watching a day, and names any values to change.

## Research notes

- Render into a float target and tonemap to the existing 8-bit target, which the UI, the blit, and captures keep using.
- Khronos PBR Neutral keeps base colors' hues, where ACES shifts them and AgX desaturates them.
- Draw the overlay and editing marks after the tonemap, with the scene's depth, so their colors are exact.
- Author the day as keyframes at hours that wrap at midnight, and interpolate them in linear color. Fade the sun and moon over a twilight band.

The details are in RESEARCH.md.
