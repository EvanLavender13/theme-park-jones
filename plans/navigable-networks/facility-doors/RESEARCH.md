# Research: Facility Doors

## How wide should a small facility's walkway be?

A connector's walkway takes its kind's path width today: 3 m for guests and 2 m for backstage. Every door so far sits on a face wider than that. Shops and toilet blocks are 8 m wide, a depot is 12 m, and the entrance is 10 m. A bench is 2 m wide and 1 m deep, so a 3 m walkway would stick out half a meter past each end of the bench. Across the 1 m frontage setback, that reads as a paved pad wider than the thing it serves.

Capping the walkway at the width of the door's face keeps every existing walkway as it is and gives a bench a 2 m walkway, flush with its ends when it runs straight out of the bench's front. If the walkway leaves the door at an angle, its door end is wider than its width along the face, as every walkway's is, and can overhang the bench. Only freehand placement beside a path's end makes that happen. The connector's carrier takes the same width, so guests stepping out to a bench spread across the walkway they see. The width is the box's own size, not a tuning value, so the player still controls the shape by where they place the bench.

Rejected: keeping the path width for every walkway — a bench's pad overhangs it on both sides. A separate walkway width for each box kind — a table of fixed values where the face width already gives the answer. Placing benches on the paving as path furniture, as RollerCoaster Tycoon does — the slice settled benches as boxes that front the path and publish offers, so guests choose them from a distance.

Sources: src/sim/park/intent.h — box sizes and path widths; src/sim/routes/networks.cpp — connectors take pathWidth(kind); plans/slices/day-at-the-park/RESEARCH.md — why benches publish offers rather than being stepped on.

## Does adding doors change what guests or shops do?

Guests take only sources that publish a food offer, and operations finds shops and depots by box kind. A bench or toilet block publishes no offer until served-needs, so its route distance entries are read by nobody. Its connector does split the guest segment it meets with a new node. Guests choose a little before each node, so in a park with guests a new connector adds a choice point, and guests may then walk differently. tests/parks/facilities.park is the only park holding benches or toilet blocks, and it has no guests, so every other park simulates exactly as before.

Sources: src/sim/guests/guests.cpp — offers filter choices; src/sim/operations/operations.cpp — shops and depots by kind; tests/parks — the parks that hold facilities.
