# Milestone: Wide Routes

Slice: good-paths

Status: complete

## Summary

wide-routes builds the guest and backstage networks from the points and segments the player drew. A network's nodes come only from points and from the two ends of each connector, so paths join on the network exactly where they join on the paving. Segments of one kind that cross or overlap without sharing a point stay apart, as their edge lines show. Each segment's carrier takes the segment's own width. A door connects when it lies within reach of a segment's edge, not its center line, so the shops along main-street.park's 10 m street finally connect to guests. It is the slice's third member. path-network has published segments with their points and widths, and carrier-widths has given every carrier a width. wide-overlay and spread-guests read the widths and nodes this milestone derives.

## Acceptance criteria

1. Each segment with a ground line is a carrier of its kind's network, keyed by the segment and following its ground line, with the segment's width. Each connector is a carrier with its kind's walkway width, today's fixed path width. A network has a node for each point that one of its carriers ends at, shared by every carrier that ends there, and a node at each end of each connector. Where a connector meets a segment within JUNCTION_TOLERANCE of the segment's end, or of where another connector meets it, they share one node. A network has no other nodes: segments of one kind that cross, touch, or overlap without sharing a point don't join. (capture: main-street.park with --graph; play: hold the suppress key and draw a guest path along another, and the graph view shows no node where they overlap)
2. A door connects to the segment of the kind it serves whose paving, as drawn, lies nearest, when that paving lies within CONNECTION_REACH, 3 m. A segment's paving is its own pieces of the paved surface and the pieces of the points it ends at. Ties go to the segment whose center line is nearer, then to the lower key. The connector runs straight out of the door's face to the paving, then to the nearest point of that segment's center line, or straight to that point when it lies straight ahead, and its length counts in routes (principle 4). (capture: main-street.park with --graph shows a connector from every shop's front door to the street, every shop's back door and the depot's door to the backstage path, and the entrance to its path)
3. A shop or depot placed by the frontage snap beside paving of any shape, whether a straight side, a free end, a taper, a junction, or the inside or outside of a corner, sharp or rounded, is accepted wherever it has room and gets a front-door connector. (check: drawn-edges' probe; play; the slice's integration test 2 checks the widest width)
4. A door's walkway is drawn as part of the paving, joined to the segment it reaches like a path drawn onto it: no edge line crosses it there. (capture, and play by the user)
5. The park world checks pass over every park, so the networks are derived from intent alone and loads and candidates derive them equal. food_loop_test passes. (ctest)
6. src/sim/routes/SPEC.md describes the networks and connectors as built. src/sim/park/SPEC.md no longer says routes joins segments wherever they cross. This capability's brief describes nodes from points and reach to a segment's drawn paving. The slice's Door reach question is closed with the value in Tuning. (check: read each beside the code)
7. In the windows-release build, the user opens main-street.park with the graph view on, then draws, joins, widens, splits, and merges segments. The nodes stay at the points. The street's shops stay connected as the street widens, and a shop disconnects only when narrowing puts its door more than 3 m from the paving. (play, by the user)

Simulation output changes on purpose. A door that was out of reach can now connect, and a junction that only geometry made is gone.

## Medium

The milestone adds no fields or flows. It changes what the networks hold.

- Networks: point-nodes takes nodes from points and widths from segments, and edge-reach changes which doors connect. routes produces them in shared-medium's network type. Consumers: believable-guests' spread-guests walks guests through nodes and draws them across each carrier's width, and legible-simulation's wide-overlay sizes the overlay by width. plausible-operations and every field read the networks unchanged.
- Park intent (decision 0025): routes reads segments with their point keys and widths through parkSegments, doors through parkEntrances, parkBoxes, and footprintOf, and the drawn paving that reach is measured to through parkSurface and pieceDistance. It no longer reads where lines meet.
- Route distance: unchanged in kind. It is measured along segment center lines, the arcs of their rounded corners, and connectors.

## Dependencies

- shared-medium/carrier-widths: carrier widths and ground carry-over. Complete.
- effortless-building/path-network: points and segments, published with their point keys and widths, and the frontage snap. Complete.

## Core feature

`point-nodes` is the core. It makes the networks follow the paths the player joined, and gives every carrier its segment's width, which both later slice members read. The graph view shows the result on main-street.park at once.

## Features

1. `point-nodes`: carriers take their segments' widths, and nodes come only from points and connector ends. The search for where lines meet is removed. The feature rewrites the routes spec's Networks section, the park spec's sentences about routes joining crossings, and this capability's foundation criteria. Depends on: none.
2. `edge-reach`: doors connect within CONNECTION_REACH of the nearest segment edge of their kind, with the reach set to 3 m. The feature updates the routes spec's Connectors section and this capability's brief, and closes the slice's Door reach question. Depends on: none.
3. `joined-walkways`: each door's walkway is drawn as part of the paved surface, joined to the segment it reaches, so no edge line crosses it there. Depends on: feature 2.
4. `drawn-edges`: the frontage snap fronts the paving as drawn, at every shape it takes, and doors connect by their distance from the drawn paving. Depends on: features 2 and 3.

Only drawn-edges adds a test, for the bug it fixes (docs/testing.md). The slice adds the integration tests for crossing splits, frontage on the widest segment, and undoing a deletion.

## Tuning

- Connection reach: 3 m from a segment's drawn paving. It keeps every connection a door beside a default-width path has today, and it covers the 1 m frontage setback at every width.
- Junction tolerance: 1 mm, used where a connector meets a segment near its end or near another connector, as where shops face each other across a street, and to tell when segments' paving lies equally near a door.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Connectors that join a wide segment at its edge, so a route doesn't count the walk from the edge to the center line. Gated on: spread-guests showing that the extra length matters.

## Open questions

None.

## Research notes

- Nodes come from shared points, as in road games built on nodes and segments. A geometric meeting search was rejected because it joins paths the player kept apart.
- Reach is 3 m from the edge, measured as frontage measures it. Requiring a door to touch the paving, and keeping 4 m, were both rejected.

Depth is in RESEARCH.md.
