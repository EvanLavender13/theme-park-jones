# Research: Seat Places

## How a piece of furniture should appear in the medium

The slice's RESEARCH.md settles why seats reach guests through fields: one field per need, seeded from every seat at once, costs work that grows with the network and the seats, never the two multiplied. This feature only publishes where the seats are. The medium already has what it needs, so no outside search was made.

An entry field holds entries at places on a network, each attributed to a source, and its resolved layer is derived data that every resolution replaces (src/sim/medium/SPEC.md, Fields). The offers in sim/operations are the pattern to follow: an entry type with its visitFields, a field type naming it, addField before the resolvers that publish, and one publishResolved per source in each resolution.

A piece stands on the verge, off the network. A guest reaches it from the guest paving, so its entry belongs at the place on its own segment's carrier nearest the piece. Network::nearestPlaceOn gives that place, and the medium allows it because it snaps a ground position onto a network and measures no travel (principle 4). The guest network's carriers for drawn paths are keyed by their segment, as legible's preview already relies on.

Furniture resolves after path-networks, through its dependency on walked-surface, so the guest network is current when the pieces are laid. Furniture keeps unchanged chains from the last resolution, but the field's resolved layer is replaced in full each resolution. Each furnished segment therefore publishes all its pieces' entries every time, read from the pieces as laid. That costs work in proportion to the pieces, with no route search.

Rejected: each piece as its own source. The slice's medium map makes the segment the source, so a segment's entries come and go together with its furnishing. Rejected: entries at the piece's own position on a carrier of its own. The furniture would then make network carriers, which belongs to routes, and a guest would still have to reach the piece from the paving.

Sources: src/sim/medium/SPEC.md — fields, places, and nearestPlaceOn; src/sim/operations/operations.h — the offer fields as the pattern; plans/slices/somewhere-to-sit/SLICE.md — the seat places entry and its source.
