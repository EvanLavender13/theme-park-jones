# Research: stylized-presentation

## How was RollerCoaster Tycoon 2 drawn?

The user wants the game to look like RCT2 in 3D: less cartoony than Parkitect, less detailed than Planet Coaster.

RCT2's graphics were 3D first. Simon Foster modeled them in Lightwave and rendered them in 3ds Max. The camera was orthographic, turned 45 degrees and looking down 30 degrees, with a tile exactly 64 pixels wide. One main light, about 90 degrees anticlockwise from the camera, cast shadows. A dome of point lights lit the shadowed sides, and lower fill lights shone at about half the main light's strength. Sprites were anti-aliased with a 1.4-pixel cubic filter, then dithered down to the game's 256-color palette in Photoshop or Paint Shop Pro. The palette holds 18 base colors with 12 shades each. Recoloring a ride swaps one 12-shade ramp for another, and water shimmers by cycling palette entries.

So RCT2's look comes from a few things:

- a soft key light with shadows and a dome of fill, so things look solid and sit on the ground
- a limited palette of shaded color ramps, so colors are muted and consistent
- dither where shades meet
- small sprites with soft, anti-aliased edges and no outlines

Sources: https://github.com/OpenRCT2/OpenRCT2/wiki/Original-Graphics — modeling, camera, lighting, anti-aliasing, and dithering of the original graphics; https://github.com/OpenRCT2/OpenRCT2/discussions/19428 — the palette's 12-shade remap ramps; https://trigger-segfault.github.io/tools/rct2-tools/water-creator/ — water's palette animation.

## Is there a name for the late-1990s 3D look that indie games recreate?

There is no single name. The general terms are "retro 3D" and "early 3D". Each has a console-specific form:

- "PS1 style" is jittery vertices and warping textures, from the PlayStation's lack of perspective correction.
- "N64 style" is blurry, filtered, low-resolution textures.

The late-1990s PC look, Half-Life's and Quake's, is usually named after its engine, for example "GoldSrc style". Shooters in that style are called boomer shooters or retro FPS. Examples include DUSK, Prodeus, Graven, and DEFICIT, and mods such as Delta Particles and Diffusion.

The GoldSrc look comes from low-poly models, small textures with visible texels, and soft baked lighting. Each GoldSrc texture is 8-bit, with its own 256-color palette. Quake used one palette for the whole game. That puts RCT2 and Half-Life in the same family: late-1990s 3D, softly lit, with colors limited to 8-bit palettes.

Prodeus states the modern approach: retro looks made with modern rendering, imitating old hardware's limits by choice rather than by necessity.

The user named two recent games in this family. Abiotic Factor (Deep Field Games, Unreal Engine 4) takes its art direction from Half-Life. It combines low-poly models and small, grainy textures painted with a limited palette with fully modern lighting, soft shadows, and post-processing. RetroSpace (2026) is a low-poly immersive sim with a 1960s and 1970s retro-futurist look. Both keep the old games' simple assets and chunky surfaces, and light them with modern techniques.

A visible texel is a texture pixel drawn large enough to see. A texture of only a few texels per meter, sampled without smoothing, shows each texel as a crisp square fixed to the surface. Up close a wall reads as a grid of painted squares, while its edges and its light stay smooth at screen resolution. Because the squares are fixed to the surface, they don't crawl as the camera moves, unlike pixel art rendered at low resolution. Distant surfaces need mipmaps, or the texels shimmer. Without an artist, texels can come from small textures generated in code, or a shader can snap a surface's position to a grid of the chosen size and color each cell from a pattern.

Rejected: PS1-style vertex jitter and texture warping — artifacts of one console, and they would make a builder's precise shapes wobble.

Sources: https://www.resetera.com/threads/retro-3d-indie-games-lots-of-gifs.87536/ — "retro 3D" as the general term; https://www.pcgamer.com/games/fps/10-years-in-the-making-this-total-conversion-based-on-half-life-is-every-bit-as-ambitious-as-black-mesa/ — Diffusion; https://www.dsogaming.com/news/deficit-is-a-new-indie-fps-inspired-by-half-life-condemned/ — DEFICIT; https://gg.deals/blog/best-boomer-shooters-to-playif-you-like-retro-fps-and-doom/ — boomer shooters, Prodeus, Graven; https://twhl.info/wiki/page/texture — GoldSrc's 8-bit textures with a palette each; https://www.resetera.com/threads/pick-one-pixelated-jittery-ps1-look-or-n64-vomit-looking-textures.397156/ and https://forum.beyond3d.com/threads/psx-vs-n64-graphical-look.56516/ — PS1 and N64 styles; https://en.wikipedia.org/wiki/Abiotic_Factor and https://foro3d.com/en/2026/march/abiotic-factor-low-poly-nostalgia-with-modern-techniques-in-ue4.html — Abiotic Factor's look and pipeline; https://store.steampowered.com/app/2067820/RetroSpace/ and https://www.gamespew.com/2026/10/retrospace-impressions/ — RetroSpace.

## How do 3D games get a pixel-art or palette look, and what does a free camera cost?

There are two families of technique.

Rendering at low resolution and scaling up with sharp pixels gives true pixel art. A moving camera then makes pixels crawl and shimmer. The fixes are an orthographic camera snapped to whole texels with a screen-space offset to restore smooth motion (David Holland; t3ssel8r), or texel splatting for perspective cameras. These are complex and still restrict the camera.

Post-processing at native resolution gives the palette and shading without the pixel grid:

- posterize or quantize to a palette
- ordered (Bayer) dither
- outlines from depth and normals

Dither anchored to the screen shows a fixed pattern but doesn't crawl. Dither mapped onto surfaces swims as the camera turns.

The user wants a free camera and native resolution with post-processing, so the second family fits.

Rejected: rendering at low resolution and scaling up — it needs a snapped orthographic camera, and the user wants a free one. Error-diffusion dither such as Floyd–Steinberg — it flickers from frame to frame.

Sources: https://www.davidhol.land/articles/3d-pixel-art-rendering/ — outlines from depth and normals, and camera snapping; https://dylanebert.com/texel-splatting/ — perspective-stable pixel art; https://godotshaders.com/shader/outline-posterization-color-palette-and-dithering/ — a combined outline, palette, and dither pass; https://news.ycombinator.com/item?id=42810537 — screen-space versus world-space dither in 3D; https://tympanus.net/codrops/2025/06/04/building-a-real-time-dithering-shader/ — a real-time ordered dither.

## What does SteelJones's post-processing stack do, and what did it learn?

The user's other project, SteelJones (raylib on OpenGL 4.3), has a full stack. Every pass would need porting to SDL_GPU, but its techniques and tuning carry over.

The scene renders to a 16-bit float color target with sampleable depth. Passes then ping-pong in linear light:

1. Kuwahara flatten
2. posterize to luminance bands, with a screen-anchored 4×4 Bayer dither
3. outlines from depth and normals
4. exponential fog
5. a color grade
6. bloom on emitters only
7. an ACES tonemap and gamma
8. FXAA

Its lighting has a sun and a moon, each fading over a 10-degree twilight band. Lambert diffuse takes shadows from a 2048 map with 3×3 filtering. A hemisphere ambient mixes the sky and ground colors. Contact ambient occlusion darkens only the ambient light. A day cycle blends four keyframes, at midnight, sunrise, noon, and sunset, each holding sky, ground, key, and fog colors.

Its lessons:

- Anything that depends on the view, such as rim light or fog computed in the lit shader, slides in bands under posterizing. Apply those after posterizing.
- Posterizing alone pops at band edges. It needs the flatten first.
- Anchor dither to the screen so it doesn't crawl.
- Antialias after the tonemap with FXAA. MSAA can't smooth lines drawn in post.
- Ambient occlusion must darken only the ambient light. Multiplied over the whole frame, it bands into rings.
- Take fog's color from the sky's horizon, or distant shapes look paler than the sky behind them.
- Kuwahara is the costliest pass.

Sources: /mnt/c/Users/EvanUhhh/source/repos/SteelJones — src/render/post_chain.cpp and its passes, src/render/lighting.cpp, src/render/day_cycle.cpp, docs/specs/graphics-spec.md, and plans/atmospheric-presence/RESEARCH.md, plans/legible-abstraction/post-stack-polish/RESEARCH.md, plans/legible-abstraction/emitter-glow/RESEARCH.md, plans/legible-abstraction/grounded-solidity/MILESTONE.md, and plans/legible-abstraction/grounded-solidity/contact-ao/RESEARCH.md.

## How should the light follow the time of day?

The day-at-the-park slice researched this (plans/slices/day-at-the-park/RESEARCH.md, "How do stylized games render day and night cheaply and readably?"):

- The sun moves by its hour angle, tilted so it is never straight overhead.
- At night a dim, cool moon light replaces the sun.
- A hemispheric ambient light keeps flat faces apart when the sun is gone.
- Colors are authored as ramps keyed on the time of day.
- Night is lower, cooler, and less saturated, never uniformly darker.

The renderer reads the clock and never the reverse, and it may use the C runtime's trigonometry (decision 0022). SteelJones's day cycle, above, is a working example of the same approach.

Sources: plans/slices/day-at-the-park/RESEARCH.md.
