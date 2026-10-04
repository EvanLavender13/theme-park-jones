# tools

The park's editing tools: what the player's pointer does to the park. A tool never changes the world. It reads intent through sim/park's public queries and geometry (sim/park/SPEC.md). It gives edits, as ParkEdit values, which the app queues for the next cycle. The library tpj_tools links only tpj_sim, so it runs without a window (principle 10).

## Boundary

A ToolState holds one tool, of a ToolKind: None, GuestPath, BackstagePath, PlaceShop, PlaceDepot, MoveBox, or Delete. The app drives it with four calls: selectTool, movePointer, pressPointer, and releasePointer. The app reads tentativeEdit, the edit the ghost shows, and highlightedEntity, the entity the tool marks. boxAt, pathAt, and snapToPath are the picking queries the tools use.

## Input

selectTool changes the tool and drops anything held or drawn without committing it. movePointer records where the pointer meets the ground, or that it meets none, as when it is over a panel. pressPointer and releasePointer are the primary button going down and up.

A release commits exactly the edit tentativeEdit showed just before it, so what the player sees in the ghost is what gets committed. A tool refuses nothing. It commits its edit whether or not the simulation will accept it, and only the simulation refuses edits (principle 5). Neither tentativeEdit nor highlightedEntity changes anything.

## Picking

boxAt gives the box whose footprint holds a ground point. pathAt gives the path whose width covers the point. If several do, it gives the first in key order. Neither gives the entrance. snapToPath moves a point onto the nearest line of a path of the given kind when one lies within SNAP_REACH. Otherwise it leaves the point where it is. A path never snaps onto a line of the other kind.

## Tools

Look (ToolKind None) edits nothing.

Guest path and Backstage path draw a path of their kind. Each click adds a point. The point snaps onto a nearby path of the same kind, so the new path joins it. Clicking the last point again finishes and commits the path. The ghost shows the path through the drawn points and the pointer. A path of fewer than two points commits nothing.

Place shop and Place depot place a box at the pointer. Pressing puts the box down, and dragging before release turns it to face the drag. The next box faces the same way as the last.

Move box marks the box under the pointer, and dragging moves it, keeping its facing. A click without a drag commits nothing.

Delete removes the box under the pointer. If there is no box, it removes the path under the pointer. The ghost shows what it removes.
