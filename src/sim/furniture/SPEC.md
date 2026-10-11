# furniture

This module serves the effortless-building capability (plans/effortless-building). It lays the furniture each guest segment's furnishing asks for, on the verge beside the paving, so a park holds hundreds of seats without the player placing each one (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

furniture reads intent only through parkSegments, parkPathPoints, and parkSurface (sim/park/SPEC.md). addFurniture registers the derived type furniture-piece and its resolver. addPark calls it after addRoutes. Other modules read the furniture through parkFurniture and furnitureSize.

## Pieces

Furniture is derived in each resolution from intent alone, so saves never hold it (principle 1). parkFurniture gives every piece, in key order. A FurniturePiece has its key, its segment, the side of the segment it stands on, its kind, and its pose. furnitureSize gives a kind's footprint, its width along the paving and its depth across it.

Each piece is an entity, so flows can reach it. Its key is derived from its segment, its side, the stretch of that side it stands in, and its place in that stretch. A piece exists while a resolution lays it, and the resolution that no longer lays it removes it.

## Laying

Pieces stand on each side of a guest segment that has furnishing, its left or its right, seen walking from its From point to its To point. Each side has its own kind and spacing. They stand beside that side of the segment's body, as parkSurface gives it without walkways, on the verge just outside the paving, each with its front facing it. Each such side is a stretch. Pieces in a stretch keep clear of its ends, and they are evenly spaced, as close to its side's spacing as the stretch allows and never so close that neighbours touch. If a stretch is too short for one piece, it has none.
