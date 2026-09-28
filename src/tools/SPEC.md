# tools

The park's editing tools. The library tpj_tools links tpj_sim alone, so its logic runs and is tested without a window (principle 10). A tool never changes the world: it reads intent through sim/park's public queries and geometry (sim/park/SPEC.md), and gives edits, ParkEdit values, which the app queues for the next cycle.

## Input

A ToolState holds one tool, of a ToolKind: None, PlaceShop, PlaceDepot, MoveBox, or Delete. Its fields are public for reading, but only four calls change them. selectTool sets the kind and drops any hold without committing, keeping the pointer and the place tools' facing. movePointer records where the pointer meets the ground, or none when it meets no ground or is over a panel. pressPointer is the primary button going down, and whether it takes hold is up to the tool; a press while holding changes nothing. releasePointer is the button going up. When the press before it took hold, it gives exactly the edit tentativeEdit gave just before it for the same world, and otherwise none, and then it holds nothing.

tentativeEdit gives the edit the ghost shows, if any, and highlightedEntity the entity the tool marks, if any, which only MoveBox ever gives. Neither changes the state or the world. A tool refuses nothing: its edit is committed whether or not isAccepted is true for it, and the simulation's refusal is the only one (principle 5).

## Picking

boxAt gives the first box, in parkBoxes' key order, whose footprint holds a ground point: with d the point minus the pose's position, and Forward and Right from footprintOf for its kind's boxSize, |d · Forward| is at most half the depth and |d · Right| at most half the width. A box with no footprint holds nothing. pathAt gives the first path, in parkPaths' order, for which some segment between consecutive points of its ground line lies within half its pathWidth of the point, a segment's distance being that of its nearest point. Neither gives the entrance.

## Tools

None gives no edit and no highlight, and never takes hold.

PlaceShop and PlaceDepot place a shop and a depot. The tool keeps a facing, (0, -1) at first. Not holding, its edit is an AddBox of its kind at the pointer's position with that facing, and none when the pointer has no ground position. A press with a ground position takes hold, landing the box at that position with the tool's facing. While it holds, each movePointer to a ground position whose dx * dx + dz * dz from the landing position is at least MIN_FACING_DRAG squared, MIN_FACING_DRAG being 1 m, sets the landing facing to the pointer minus the landing position, as (dx, dz); a nearer position or none leaves it. Holding, its edit is the AddBox of its kind at the landing position with the landing facing. A release while it holds sets the tool's facing to the landing facing, so the next box faces the same way.

MoveBox moves a box. Not holding, it gives no edit and highlights the box boxAt gives under the pointer. A press with a ground position over a box takes hold of it, keeping its key, its pose, and the offset of its position from the pointer, and setting the target to its pose. While it holds, each movePointer to a ground position moves the target's position to the pointer plus the offset, keeping the facing; none leaves it. Holding, its edit is a MoveBox of the held key to the target, or none while the target equals the kept pose, so a click without a drag commits nothing. It highlights nothing while holding.

Delete gives DeleteBox of the box boxAt gives under the pointer, or when there is none DeletePath of the path pathAt gives, or none, whether or not it holds, and highlights nothing: its ghost marks what it deletes. Every press takes hold.
