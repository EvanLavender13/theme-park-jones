# Feature: Point Nodes

## Summary

routes builds each network's nodes from the path points the player drew, not from where lines happen to meet. Every segment whose ground line ends at a point stops at one node there, shared with every other segment of its kind that ends at that point. Each connector still has a node at its door and one where it meets its segment, and connectors that meet a segment at the same place share that node. A network has no other nodes. Segments of one kind that cross, touch, or overlap without sharing a point stay apart on the network, as their edge lines already show on the paving. Each segment's carrier also takes the segment's own width, so a 10 m street's carrier is 10 m wide. Connectors keep their kind's path width. Route distance is measured as before, along the carriers.

## Acceptance criteria

1. Each segment's carrier has the segment's width, and each connector's carrier has its kind's path width. (check: the scratch program in PLAN.md prints the width at a place on main-street.park's street, on a side path, and on a backstage connector: 10, 3, and 2)
2. In main-street.park, every node of each network is a carrier's end, and carriers whose segments end at the same point share that point's node. (check: the scratch program prints 0 other nodes and 0 unshared points for each kind)
3. A guest path added along the street's center line, sharing no point with the street, leaves the street's carriers with the same stops they had. Before this feature it adds two. (check: the scratch program prints that the stops are unchanged)
4. A capture of main-street.park with --graph shows a node at each path point and at both ends of each connector. It shows none where the backstage path crosses a guest path. (capture)
5. The slice parks and the full park simulate as before. Their segments all have their kind's default width, and every join in them is a shared point. Regenerating fed.park's warm.park and cut.park, and regenerating full.park, writes files identical to the checked-in ones. (check: the commands in PLAN.md print "same" for each)
6. src/sim/routes/SPEC.md, src/sim/park/SPEC.md, and plans/navigable-networks/CAPABILITY.md say that segments join only at shared points and that a segment's carrier has its width. (check: read each beside networks.cpp)

## Tests

None. The feature changes how routes derives networks, which a single module owns, and the park world checks already cover the networks it derives from every park in tests/parks (docs/testing.md). The slice adds the integration tests that rest on it.

## Medium

The feature adds no fields or flows. It changes the networks routes produces in shared-medium's network type.

- Networks: nodes now come from the point keys parkSegments publishes with each segment, plus the connectors' ends. Each segment's carrier has the segment's width. Consumers read them as before through the network type's queries. believable-guests' spread-guests and legible-simulation's wide-overlay will read the widths.
- Park intent (decision 0025): routes reads each segment's From and To keys and its width through parkSegments. It no longer compares segments' ground lines.

## Spec changes

src/sim/routes/SPEC.md, Networks: the second paragraph says each segment's carrier has the segment's width and each connector's has its kind's path width. The paragraph about junctions and its bullet list are replaced by a paragraph saying segments join only where they share a point:

    Carriers join only at path points and at connectors' ends. Every carrier whose segment ends at a point stops there, and all of them share one node. Segments of one kind that cross, touch, or overlap without sharing a point don't join. At a rounded corner, the node lies at the middle of the arc, where the two segments' ground lines meet.

The sentence "If nodes are closer together along a carrier than JUNCTION_TOLERANCE, they are one node." stays, and now covers connectors that meet a carrier near its end or near each other. In Connectors, "joins the path's network there like any other meeting" becomes "stops that path's carrier there at a node".

src/sim/park/SPEC.md, Intent: "Routes also joins segments of one kind wherever they cross, whether or not they share a point there (sim/routes/SPEC.md)." becomes "Routes joins segments only where they share a point (sim/routes/SPEC.md)." Physical validity: "Segments never conflict with each other. Where they cross, routes derives a junction." becomes "Segments never conflict with each other, even where they cross or overlap."

Plan changes: plans/navigable-networks/CAPABILITY.md, Foundation criteria. The list of where nodes sit becomes one paragraph saying nodes sit at the path points segments end at, shared by every segment of the kind that ends there, and at both ends of each connector, and that paths of one kind that cross or overlap without sharing a point stay apart.

## Files affected

- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: src/sim/park/SPEC.md
- Modify: src/sim/park/intent.h
- Modify: plans/navigable-networks/CAPABILITY.md

## Dependencies

- effortless-building/path-network: parkSegments publishes each segment's point keys and width, and every edit that joins segments shares a point. Merged.
- shared-medium/carrier-widths: carriers hold a width. Merged.

## Out of scope

- Doors reaching from a segment's edge. edge-reach does it.
- Guests passing through shaping points without choosing. believable-guests' spread-guests does it.
- Drawing carriers' widths in the graph view. Nothing in the slice needs it.

## Open questions

None.
