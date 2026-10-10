# Capability: Stylized Presentation

## Summary

stylized-presentation makes the park look like a coherent retro-3D place, at every hour and from a free camera. The target is RollerCoaster Tycoon 2 in 3D: less cartoony than Parkitect, less detailed than Planet Coaster. It belongs to the late-1990s family of RCT2 and Half-Life, which recent games such as Abiotic Factor and RetroSpace recreate by pairing chunky, low-poly shapes and visible texels in a limited palette with modern soft lighting, shadows, and light post-processing. The capability owns the rules for how things are drawn: the light, the sky, the texel patterns and palette of surfaces, and the post-processing chain. Those rules apply alike to the ground, paths, boxes, and guests. They never change the colors of overlays or editing marks, because those colors carry meaning. The shapes belong to the capabilities that derive them from intent, and overlays and their meaning belong to legible-simulation. Light and color carry most of the look, and they don't depend on detailed geometry, so the capability starts with light that follows the park's clock through a day and a readable night. It never finishes: every new kind of thing the park gains needs its look, and the look deepens with shadows, surfaces, and post-processing.

## Foundation criteria

- One light rig lights the ground, paths, boxes, and guests. It holds a sun, a moon, and a hemispheric ambient light that mixes a sky color and a ground color by which way a surface faces. The sky behind the scene and the distance fade take the same hour's sky color.
- The scene is lit in linear light and tonemapped to the screen, which is the first step of the post-processing chain.
- Every part of the light is a function of the time of day alone, read from the park's clock. Two captures of the same park at the same tick look the same, whatever the speed, the frame rate, or whether the park was loaded from a save.
- By day, the sun moves across the sky with the hour and its light warms toward evening. It is never straight overhead, so the faces of a box stay distinct at noon. (capture: noon.park and evening.park)
- At night, no light comes from below the ground. A dim, cool moon light and the ambient light keep every kind of box, both kinds of path, and the ground distinguishable from each other. (capture: day.park at 23:00)
- The light changes smoothly through the whole day, with no jump at sunrise, sunset, or midnight. (play: watch a day at 4x)
- The colors that carry meaning keep it at every hour:
  - The food overlay keeps its colors. (capture: noon.park and day.park at 23:00, each with --overlay food)
  - A guest's need shading reads the same at every hour, so a contented guest in warm evening light never looks as needy as a desperate one. (capture: noon.park and evening.park)
  - Ghosts, highlights, handles, and join marks keep their colors. (play: at 23:00, hover a placement ghost, a highlight, and a path's handles)
- The release app's frames on tests/parks/stress/full.park stay within the 33 ms budget. (check: the frames stage of scripts/runtime-report.sh, reported before and after)

## Medium

stylized-presentation introduces no fields or flows, and the simulation uses nothing it produces.

- It reads the park's clock, which deterministic-simulation provides as a pure function of World::Tick. The clock is part of the tick cycle, not an interaction between entities (the day-at-the-park slice's medium map).
- It draws what the renderer already reads: intent through sim/park's queries, the networks, and guests' and shops' inspection records (src/render/SPEC.md).

Reading the clock is render's one new read. Render will read the time of day through parkTime, and day-and-night adds that read to src/render/SPEC.md's Boundary.

It is not an engineering capability, because a player would miss the light (decision 0028).

## Principles

- Principle 10 is at risk if anything the renderer computes reaches the simulation, or if the clock depends on rendering. The light is computed in render from the time of day, and nothing flows back. The light may use the C runtime's trigonometry, which only render runs (decision 0022). The layer check in tests/checks stops sim from depending on render. A tpj_scenarios --compare of output from before and after each milestone shows the simulation unchanged.
- Principle 1 is at risk if the light, a surface's pattern, or their settings are saved. They are derived from the tick and from intent, which are already saved, and nothing new goes into a park file.
- Principle 8 is at risk if the light or the post-processing changes what a color means. The food overlay, guests' need shading, and the editing marks keep their meaning at every hour, as the captures and a night of play show.

## Dependencies

- deterministic-simulation (park-clock): the time of day as a function of the tick, and the speed controls. Met.
- The renderer and its shaders in src/render, from effortless-building's sketch-a-park and the milestones since. Met.
- day.park, noon.park, and evening.park in tests/parks, from believable-guests' a-day-out. Met.
- SteelJones's post-processing stack and day cycle, as a working reference to port from raylib to SDL_GPU. Met.

## Foundation

The foundation is the light rig and its day: a sun, a moon, a hemispheric ambient light, and a sky, all driven by the time of day, lit in linear light and tonemapped, with a night that stays readable. It is the smallest version that gives the look one owner, and it produces value on its own. The player sees the day pass and can still build at night. It is also the day-at-the-park slice's member 6.

## Milestones

1. `day-and-night`: The sun, the moon, the sky, and a hemispheric ambient light change with the time of day alone, from authored ramps. The scene is lit in linear light and tonemapped, and night keeps a dim, cool light that stays readable. Member of the day-at-the-park slice; it delivers the light in the slice's captures of noon.park and of day.park at 23:00. Depends on: deterministic-simulation's park-clock.
2. `grounded-light`: The sun and the moon cast soft shadows, and contact shading darkens the ambient light where boxes and guests meet the ground, within the frame budget on full.park. Depends on: milestone 1.
3. `retro-surfaces`: Ground, paths, and boxes show visible texels in a limited palette of shaded ramps, generated in code, fixed to their surfaces, and mipmapped so distant surfaces don't shimmer. Depends on: milestone 1.
4. `retro-post`: The post-processing chain gains what the look needs after experiment: ordered dither anchored to the screen, outlines from depth and normals, and FXAA after the tonemap. Depends on: milestones 1 and 3.

Milestones 3 and 4 are planned after the day-at-the-park slice closes, and their contents may change once the light has been seen.

## Deepening candidates

Unordered pool this capability draws later milestones from.

- A view toggle that keeps the scene in daylight whatever the time of day, for building at night.
- A sky drawn as a gradient with halos around the sun and at the horizon, instead of one color.
- One palette for the colors of every kind of box and path, chosen so the kinds stay distinct for colorblind players at every hour.
- Bloom on things that emit light, such as lamps at night, and only on them.
- A flatten pass, such as SteelJones's Kuwahara filter, if posterized shading pops at band edges. It is SteelJones's costliest pass.

## Open questions

- When the sun rises and sets against the default opening hours of 9:00 to 21:00, and how dark night gets. These are tuning values, set while planning day-and-night and adjusted when the user watches a day at day.park.
- How many texels a meter surfaces get, and which palette. The user decides by experiment while retro-surfaces is planned.
- Whether outlines belong in the look. Neither RCT2 nor Half-Life has them. The user decides by experiment in retro-post.
- The player-facing UI stays open (docs/open-questions.md). This capability shows no UI of its own.

## Research notes

- RCT2 was 3D first: models rendered with one shadow-casting key light and a dome of fill, anti-aliased, and dithered to a 256-color palette of 12-shade ramps. Half-Life's look shares the soft light and 8-bit palettes.
- The general name is "retro 3D". Abiotic Factor and RetroSpace recreate the late-1990s PC look by pairing low-poly shapes and visible texels with modern lighting.
- A free camera rules out low-resolution pixel art, which needs a snapped orthographic camera. Post-processing at native resolution and texels fixed to surfaces both stay stable as the camera moves.
- SteelJones's stack is a working reference. Its lessons: apply view-dependent terms after posterizing, anchor dither to the screen, run FXAA after the tonemap, and let ambient occlusion darken only the ambient light.
- Night stays readable when a hemispheric ambient light keeps a floor that is cooler and less saturated, not just darker.

The details are in RESEARCH.md and in the day-at-the-park slice's RESEARCH.md.
