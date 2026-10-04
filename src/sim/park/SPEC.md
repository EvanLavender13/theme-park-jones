# park

This module holds park intent: what the player authored, from which the rest of the park is derived (principle 1, decision 0025). It also makes the game's one refusal, physical validity (principle 5). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

addParkIntent registers the intent types entrance, path, and box. Their components are private to this module (decision 0016). addParkEdits registers the five commands. makeParkSchema calls both after the medium's types.

Other modules read intent only through parkEntrances, parkPaths, and parkBoxes. Each returns every entity holding that kind of intent, in key order, as the public values ParkEntrance, ParkPath, and ParkBox. Other modules change intent only through commands.

groundLine, footprintOf, and the sizes pathWidth, boxSize, ENTRANCE_SIZE, and PARK_SIZE are public. So the renderer, the tools, and routes measure the same shapes this module validates.

## Intent

The park is a square PARK_SIZE on a side, centered on the origin of the ground.

- A path has a kind, guest or backstage, and the points the player clicked, in order.
- A box has a kind, shop or depot, and a pose.
- An entrance has a pose.

A Pose is a position and a facing on the ground, held as given. A default Pose is at the origin, facing -z.

The save lines hold:

- an entrance's x, z, facing-x, and facing-z;
- a path's kind and points, each point as {x=... z=...};
- a box's kind, x, z, facing-x, and facing-z.

Kinds are saved by name: guest, backstage, shop, and depot.

## Ground lines

A path's ground line is its curve as a line of CarrierPoints (sim/medium/SPEC.md). Each CarrierPoint is a ground position with its distance along the line. The ground line is a pure function of the path's points. The renderer, the validity check, and routes all use it, so what is drawn, what is refused, and what is walked are the same line.

First, any point closer than MIN_POINT_SPACING to the last kept point is dropped. keptPoints returns the points kept. The line is a smooth curve that passes exactly through the kept points. A path of two kept points is straight. Distances start at 0 and strictly increase. The line is empty when a point is not finite or lies outside the park, or when fewer than two points are kept.

## Footprints

footprintOf returns the rectangle a pose covers for a size: its width across the facing and its depth along it. It returns none when the pose is not finite or its facing has zero length. Its corners run front left, front right, back right, back left. So the front face runs from the first corner to the second, and the back face from the third to the fourth.

## Physical validity

A world is physically valid when no object leaves the park, no two solids overlap, and no path meets a solid. Solids are the footprints of the entrance and the boxes. A path occupies its ground line widened by half its pathWidth.

Objects that only touch, within CONTACT_TOLERANCE, never conflict, whatever rounding their facings bring. Paths never conflict with each other: where they cross, routes derives a junction. An object with no footprint or with an empty ground line makes the world invalid. Only a hand-written save can hold one. isPhysicallyValid returns whether a world is valid.

## Commands

Five commands change intent: AddPath, AddBox, MoveBox, DeletePath, and DeleteBox. No command moves or deletes the entrance. Tools submit commands to the command queue, so they apply between ticks.

On a valid world, a command is accepted exactly when it names what it acts on and the world it describes is physically valid. isAccepted returns whether it would be. applyCommand changes nothing when the command is not accepted. So what a ghost shows as valid is what its commit does.

An accepted command leaves exactly the intent it describes and changes nothing else:

- AddPath and AddBox each add one entity from the key counter.
- MoveBox replaces a box's pose and keeps its key and kind.
- DeletePath and DeleteBox destroy the entity they name.

A pose or path is kept exactly as given.

A ParkEdit holds any one of the five commands. It is the value a tool shows as a ghost and commits. isAccepted on a ParkEdit is isAccepted on its command. queueEdit pushes the command, so a cycle applies it as if it were pushed directly.

## The new park

makeNewPark(schema, seed) returns a physically valid world at tick 0, pending resolution. The world holds an entrance with its back on the park's edge, facing in, and a short guest path in front of it. tests/parks/new.park is the save of makeNewPark(seed) with seed 1.
