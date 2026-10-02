# Research: full-park

## What layout gives about 4 km of guest path, about 100 junctions, and 30 fed shops in a 256 m park?

A grid does it with nothing drawn at random. Ten guest paths along z and ten along x, each 200 m long and 20 m apart, give 4,000 m of path crossing at exactly 100 points. A 20 m cell leaves 17 m between path edges, since a guest path is 3 m wide. The template's entrance at (0, 126.5) joins the grid's top path at z = 90 by a 33 m path down x = 0.

Shops and their supply lines fit inside the cells. A backstage row runs along the middle of every other gap between guest paths, at z_b = -80, -40, 0, 40, and 80, each 10 m from the guest path on either side. A shop, 8 m wide by 6 m deep, faces -z at (x, z_b - 4.5). Its front face is 2.5 m from the guest path at z_b - 10, and its back face 1.5 m from the row. The guest path's 1.5 m half width and the row's 1 m half width then leave 1 m and 0.5 m clear. Each door is within CONNECTION_REACH, 4 m, of its network, so every shop gets both connectors. The vertical guest paths are 10 m either side of a cell's center, and a shop reaches 4 m from it. Taking six of a row's nine cells, those with k mod 3 not 2, gives 30 shops with empty cells between groups.

A depot is 8 m deep, too deep for the 7.5 m between a row's edge and the next guest path's. So the depots stand outside the grid, in the 28 m west margin. A backstage spine at x = -106 joins every row's west end. Three depots face it from x = -111.5, at z = -60, 0, and 60, with their front doors 1.5 m from it. Every shop then reaches some depot, so none is starved, and supply routes run from about 30 m to about 200 m. At SUPPLY_SPEED, 2 m/s, that is 15 to 100 s, so a save a minute into play holds shipments in transit to the far shops and stock at the near ones.

Paths of different kinds may cross (sim/park/SPEC.md), so the vertical guest paths cross the rows freely. They never share a node, since the networks are separate.

Rejected: a randomly drawn layout — every rule a draw could break would need a retry loop and a check, and it would add nothing a grid lacks for timing. Depots inside the cells — they do not fit. One depot per row — the milestone asks for a few depots supplying many shops over shared backstage paths, which the spine gives.

## How are 2,000 guests spread, and how long do they stay?

Each guest stands on an edge of the guest network picked by drawPick over the edges' lengths, at a uniformly drawn distance along it. So guests spread evenly along the network's length, short connectors rarely get one, and every place resolves, since it lies on an edge. The draws are keyed on NULL_KEY and the guest's index, at tick 0, so they depend on the seed alone.

Each added guest's stay ends 36,000 ticks, 20 minutes, after the save: 120 times tpj_bench's default run of 300 ticks, so none heads home during a run. The entrance still admits a guest every 60 ticks, about 30 during the minute of warm-up, and they come and go as in any park. So the park holds 2,000 guests and a few more through every run.

## Where does the file live?

Each guest's line is about 400 characters, now that a guest keeps no last choice (unsaved-choices), so the save is about 1 MB and is committed, as the milestone says. With the last choice it would have been about 13 MB, the reason unsaved-choices came first.

Rejected: generating the park for each report — it adds a minute of stepping to every report, and nothing gains from it once the file is small.

Sources: sim/park/SPEC.md (sizes, physical validity, the new park); sim/routes/SPEC.md (doors, CONNECTION_REACH); sim/operations/SPEC.md (supply routes, SUPPLY_SPEED); tests/parks/warm.park (line sizes).
