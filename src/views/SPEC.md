# views

What the app shows, joined from render's mesh builders and legible's explanations. render depends on no module but sim and core, and legible on sim alone, and the two share a layer, so neither may join them; views sits in the layer above both and below app (decision 0027). The library tpj_views links tpj_render and tpj_legible, builds only with the windowed application, as render does, and builds on the CPU, so it is tested without a GPU. It takes a world by const reference and changes nothing.

## Food overlay

buildFoodAvailabilityOverlay(world) gives buildFoodOverlay of the world with, as its value function, foodAvailability's Value at each place in the world (render/SPEC.md, legible/SPEC.md). The app builds its food overlay with it, and tpj_bench times it, so what is timed is what is shown.
