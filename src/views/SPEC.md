# views

Builds what the app shows by combining render's mesh builders with legible's explanations. render depends on no module but sim and core, and legible depends on sim alone. The two share a layer, so neither may combine them. views sits in the layer above both and below app (decision 0027).

## Boundary

The library tpj_views links tpj_render and tpj_legible. Like render, it builds only with the windowed application. It takes a world by const reference and changes nothing. The app and tpj_bench call availability_overlay.h.

## Availability overlays

availabilityShades(world, shape, definition, lists) gives render's shades of an overlay shape, in lists.Shades: the definition's availability at each of availabilityBends' places, halfway up the ramp at its Half. The app shades the overlay it shows with its need's availability. tpj_bench times the same call with food's, so what is timed is what is shown.

availabilityShades fills AvailabilityShadeLists that the caller keeps, and allocates nothing once its lists have grown.
