# Research: Guest Doors

## Where does a connector's width come from, and what draws it?

routes gives every connector its kind's path width, passed once per network to the code that finds connections. The walkway takes the connector carrier's width, and parkSurface draws a committed walkway at the width it is given. The ghost of an edit draws the candidate's walkways a second way, as strips, and that drawing takes the kind's path width, not the walkway's. A narrower walkway would show at 2 m in the park and at 3 m in the ghost.

A door already knows its face, and the face's width is the box's width, since the front and back corners of a footprint lie a box's width apart. Giving each ParkDoor its face's width lets routes cap each connector by its own door, and the tools and renderer can read it where they read doors today.

Rejected: a per-kind walkway width table in routes — duplicates the box sizes park already publishes. Reading the box size again inside routes when making each connector — a door is already the place routes keeps what it knows about a face. Leaving the ghost strips at the path width — the ghost and the commit would draw different walkways (principle 8).

Sources: src/sim/routes/networks.cpp — findConnections takes one width per network, keyedWalkways copies the carrier's width; src/render/park_mesh.cpp — appendWalkway draws pathWidth(kind); src/sim/park/geometry.cpp — footprint corners.
