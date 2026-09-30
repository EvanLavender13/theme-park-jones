# Capability: Navigable Networks

## Summary

navigable-networks deepens how the park can be travelled: which places connect, how far apart they are along the routes things actually take (principle 4), and which way to go. It turns path intent into the park's networks. It connects boxes to them, derives the route distance field that measures every place against every source, and gives movers a direction to follow.

Its value is that distance means what a guest's feet or a supply cart would experience. A shop across a fence is far away however close it looks. Every choice, supply route, and overlay in the game rests on that measure. It deepens with transport rides as edges, one-way and queue paths, crowding, bridges and tunnels, and scale, and never finishes.

## Foundation criteria

- Resolving path intent derives one graph per network kind, guest and backstage. Nodes sit at path endpoints, at snapped junctions, where same-kind paths cross, and where connectors meet a path. Each edge's length is its length along the path's ground line, the line that is drawn and refused. Paths of different kinds never connect, even where they cross.
- Each kind of box connects from the face that serves each network kind to the nearest point of a path of that kind within a short fixed reach:
  - a shop's front connects to a guest path and its back to a backstage path;
  - a depot's front connects to a backstage path;
  - the entrance connects to a guest path.
  The connector is a derived edge whose length counts in routes, and its end node is anchored to the box's entity. Beyond reach there is no connection, which is a legitimate state (principle 2).
- Every anchored entity is a route distance source on each network it connects to. Sampling the field at any place gives, for each source reachable from it, the distance along the network and the next edge toward the source. A place with no route to a source has no entry for it. On random synthetic graphs, the distances equal a reference shortest-path computation, and ties between equal routes break by a fixed rule.
- From any place with an entry, repeatedly following the next edges reaches the source, and the length walked equals the sampled distance.
- Carriers are keyed from intent: each path is a carrier, and each box connector is a carrier keyed by its box and face. A place is a carrier and an arc length along it, so it names the same ground position after any edit that leaves its carrier in place, even when a new connection splits the edge under it. Resolution keeps the previous networks available until holders have carried their places over (shared-medium's carry-over operation), a mechanism designed with the first holder, believable-guests' hungry-guests. A place on a carrier whose geometry changed, such as the connector of a moved box, moves to the nearest point on the same carrier, never onto another carrier. A place on a deleted path or connector resolves to no place. Tests hold a place on a connector while its box moves, and on a path while a new connection splits it. Ties between equal routes break by carrier key, then by arc length.
- The derived networks and field depend only on the current intent. Different edit sequences that reach the same intent give identical networks and identical state hashes. Resolving tentative intent in a candidate copy gives exactly what committing it gives (decision 0025).
- A tooling view in ImGui shows the derived graphs, with nodes, edges, connectors, and anchors, over the park.

## Medium

- Networks: this capability produces the park's guest and backstage graphs in shared-medium's network type, including the nodes anchored to box entities. believable-guests moves guests along the guest graph. plausible-operations finds its shop's and depot's anchored nodes. Every field is sampled at their places.
- Route distance: an entry field this capability produces, with one entry per source per place, holding the distance and the next edge. believable-guests samples it to choose (decision 0019) and to move. plausible-operations samples it for a shop's supply route length, whether the shop is supplied, and the delay it gives supply packets. legible-simulation samples it for the overlay's discount and for preview context.
- Park intent (decision 0025): read from effortless-building to derive everything above, committed or candidate. Connections are derived here and never saved.

## Principles

- Principle 4 is this capability's reason to exist. Distances are only ever measured along graph edges. The one straight-line measure is the fixed reach of a connection, from a box face to the nearest path point. The reference shortest-path tests hold the rest.
- Principle 1 and decision 0007 are at risk if derived networks depend on edit history. They are checked by the test that different edit sequences reaching the same intent hash the same.
- Principle 10 is at risk from tie-breaking and from floating-point lengths and meetings. Tie rules are fixed, lengths and meetings use basic operations under the simulation's flags, and the cross-build script covers both.
- Principle 2 is at risk when a box or path is removed. Unconnected boxes and unreachable places are ordinary states: they have no entries, and nothing crashes.
- Principle 6 is at risk if other capabilities read graph internals rather than sampling. They reach networks only through shared-medium's network type and the route distance field.

## Dependencies

- deterministic-simulation (world-as-value): resolvers, the cycle, and state registration. Unmet; planned.
- shared-medium (first-field-and-flow): the network type with places and anchors, and entry fields. Unmet; planned.
- effortless-building (sketch-a-park): path curves, box poses, and the entrance as intent, plus the shared curve evaluation. Unmet; planned.

## Foundation

The foundation is graphs derived from path intent, box connections by face, and the route distance field with next edges, all rebuilt on each resolution. That is the smallest version that gives the slice's guests a way to walk and choose, and its shops a supply route. It produces value on its own: the tooling view shows exactly how the drawn park connects, before any guest exists.

## Milestones

1. `paths-become-routes`: guest and backstage graphs derived from committed or candidate path intent, with junctions, crossings, and arc-length edges, box connections by face within a fixed reach, the route distance field with distances and next edges for every anchored source, and the ImGui graph view. Member of the boxes-and-tubes slice. Depends on: deterministic-simulation's world-as-value, shared-medium's first-field-and-flow, and effortless-building's sketch-a-park.

Later milestones are drawn from the deepening candidates once the slice shows how networks are used.

## Deepening candidates

- Incremental resolution: rebuild only the parts of the graphs and field an edit touches, proven equal to a full rebuild. Gated on: resolution time mattering at larger parks.
- Composed networks: a network built from several carrier kinds, joined only at deliberate places such as staff gates, buildings open on both sides, or ride stations, with a cost per kind and its own sources rather than every anchor. First uses are a staff network spanning guest and backstage paths, and guests or supplies crossing kinds at a high cost only when there is no alternative, as in Parkitect. Gated on: the first mover that needs one, likely plausible-operations' staff.
- Transport rides as edges with travel time, wait, capacity, and price (docs/design-notes.md, "Networks").
- One-way paths and queue lines as edge properties.
- Crowding: edge cost rising with footfall, so busy routes become slower and guests spread out.
- Bridges and tunnels: crossings at different heights that form no junction. Gated on: terrain editing.
- Effort-weighted distance: slopes and steps count for more than flat ground. Gated on: terrain editing.
- Hierarchical routing for large parks, as flow-field practice pairs with HPA*. Gated on: profiling.

## Open questions

- Whether one entry per source per place stays affordable as sources multiply. Resolved by profiling when a park has many shops. Hierarchical routing is the answer in waiting.

## Research notes

- Brogue's Dijkstra maps and Supreme Commander 2's flow fields justify distance-plus-next-edge entries shared by all movers.
- Parkitect keeps guest and staff networks apart, with a fallback for guests that suits a later deepening.
- Edge lengths come from the ground line's distances, which sketch-a-park settled, rather than quadrature on the curve (paths-become-routes/RESEARCH.md).

Depth is in RESEARCH.md.
