# Research: explained-needs

The capability's RESEARCH.md covers attribution, previews, and ramps, and explained-food's covers the availability measure. This file covers what the slice adds: an overlay for each need, the departures panel, and the hours panel's preview.

## What already exists?

The availability library is already generic over a need (src/legible/availability.h). An AvailabilityDefinition names the offer field, the discount curve, and the value an overlay shades halfway up its ramp. Food is the only definition. The food tooltip, the Debug panel's checkbox, --overlay food, scene uploads, and tpj_bench all name food directly.

served-needs publishes drink-offer, rest-offer, and toilet-offer in the same OfferEntry shape as food-offer (src/sim/operations/operations.h, facilities.h). Adding an overlay for each need takes a definition each and no new library code.

The guest Inspector already shows the four needs, the target's need, heading home's reason, and the choice table with heading home's terms (src/legible/SPEC.md, Inspectors). The shop Inspector already shows stock and orders for each good. Benches and toilet blocks publish a FacilityRecord (places, taken, queue), but the Inspector can't pick them: an InspectorSubject is a guest or a shop.

believable-guests publishes departureTally (today's departures by reason since the park last opened) and arrivalCurve(world, second), the rate each entrance admits at a second of the day under the world's hours. Nothing shows either yet. The Hours panel already makes a candidate with the tentative hours its preview, as a SetOpeningHours edit (src/app/SPEC.md, Tooling UI).

## In what units should the discount curves be set?

Availability's effective time is a route distance at the guests' walking speed plus the offer's wait, in seconds of real time at 1x. FOOD_DISCOUNT_CURVE reaches 0 at 240 s. The park clock runs one game minute for each real second at 1x, and guests walk 1.3 m in it, so 240 s is four game hours and 312 m of walking. In day.park the shops stand about 90 m apart and about 70 m from the entrance, so the food overlay covers the whole park evenly and the two shops can't be told apart (believable-guests/a-day-out/day-parks/RESEARCH.md).

Setting the curves in game time makes the overlay and its tooltip read in the clock's units. A walk shown as "12 min" matches what the player sees the clock do. Real seconds at 1x and game minutes are the same number, so the change is a relabeling of the time and the curves' points, not a new measure.

Walk Score, the best-known accessibility index, gives an amenity full value within a 5-minute walk, decays it after that, and gives nothing past a 30-minute walk. A park is smaller than a city, but the first part of that shape fits a guest with a need: anything close counts fully, and farther things count less. At this park's 1.3 m per game minute, a 20-minute walk is 26 m and an hour is 78 m.

The part where the value reaches zero doesn't fit. Where a path crosses the time at which the curve reaches 0, the shading jumps from the ramp's darkest color to the zero color, which is white so that "nothing" stands apart. Each tick the offers' waits change, so that time moves along the path and the edge flickers. The user has seen it on the food overlay and rejected it: an overlay should never stop partway along a path a guest can walk.

So the curve should never reach 0. It halves for about every 20 game minutes of walk and wait, and from about an hour and a half on, where halving would pass 5%, it holds at a faint floor of about 5% instead of falling to nothing. Every path that reaches a supplied source is shaded, the farthest faintly. The zero color then shows only where no supplied source of the need can be reached, on a path cut off from them or when every source is starved, so its edges fall at breaks in the paths or the supplies. The curve stays piecewise linear, with a flat last segment at the floor, so availabilityBends still makes the shading exact between its places.

Guests score every offer's distance the same way, whatever its need (DISTANCE_WEIGHT, src/sim/guests/guests.h). A curve for each need would show a difference that guests don't act on. One shared curve keeps the overlays honest about what guests feel. A need-specific curve still fits AvailabilityDefinition if guests ever weigh distance by need. Each overlay still shades around its own sources: food around shops that sell food, drink around shops that sell drink, rest around benches, and the toilet around toilet blocks.

Rejected: keeping the curve in real seconds — it reads as four game hours and covers day.park evenly. A curve that reaches 0 — the shading ends abruptly partway along a path and its edge flickers as waits change. An exponential curve — it never reaches zero, but it isn't piecewise linear, so the shading between bends would no longer be exact. A separate curve for each need — guests don't weigh distance by need, so the overlays would disagree with the guests.

Sources: https://cem.tti.tamu.edu/tool/walk-score/ — Walk Score's full value within 5 minutes and nothing past 30; https://radiusmapper.com/blog/walk-score-explained — its decay bands.

## How do other games switch between need views?

Cities: Skylines shows one info view at a time. A coverage view colors the roads by how well a service covers them, gray where it doesn't, which is the same idea as shading the paving. Placing a service building switches to its view, which some players find intrusive and mods turn off. Planet Coaster 2 adds heatmaps of hunger, thirst, and toilet need, chosen from one Heatmaps button, red where there's a problem and blue elsewhere. Its heatmaps show how needy guests are where they stand, not how well facilities serve a place.

One overlay at a time, chosen from a list, fits the paving overlay, which has one shade per spot. Two needs' shades on the same paving would hide each other.

Rejected: several overlays at once — one spot of paving shows one shade. Switching the overlay to a ghost's need automatically — players find it intrusive in Cities: Skylines, and the player here chooses the overlay that previews the change they care about. It could be an option later.

Sources: https://skylines.paradoxwikis.com/Info_views — Cities: Skylines' info views and coverage on roads; https://www.planetcoaster.com/player-guides/park-management — Planet Coaster 2's heatmaps.

## How do park games show why guests leave?

RollerCoaster Tycoon 2 shows a guest's thoughts, such as being hungry, needing the toilet, or wanting to go home, in each guest's window and in a list of thoughts across the park. A guest that decides to leave never changes its mind. Planet Coaster 2 has guests leave early when hunger, thirst, or the toilet go unmet, or when their park fatigue fills, and shows guests' moods and needs in its park management panel. Neither shows a count of departures by cause.

Here going home is a scored option, and the guest records its largest term as its reason, so the park can count departures by reason exactly. A panel of five fixed rows, one per reason, shows today's tally. Its layout doesn't grow with the park.

Sources: https://rct.fandom.com/wiki/Guest_Thoughts — RCT's thoughts; https://www.planetcoaster.com/player-guides/keeping-guests-happy — Planet Coaster 2's reasons to leave early.

## How should the Hours panel preview the arrival curve?

The slice requires reading the curve from a candidate with the tentative hours applied, never computing it in legible-simulation. arrivalCurve already takes a world and a second of the day. The app's preview holds one edit, and the hours choice is that edit only while the tool and the Inspector give none of their own. So the panel needs a candidate of its own with only the chosen hours applied, made when the choice changes, and samples arrivalCurve on it at even steps through the day. Then the curve follows the chosen hours while a ghost previews something else. Marking the park's time of day on the curve shows where the day stands. ImGui's PlotLines draws a sampled curve with no extra library.

Rejected: reading the curve from the previewed world — while a ghost or a stock choice is the preview, that world has the park's hours, so the curve wouldn't match the hours the panel shows.
