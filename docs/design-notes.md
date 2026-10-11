# Design Notes

These notes are provisional. They record current thinking from design conversations and are illustrative, not binding. Don't implement them as fixed structures; treat them as starting points to test. When code comes to depend on a note, move it into a module spec or a decision record.

## Fields and flows

Candidate fields include noise, theme, footfall, spectacle, smell, and food availability. Candidate flows include supplies, staff, garbage, and guests.

Guests may be a flow that carries a payload of state, such as hunger, fullness, nausea, and wetness. Rides and shops would read and modify that state without knowing about each other.

## Networks

The current thinking has three networks: a guest path network, a backstage or service network, and ride tracks. Transport rides would be edges in these networks with travel time, wait, capacity, and price. Guests would then use them naturally when they are faster. Guest choice might combine utility (time saved) and experience (how fun or scenic the trip is).

## Field representation

"Field" is an interface (sample a field at a place), not a storage format. Storage can differ per field. It could be:

- a coarse grid;
- an on-demand sum over nearby emitters with falloff;
- values on network nodes;
- cached visibility.

Scale is not a concern at park size, so a naive grid per field is fine at first. Most places have no stored value and read as the default.

## Resolution over time

Entities re-resolve when their sampled context changes meaningfully, not every tick. Most of the world is at rest most of the time.

## The generic food shop (first test case)

The shop samples demand, theme, and ambience. It draws supplies over the backstage network, and it draws staff. It emits food availability, smell, and litter pressure, and it sends garbage as a flow into bins. Its throughput is limited by the scarcest of demand, supply, and staff.

Its form comes from demand, theme, and nearby seating: a kiosk, a counter stall, or a sit-down shop. Its status should be visible in the building itself, such as a shuttered window or a harried cook.

Later, a shop's identity (pizza, drinks, or both) could emerge from what it is supplied with. Food and drinks should not be separate shop types. Payment is the result of an exchange, not an input.

## Coasters

Physical validity is the only thing that can block a coaster. It needs a complete circuit, an energy source, brakes, and a station. The ride runs its own encapsulated train simulation (block sections, dispatch, forces). It exposes only emitted fields and flows: throughput, noise, spectacle, and wear. Noise sources extend along the track; they are not points.

Wear is a stock drained by a maintenance flow. Transfer tracks and service access are capacity for that flow. Track reachability for inspection and evacuation (catwalks, ground access for lifts) could be derived per section. Unreachable sections would cost more downtime.

Visible backstage could lower theme relative to what an area promises. Disney hides infrastructure and Cedar Point shows it, and both are valid.

## Flat rides

Flat rides are similar to shops, with a cycle-based internal simulation. Tall flats are mostly spectacle. Splash zones affect bystanders.

## Ground appearance

Ground texture is derived from fields rather than hand-painted. Examples:

- construction disturbance, as a decaying stock;
- foot traffic wear, which produces desire paths that show where paths should go;
- grass length, as a stock drained by groundskeepers;
- gravel under coasters, derived from service access.

Player paint is intent that overrides the derived look. Recovery should be fast by default, or skippable for a cost, so building is not punished. Grounds are managed by per-area standards (manicured, tended, natural). Neglect can be a deliberate look.

## Staff work

Most staff work is a stock at a place. The stock builds up, and staff capacity drains it. The capacity is drawn over route distance from a staff room. Examples of such stocks are litter on a path segment, vomit, a full bin, a flower bed's thirst, a lawn's height, and a ride's wear. Garbage leaving a bin is a conserved flow, like supplies.

The logistical play is in the layout, not in who walks: where staff rooms sit, how far they are from what needs care, and which routes staff travel. Whether a pool or agents drain a stock stays inside the entity (decision 0018, principle 6). If a job feels dull as a pool, it could become agents without changing anything that reads its work.

Staff pass between guest and backstage paths only through buildings open on both sides, such as a staff room (decision 0040).

## Figures

Visible figures (janitors, gardeners, haulers with carts) are drawn where and when capacity is actually spent. They walk the real route from their source in the real travel time. A well-kept corner shows staff at work, and a neglected one shows none.

A figure's position is computed from current state each frame, such as when capacity set off for a place and how long the route is, and is never accumulated. This lets the game draw figures only for places in view, and they appear in a consistent state when the camera arrives.

Only presentation may depend on the camera. Because the simulation is the same everywhere (principle 10), if guests switch between individual and aggregate, the switch must depend on something like crowd density, never on the view.

## Legibility

Every field should be viewable as an overlay. Fields should be added only when a wanted mechanic needs them. Field values should be attributable to their sources. Placements should preview their consequences. Guests should explain decisions in terms of their actual decision factors.

The known risk area is flows with nonlocal effects, such as a new transport line shifting demand across the park. Previews and attribution are essential there.
