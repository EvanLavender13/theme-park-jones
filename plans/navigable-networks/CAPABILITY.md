# Capability: Navigable Networks

## Summary

navigable-networks covers how things travel through the park. It decides which places connect, how far apart they are along the routes things actually take (principle 4), and which way to go.

It does four things:

- It turns path intent into the park's networks.
- It connects boxes to those networks.
- It derives the route distance field, which measures every place against every source.
- It gives movers a direction to follow.

This matters because distance should mean what a guest on foot or a supply cart would experience. A shop across a fence is far away, however close it looks. Every choice, supply route, and overlay in the game depends on this measure.

The capability grows over time, with transport rides as edges, one-way and queue paths, crowding, bridges and tunnels, and scale. It is never finished.

## Foundation criteria

Resolving path intent derives one graph for each network kind: guest and backstage. Nodes sit at:

- path endpoints;
- snapped junctions;
- places where paths of the same kind cross;
- places where connectors meet a path.

Each edge's length is measured along the path's ground line. The game draws that line, and checks it when it refuses a path. Paths of different kinds never connect, even where they cross.

Each kind of box connects to a network from the face that serves that network kind. It connects to the nearest point of a path of that kind, within a short fixed reach:

- a shop's front connects to a guest path, and its back connects to a backstage path;
- a depot's front connects to a backstage path;
- the entrance connects to a guest path.

The connector is a derived edge, and its length counts in routes. Its end node is anchored to the box's entity. If no path of the right kind is within reach, the box has no connection on that network, and that is a legitimate state (principle 2).

Every anchored entity is a route distance source on each network it connects to. When the field is sampled at a place, the sample has an entry for each source reachable from that place. Each entry holds the distance along the network and the next edge toward the source. If a place has no route to a source, it has no entry for that source. Each distance is the shortest distance along the network, and ties between equal routes break by a fixed rule.

From any place with an entry, if a mover follows the next edges one after another, it reaches the source. The length it walks equals the sampled distance.

Carriers are keyed from intent. Each path is a carrier. Each box connector is a carrier keyed by its box and face.

A place is a carrier plus an arc length along it. After any edit that leaves its carrier in place, a place names the same ground position, even if a new connection splits the edge under it.

Resolution keeps the previous networks available until holders have carried their places over, using shared-medium's carry-over operation. The mechanism is designed together with its first holder, believable-guests' hungry-guests.

When a carrier's geometry changes, such as the connector of a box that moved, a place on it moves to the nearest point on the same carrier. It never moves onto another carrier. If its path or connector is deleted, the place resolves to no place. Check two cases by playing: a place on a connector while its box moves, and a place on a path while a new connection splits it.

Ties between equal routes break by carrier key, then by arc length.

The derived networks and field depend only on the current intent. If different edit sequences reach the same intent, they give identical networks and identical state hashes. If tentative intent is resolved in a candidate copy, the result is exactly what committing it gives (decision 0025).

A tooling view in ImGui draws the derived graphs over the park, with their nodes, edges, connectors, and anchors.

## Medium

- Networks: this capability produces the park's guest and backstage graphs in shared-medium's network type, including the nodes anchored to box entities. believable-guests moves guests along the guest graph. plausible-operations finds the anchored nodes of its shops and depots. Every field is sampled at places on these networks.
- Route distance: an entry field this capability produces. It has one entry for each source at each place, and each entry holds the distance and the next edge. Three capabilities sample it:
  - believable-guests, to choose (decision 0019) and to move;
  - plausible-operations, for a shop's supply route length, for whether the shop is supplied, and for the delay it gives supply packets;
  - legible-simulation, for the overlay's discount and for preview context.
- Park intent (decision 0025): this capability reads it from effortless-building, committed or candidate, and derives everything above from it. Connections are derived here and are never saved.

## Principles

- Principle 4 is this capability's reason to exist. Distances are measured only along graph edges. The one straight-line measure is a connection's fixed reach, from a box face to the nearest path point. Check that every other distance follows the network by playing and with the graph tooling view.
- Principle 1 and decision 0007 are at risk if the derived networks depend on edit history. The park world checks guard them, because a saved world must load back and resolve equal, and a loaded world keeps none of its edit history.
- Principle 10 is at risk from tie-breaking and from floating-point lengths and meetings. The tie rules are fixed. Lengths and meetings use basic operations under the simulation's flags. The cross-build script covers both.
- Principle 2 is at risk when a box or path is removed. Unconnected boxes and unreachable places are ordinary states. They have no entries, and nothing crashes.
- Principle 6 is at risk if other capabilities read graph internals instead of sampling. They reach networks only through shared-medium's network type and the route distance field.

## Dependencies

- deterministic-simulation (world-as-value): resolvers, the cycle, and state registration. Met.
- shared-medium (first-field-and-flow): the network type with places and anchors, and entry fields. Met.
- effortless-building (sketch-a-park): path curves, box poses, and the entrance as intent, plus the shared curve evaluation. Met.

## Foundation

The foundation is:

- graphs derived from path intent;
- box connections by face;
- the route distance field with next edges.

All three are rebuilt on each resolution.

It is the smallest version that gives the slice's guests a way to walk and choose, and gives the slice's shops a supply route.

It is useful on its own. Before any guest exists, the tooling view shows exactly how the drawn park connects.

## Milestones

1. `paths-become-routes`: guest and backstage graphs derived from committed or candidate path intent, with junctions, crossings, and edges measured by arc length. It also adds box connections by face within a fixed reach, the route distance field with distances and next edges for every anchored source, and the ImGui graph view. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value, shared-medium's first-field-and-flow, and effortless-building's sketch-a-park.

Later milestones come from the deepening candidates, once the slice shows how networks are used.

## Deepening candidates

- Incremental resolution: rebuild only the parts of the graphs and field that an edit touches, with results equal to a full rebuild. Gated on: resolution time mattering in larger parks.
- Composed networks: a network built from several carrier kinds. The kinds join only at deliberate places, such as staff gates, buildings open on both sides, or ride stations. Each kind has its own cost, and the network has its own sources instead of every anchor. The first uses would be a staff network that spans guest and backstage paths, and guests or supplies that cross kinds at a high cost only when there is no other way, as in Parkitect. Gated on: the first mover that needs one, likely plausible-operations' staff.
- Transport rides as edges, with travel time, wait, capacity, and price (docs/design-notes.md, "Networks").
- One-way paths and queue lines as edge properties.
- Crowding: an edge's cost rises with footfall, so busy routes get slower and guests spread out.
- Bridges and tunnels: crossings at different heights that don't form a junction. Gated on: terrain editing.
- Effort-weighted distance: slopes and steps count for more than flat ground. Gated on: terrain editing.
- Hierarchical routing for large parks, the way flow-field practice pairs flow fields with HPA*. Gated on: profiling.

## Open questions

- Whether one entry for each source at each place stays affordable as sources multiply. Profiling will answer it when a park has many shops. If it isn't affordable, hierarchical routing is the planned answer.

## Research notes

- Brogue's Dijkstra maps and Supreme Commander 2's flow fields support entries of distance plus next edge, shared by all movers.
- Parkitect keeps guest and staff networks apart. It has a fallback for guests that would suit a later deepening.
- Edge lengths come from distances along the ground line, which sketch-a-park settled, instead of quadrature on the curve (paths-become-routes/RESEARCH.md).

The details are in RESEARCH.md.
