# views

What the app shows, joined from render's mesh builders and legible's explanations. render depends on no module but sim and core, legible on sim alone, and the two share a layer, so neither may join them. views sits in the layer above both and below app (decision 0027).

## Boundary

The library tpj_views links tpj_render and tpj_legible and builds only with the windowed application, as render does. It takes a world by const reference and changes nothing. The app and tpj_bench call availability_overlay.h.

## Availability overlays

buildAvailabilityOverlay(world, definition) gives render's band overlay of the world shaded by the definition's availability at each place, saturating at its Full. The app builds its food overlay with FOOD_AVAILABILITY, and tpj_bench times the same call, so what is timed is what is shown. An overlay of another need is the same call with its definition.
