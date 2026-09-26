# Design Notes

Provisional. This is current thinking from design conversations: illustrative, not binding. Do not implement these as fixed structures; they are starting points to be tested. When a note hardens into something code depends on, it moves into a module spec or a decision record.

## Fields and flows

Candidate fields include noise, theme, footfall, spectacle, smell, and food availability. Candidate flows include supplies, staff, garbage, and guests. Guests may be a flow carrying a payload of state such as hunger, fullness, nausea, and wetness, which rides and shops read and modify without knowing about each other.

## Networks

Current thinking has a guest path network, a backstage or service network, and ride tracks. Transport rides would be edges in these networks with travel time, wait, capacity, and price, so guests use them naturally when faster. Guest choice might combine utility (time saved) and experience (how fun or scenic).

## Field representation

"Field" is an interface (sample a field at a place), not a storage format. Storage can differ per field: a coarse grid, an on-demand sum over nearby emitters with falloff, values on network nodes, or cached visibility. Scale is not a concern at park size; a naive grid per field is fine at first. Most absent values are the default, like a vacuum.

## Resolution over time

Entities re-resolve when their sampled context changes meaningfully, not every tick. Most of the world is at rest most of the time.

## The generic food shop (first test case)

Samples demand, theme, and ambience. Draws supplies over the backstage network and staff. Emits food availability, smell, and litter pressure, plus garbage as a flow into bins. Throughput is limited by the scarcest of demand, supply, and staff. Form comes from demand, theme, and nearby seating (kiosk, counter stall, sit-down). Status should be visible in the building itself (shuttered window, harried cook). Later, a shop's identity (pizza, drinks, both) could emerge from what it is supplied with. Food and drinks should not be separate shop types. Payment is the result of an exchange, not an input.

## Coasters

Physics validity is the gate: complete circuit, energy source, brakes, station. The ride runs its own encapsulated train simulation (block sections, dispatch, forces) and exposes only emitted fields and flows: throughput, noise, spectacle, wear. Noise sources are extended along the track, not points. Wear is a stock drained by a maintenance flow; transfer tracks and service access are capacity for that flow. Track reachability for inspection and evacuation (catwalks, ground access for lifts) could be derived per section, with unreachable sections costing more downtime. Visible backstage could lower theme relative to what an area promises: Disney hides infrastructure, Cedar Point shows it, both are valid.

## Flat rides

Similar to shops with a cycle-based internal simulation. Tall flats are mostly spectacle. Splash zones affect bystanders.

## Ground appearance

Ground texture derived from fields rather than hand-painted: construction disturbance as a decaying stock, foot traffic wear producing desire paths that show where paths should go, grass length as a stock drained by groundskeepers, gravel under coasters derived from service access. Player paint is intent that overrides the derived look. Recovery should be fast by default or skippable for a cost, so building is not punished. Grounds are managed by per-area standards (manicured, tended, natural), and neglect can be a deliberate look.

## Legibility

Every field should be viewable as an overlay, and fields should be added only when a wanted mechanic needs them. Field values should be attributable to their sources. Placements should preview their consequences. Guests should explain decisions in terms of their actual decision factors. The known risk area is flows with nonlocal effects, such as a new transport line shifting demand across the park; previews and attribution are essential there.
