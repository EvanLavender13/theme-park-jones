# Capability: Legible Simulation

## Summary

legible-simulation helps the player see why the park behaves as it does. It also shows what a change would do before the player makes it (principle 8).

It works in three ways:

- Every overlay answers "why here" by attributing its value to its sources.
- Every change, whether a placement or a deletion, previews its consequences on an exact candidate world.
- The player can ask any guest or shop what it is doing and why. The answer uses its actual decision factors.

This is the answer to Cities: Skylines' overlays, which show what is but not why or what if (docs/vision.md).

This matters because the player can then direct the park by understanding it, instead of by trial and error.

The capability never finishes. It grows with an overlay for every field, traces through chains of causes, histories, and the player-facing form of explanations.

## Foundation criteria

Food availability at a place is a sum over shops. A shop counts if its offer says meals are available. Each shop adds its relief, discounted by an authored curve (decision 0020). The curve's input is effective travel time: route distance at a reference walking speed, plus the offer's expected wait.

Each shop's term is its attributed contribution. At every sampled place, the terms rebuild the value exactly, bit for bit (slice criterion 4).

A library computes availability and its attribution. It reads the simulation's fields and has no rendering, so integration tests can use it headless. It doesn't write to the world, and it isn't part of the simulation's state, hash, or saves.

The food-availability overlay shades the paving of guest paths across its whole width. Each spot of a path's paving takes the value at the place on that path's line nearest the spot. Junction paving takes it from the nearest of its paths, and where paving of unjoined paths overlaps, the nearest line's value shows. The ground beside paths, door walkways, and backstage paths are not shaded. The color ramp is perceptually uniform and colorblind-safe. Zero is clearly distinct from the rest of the ramp and from unshaded paving. The app's --overlay food option turns the overlay on from the start, for scripted captures.

When the player hovers over a shaded spot, the game lists each contributing shop with:

- its relief;
- its route distance;
- its expected wait;
- its term.

The terms sum exactly to the shown value.

A candidate world is kept current while either of these is true:

- a valid placement ghost for a path or box is shown;
- the delete tool hovers over a path or box.

The candidate is rebuilt on every tick and whenever the tentative edit changes. It starts as deterministic-simulation's copy of the world after that tick's swap. The tentative command is applied and resolution runs, but no ticks are stepped. The overlay then shows the candidate's availability.

A shop ghost's context shows two things:

- hungry footfall where its connector meets the guest path;
- the length of its supply route, or "no supply route".

A deletion preview shows the park without the hovered path or box. For example, it might show a shop starved and the overlay dimmed around it.

If the ghost is invalid, there is no candidate, and the overlay stays on the committed park.

If you make a candidate from the world after a cycle's swap, apply a command to it, and resolve it, the candidate equals, field for field, the world you get by committing that command in the same cycle (slice criterion 5). Placements and deletions both behave this way. Previewing any number of times leaves the committed world's state hash unchanged.

Clicking a guest opens its inspector, which shows:

- the guest's state;
- its hunger;
- its target;
- its last choice, as a table of options with each option's terms, score, and probability, and which one it picked.

Clicking a shop opens its inspector, which shows:

- the shop's stock;
- its queue;
- its supplies on order;
- its limiting factor;
- whether it is starved.

Both inspectors read only inspection records and intent (decision 0025). A click picks a guest by the ground position that its inspection record publishes. It picks a shop by the box pose in intent.

## Medium

This capability produces nothing that the simulation uses. It samples and reads:

- Food offer from plausible-operations, route distance from navigable-networks, and hungry footfall from believable-guests. It reads all three through shared-medium's field interface, in committed or candidate worlds.
- Candidate worlds from deterministic-simulation. navigable-networks and plausible-operations resolve them from the tentative intent that effortless-building's ghost supplies (decision 0025). This capability doesn't call another capability's derivation directly.
- Inspection records from believable-guests and plausible-operations (decision 0025). The guests' records include each guest's ground position, for picking.
- Park intent from effortless-building. It uses the paved surface of guest paths to place the overlay, and box kinds and poses to pick shops and find depots. It also reads the tentative edits, placements and deletions, that previews resolve.
- Networks from navigable-networks, through shared-medium's Network queries, to find path places, anchors, and connectors.

Food availability is a scalar field that this capability computes for display, from the fields above. If a park entity ever needs it, move it into the medium. deterministic-simulation's walk will then cover it.

## Principles

- Principle 8 is this capability's reason to exist. The hover shows that attribution rebuilds each value exactly. The world checks confirm that a candidate equals the world that commits the same commands, so previews equal the committed result.
- Principle 10 is at risk if tooling feeds back into the simulation. The library only reads. Previews run on copies, so they leave the committed world's hash unchanged.
- Principle 6 is at risk if inspectors read components directly. They read only inspection records and fields.
- Principle 4 holds because availability is measured along routes. Shading on a path takes the value at a place on that path's line, so it never implies straight-line access.
- Principle 1: overlays, previews, and inspectors are derived, and they aren't saved.

## Dependencies

- deterministic-simulation (world-as-value): candidate copies. Met.
- shared-medium (first-field-and-flow): field sampling. Met.
- effortless-building (sketch-a-park): ghosts and tentative intent, and the rendering that the overlay draws into. Met.
- navigable-networks (paths-become-routes): route distance and candidate resolution. Met.
- plausible-operations (supplied-food-shop): food offers, candidate resolution of shops, and shop inspection records. Met.
- believable-guests (hungry-guests): hungry footfall and guest inspection records. Met.
- The ImGui tooling UI: met. The player-facing UI question (docs/open-questions.md) stays open, and ImGui stands in for it.

## Foundation

The foundation is the smallest version in which the player can ask why and what if about the slice's park:

- one overlay with exact attribution;
- previews of placements and deletions on candidate worlds;
- inspectors for guests and shops.

It is useful for development too. Every other capability's behavior becomes observable without a debugger.

## Milestones

1. `explained-food`: the food-availability library with exact attribution, and the food overlay with hover attribution and the --overlay option. Previews of placements and deletions run on candidate worlds rebuilt each tick, and a shop ghost shows its context. Guests and shops get inspectors. Member of the boxes-and-tubes slice. Depends on: every other member milestone of the slice.
2. `wide-overlay`: the food overlay shades the guest paving itself, across every width, with the hover over the shaded paving. Member of the good-paths slice. Depends on: navigable-networks' wide-routes.

Later milestones are drawn from the deepening candidates once the slice has been played.

## Deepening candidates

- An overlay for any field, generated from shared-medium's field enumeration instead of written by hand for each field.
- A difference view in previews: show the change the ghost would make, not just the result.
- Cause chains: follow an outcome back through fields and flows. For example, a hungry guest leads to a starved shop, which leads to a cut route.
- Histories: fields and flows over time, and graphs of a shop's stock, queue, and service.
- Flows made visible: supplies drawn as carts and guest visits drawn as queues, derived from the ledger for presentation (decision 0018).
- The player-facing form of overlays, previews, and explanations. Gated on: the player-facing UI question.
- Previews that simulate ahead in time. They are out of scope for the slice. Gated on: candidate copies being cheap enough to step.
- A park statistics panel, designed to grow with the park's needs and services, with a fixed layout that doesn't resize as values change. It replaces the Debug panel's per-shop lines and its guest hunger and meals line, which were removed because they grew without bound and named only food.
- Availability estimated from what guests experience, instead of computed from every route and queue. The game would record the walk and wait that guests actually took to reach a meal. It would keep each record at the place the guest started, the way it keeps hungry footfall. The overlay would then be a realistic, statistical picture instead of an all-knowing one.

## Open questions

- Whether copying and resolving a candidate on every ghost move stays interactive. explained-food's candidate-previews resolves this by measurement. The fallbacks are throttling or incremental resolution.
- The player-facing UI (docs/open-questions.md). The user decides.

## Research notes

- Players play their mental model, so the game must show the medium's inputs.
- Cities: Skylines shows what is, without why or what if.
- Perceptually uniform ramps such as viridis keep overlays readable and colorblind-safe.

The details are in RESEARCH.md.
