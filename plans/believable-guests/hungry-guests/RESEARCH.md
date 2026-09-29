# Research: hungry-guests

## How does a guest keep its place across an edit?

A guest holds a place, a carrier and a distance, and an edit re-derives the guest network under it. shared-medium's carryOver moves a place given the networks before and after, and navigable-networks left it to this milestone to keep the previous network available until holders have carried their places over. The medium already has the pattern: a field's resolver keeps the previous resolution's entries in a Previous slot of its derived component, and the field's finisher compares against them and then drops them, so nothing extra is left in the world after a resolution and a save still loads back equal. path-networks can keep the previous networks the same way, and a guests finisher carries every guest's place with carryOver during the resolution. The field's own finisher drops its Previous slot, but finishers run in registration order and routes registers before guests, so the finisher that drops the previous networks is a routes finisher that addPark registers after the guests module. Every holder's finisher reads them first, no resolution ends holding them, and guests never clear routes' data. Then every world the cycle leaves, and every candidate, already holds carried places, so a preview's guests are where the committed edit will put them (principle 8), and the tick after an edit samples fields at places that resolve. A world's first resolution has no previous network, and a loaded guest's place is already on the network the load re-derives, so the finisher does nothing then, which keeps the rule that a first resolution changes no state. The sim contract says a finisher may only clear state its module's spec says a resolution invalidates. Carrying a place is a change of state that the resolution invalidates in the same sense, so the contract's sentence widens to cover it.

Rejected: each guest holding its ground position as state and re-snapping at its next step. It needs no previous network, but the places stay stale between the resolution and the step, so a candidate's guests stand on places that may not resolve, and carryOver goes unused while the guest repeats its rules. Rejected: keeping the previous network as saved state until guests step. Networks are derived, and a save would hold derived data (principle 1).

Sources: src/sim/medium/field.h, the Previous slot and settleSteppedEntries; src/sim/SPEC.md, finishers; plans/navigable-networks/paths-become-routes/MILESTONE.md, the hand-off.

## How does a guest move each tick?

A guest walks WALK_SPEED times SIM_TICK_SECONDS along its current edge each tick. When that reaches a node, it decides there, within the same tick, and walks the rest of the tick's distance along the edge it chose, so its speed does not depend on how closely nodes are spaced. Toward a target it takes route distance's Next step, which by the routes spec's walk arrives and walks exactly the distance first sampled. Wandering takes a keyed draw among the node's edges other than the one it came in on, and at a node with only that edge it turns back. Every walk is along carriers, so distance is measured along the routes (principle 4).

Rejected: stopping at each node and deciding next tick. Guests would slow on paths with many junctions, and speed would depend on how the network happens to be cut into edges.

Sources: src/sim/routes/SPEC.md, the mover's walk; plans/believable-guests/RESEARCH.md, on wandering.

## How is hungry footfall averaged?

An exponential moving average per stretch of the guest network, of the summed hunger of the guests on it each tick: each tick, the value moves 1/FOOTFALL_TIME of the way toward that sum. While the same guests stand on a stretch it approaches their summed hunger, so it counts guests weighted by hunger. It needs one number per stretch, where a sliding window needs each tick's history, and it has one tuning value, its time constant. It has no threshold (principle 5), and a stretch whose path is deleted loses its value with its carrier. Parkitect and Planet Coaster both give players heat maps, though a path-traffic map is a long-standing request in Planet Coaster, which suggests players want exactly this context.

Rejected: a sliding window. It needs a ring of past values per stretch, saved as state, for no visible difference. Rejected: counting guests above a hunger threshold. That is a gate, which principle 5 rules out.

Sources: https://en.wikipedia.org/wiki/Exponential_smoothing, the exponential window and its single parameter; https://forums.planetcoaster.com/showthread.php/21505-Heatmaps-for-guests, players asking for guest traffic heat maps; https://www.planetcoaster.com/player-guides/park-management, Planet Coaster 2's heat maps.

## How are guests drawn?

Guests move every tick, but the park mesh is rebuilt only when intent changes, so guests need a mesh of their own, rebuilt every frame from each guest's inspection record's ground position. A small upright box per guest, colored along a ramp by its hunger, shows hunger rising and falling without any UI. At 30 ticks a second, drawing the latest tick with no interpolation is smooth enough for simple shapes.

Rejected: adding guests to the park mesh. It would be rebuilt every tick, with every path and box. Rejected: interpolating between ticks. It needs the previous tick's positions and gains little at this tick rate.

Sources: src/app/SPEC.md, when the park mesh is rebuilt; src/render/SPEC.md, the park mesh.
