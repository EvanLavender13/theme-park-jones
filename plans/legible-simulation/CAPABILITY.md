# Capability: Legible Simulation

## Summary

legible-simulation deepens how well the player can see why the park behaves as it does, and what a change would do before they make it (principle 8). Every overlay answers "why here" by attributing its value to its sources. Every change, placement or deletion, previews its consequences on an exact candidate world. Every guest and shop can be asked what it is doing and why, in terms of its actual decision factors. This is the answer to Cities: Skylines' overlays, which show what is but never why or what-if (docs/vision.md).

Its value is that the player can direct the park by understanding it rather than by trial and error. It deepens with an overlay for every field, traces through chains of causes, histories, and the player-facing form of explanations, and never finishes.

## Foundation criteria

- Food availability at a place is the sum, over shops whose offers say meals are available, of each shop's relief discounted by an authored curve (decision 0020). The curve's input is effective travel time: route distance at a reference walking speed plus the offer's expected wait. Each shop's term is its attributed contribution, and the value is reconstructed exactly from them, bit for bit, at every sampled place (slice criterion 4).
- Availability and its attribution are computed by a library that reads the simulation's fields and has no rendering, so integration tests use it headless. It never writes to the world, and it is not part of the simulation's state, hash, or saves.
- The food-availability overlay shades the ground within a fixed band of a guest path, taking the value at the nearest path place. Beyond the band there is no shading. It uses a perceptually uniform, colorblind-safe ramp, with zero clearly distinct. The app's --overlay food option turns it on from the start, for scripted captures.
- Hovering a shaded spot lists each contributing shop with its relief, route distance, expected wait, and term. The terms sum exactly to the shown value.
- While a valid placement ghost for a path or box is shown, or the delete tool hovers a path or box, a candidate world is kept current. It is rebuilt on every tick and on every change to the tentative edit, as deterministic-simulation's copy of the world after that tick's swap, with the tentative command applied and resolution run, and no ticks stepped. The overlay then shows the candidate's availability. A shop ghost's context shows hungry footfall where its connector meets the guest path, and the length of its supply route, or "no supply route". A deletion preview shows the park without the hovered path or box, such as a shop starved and the overlay dimmed around it. An invalid ghost produces no candidate, and the overlay stays on the committed park.
- A candidate built from the world after a cycle's swap, with a command applied and resolved, equals the committed world when that same command is applied in that cycle, field for field (slice criterion 5). This holds for placements and deletions. Any number of previews leaves the committed world's state hash unchanged.
- Clicking a guest opens its inspector: its state, hunger, target, and last choice as a table of options, with each option's terms, score, probability, and the one picked. Clicking a shop opens its inspector: stock, queue, supplies on order, limiting factor, and whether it is starved. Both read only inspection records and intent (decision 0025). A click is matched to a guest by the ground position its inspection record publishes, and to a shop by its box pose in intent.

## Medium

This capability produces nothing the simulation consumes. It samples and reads:

- Food offer from plausible-operations, route distance from navigable-networks, and hungry footfall from believable-guests, all through shared-medium's field interface, in committed or candidate worlds.
- Candidate worlds from deterministic-simulation, resolved by navigable-networks and plausible-operations from tentative intent that effortless-building's ghost supplies (decision 0025). It never calls another capability's derivation directly.
- Inspection records from believable-guests (including each guest's ground position, for picking) and plausible-operations (decision 0025).
- Park intent from effortless-building: guest paths, to place the overlay's band, box kinds and poses, to pick shops and find depots, and the tentative edits (placements and deletions) that previews resolve.
- Networks from navigable-networks, through shared-medium's Network queries, to find path places, anchors, and connectors.

Food availability is a scalar field this capability computes for display, from the fields above. If a park entity ever needs it, it moves into the medium, and deterministic-simulation's walk then covers it.

## Principles

- Principle 8 is this capability's reason to exist. Attribution is tested to reconstruct exactly, and previews are tested to equal the committed result.
- Principle 10 is at risk if tooling feeds back into the simulation. The library only reads, previews run on copies, and a test checks that previews leave the committed hash unchanged.
- Principle 6 is at risk if inspectors read components directly. They read only inspection records and fields.
- Principle 4 is kept by availability being measured along routes. Shading near a path takes the path's value and never implies straight-line access.
- Principle 1: overlays, previews, and inspectors are derived and never saved.

## Dependencies

- deterministic-simulation (world-as-value): candidate copies. Met.
- shared-medium (first-field-and-flow): field sampling. Met.
- effortless-building (sketch-a-park): ghosts and tentative intent, and the rendering the overlay draws into. Met.
- navigable-networks (paths-become-routes): route distance and candidate resolution. Met.
- plausible-operations (supplied-food-shop): food offers, candidate resolution of shops, and shop inspection records. Met.
- believable-guests (hungry-guests): hungry footfall and guest inspection records. Met.
- The ImGui tooling UI: met. The player-facing UI question (docs/open-questions.md) stays open; ImGui stands in.

## Foundation

The foundation is one overlay with exact attribution, previews of placements and deletions on candidate worlds, and inspectors for guests and shops. That is the smallest version in which the player can ask why and what if of the slice's park. It produces value for development as well: every other capability's behavior becomes observable without a debugger.

## Milestones

1. `explained-food`: the food-availability library with exact attribution, the banded overlay with hover attribution and the --overlay option, previews of placements and deletions on candidate worlds rebuilt each tick, with a shop ghost's context, and guest and shop inspectors. Member of the boxes-and-tubes slice. Depends on: every other member milestone of the slice.

Later milestones are drawn from the deepening candidates once the slice has been played.

## Deepening candidates

- An overlay for any field, generated from shared-medium's field enumeration rather than written per field.
- A difference view in previews: the change the ghost would make, not just the result.
- Cause chains: following an outcome back through fields and flows, such as a hungry guest, to a starved shop, to a cut route.
- Histories: fields and flows over time, and graphs of a shop's stock, queue, and service.
- Flows made visible: supplies drawn as carts and guest visits as queues, derived from the ledger for presentation (decision 0018).
- The player-facing form of overlays, previews, and explanations. Gated on: the player-facing UI question.
- Previews that simulate ahead in time, out of scope for the slice. Gated on: candidate copies being cheap enough to step.

## Open questions

- Whether copying and resolving a candidate on every ghost move stays interactive. Resolved by measurement in explained-food's candidate-previews; throttling or incremental resolution are the fallbacks.
- The player-facing UI (docs/open-questions.md). Evan decides.

## Research notes

- Players play their mental model, so the medium's inputs must be shown.
- Cities: Skylines shows what is without why or what if.
- Perceptually uniform ramps such as viridis keep overlays readable and colorblind-safe.

Depth is in RESEARCH.md.
