# Milestone: Furnished Paths

Slice: somewhere-to-sit

Status: planned

## Summary

The player furnishes guest paths instead of placing benches one at a time. A Furnish tool works like a brush. The player picks benches, tables, or none, a spacing, and one side or both. Hovering a guest segment shows the furniture that setting would lay there, and a click applies it. The game lays the pieces on the verge beside the paving. It spreads them evenly within each free stretch, and keeps them clear of boxes and their frontage, walkways, other paving, and junction mouths. When a later edit takes some of a stretch, as a new shop or a crossing path does, the pieces in that stretch spread again over the room left, and the ghost shows it before the commit. Each piece is an entity and publishes a seat place, so plausible-operations can serve seats in served-seats, the slice's next member. This is member 1 of somewhere-to-sit. Every later member builds on the pieces and the seat places it lays.

## Acceptance criteria

1. The Tools panel has Furnish. With it chosen, the panel shows Kind (None, Benches, Tables), Spacing in meters, and Sides (One side, Both sides). Hovering a guest segment makes the ghost: the segment highlighted, and the furniture the candidate lays on it. With One side, the side is the one the pointer is on, and the other side keeps its furniture. A click commits a SetFurnishing edit for that segment. Kind None removes the furniture from the side or sides chosen. Hovering a backstage segment, or no segment, shows no ghost, and a click commits nothing. (play)
2. A segment's furnishing is saved intent, held for each side, left and right, seen walking from its From point to its To point. Each side has its own kind and spacing, or none, and is saved in a line of its own. SetFurnishing is accepted when the key holds a guest segment and each side's spacing is finite and above 0. It changes nothing else. When a furnished segment splits, every part keeps its furnishing. When a corner splits, the new segments along its arc take furnishing as they take width: the last takes the second segment's, and the rest the first's. When RemovePoint merges two segments, the merged segment keeps each side of the lower-keyed segment's furnishing, or takes the other's on a side that has none. Whenever a segment takes another's furnishing and the two run opposite ways, left and right swap. Undo and redo restore a furnishing change. A segment's record in the history and in RestoreIntent carries its furnishing, so undoing a furnished segment's deletion brings back its furniture. Every park file saved before this milestone loads, has no furnishing, and saves again to its own text. (play for the tool's commands, splits, merges, undo, and redo; the park world checks; and a scratch check that loads each older park file and compares its save with the file)
3. Furniture is derived in resolution from the segments' furnishing and the park as it stands. Each piece stands on the verge outside the paving, square to the paving's edge line and facing it. Within each stretch of verge free of obstacles, the pieces are evenly spaced, as close to the set spacing as the stretch allows. Pieces follow the edge line around curves and tapers. No piece stands on paving of either kind, on a box or the entrance, on a box's frontage strip (the area in front of its front face, as wide as the face, out to the paving), on a walkway, on another piece, across a junction's mouth, or outside the park. (scripted capture of tests/parks/furnished-paths.park, and play)
4. An edit moves only the pieces in the stretches it changes, and a piece in a stretch the edit leaves alone keeps its key and its pose. Placing a shop beside a furnished segment removes the pieces under it and its frontage strip, and re-spreads the pieces left in that stretch. Pieces in other stretches don't move. Furniture never makes an edit refused, and a box or path can be placed across it (principle 5). (play, on a copy of furnished-paths.park; and a scratch check that places a shop in the first stretch of a furnished side there, and finds every piece in the side's other stretches with its key and pose unchanged)
5. Every edit's ghost shows the candidate's furniture. Pieces the edit adds or moves stand out, pieces it removes show in the delete tint, and pieces it leaves as they were blend into the park. A committed edit lays exactly the furniture its ghost showed (principle 8). (play, and the park world checks' chosen edits)
6. Each piece is an entity with a derived key, so it can receive flows from served-seats on. Its key depends only on its segment, its side, and where it stands along the segment, so a change elsewhere on the side never gives it another piece's key. It exists while its furniture stands, and it is gone when a resolution no longer lays it. The renderer draws benches and tables in their own colors and shapes. Nothing reads the pieces' seat places yet, so the simulation is unchanged. The parks that `tpj_scenarios --slice-parks` remakes are the same text as before each feature, except that from seat-places on, each holds an empty seat-places-stepped section, as every registered field adds one. (the remade parks compared with cmp for features 1 and 2, and with diff for feature 3, showing only that section; the new registered types change every world's hash, so hashes can't show it)
7. Seat places is a resolved entry field on the guest network. Each furnished segment is the source of one entry per piece. The entry lies at the place on the segment's ground line nearest the piece. It holds the piece's key, its position and facing, its kind, its number of seats, and its segment. (the park world checks, which hold the field through furnished-paths.park, and a scratch check that samples the field at each piece's place in furnished-paths.park and finds that piece's entry)
8. tests/parks/furnished-paths.park holds guest paths at MIN_PATH_WIDTH, the default width, and MAX_PATH_WIDTH, a taper, a tight and a wide rounded corner, a junction, shops fronting furnished segments, and connectors crossing their verges. Every guest segment carries furniture, both kinds appear, and one side and both sides each appear. The park world checks gain a chosen edit of noon.park that furnishes a guest segment, and require that its candidate lays pieces. (ctest)
9. tests/parks/stress/full.park carries furniture on every guest segment, several hundred pieces in all. The runtime report on full.park after the milestone is compared with one from before its first feature. Laying furniture costs work that grows with the pieces and with what lies near each furnished segment, never with segments times boxes. (runtime report, and review)
10. The sim, park, tools, render, app, scenarios, and bench specs describe the furnishing intent, its command, the Furnish tool, the drawn pieces, the parks, and furniture's place in the registration order. A new SPEC.md for sim/furniture states the layout, the pieces, and the seat places field. (review)

## Medium

The milestone adds one field, seat places. The rest is park intent, tentative intent, and derived data that only display reads, which decision 0025 places outside principle 3.

- Furnishing intent: produced by `furnished-segments`, as part of the segment's intent. sim/furniture reads it through parkSegments, and the history and RestoreIntent carry it in the segment's record.
- Furniture pieces: derived by sim/furniture in `furnished-segments`, and made to yield in `yielding-furniture`. The renderer and the ghost read them through a public query, parkFurniture, as they read parkSurface.
- Seat places: a resolved entry field on the guest network, produced by `seat-places`. Each furnished segment is the source of its pieces' entries. plausible-operations' served-seats, navigable-networks' best-seats, and believable-guests' seated-meals consume it, as the slice's medium map says.

sim/furniture reads park intent through parkSegments, parkPathPoints, parkBoxes, parkEntrances, and footprintOf, and doors, walkways, and the paved surface with its walkways through routes' parkDoors, keyedWalkways, and walkedSurface. It reads nothing private to another module (principle 6).

The new module src/sim/furniture serves effortless-building. It sits in its own layer in cmake/layers.txt, between sim/routes and sim/operations, because it reads routes' walkways and plausible-operations reads its seat places. addPark registers it after addRoutes. The Furnish tool goes in src/tools, its controls in the app's Tools panel, and the pieces' drawing in render's park mesh.

## Dependencies

- path-network: the paved surface and its edge lines, splits and merges, undo and redo, and RestoreIntent. Complete.
- facilities-and-hours: the frontage snap, which sets where a box's frontage strip lies. Complete.
- navigable-networks' connector walkways. Met.
- The day-at-the-park slice. Complete.

## Core feature

`furnished-segments`. It gives the player the brush and shows furniture along paths from the first feature, on straight verges clear of segment ends. Everything else in the slice builds on its intent and pieces. The yielding rules carry the most risk, so they land separately, on a working base.

## Features

1. `furnished-segments`: furnishing as segment intent, with the SetFurnishing command, its split and merge rules, the history and RestoreIntent, and its save. Adds the Furnish tool and its ghost, and the sim/furniture module, which lays pieces as entities along each furnished side's straight verge, evenly spread and clear of the segment's ends. Pieces are drawn in the park and in ghosts. Any edit's ghost shows the pieces it moves and removes. Adds the noon.park chosen edit. Depends on: none.
2. `yielding-furniture`: pieces yield to boxes, the entrance, frontage strips, walkways, other paving, other pieces, junction mouths, and the park's edge. They follow curves and tapers, and re-spread within each free stretch. Adds tests/parks/furnished-paths.park. Depends on: feature 1.
3. `seat-places`: pieces publish the seat places field on the guest network, with each piece's seats by kind. Furnishes full.park on every guest segment, and compares the runtime report with one taken before feature 1. Depends on: feature 2.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Painting furnishing across several segments in one drag, as one edit.
- Picking up a segment's furnishing with the brush, to apply it elsewhere.
- How far furniture stands from the paving, and how far it keeps from a junction's mouth, as settings the player controls.

## Open questions

- Tuning values, which the features set and playing adjusts: each kind's footprint and number of seats, the spacing's range and each kind's default, how far pieces stand from the paving, and how far they keep from a junction's mouth. The last two are fixed values that shape the park. The player can't change them yet. The deepening candidate above would hand them over.

## Research notes

Cities: Skylines 2's Line Tool spreads objects evenly over a line, at a spacing as close to the set one as fits, with a live preview. Furnishing does the same within each free stretch of verge. Street guides keep furniture clear of junctions and driveways. Details are in RESEARCH.md.
