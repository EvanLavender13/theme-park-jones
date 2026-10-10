# Research: need-overlays

The milestone's RESEARCH.md settles the curve's shape, the worst color for nothing, and one overlay at a time. This file covers how the code changes.

## How does an effective time become game minutes?

Availability turns a route distance into time with REFERENCE_SPEED, the guests' WALK_SPEED of 1.3 m per real second at 1x, and an offer's wait in ticks with SIM_TICK_SECONDS (src/legible/availability.cpp). A tick is SIM_TICK_SECONDS of real time and GAME_SECONDS_PER_TICK game seconds (src/sim/clock.h). A guest therefore walks WALK_SPEED × SIM_TICK_SECONDS × 60 / GAME_SECONDS_PER_TICK meters per game minute, which is 1.3, and a wait of n ticks is n × GAME_SECONDS_PER_TICK / 60 game minutes. Writing both from the constants keeps them right if the tick rate or the clock changes.

addBends places a bend where the effective time reaches each curve point's X. It works in the curve's units, so it changes with the same two conversions and nothing else.

## What should the shared curve's points be?

Halving every 20 game minutes and reaching 0 at about two hours gives (0, 1), (20, 0.5), (40, 0.25), (60, 0.125), (80, 0.0625), and (120, 0). Each point adds bends wherever a source's effective time crosses it inside an edge. The old food curve had four points and reached 0 at 240 real seconds, 312 m. The new one has six and reaches 0 at 120 game minutes, 156 m, so the bends within reach of each source are about as many as before. The runtime report's food-overlay stage measures it.

## How does the renderer show 0 as the worst color?

overlay.frag returns the ramp's zero color, a near white, for any value that is not greater than 0, and the ramp's colors otherwise. The ramp maps a value v to v / (v + half) of the way up, which is the bottom color at 0. So showing 0 as the ramp's bottom takes only removing the zero branch and clamping a value below 0 to 0. The near white, OVERLAY_ZERO_COLOR, and its slot in the ramp's uniforms go with it.

## Where do the needs' definitions live?

food.h holds FOOD_AVAILABILITY and foodAvailability. With four needs, one function that gives a need's definition, keyed by believable-guests' GuestNeed, keeps the app and tpj_bench from naming each need. The offers come from sourceEntryAtNode of FoodOffer, DrinkOffer, RestOffer, and ToiletOffer (src/sim/medium/field.h, src/sim/operations). Each Half is its serving's relief: MEAL_RELIEF, DRINK_RELIEF, REST_RELIEF, and TOILET_RELIEF.

## How should the player pick an overlay?

The Debug panel's Food overlay checkbox becomes an Overlay row of radio buttons, Off, Food, Drink, Rest, and Toilet, like the Time panel's speed buttons. The names follow what the player places: food and drink from shops, rest from benches, the toilet from toilet blocks. --overlay takes the same names in lower case.
