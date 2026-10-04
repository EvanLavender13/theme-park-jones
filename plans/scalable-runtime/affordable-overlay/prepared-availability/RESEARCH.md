# Research: prepared-availability

## How is an availability overlay defined, so a later one is a definition and not new code?

Food availability is a sum over reachable sources of a relief discounted by an effective time, and its only food-specific parts are the field it reads offers from, FoodOffer, the curve that discounts, FOOD_DISCOUNT_CURVE, and the value at which the overlay's ramp saturates, render's OVERLAY_FULL. Everything else, which sources count, where an offer is read, the effective time from route distance, REFERENCE_SPEED, and the wait, and the order the terms are added in, is the same for any need served by shops. A thirst overlay would read a drink offer whose entries are the same OfferEntry, with its own curve and saturation.

So a definition is a small value: a function giving a source's offer at a node, which for food is sourceEntryAtNode of FoodOffer, a span of curve points, and the saturating value. Legible's availability takes it as an argument, and foodAvailability becomes the availability of the food definition, so the tooltip and foodNear keep their meaning and their types under aliases. The saturating value moves into the definition because legible cannot include render, which shares its layer (decision 0027), and render's ramp takes it as an argument.

Rejected: a template parameter naming the offer field, as the medium's FieldDefinition types do. It would make every availability function, and views' overlay builder, a template in a header, for no gain over a function pointer read once per source. A definition naming offer fields by string. The medium's sampling is typed by field, so a string would need a registry to map back to a type.

## How does a build find each source's offer once?

An offer depends only on its source: the source's lowest anchored node and the offer field's entry there. Today foodAvailability finds it again at every place for every source, which the profile puts at about 37% of the full park's build. A function given all of a build's places at once can find each source's offer the first time a place reaches it, keep it for the rest of the call, and drop it when the call returns, so nothing is kept between builds (principle 1). Computing each place's value with the same code that computes one place's, adding the same terms in the same order, keeps the values equal bit for bit to the single-place function's, which the tooltip relies on (principle 8).

Rejected: a prepared object the caller makes once and passes to each place's call. It would be kept state with a lifetime the caller manages, and the caller would still call once per place, so feature 2's batched route distance sample would need a second interface anyway. Finding every shop's offer up front from parkBoxes. It would read boxes, which the availability does not otherwise read, and find offers for shops no place reaches.

## How does the band ask for its values?

The band builder already samples every guest path line and asks a value function for each place. Collecting every row's place first, asking for all values in one call, then coloring the rows, keeps the geometry exactly as it is and lets the value side see the whole build. The places come grouped by line and ascending along it, which is what feature 2's batched sampling wants: consecutive places on one edge.

Rejected: one call per line. It would find offers again for each of the full park's lines, and feature 2 shares lookups only within one edge anyway, so the whole build in one call costs nothing extra.

Sources: src/legible/food.cpp, src/render/food_overlay.cpp, src/views/food_overlay.cpp — today's build; src/sim/medium/field.h — sourceEntryAtNode; cmake/layers.txt — legible and render on one layer; plans/scalable-runtime/affordable-overlay/RESEARCH.md — the profile.
