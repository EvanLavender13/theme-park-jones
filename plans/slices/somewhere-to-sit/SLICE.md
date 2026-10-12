# Slice: Somewhere to Sit

Status: planned

## Summary

When this slice lands, seating is a setting on the paths, not a thing the player places piece by piece. The player sets a guest path segment to carry benches or tables, with a spacing and a side, and the game lays the furniture along the verge beside the paving. Tired guests walk to the nearest seat, and move on to the next if it is full. Guests who buy food carry it to a seat and eat there. If no seat is near, they eat standing, and the meal relieves less. The player sees where seats are missing on the rest and eating-seat overlays, sees guests sitting, and reads in the guest inspector whether a meal was eaten seated. This is the right next goal because a real park holds hundreds of benches, and day-at-the-park's bench boxes cost one route search each, so they can't grow that far. Seats reach guests through one field per need, so a seat adds an entry, not a route search of its own. The same pattern will carry bins and litter in the next slice. The slice starts after day-at-the-park closes and comes before supply-lines. It ends by removing the bench box.

## End-to-end scenario

The player opens day.park, laid out as a T. A guest path runs in from the entrance to the middle of a crossing path. The crossing path's west arm leads to a food shop at its end, and its east arm leads to a drink shop at its end. A toilet block faces the path in from the entrance, and a depot and a backstage path stand behind the shops. The Place bench tool is gone.

The player selects the west arm and sets its furnishing to tables, on both sides, every 6 m. Before they confirm, a ghost shows the tables along the verge on both sides of the paving. They follow the junction's edges and none stands across an arm's opening. None stands in front of the food shop, across its frontage or on its door's walkway. The tables beside the shop are spread evenly over the room left to them. The player confirms. They then set the path in from the entrance to benches, on the side away from the toilet block, every 10 m.

Guests arrive through the morning. A guest buys food at the shop and carries it. It samples the eating-seat field where it stands, which gives the walk to the best few seats for a meal, and it scores sitting down to eat against eating now (decision 0019). The tables are a few metres away, so it walks to them and sits. Eating at a table gives the meal's full relief and rests the guest a little. Another guest buys food just as every nearby seat fills. It finds its table full when it arrives and remembers that. It moves on toward the next best table, finds that full too, and has no other seat within a short walk, so it eats as it walks and gets less relief.

As guests walk, they tire. A tired guest samples the rest-seat field, scores sitting down against its other options, and walks to the nearest bench. It sits for a while and stands up rested.

The player opens the eating-seat overlay. The paving is shaded best along the west arm, less well along the benches, and worst along the east arm. Hovering a place names the segment whose furniture serves it. The player clicks the guest that found the tables full. The inspector shows its last meal, eaten standing, and the relief that cost it. The player sets the east arm to tables too. The overlay previews the coverage it adds before they confirm.

Later, the player places a second food shop facing the path in from the entrance, on the benched side. The ghost shows the benches it displaces and the benches beside it spread evenly over the room left. Benches farther along don't move. Placing the shop never fails because of furniture.

## Acceptance criteria

The slice regenerates every park under tests/parks, because the bench box goes away. day.park loses its bench box. It gains tables on both sides of the west arm, and benches along the path in from the entrance, on the side away from the toilet block. noon.park, evening.park, and no-toilet.park follow from day.park as before. Three park files join them:

- far-tables.park is day.park with its tables moved from the west arm to the east arm.
- unfurnished.park is day.park with no furniture.
- furnished-paths.park holds guest paths of every width the tools allow, a taper, a tight and a wide rounded corner, a junction, shops fronting furnished segments, and connectors crossing them. Every segment carries furniture.

tests/parks/stress/full.park carries furniture on every guest path segment. The runtime report measures it.

The park world checks read every park in tests/parks. Once these parks hold furnishing settings and seated guests, the checks cover the slice's new state without a new test. The slice adds two edits of noon.park to the world checks' chosen edits, so each preview is checked against its commit (principle 8). Member 1 adds one that sets a segment's furnishing. Member 4 adds one that places a shop on a furnished segment, once noon.park holds furniture.

1. Moving the tables away from the food shop makes fewer meals seated: over one day, a smaller share of meals is eaten seated in far-tables.park than in day.park. (integration test)
2. Removing the furniture makes more guests leave tired: over one day, more guests leave unfurnished.park with tiredness as their reason than leave day.park for it. (integration test)
3. A capture of furnished-paths.park shows furniture along every furnished edge. No piece stands on paving, on a box or its frontage strip, on a walkway, or across a junction's mouth, and the pieces in each free stretch are evenly spaced. (scripted capture: effortless-building)
4. A capture of noon.park shows guests seated at the tables beside the food shop and on the benches. (scripted capture: believable-guests)
5. A capture of noon.park with the eating-seat overlay on shows the paving shaded best around the tables, and less well along the benches. (scripted capture: legible-simulation)
6. The runtime report on full.park after the slice, with furniture on every guest segment, is compared with the report from before member 1, with 45 bench boxes. The park now holds hundreds of seats, yet tick and resolution times stay about the same, because seats add no route search of their own. (manual)
7. In the running app, these work as the scenario describes: setting a segment's furnishing kind, spacing, and side with its ghost, undoing and redoing it, placing a shop or drawing a path across furniture with the displaced and re-spread pieces previewed, the rest and eating-seat overlays with hover naming the serving segment and a preview of the coverage a setting adds, and the guest inspector's seated or standing meal. (manual)
8. In the running app, a guest who finds its seat's furniture full moves on toward the next best seats. A guest with food and no free seat near eats as it walks. (manual)
9. In the running app, the Place bench tool, the bench box, and the bench inspector are gone, and no park holds a bench box. (manual)

## Medium

Abbreviations: EB is effortless-building, PO is plausible-operations, NN is navigable-networks, BG is believable-guests, and LS is legible-simulation.

- Seat places: a resolved entry field on the guest network, produced by EB, consumed by PO, NN, BG, and LS. EB derives it in resolution from each segment's furnishing setting and the paved surface, so a candidate world shows the furniture a setting would lay (principle 8). Each entry is one piece of furniture, published at the place on the guest paving where a guest reaches it. It holds the piece's key, its position and facing on the verge, its kind (bench or table), its number of seats, and the segment it belongs to. Each piece is an entity while its furniture stands, so it can receive a flow. Each furnished segment is the source of its pieces' entries.
- Seat offers: a resolved entry field on the guest network, produced by PO, consumed by NN, BG, and LS. Each piece is the source of one entry, at its place in seat places. It holds the rest relief and the meal relief a seat there gives, which differ by kind.
- Best seats: resolved entry fields on the guest network, one for rest and one for meals, produced by NN, consumed by BG and LS. Sampled anywhere on the guest paving, including beside a piece between nodes, a field gives the few pieces that serve the need best from there, counting the walk. For each, it gives the piece's key, place, route distance, relief, and segment, and the way toward it. Each field has one source, so a sample's cost doesn't grow with the number of pieces. Each entry names its piece, so a choice traces to the piece (principle 8). NN computes the fields in resolution only (decision 0031), so they don't know which seats are taken. A guest learns that only when it visits a piece, and it keeps the pieces it found full to itself. The fields' cost grows with the network and with the number of pieces, never with the two multiplied, so a seat adds no route search of its own (RESEARCH.md, "What a bench costs in the current design").
- Seat visits: a flow of guests to a piece of furniture and back out, produced by BG, consumed by PO. A guest sends its visit to the piece's key. If a seat is free, PO seats the guest and returns it when its sitting time ends, with a rest serving. If every seat is taken, PO returns the guest unseated, as soon as a flow allows. If the piece is gone, as when the furniture was laid again, the ledger returns the visit to the guest. Free seats and sitting times stay private to PO.
- Guests: BG publishes each guest as today. Its guest record adds whether it is carrying food, the pieces it found full, and whether its last meal was seated. While it is seated, the record also gives the piece and the piece's pose, and the renderer puts the guests at one piece on its seats. No other module reads any of these in the simulation. LS reads them through the guest record, as it reads the choice today.

The rest-offer field, the bench's door, connector, and route distance source, and the bench's guest visits go away with the bench box. Rest servings stay, now sent by furniture. food-offer, drink-offer, and toilet-offer stay as they are.

## Members

The slice builds on day-at-the-park, which must close first: its need offers, need overlays, and the frontage snap.

1. `effortless-building/furnished-paths`: The player sets a guest segment's furnishing kind, spacing, and side as saved intent, with a ghost, undo, and redo. Furniture is laid along the paved surface's edge lines on the verge, yields to boxes, frontage strips, walkways, other paving, and junction mouths, and spreads evenly within each free stretch. Each piece is an entity that can receive flows. It is drawn, and it publishes seat places. full.park and furnished-paths.park carry it. Depends on: none.
2. `plausible-operations/served-seats`: Each piece of furniture publishes a seat offer with rest and meal relief by kind. Each piece receives seat visits, seats guests while seats are free, and sends rest servings. Depends on: member 1.
3. `navigable-networks/best-seats`: Produces the two best-seats fields in the Medium, rest and meals. Sampled anywhere on the guest paving, each gives the few pieces that serve the need best, counting the walk, and the way toward each. They replace a route search per bench with work that grows with the network and the pieces, never the two multiplied. Depends on: member 2.
4. `believable-guests/seated-meals`: Guests carry food after buying it, score sitting down to eat or rest against their other options, walk to the best seat, visit it, and remember pieces they found full and move on to the next best. With no free seat near, they eat as they walk with less relief. Seated guests are drawn sitting. It regenerates the day parks: day.park without its bench box and with its furniture, far-tables.park, and unfurnished.park. Depends on: members 2 and 3.
5. `legible-simulation/explained-seating`: The rest overlay reads the best rest seats, the eating-seat overlay joins it, hover names the serving segment, a ghost previews a setting's coverage, and the guest inspector shows the last meal seated or standing with the relief lost. Depends on: members 1 through 4.

Retiring the bench box is the slice's last step, after member 5. It is one change across the capabilities that own the parts, and each capability's brief drops bench boxes in the same change:

- effortless-building removes the bench box kind and the Place bench tool.
- navigable-networks removes the bench's door and connector.
- plausible-operations removes the bench's service and the rest-offer field.
- believable-guests removes guests' use of rest-offer and the bench's guest visits.
- legible-simulation removes the bench inspector.

Every remaining park is then regenerated.

## Out of scope

- Drinks carried to a seat. Guests drink at the shop as today.
- Bins, litter, and the staff who empty them. They are the next slice, built on the same furniture and field pattern.
- Furniture standing on the paving, as obstacles walking guests avoid.
- Plazas and dining areas laid out over an area.
- Layout patterns such as clusters and alternating sides, and trims and offsets along the segment. These are deepening candidates for furnished-paths.
- Placing or removing single pieces of furniture as exceptions to a setting.
- Shaded pavilions, quiet gardens, and show seating as destinations.
- Demand maps: where tired, hungry, or thirsty guests are, shown against each need's coverage overlay.
- A park stats panel, such as the share of meals eaten seated.

## Open questions

None.

## Research notes

Every park game surveyed stands seating on the path, and none has guests seek it at a reasonable cost. The slice keeps seeking and makes it affordable with one field per need, seeded from every seat at once. Details are in RESEARCH.md.
