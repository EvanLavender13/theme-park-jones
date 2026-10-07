# Feature: Edge Reach

## Summary

A door now connects when it lies near a segment's edge, not its center line. A door's distance from a segment is its distance from the segment's ground line, less half the segment's width. This is how far the door stands from the paving, and it is what the frontage snap measures. The door connects to the segment of its kind whose edge is nearest, when that edge lies within CONNECTION_REACH, now 3 m. The lower segment key wins a tie. The connector still runs from the door to the nearest point of that segment's ground line, and its length counts in routes. The shops along main-street.park's 10 m street now connect to guests. A shop placed by the frontage snap connects beside a segment of any width the tools allow. Every door that connects today beside a default-width path still connects. A guest door now reaches 0.5 m farther from a default path's center line, and a backstage door reaches exactly as far as before.

## Acceptance criteria

1. In main-street.park, every shop's front door connects to the street. Every shop's back door and the depot's door connect to the backstage path, and the entrance connects to its path. (check: the scratch program in PLAN.md prints 12 guest connectors and 12 backstage connectors; capture: main-street.park with --graph shows them)
2. A shop placed by the frontage snap beside a guest segment 16 m wide, and beside one 1.5 m wide, gets a front-door connector. Before this feature, only the 1.5 m one does. (check: the scratch program prints `frontage 16 connected` and `frontage 1.5 connected`)
3. In every other park in tests/parks and tests/parks/stress, every door that connected before connects to the same segment at the same place. The only new connector is shop 17's front door in degenerate-intent.park, which lies 4.49 m from a 3 m guest segment's line, inside the guest reach's extra 0.5 m. (check: the scratch program's connector listing before and after differs only in those lines and main-street.park's)
4. The slice parks and the full park simulate as before. Regenerating fed.park's warm.park and cut.park, and regenerating full.park, writes files identical to the checked-in ones. (check: the commands in PLAN.md print "same" for each)
5. src/sim/routes/SPEC.md, plans/navigable-networks/CAPABILITY.md, and the slice say a door reaches from a segment's edge, and the slice's Door reach question is closed. (check: read each beside networks.cpp)

## Tests

None. The slice's integration test 2 checks that a shop posed by frontage beside the widest segment connects (plans/slices/good-paths/SLICE.md). It is written when the slice closes. The park world checks cover the networks derived from every park.

## Medium

The feature adds no fields or flows. It changes which doors routes connects, and so which entities are sources on each network.

- Networks: more doors can connect, each to the segment whose edge is nearest. plausible-operations, believable-guests, and legible-simulation read connections only through anchors and route distance, so none of them changes.
- Park intent (decision 0025): routes reads each segment's width through parkSegments, as point-nodes already does, and doors through parkEntrances, parkBoxes, and footprintOf.
- effortless-building's frontage snap sets a front face FRONTAGE_SETBACK, 1 m, from the paving's edge, measured the same way. The reach of 3 m keeps every frontage placement connected.

## Spec changes

src/sim/routes/SPEC.md, Connectors. The paragraph starting "If a door is within CONNECTION_REACH" becomes:

    A door's distance from a segment is its distance from the segment's ground line, less half the segment's width: how far it stands from the segment's edge. If a segment of the kind a door serves lies within CONNECTION_REACH of it, the door gets a connector to the nearest such segment, the lower key winning a tie. A connector is a short straight carrier keyed connectorKey(entity, face). It runs from the door to the nearest point on that segment's ground line, and stops that segment's carrier there at a node. If no segment of its kind is within reach, a door has no connector, and nothing else about its entity changes (principle 2). If a door is flush against a segment's edge, it still gets a connector. If it lies on the nearest segment's ground line, within JUNCTION_TOLERANCE, which only a hand-written save allows, it has no connector.

Plan changes:

- plans/navigable-networks/CAPABILITY.md, Foundation criteria: "It connects to the nearest point of a path of that kind, within a short fixed reach:" becomes "It connects to the path of that kind whose edge is nearest, within a short fixed reach of that edge, at the nearest point of the path's center line:". Principles: "The one straight-line measure is a connection's fixed reach, from a box face to the nearest path point." becomes "The one straight-line measure is a connection's fixed reach, from a door to the nearest path edge."
- plans/slices/good-paths/SLICE.md, Open questions: the Door reach item is removed, leaving "None."

## Files affected

- Modify: src/sim/routes/SPEC.md
- Modify: src/sim/routes/networks.h
- Modify: src/sim/routes/networks.cpp
- Modify: plans/navigable-networks/CAPABILITY.md
- Modify: plans/slices/good-paths/SLICE.md

## Dependencies

- point-nodes: each segment's carrier has its width. Merged.
- effortless-building/path-network: the frontage snap and FRONTAGE_SETBACK. Merged.

## Out of scope

- Connectors that end at a wide segment's edge instead of its center line. It is a deepening candidate in MILESTONE.md.
- Reach measured to the drawn surface, with its square ends and junction paving. It is a deepening candidate in MILESTONE.md.
- Drawing a connector's walkway only from the door to the paving's edge. The walkway is drawn as before, from the door to the center line, over paving of the same color.

## Open questions

None.
