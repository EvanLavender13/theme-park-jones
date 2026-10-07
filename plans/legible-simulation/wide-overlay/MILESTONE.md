# Milestone: Wide Overlay

Slice: good-paths

Status: complete

## Summary

wide-overlay moves the food overlay onto the paths. Before it, the overlay shaded a band of ground beside each guest path, under the paving. On main-street.park's 10 m street, the paving covered most of that band, so the overlay was hidden where the park matters most. After this milestone, the overlay shades the guest paving itself, across the whole width of every segment and junction, through tapers and rounded corners. Each spot takes the food availability at a place on the line of the path whose paving it is. The hover tooltip shows over the shaded paving. This is the slice's fourth member. wide-routes has made every path's paving and network match what the player drew, so the overlay can now follow the paving.

## Acceptance criteria

1. With the Food overlay on, all guest paving is shaded: every segment at its own width, every junction, taper, and rounded corner. The ground beside paths, door walkways, and backstage paths are not shaded. Each spot of a segment's paving takes the food availability at the place on that segment's line nearest the spot, so the shading changes along a path as it does today. Junction paving takes it from the nearest of the point's segments, and where paving of unjoined paths overlaps, the nearest line's value shows. The paving's edge lines draw on top of the shading, and boxes, guests, and ghosts draw over it. Checked by a capture of main-street.park with `--ticks 3000 --overlay food`, and by playing it with the overlay on. In play, widen a side path to the widest width the tools allow, and draw a sharp corner between two segments of that width. Then look close at both, and at a junction, a taper, and a rounded corner, for paving left unshaded.
2. A value of zero shows in a color distinct from every color of the ramp, from guest paving, and from backstage paving. Checked by a capture of new.park with `--overlay food`, whose one path reaches no shop, next to the backstage paving of main-street.park.
3. Hovering shaded paving shows the food tooltip for the place the shading under the pointer used, by the same rule, and hovering the ground beside a path shows no tooltip. Checked by playing main-street.park with the overlay on.
4. While a ghost is shown, the shading follows the candidate park's paving. Widening a segment with Edit path shades the new width under the ghost, and a path the Delete tool hovers shows no shading. Checked by playing main-street.park with the overlay on.
5. The render, views, legible, and app SPEC.md files describe the overlay on the paving and the hover over it, and no text in them or in the capability brief describes a band beside the paths. Checked by reading them against the code.

## Medium

The milestone adds no fields or flows, and the simulation uses nothing it produces.

- Park intent's derived shape (decision 0025): the paved surface sim/park publishes, its guest pieces only, without walkways. paving-overlay reads it to place the shading and the hover.
- Networks (shared-medium, navigable-networks): the guest network's segment carriers, to find the place on a path's line that each spot of its paving takes its value from.
- Food availability (legible-simulation's own library): computed from food offer and route distance, as today. paving-overlay reads it unchanged.
- Candidate worlds (deterministic-simulation): the overlay and hover read the previewed world, as today.

## Dependencies

- navigable-networks/wide-routes: complete. Guest paving matches the segments drawn, and doors connect to it.
- effortless-building/path-network: complete. sim/park publishes the paved surface with its pieces.
- legible-simulation/explained-food: complete. The availability library, the overlay, its hover, and candidate previews.

## Core feature

paving-overlay. It is the whole milestone: the shading moves onto the paving, zero gets its own color, and the hover follows the shading. The parts can't be split usefully, because a hover over paving with the shading still beside it would disagree with what the player sees.

## Features

1. `paving-overlay`: the food overlay draws the guest paving's pieces shaded by the food availability at the nearest place on each piece's own path, with the edge lines on top, a zero color distinct from both kinds of paving, and the hover tooltip over the shaded paving. Depends on: none.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Shading the door walkways by the value where each meets its path, so a shop's front reads as part of the shaded network. Gated on: players finding unshaded walkways confusing.

## Open questions

None. The runtime report settled the overlay's cost: on windows-release, the food-overlay stage fell from 3.61 ms to 3.26 ms per tick on full.park and from 256 µs to 27 µs on winding-path.

## Research notes

- Cities: Skylines colors the roads themselves by traffic and service coverage, and Planet Coaster players asked for heatmaps on the paths, so values that belong to path places read best on the paths.
- The overlay draws the guest paving's own pieces, so every spot of paving is shaded by construction. A shader colors each pixel by the value at the nearest place on its piece's lines, and where pieces of other owners overlap, the nearest line's value shows. Availability is linear between the bends the CPU finds along each line, so the shading is exact, and each tick uploads only the values at the bends. The hover uses the same rule. A band cut or masked to the paving was rejected, because its reach would have to be sized to the paving, and one flat color per piece loses the gradient along a street.
- The zero color must differ from backstage paving's blue gray as well as from the ramp, so a guest path with no food doesn't look like a backstage path.

Depth is in RESEARCH.md.
