# Feature: Need Overlays

## Summary

Each of the guests' four needs gets an overlay on the guest paving. Food is served by shops that sell food, drink by shops that sell drink, rest by benches, and the toilet by toilet blocks. The Debug panel's Food overlay checkbox becomes an Overlay row of radio buttons, Off, Food, Drink, Rest, and Toilet, and --overlay takes food, drink, rest, or toilet. Every overlay discounts a facility's relief by one shared curve of the effective time, now in game minutes: it halves for about every 20 game minutes of walk and wait and reaches 0 at about two hours. A value of 0 shows the ramp's worst color, and the near white the overlay showed for nothing goes away, so the shading darkens smoothly and never ends abruptly partway along a path. Hovering shaded paving lists each contributing facility of the shown need, with its times in game minutes. Previews work for every need as they do for food, because each overlay reads the previewed world.

## Acceptance criteria

1. The Debug panel's Overlay row shows one need's overlay at a time, or none, and --overlay food, drink, rest, or toilet starts with that one shown. Each shades the guest paving by its need's availability. A capture of noon.park with the toilet overlay shows the paving in the ramp's best colors around the toilet block (slice criterion 6). A capture of noon.park with the drink overlay shows it in the ramp's best colors around the drink shop and its worst around the food shop (slice criterion 7). (capture: `--park tests/parks/noon.park --speed 0 --overlay toilet`, and the same with `--overlay drink`, `--overlay rest`, and `--overlay food`)
2. Every overlay uses the same curve of the effective time in game minutes, which halves a relief about every 20 minutes and reaches 0 at about two hours. A value of 0 shows the ramp's worst color, and no overlay shows a near white anywhere, so along every path the shading changes gradually and never ends abruptly. With the park running, the shading far from the food shop changes gradually from tick to tick, with no edge moving along a path. (capture: the food capture of criterion 1, against one made before this feature; play: day.park at 4x with the food overlay)
3. Hovering shaded paving lists each facility that contributes to the shown overlay, with its relief, route distance, wait and effective time in game minutes, and term, and the value. The value is the sum of the terms, to the precision shown (slice criterion 10). If no supplied facility of the need can be reached, the tooltip says so. (play: noon.park with each overlay)
4. While a ghost is shown, the overlay shows the candidate park. Placing a toilet block with the toilet overlay shows the coverage it would add, and choosing Both for the drink shop in the Inspector with the food overlay shows the food coverage it would add, before either is committed. (play: noon.park)
5. --overlay with any other name, or with --hash, prints the usage and exits with a nonzero status. (check: PLAN.md Task 13)
6. The simulation is unchanged: tpj_scenarios --compare of windows-debug output from before and after exits 0. The runtime report's food-overlay stage is reported before and after. The legible, views, render, and app specs describe the overlays. The windows-debug build has no warnings, ctest passes, and tidy is clean. (check: PLAN.md Tasks 1 and 15)

## Tests

None. The overlays are checked by captures and play, and the scenarios comparison shows the simulation unchanged (docs/testing.md).

## Medium

No fields or flows are added, and the simulation uses nothing this feature produces.

- Need offers (plausible-operations): food-offer and drink-offer at each shop's guest anchor, rest-offer at each bench's, and toilet-offer at each toilet block's, found as guests find them. Read for the overlays and their hover.
- Route distance (navigable-networks): guest route distance from each source, read for the overlays and their hover.
- Candidate resolution (deterministic-simulation, decision 0025): the previewed world, which every overlay reads as the food overlay does today.

Each availability is computed for display and never enters the world.

## Spec changes

src/legible/SPEC.md:

- Boundary, the bullet "food.h for foodAvailability" under "The app calls:" becomes two bullets: "availability.h for availabilityAt" and "needs.h for needAvailability and needAvailabilityName".
- Boundary, the last sentence becomes: "The app and tpj_bench read each need's availability from needs.h."
- Availability, the paragraph on AvailabilityDefinition: its last sentence becomes "Every need of a guest has one."
- Availability, the availabilityAt paragraph: "its relief, route distance, wait, effective time, and term" becomes "its relief, route distance, wait and effective time in game minutes, and term".
- Availability, the last paragraph becomes: "needAvailability(need) gives a need's availability, in needs.h. Food's sources are shops' food offers, drink's are shops' drink offers, rest's are benches' offers, and the toilet's are toilet blocks' offers. Every need is discounted by the same curve, because guests weigh a walk and a wait the same whatever they go for. The curve halves a relief for about every 20 game minutes and reaches nothing at about two hours. Each need's Half is the relief of one serving with no walk and no wait, so a color means the same availability in every park."

src/render/SPEC.md, Paving overlay, in the shadeOverlay paragraph:

- "a near white for none, and above none a perceptually uniform, colorblind-safe ramp from dark purple to yellow" becomes "a perceptually uniform, colorblind-safe ramp from dark purple at 0, the worst, to yellow".
- "The ramp keeps every positive value distinguishable from none, and none is distinguishable from both kinds of paving." becomes "Every color of the ramp is distinguishable from both kinds of paving."

src/views/SPEC.md, Availability overlays: "The app shades its food overlay with FOOD_AVAILABILITY. tpj_bench times the same call, so what is timed is what is shown. For another need, the shades come from the same call with that need's definition." becomes "The app shades the overlay it shows with its need's availability. tpj_bench times the same call with food's, so what is timed is what is shown."

src/app/SPEC.md:

- Tools and the ghost: "so the ghost, the food overlay, and the food tooltip agree" becomes "so the ghost, the overlay, and its tooltip agree".
- Tooling UI, the Debug panel: "the Graph and Food overlay checkboxes" becomes "the Graph checkbox, and the Overlay row of radio buttons Off, Food, Drink, Rest, and Toilet".
- Tooling UI, the paragraph that begins "The Graph checkbox draws" becomes: "The Graph checkbox draws the networks' lines and nodes over the scene and behind the panels. The Overlay row shows one need's availability on the guest paving of the previewed park, or none while Off is chosen (legible/SPEC.md, views/SPEC.md). Food shows how well shops that sell food serve each spot, Drink shops that sell drink, Rest benches, and Toilet toilet blocks. The overlay's shape is built again only when the previewed park's path points or segments change. While an overlay is shown, hovering shaded paving shows its need's value at the overlay place of the point under the cursor at the overlay's height (render/SPEC.md) and, for each facility that contributes, its relief, route distance, wait and effective time in game minutes, and term. Hovering anywhere else shows no tooltip. If no supplied facility of the need is reachable, the tooltip says so. While the preview places a shop, a tooltip shows the hungry footfall at its door and its supply route, or says that it has no supply route."
- Command line: "--graph and --overlay food check those boxes at start, so a capture shows them. food is the only overlay." becomes "--graph checks the Graph checkbox at start, and --overlay food, drink, rest, or toilet chooses that need's overlay at start, so a capture shows them."

The public interface:

- src/legible/availability.h:
  - REFERENCE_SPEED becomes the meters a guest walks in a game minute, `WALK_SPEED * SIM_TICK_SECONDS * 60.0 / static_cast<double>(GAME_SECONDS_PER_TICK)`.
  - A new `inline constexpr std::array<CurvePoint, 6> DISCOUNT_CURVE{{{0.0, 1.0}, {20.0, 0.5}, {40.0, 0.25}, {60.0, 0.125}, {80.0, 0.0625}, {120.0, 0.0}}};`, in game minutes.
  - AvailabilityContribution's `Shop` becomes `Source`, and its Wait and Time are in game minutes.
- src/legible/needs.h, new, replacing src/legible/food.h:
  - `const AvailabilityDefinition &needAvailability(GuestNeed need);`
  - `std::string_view needAvailabilityName(GuestNeed need);`, giving Food, Drink, Rest, and Toilet.
- src/render/paving_overlay.h: OVERLAY_ZERO_COLOR is removed.
- src/app/options.h: Options's `bool ShowFoodOverlay` becomes `std::optional<GuestNeed> Overlay`.
- src/app/ui/tooling_ui.h: ShownViews's `bool FoodOverlay` becomes `std::optional<GuestNeed> Overlay`.
- src/app/scene/scene_sync.h: PreviewLook's `bool FoodOverlay` becomes `std::optional<GuestNeed> Overlay`.
- src/app/ui/debug_panel.h: `void drawDebugPanel(const DebugStats &stats, bool &showGraph, std::optional<GuestNeed> &overlay);`
- src/app/ui/need_tooltip.h, new, replacing src/app/ui/food_tooltip.h: `void drawNeedTooltip(const World &world, GuestNeed need, const OverlayShape &shape, std::optional<ParkPoint> point);`

## Files affected

- Modify: src/legible/SPEC.md, src/render/SPEC.md, src/views/SPEC.md, src/app/SPEC.md
- Modify: src/legible/availability.h, src/legible/availability.cpp, src/legible/CMakeLists.txt
- Create: src/legible/needs.h, src/legible/needs.cpp
- Delete: src/legible/food.h, src/legible/food.cpp
- Modify: src/render/paving_overlay.h, src/render/renderer.cpp, src/render/shaders/overlay.frag
- Modify: src/app/options.h, src/app/options.cpp
- Modify: src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp, src/app/ui/debug_panel.h, src/app/ui/debug_panel.cpp
- Create: src/app/ui/need_tooltip.h, src/app/ui/need_tooltip.cpp
- Delete: src/app/ui/food_tooltip.h, src/app/ui/food_tooltip.cpp
- Modify: src/app/scene/scene_sync.h, src/app/scene/scene_uploads.cpp, src/app/application.cpp, src/app/CMakeLists.txt
- Modify: src/bench/stages.cpp

## Dependencies

- plausible-operations' served-needs: drink-offer, rest-offer, and toilet-offer. Met.
- legible-simulation's wide-overlay: the paving overlay and its hover. Met.
- noon.park and day.park in tests/parks. Met.

## Out of scope

- A ghost's context for needs other than hunger. It is in the milestone's deepening candidates.
- Switching the overlay to a ghost's need by itself. It is in the milestone's deepening candidates.
- Timing the drink, rest, and toilet overlays in tpj_bench. They cost what food's does, since they run the same code over one offer field each.

## Open questions

- The curve's values. The user adjusts them by watching day.park and noon.park with each overlay.
