# Research: snaps-and-guides

The slice's RESEARCH.md (plans/slices/good-paths/RESEARCH.md, "How do the best tools make visible snapping feel good?") covers SketchUp, AutoCAD, Figma, tldraw, and snap priority, stickiness, reach, guides, readouts, and suppression. This file adds a focused look at Cities: Skylines 2 and the mods players use to fix its road snapping.

## What do Cities: Skylines 2 and its mods teach about snapping a drawn network?

Cities: Skylines 2's net tool has these snap options, each with its own toggle, and roads use most of them by default:

- existing geometry;
- zoning cell length, which grows a road in 8 m steps;
- 90° angles;
- guidelines from other roads;
- the zone grid;
- building sides.

Guidelines extend nearby roads and line up new roads with them. Players who want clean grids are told to keep existing geometry, cell length, and 90° only. Guidelines and the zone grid pull roads a little off, misalign zoning, and leave half-cell gaps. Players also report that the 90° snap is not sticky enough: it lets go too easily, and they ask for a way to lock it. Its readouts round length and angle to whole numbers. The Road Precision mod adds decimal readouts, because a rounded readout hides a road that is a fraction off.

Precision Engineering, the main snapping mod for Cities: Skylines 1, shows how such snaps are built. Its source does the following:

- Angle snap measures the new road's angle from the existing segment at the start node nearest in direction. It rounds that angle to a fixed step, 5° by default, and keeps the pointer's distance from the node. The reference is a segment already at the node, not the world's axes.
- Guidelines are the straight-on extensions of nearby segments of the same class, from their end nodes. An extension is left out when the node already has a segment within 45° of it, or when it would run into another road. The new road's end snaps to where the line from its start to the pointer crosses a guideline, within a query distance. It is a crossing, not a slide along the guide.
- Readouts show the length along a snapped guideline and the angles on both sides of the join.
- Each kind of snap is held on with its own key: Ctrl for angles and Alt for guidelines.

Road Protractor, another Cities: Skylines 1 mod, snaps to 5° steps when the road joins another road, and to 15° steps when it starts free.

For this game, these findings suggest the following:

- Measure direction snaps from the segments already at the point the new segment starts from: straight on, 45°, and 90° from each. Use the world's axes only when no segment is there. That is the reference both mods use, and it keeps a new path square to the path it leaves.
- Prefer crossings over slides. The most useful guideline snap ends a segment exactly where a direction guide meets an existing segment, or where two guides meet, for example to close a rectangle. Rank crossings above plain segment and guide snaps, as the slice research's priority already does.
- Keep length steps at whole meters. A park has no zoning cell to match.
- Make the direction snap hold firmly. A release reach wider than the catch reach answers the "not sticky" complaint.
- Show lengths to two decimals, never rounded to whole meters.
- Keep one held key that turns every snap off, rather than a key to turn each kind on. A theme park player draws freely more often than a city builder draws grids, so snaps stay on by default.

Rejected:

- Guidelines from every nearby path end, as Cities: Skylines 2 and Precision Engineering draw them. Cities: Skylines 2 players turn them off because they pull roads off the grid. In this milestone, guides come only from the path's own anchors. Extensions of nearby path ends go to the backlog.
- Snapping to a fixed cell length such as 8 m. There is no zoning grid to match, and whole meters are finer.
- A separate toggle or held key for each snap kind. It costs a menu trip or a key chord, where one suppress key is enough (slice research).
- Angle steps of 5° or 15°. The milestone asks for straight on, 45°, and 90°. Finer steps catch nearly every direction, so freehand drawing stops being free.

Sources:

- https://cs2.paradoxwikis.com/Roads — default snaps and what each does.
- https://cs2.paradoxwikis.com/Editor:_Snapping_and_Tool_Modes — snap options and the 8 m cell length step.
- https://steamcommunity.com/app/949230/discussions/0/4328601129273074437 — guidelines and the zone grid breaking alignment, and the advice to keep geometry, cell length, and 90°.
- https://steamcommunity.com/app/949230/discussions/0/4030224296642027359 — the 90° snap not being sticky, and requests to lock it.
- https://github.com/gadeleon/road_precision — rounded readouts, and decimal ones as the fix.
- https://github.com/Simie/PrecisionEngineering — source of the angle snap from the nearest segment at the start node, extension guidelines from nearby ends, crossings with them, and readouts (Src/PrecisionEngineering/Detour/SnapController.cs, Data/Calculations/Guides.cs).
- https://www.gamewatcher.com/mods/cities-skylines-mod/road-protractor — 5° steps when joined and 15° when free.
