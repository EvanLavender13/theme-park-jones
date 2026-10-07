# Research: Drawn Edges

## How should a placed building meet a path that has corners, curves, and changes of width?

Builders that let players draw free-form paths or roads snap buildings to what is drawn, not to an abstract center line. Cities: Skylines II snaps a building to a road when it is held near it, and also snaps to the sides of other buildings. Its known trouble spot is the corner lot: where a road curves away from a lot, the game still treats the lot as a corner, and entrances on the curving side lose road access. The lesson is that the snap and the access test must agree about which edge a building faces. Planet Coaster's paths are free-form too, with square or rounded corners made by grid and rounding tools. Its players complain most about objects that don't line up with path edges at corners.

For this game, the drawn paving is already a value: parkSurface gives its pieces and its visible edge lines. Snapping to the nearest place on those edge lines gives the right answer beside straight sides, tapers, curves, and free ends without any special case. Only two places need a rule of their own:

- A sharp outside corner. The nearest place is the corner itself for a whole wedge of pointer positions. Turning the box to follow the pointer there swings it diagonally around the corner. Lining it up with one of the two sides matches how players line shops along a street.
- An inside corner, a junction pocket, or the inside of a tight curve. The box at the nearest place overlaps the other paving. Moving it the shortest distance that clears all paving, either along the edge or straight out, keeps it as near the pointer as it can be.

For connections, measuring a door's distance to the drawn pieces makes the connection agree with what the player sees. It also agrees with the snap, as the Cities: Skylines II corner-lot trouble shows it must.

Rejected: keep the center line and add a special case for each shape (taper, outer edge radius, junction) — each new paving feature would need its own case, which is how the current failures came about. Swing the box around every corner the way it follows a curve — at a sharp corner it stands diagonally, which players don't expect. Refuse the box at inside corners and leave it to the player — the user found exactly this unacceptable. Measure reach to edge lines instead of pieces — hidden parts of edge lines, inside joined paving, would leave gaps where paving is drawn.

Sources: https://steamcommunity.com/app/949230/discussions/0/3951406499787420891/ — Cities: Skylines II corner lots losing road access where the road curves away; https://prodigygamers.com/2023/10/31/cities-skylines-2-all-road-tools-explained-and-uses/ — Cities: Skylines II snapping to existing geometry and building sides; https://steamcommunity.com/app/493340/discussions/0/152390014782741732/ — Planet Coaster players making square path corners and lining objects up with them.
