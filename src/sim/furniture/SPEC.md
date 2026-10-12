# furniture

This module serves the effortless-building capability (plans/effortless-building). It lays the furniture each guest segment's furnishing asks for, on the verge beside the paving, so a park holds hundreds of seats without the player placing each one (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

furniture reads intent only through parkSegments, parkPathPoints, parkBoxes, parkEntrances, and footprintOf (sim/park/SPEC.md), and doors, walkways, and the paved surface with its walkways only through routes' parkDoors, keyedWalkways, and walkedSurface (sim/routes/SPEC.md). It reads the guest network through routes' parkNetwork. addFurniture registers the derived types furniture-piece and laid-furniture, the field seat places, and the furniture resolver. addPark calls it after addRoutes. Other modules read the furniture through parkFurniture and furnitureSize, and its seats through the seat places field, furnitureSeats, and seatPose.

## Pieces

Furniture is derived in each resolution from intent alone, so saves never hold it (principle 1). parkFurniture gives every piece, in key order. A FurniturePiece has its key, its segment, the side of the segment it stands on, its kind, and its pose. furnitureSize gives a kind's footprint, its width along the paving and its depth across it.

Each piece is an entity, so flows can reach it. Its key is derived from its segment, its side, and where it stands along the segment. A piece exists while a resolution lays it, and the resolution that no longer lays it removes it.

## Laying

Pieces stand on each side of a guest segment that has furnishing, its left or its right, seen walking from its From point to its To point. Each side has its own kind and spacing. The pieces stand along that side's line, as parkSurface gives it, on the verge just outside the paving. Each piece is square to the line where it stands and faces the paving. The line follows the paving around a bend, a rounded corner, a junction's edge, or a taper, as far as halfway to the next segment's side, so furniture follows curves and never stands across a segment's opening into a junction. It stops where the segment ends at a point no other segment uses.

Where a side runs on into another side with the same kind and spacing, the two are laid as one line, and so on along the curb. Where the curb closes on itself, as around the inside of a loop, the line closes too. Each piece belongs to the side it stands beside. Lines are laid in key order of their lowest segment, its left side first.

A piece can't stand where it, with a margin around it, would overlap paving of either kind, a walkway, a box, the entrance, a door's frontage strip, or a piece laid before it. It also can't stand where it would reach outside the park. Where only the paving is in its way at the verge's usual distance, as on the inside of a tight curve, it steps back from the paving, up to a meter, until nothing is in its way. Where anything else is in its way, it doesn't stand there. A door's frontage strip runs straight out from the face the door is on, as wide as that face, to where the door's walkway ends. A door without a walkway has none.

The places along a line where a piece can stand make its stretches. Pieces in a stretch are evenly spaced along the line through their centers, as close to the side's spacing as the stretch allows and never so close that neighbours touch. Where a piece would touch the one before it, as around a tight curve, it moves on along the line until it doesn't, and the pieces after it share the room left evenly. Where a line stops, its pieces keep clear of its end. Where it runs on into a side with other furnishing or none, the nearest piece stands half the spacing from its end. A closed line with nothing in its way, or nothing but a place too short for a piece, is spread evenly all the way around. If a stretch is too short for one piece, it has none.

If an edit leaves a stretch and everything that blocks it as they were, the stretch's pieces keep their keys and poses, wherever else the edit changes the line. Furniture never makes an edit refused. Where a box or a path takes the place of pieces, they yield (principle 5).

## Seat places

Seat places is a resolved entry field on the guest network, named seat-places. Each furnished guest segment is the source of one entry for each piece it lays, in key order of the pieces. The entry lies at the place on the segment's ground line nearest the piece's position. A SeatPlace holds the piece's key, its pose, its kind, its number of seats, and its segment. furnitureSeats gives a kind's number of seats: 2 for a bench and 4 for a table. If the guest network has no carrier for a piece's segment, the piece has no entry. A segment that lays no piece publishes nothing.

seatPose(kind, pose, seat) gives the pose of a seat of a piece of the kind standing at the pose. A bench's two seats lie side by side along it, 0.45 m either side of its middle, facing as the bench faces. A table's four seats lie in pairs on its two long sides, 0.45 m either side of its middle along it and 0.5 m from its middle across it, each facing the middle. Seats are numbered from the left, seen from the paving, and a table's paving side comes first.
