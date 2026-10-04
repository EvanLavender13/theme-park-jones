# park

Park intent: what the player authored, from which the rest of the park is derived (principle 1, decision 0025), and the one refusal the game makes, physical validity (principle 5). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

addParkIntent registers the intent types, entrance, path, and box, whose components are private to this module (decision 0016). addParkEdits registers the five commands. makeParkSchema calls both after the medium's types.

Other modules read intent only through parkEntrances, parkPaths, and parkBoxes, which give every entity holding each kind of intent, in key order, as the public values ParkEntrance, ParkPath, and ParkBox. They change it only through commands. groundLine, footprintOf, and the sizes pathWidth, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public, so the renderer, the tools, and routes measure the same shapes this module validates.

## Intent

The park is a square PARK_SIZE on a side, centered on the origin of the ground. A path has a kind, guest or backstage, and the points the player clicked, in order. A box has a kind, shop or depot, and a pose. An entrance has a pose. A Pose is a position and a facing on the ground, held as given; a default Pose is at the origin, facing -z.

Their save lines hold an entrance's x, z, facing-x, and facing-z; a path's kind and points, each as {x=... z=...}; and a box's kind, x, z, facing-x, and facing-z. Kinds are saved by name: guest, backstage, shop, and depot.

## Ground lines

A path's ground line is its curve as a line of CarrierPoints (sim/medium/SPEC.md), each a ground position with its distance along the line. It is a pure function of the path's points, and the renderer, the validity check, and routes all use it, so what is drawn, refused, and walked is the same line.

The line is a smooth curve that passes exactly through the points the player placed, after points closer than MIN_POINT_SPACING to the last kept one are dropped; keptPoints gives the points kept. A path of two kept points is straight. Distances start at 0 and strictly increase. The line is empty when a point is not finite or lies outside the park, or fewer than two points are kept.

## Footprints

footprintOf gives the rectangle a pose covers for a size, its width across the facing and its depth along it, or none when the pose is not finite or its facing has zero length. Its corners run front left, front right, back right, back left, so the front face runs from the first corner to the second and the back face from the third to the fourth.

## Physical validity

A world is physically valid when no object leaves the park, no two solids overlap, and no path meets a solid. Solids are the footprints of the entrance and the boxes, and a path occupies its ground line widened by half its pathWidth. Objects that only touch never conflict, within CONTACT_TOLERANCE, whatever rounding their facings bring. Paths never conflict with each other: where they cross, routes derives a junction. An object with no footprint or an empty ground line, which only a hand-written save can hold, makes the world invalid. isPhysicallyValid gives whether a world is valid.

## Commands

Five commands change intent: AddPath, AddBox, MoveBox, DeletePath, and DeleteBox. No command moves or deletes the entrance. Tools submit them to the command queue, so they apply between ticks.

On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. isAccepted gives whether it would be, and applyCommand changes nothing when it is not, so what a ghost shows as valid is what its commit does. An accepted command leaves exactly the intent it describes and changes nothing else: AddPath and AddBox each add one entity from the key counter, MoveBox replaces a box's pose keeping its key and kind, and the deletes destroy the entity they name. A pose or path is kept exactly as given.

A ParkEdit holds any one of the five commands, the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on its command, and queueEdit pushes the command so a cycle applies it as if pushed directly.

## The new park

makeNewPark(schema, seed) gives a physically valid world at tick 0, pending resolution, holding an entrance with its back on the park's edge, facing in, and a short guest path in front of it. tests/parks/new.park is the save of makeNewPark(seed) with seed 1.
