# Research: Seat Service

## How does a resolver read every piece's seat place?

Seat offers need one entry for each piece, at the place where the piece's seat place lies. The seat places field holds them, one slot per furnished segment (src/sim/furniture/furniture.cpp, publishSeatPlaces). sim/medium offers no public read of a whole field. sampleField and sampleResolvedField read a place, and sourceSlot reads one source. The resolved entries sit in the ResolvedEntries component on the field's holder, which only medium code reads today.

A small medium function that gives a field's resolved slots, sources ascending, fills the gap. A resolver can then walk every seat place in one pass. best-seats will need the same read, because it seeds its search from every seat offer at once. Only resolvers may call it, as with sampleResolvedField, since stepped entries are state.

publishResolved inserts each source's slot in key order. Seat places come grouped by segment, so their pieces aren't in key order. Sorting the pieces by key before publishing lets each insert go at the end.

Rejected: reading pieces through parkFurniture and then each segment's slot through sourceSlot. It works with today's API, but sourceSlot reads the stepped layer first, which a resolver must not depend on. It also reads furniture through two routes where one field is the medium. Putting the seats into furniture's derived component and reading it from operations. That reads another module's internals (principle 6).

## Which pieces does a tick step?

A tick must not visit every piece (decision 0031). A piece acts only on visits it holds, and the seat-visits ledger lists its stocks by endpoint. A piece holds a visit under the visiting guest's key. A guest holds its own visit under its own key, before it sends the visit and after the visit comes back. So the endpoints that hold seat visits under another key are the pieces to step, in ascending key order. Every visit a piece holds in a step is either seated or sent back in that step, so a piece never carries a visit into the next tick, and it keeps no queue.

A piece reads its seats from its own seat offer through sourceSlot, which allocates nothing. If a piece has no seat place, it has no offer and so no seats, and it sends back every visit. That keeps a visit from waiting forever at a piece that guests can't reach (principle 2).

When a seated guest leaves, its visit and rest come back to the piece together, because the ledger returns both packets after the same delay. The piece is stepped for the returned visit, and it consumes both as abandoned, as a bench box does in facilities.cpp's abandonGone.

## Where does the seat state live?

The tick each taken seat frees is state, kept on the piece's entity in a seat-service component, as FacilityService is kept on a bench box's entity. A piece's key is derived. When a save loads, the entity holding the state is created without an origin, and the resolution that lays the piece claims it (World::createDerivedEntity). When a resolution no longer lays a piece, destroying the entity drops its state too. The ledger then sends whatever the piece held to each guest. Packets on their way to the piece come back to their senders.
