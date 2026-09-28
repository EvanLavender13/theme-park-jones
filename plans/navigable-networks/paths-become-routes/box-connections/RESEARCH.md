# Research: box-connections

## Where does a connector meet the network?

The milestone settled the shape: a straight carrier from a face's midpoint, the door, to the nearest point of a same-kind line within CONNECTION_REACH, found with the medium's nearestPlace (../RESEARCH.md). Two details follow from how path-networks builds nodes. First, the connector's far end is best treated as one more meeting between two carriers, the connector at its length and the path at the connection place's distance. The path's stop grouping then absorbs it like any other: a connection within a millimeter of a junction or a path end lands on that node rather than beside it, and two connectors reaching one point share a node. Second, nearestPlace must search a network of the path carriers alone, before any connector exists, so one door's connector never snaps onto another's, and the result depends only on the paths and the door, never on the order doors are handled in.

A connector's key has the derived-key bit set, as every deriveKey result does, and path keys usually come from the world's counter below it, so connectors usually sort after every path. A hand-written save can still give a path a derived key, since the loader rejects derived keys only in [entities], so the node numbering walks paths and connectors together in key order rather than relying on that, and a door whose connector key a path already holds gets no connector, so carrier keys stay unique.

## Should a connector meet the lines it crosses?

A connector is at most 4 m long and leaves a box face, which a line of the kind cannot come within its half width of, so in a physically valid park it can cross another line only where two lines run close beside a box. Joining it there would give guests a junction partway along a connector, at a point the player never drew. Treating the connector as meeting nothing but its own path keeps it a short tube with two ends, the door and the path, which is what the graph view and route distance expect.

Rejected: running connectors through the meeting search with the paths. It adds junctions on connectors that no drawn line makes, and it makes a connector's stops depend on unrelated paths.

## What about a door that lies on a line?

A connector needs two points whose distances strictly increase, so a door within JUNCTION_TOLERANCE of its nearest point cannot have one. Physical validity keeps every line at least its half width, less a millimeter, from every box, so this happens only in worlds a hand-written save describes. Such a door gets no connector, which leaves the world legitimate (principle 2) and the network well formed.

Rejected: anchoring the path's own node at that point. A node may be anchored to one entity only, so two doors at one point would make the network malformed.
