# Research: Seat Rests

## How a guest samples a best seats field without allocating

sampleField returns a new list, and builds the slots and working lists it needs on each call (src/sim/medium/field.h, sampleField, sampleSlots, sampleSlotsAt). A guest heading to a seat needs the piece's entry where it stands every tick, and at each node of its route ahead. Calling sampleField there would allocate in the per-tick loop, which CLAUDE.md forbids.

A best seats field has one source, so its slot is found with sourceSlot, which allocates nothing. At a node, the entries sampleField gives are the slot's entries at the node's stop places, which positionsAtAny finds in a reused list. Inside an edge, gatherEdgeSample fills a reused EdgeSample from the slot, and sampleBestPlaceEdge applies the field's own rule into a reused list. Both helpers are in field.h, and the medium uses them for sampleField itself. A function in sim/wayfinding can run these steps with the caller's lists, and give exactly the values sampleField gives. A scratch check compares the two at every node and at a place inside every edge of full.park.

Rejected: sampleField per tick. It allocates in the loop. A lookup of the target piece alone, ignoring the field's rule. Inside an edge, it could give a piece the field doesn't give there, so the guest would head to a piece its own choice could not have offered.

## How a guest knows it is seated

The tick cycle runs the systems, advances the tick, and then swaps the ledgers (src/sim/SPEC.md, The tick cycle). A guest sends its seat visit at tick T with VISIT_DELAY 1, and it reaches the piece in the swap that ends T. The piece steps at T + 1 and, if it is full, sends the visit back with RETURN_DELAY 1. The visit then reaches the guest in the swap that ends T + 1, so the guest holds it when it steps at T + 2 (src/sim/operations/seats.cpp, servePiece). If the guest holds nothing at T + VISIT_DELAY + RETURN_DELAY, the piece seated it.

## Walking to a seat place inside an edge

traceRoute adds each step's points up to its To, and marks the last as a node only if the place resolves to one (src/sim/guests/route_ahead.cpp, walkStep and markNode). A step that ends at a seat place inside an edge therefore adds a point that isn't a node, and the route goes on from it by the field's entry there. At the seat place itself, that entry is the arrival entry, with no next step, so the route ends there. The aim, the nearest spot, and reaching the end then work as they do for a door.

moveToSide shifts each stretch toward the guest's own side. At the end of a route to a seat, the last stretch is shifted toward the piece instead, by half the carrier's width less GUEST_CLEARANCE. The piece stands on the verge square to the segment's line, and its seat place is the line's nearest place, so the direction from the seat place to the piece is across the line. pointAt keeps a shifted point only where the guest's box fits, so at a bend the guest stops at the middle.

## Where a seated guest is drawn

The scene rebuilds the guest mesh whenever the world ticks or its intent changes (src/app/scene/scene_sync.cpp, syncWorld). SceneSync is where the app keeps what the scene was last built from, so it can also keep each seated guest's seat between rebuilds. The renderer's guest mesh, the highlight of the inspected guest, and picking all read the same seats. A park opened afresh gives seats in key order.
