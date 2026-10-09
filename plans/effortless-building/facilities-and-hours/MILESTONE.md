# Milestone: Facilities and Hours

Slice: day-at-the-park

Status: complete

## Summary

The player gets the park's first policies and its first facilities beyond shops. They place benches and toilet blocks, which front the guest paths with the frontage snap as shops do. They set the park's opening hours in an Hours panel, and set each shop to stock food, drink, or both from the shop's Inspector. All of it is saved park intent, each change is an edit that previews before it is committed, and undo and redo cover it. Nothing reads the new intent yet: benches and toilet blocks get doors in navigable-networks' facility-doors, shops stock goods by their setting in plausible-operations' served-needs, and guests arrive by the hours in believable-guests' a-day-out. This is member 2 of day-at-the-park, and those members depend on it.

## Acceptance criteria

1. The Tools panel has Place bench and Place toilet. They place a bench, a small low box, or a toilet block, about a shop's size, each with its own footprint and color. Near guest paving, each fronts it in the pose the frontage snap gives, as a shop does. Move box, Delete, undo, and redo work on them as on shops, and their ghosts show the overlap check. They are refused only where a box would be (principle 5). (play, and a capture of tests/parks/facilities.park)
2. Until facility-doors, a bench or toilet block has no doors, so it joins no network, publishes nothing, and changes nothing guests or shops do. Every existing park simulates as before. (`tpj_scenarios --compare` on output from before and after each feature, and review)
3. When the Inspector shows a shop, it has radio buttons Food, Drink, and Both for the shop's stock, with the shop's setting marked. Choosing another makes it a tentative edit, which the preview resolves on a candidate as any edit, and Apply commits it with a SetShopStock command. A shop with no setting stocks food only, so every existing park keeps its shops' behavior. Undo and redo restore a stock change. A shop's setting is part of its record in the history and in RestoreIntent, so undoing a stocked shop's deletion brings it back with its setting. (play, and the park world checks)
4. The Hours panel shows the opening and closing times in 15-minute steps, with controls to change each. A changed time is a tentative edit, previewed on a candidate, and Apply commits it with a SetOpeningHours command. The panel keeps the closing time after the opening time. The command accepts any two times of day, refusing only a time outside the day, as SetSegmentWidth refuses a width that isn't a number. If the closing time is at or before the opening time, the hours run past midnight, and if the two are equal, the park is open all day. A park with no hours set has the default hours, 9:00 to 21:00. Undo and redo restore an hours change. (play, and the park world checks)
5. Opening hours and stock settings are saved park intent. Every park file saved before this milestone loads, reads as the defaults, and saves again to its own text, with no new section. tests/parks/facilities.park holds a bench, a toilet block, a shop set to drink, a shop set to both, and hours other than the default, so the park world checks cover the new intent, and each new edit is among the world checks' chosen edits. (ctest, and a scratch check that loads each park file saved before the milestone, compares its save with the file, and resolves it to find the default hours and stock settings and no new section)
6. The park, routes, tools, legible, app, and render specs describe the new kinds, the stock setting, the hours, their commands, and their controls. (review)

## Medium

The milestone adds no fields or flows. Everything it adds is park intent and tentative intent, which decision 0025 places outside principle 3:

- Bench and toilet block boxes: produced by `benches-and-toilets`. navigable-networks' facility-doors gives them doors, and legible-simulation picks and previews them.
- Each shop's stock setting: produced by `shop-stock`. plausible-operations' served-needs reads it.
- The opening hours: produced by `opening-hours`. believable-guests' a-day-out reads them for arrivals and closing, and legible-simulation's explained-needs previews the arrival curve from a candidate with tentative hours.

Each is read only through sim/park's public queries: parkBoxes, with the new kinds and each box's stock, and the hours query, which give the defaults where nothing is set. Routes keeps its doors to shops, depots, and entrances, so a bench or toilet block gets no connector until facility-doors.

## Dependencies

- deterministic-simulation/park-clock: the time of day the hours are stated in. Complete.
- path-network: the frontage snap, undo and redo, and RestoreIntent. Complete.
- legible-simulation's Inspector, which shows a shop and gains the stock controls. Met.

## Core feature

`benches-and-toilets`. It puts the slice's new facilities in the park, which facility-doors and served-needs build on, and it is the most visible of the three. It follows the pattern of shops and depots closely.

## Features

1. `benches-and-toilets`: the bench and toilet block box kinds with their footprints and looks, the Place bench and Place toilet tools with the frontage snap to guest paving, no doors in routes yet, and tests/parks/facilities.park holding one of each. Depends on: none.
2. `shop-stock`: each shop's stock setting as intent, with food only when unset, carried in a box's record in the history and in RestoreIntent, the SetShopStock command, Food, Drink, and Both in the shop's Inspector with Apply, its preview, undo and redo, and its save. Depends on: feature 1, for facilities.park.
3. `opening-hours`: the park's opening hours as intent, with 9:00 to 21:00 when unset, the SetOpeningHours command, the Hours panel with Apply, its preview, undo and redo, and its save. Depends on: feature 1, for facilities.park.

## Deepening candidates

Unordered pool this milestone draws later features from.

- Hours that differ by day, such as late openings on chosen days. Gated on: a calendar with weekdays.
- Benches and toilet blocks in sizes, such as a long bench or a large toilet block, with seats and stalls to match. Gated on: served-needs' seats and stalls.
- Setting the stock of several shops at once, such as every shop on a street. Gated on: selecting several boxes.
- Setting hours past midnight or all day from the Hours panel. SetOpeningHours already accepts them.

## Open questions

- The bench's and toilet block's footprints and looks are tuning values. benches-and-toilets sets them, and playing adjusts them.

## Research notes

Park games keep hours in a park panel and a shop's goods in the shop's window, and none previews a change before it applies. Here both are edits with a preview and Apply. Each new setting is its own component, so older park files load unchanged and read as the defaults. Details are in RESEARCH.md.
