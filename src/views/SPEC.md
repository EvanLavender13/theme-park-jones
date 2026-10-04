# views

Builds what the app shows by combining render's mesh builders with legible's explanations. render depends on no module but sim and core, and legible depends on sim alone. The two share a layer, so neither may combine them. views sits in the layer above both and below app (decision 0027).

## Boundary

The library tpj_views links tpj_render and tpj_legible. Like render, it builds only with the windowed application. It takes a world by const reference and changes nothing. The app and tpj_bench call availability_overlay.h.

## Availability overlays

buildAvailabilityOverlay(world, definition) gives render's band overlay of the world, shaded by the definition's availability at each place and saturating at its Full. The app builds its food overlay with FOOD_AVAILABILITY. tpj_bench times the same call, so what is timed is what is shown. For another need, the overlay comes from the same call with that need's definition.
