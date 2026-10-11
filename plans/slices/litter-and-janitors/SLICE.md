# Slice: Litter and Janitors

Status: planned

## Summary

When this slice lands, eating and drinking leave litter, and the park has its first staff. Guests carry their litter along their way and drop it in a bin they pass. If they find no bin within a short carry, they drop it on the paving. Bins fill, and janitors from a staff room empty them and sweep the paving. The garbage goes out of the staff room's back door, over the backstage paths, to a depot. The player sets bins along each path as they set seats, and places staff rooms with a door onto the guest paths and a door onto the backstage paths. Each guest has its own tolerance, which scales how much every discomfort pushes it to go home. A little litter sends a few fussy guests home, and a filthy park sends many. This is the right next goal because it builds on somewhere-to-sit's furniture, and the first staff turn layout into logistics: where a staff room sits decides how far its janitors walk, and so how clean each area stays. The slice comes after somewhere-to-sit and before supply-lines, which will send the garbage out of the park.

## End-to-end scenario

The player opens day.park, as somewhere-to-sit left it: a T of guest paths with a food shop at the west end and a drink shop at the east end, tables along the west arm, benches along the path in from the entrance, a toilet block, a depot, and a backstage path behind the shops.

They set bins on every guest segment, every 15 m, on one side. Before they confirm, a ghost shows the bins along the verge, sharing it with the tables and benches. They place a staff room between the junction and the backstage path. It snaps to face the guest path as a shop does, and its back door reaches the backstage path. Its two connectors show in the ghost.

Guests arrive. A guest finishes its meal at a table and holds the wrapper. It walks on toward the drink shop. A bin lies beside the path a few metres ahead, so the guest drops the wrapper in as it passes. Another guest drinks at the drink shop and walks off along the east arm. The next bin lies farther ahead than it will carry the cup, so it drops the cup on the paving where its carry runs out.

Bins fill through the morning. A janitor walks out of the staff room's guest door, along the paths, to a full bin, empties it, sweeps the litter near it, and walks back. The staff room's crew is fixed, so each janitor weighs how much litter a bin or a stretch of paving holds against the walk to it from the staff room. Janitors keep the places near their staff room clean, and walk to far places only once those get bad. The garbage leaves through the staff room's back door and travels the backstage path to the depot, which disposes of it.

Each guest drew its tolerance when it arrived. Walking over litter builds a guest's disgust. A fussy guest that crosses a dirty stretch of the east arm heads home early, and the departures panel counts it under litter. The guest inspector shows its low tolerance and the litter term that decided it. Most guests walk the same stretch and stay.

The player removes the bins on the east arm. By afternoon, cups and wrappers lie along it, the janitors spend their time there, and the west arm gets dirtier too. More guests leave for litter. The player opens the staff room's inspector. Its crew is always busy. The player sets the east arm's bins again. They place a second staff room near the drink shop and draw a short backstage spur from its back door to the backstage path.

Later the player deletes that spur. The second staff room's garbage has nowhere to go, so its store fills, its janitors stop emptying bins, and litter spreads around the drink shop. Its inspector says its store is full and names the reason: no backstage route to a depot. The player draws the spur again, and the garbage starts moving.

## Acceptance criteria

The day parks are remade. day.park gains bins on every guest segment and a staff room between the junction and the backstage path. noon.park, evening.park, no-toilet.park, far-tables.park, and unfurnished.park follow from day.park as before. Two park files join them:

- binless.park is day.park with no bins. Member 4 adds it.
- no-staff-room.park is day.park without its staff room. Member 5 adds it.

Each member that registers new state remakes the day parks so a checked park holds it. Member 2 gives day.park its bins and staff room. Member 4 remakes them with litter and guests' tolerances. Member 5 remakes them with crews at work and garbage in the staff room's store. No park holds litter before member 4, so member 1 must keep place stocks inside state the checks already cover, such as the ledger's, rather than register a new type that no checked world holds until then.

tests/parks/stress/full.park carries bins on every guest segment and enough staff rooms to keep it clean. The runtime report measures it. Guests gain a tolerance, so every other park that holds guests is remade too: warm.park, cut.park, and warm-main-street.park. Their integration tests must keep passing.

The park world checks read every park in tests/parks. Once these parks hold bins, staff rooms, litter, and guests' tolerances, the checks cover the slice's new state without a new test. Litter stays conserved because place stocks are part of the ledger: units held at places count among the units held, so the ledger's totals still add up (principle 3). The slice adds two edits of noon.park to the world checks' chosen edits, so each preview is checked against its commit (principle 8). Member 2 adds both: one sets a segment's bins, and one places a second staff room.

1. Removing the bins makes more guests leave for litter: over one day, more guests leave binless.park with litter as their reason than leave day.park for it. (integration test)
2. Removing the staff room makes more guests leave for litter: over one day, more guests leave no-staff-room.park with litter as their reason than leave day.park for it. (integration test)
3. A capture of noon.park shows bins along the paths, and a janitor on the paving between the staff room and a bin. (scripted capture: legible-simulation)
4. A capture of binless.park at evening with the litter overlay on shows litter drawn on the paving and the overlay shading it. (scripted capture: legible-simulation)
5. The runtime report on full.park after the slice, with bins on every guest segment and its staff rooms at work, is compared with the report from before member 1. The ticks stage stays within the 33 ms budget. (manual)
6. In the running app, these work as the scenario describes: setting a segment's bins with its ghost, undo, and redo; placing a staff room with both connectors in its ghost; janitors walking the real route from the staff room and back; the litter overlay with hover naming the amount and the nearest staff room; the staff room inspector; and the guest inspector's tolerance and litter term. (manual)
7. In the running app, in day.park, a few guests leave for litter over a day, and in binless.park many do. (manual)
8. In the running app, if a staff room loses its backstage route, its store fills, its janitors stop emptying bins, and its inspector names the missing route. Drawing a route back lets the garbage move again. (manual)

## Medium

Abbreviations: SM is shared-medium, EB is effortless-building, NN is navigable-networks, PO is plausible-operations, BG is believable-guests, and LS is legible-simulation.

- Place stocks: a new part of the medium, produced by SM, used by PO, BG, and LS. A flow can send units to a place on a network, and the place holds them as a stock. Units held at places are part of the ledger and count among the units held, so its totals still add up. The stocks along a network can be sampled like a field, and the units can be sent on from a place. A system can find the stocks that best repay a walk from a source, weighing each stock's size against its route distance from the source. The same lookup covers the stocks of entities published at places, such as bins, so a crew finds full bins and dirty paving in one search, without visiting every bin or place. Work on them grows with the movers and actors that touch them, never with the size of the park (decision 0031). PO acts for the place stocks on the guest paving. BG samples them where guests walk, and LS samples them for the overlay and to draw litter.
- Bin places: a resolved entry field on the guest network, produced by EB, consumed by PO, BG, and LS. LS reads it to draw each bin's fill. EB draws the bin itself. Each entry is one bin, published at the place on the guest paving beside it. It holds the bin's key, its position and facing on the verge, and its segment. Each bin is an entity while its setting stands, so it can receive a flow.
- Litter: a flow kind. BG creates litter for a guest when it finishes a meal or a drink, and the guest holds it. BG sends it to a bin the guest passes within its carry distance, or else to the place stock where its carry runs out. A bin holds litter up to its capacity. PO sends a bin's overflow to the place stock at its place. If a bin is removed while its paving stays, what it holds spills onto the paving there. Otherwise, when a guest leaves or an edit removes where litter is held, the litter is used up with a named cause the ledger records, such as taken home by a guest or cleared with removed paving. Litter never vanishes without a cause. PO sends the litter a janitor collects, from bins and from place stocks, to its staff room, with the delay of the janitor's walk. A staff room holds litter as garbage up to its store limit, and sends it over the backstage network to the nearest depot, with the delay of that route. The depot consumes it, with the cause disposed.
- Bin fill: each bin's litter is a stock in the ledger. PO's crews find full bins through the place stocks' lookup, and LS reads each bin's fill through the ledger's holdings query to draw it. Nothing reads a bin's internals (principle 6).
- Route distance: NN publishes each staff room as a source on the guest network and on the backstage network, through its two doors, as it does for every anchored entity today. PO reads it for how far each stock is from a crew's staff room and how long the walk takes, and for the route to a depot. LS reads it to name the staff room nearest a place by route distance. The hover doesn't claim that this staff room's crew is the one that cleans there.
- Crew trips: a stepped entry field on the guest network, produced by PO, consumed by LS. Each entry is one janitor's trip: its staff room, where it is going, when it set off, and the route's length. LS draws each janitor from it, where and when the crew's work is actually spent (docs/design-notes.md, "Figures").
- Guests: each guest's tolerance, disgust, and carried litter stay private to it. BG senses litter only by sampling the place stocks where it walks. The guest record adds the guest's tolerance and its litter term, and litter joins the departure reasons, so LS reads them as it reads the other terms today.

Staff rooms are the only place staff pass between guest and backstage paths (decision 0040).

Some dependencies are not fields or flows. Decision 0025 rules on them.

- Park intent: each guest segment's bins setting, and staff room boxes. EB authors them, and they are saved (principle 1). EB derives bin places from the setting. NN derives the staff room's two doors from its box. PO finds the staff rooms from intent to give each its crew and store.
- Inspection records: PO publishes a staff room's record: its crew, how many are busy, the garbage in its store and its limit, and why it can't send garbage, such as no backstage route to a depot. Only LS, tests, and the app read it.

## Members

The slice builds on somewhere-to-sit, which must close first: its furnishing settings, the edge-line layout, and pieces of furniture as entities.

1. `shared-medium/place-stocks`: A flow can send units to a place on a network and on from it, the stocks along a network can be sampled like a field, and a lookup finds the stocks, at places or in bins, that best repay a walk from a source, with work that follows the movers and actors. When an entity published at a place is removed, such as a bin, the ledger passes its stock to the stock at that place, or uses it up with a named cause if the place is gone too. Depends on: none.
2. `effortless-building/bins-and-staff-rooms`: Each guest segment gets a bins setting with a spacing and a side, laid by somewhere-to-sit's layout alongside the seats. Bins are entities and publish bin places. The player places staff rooms, which front guest paving and reach backstage paving behind them. Both are drawn. day.park and full.park carry them, and it adds the two chosen edits. Depends on: none.
3. `navigable-networks/staff-room-doors`: A staff room connects to the guest network by its front door and to the backstage network by its back door, and becomes a route distance source on both. Depends on: member 2.
4. `believable-guests/littering-guests`: Guests make litter after meals and drinks, carry it, drop it in a bin they pass within their carry distance or on the paving where it runs out, and build disgust from the litter they walk over. Each guest draws a tolerance that scales every going-home discomfort term, and litter becomes a departure reason. It remakes the day parks with litter and tolerances, adds binless.park, and remakes every other park that holds guests. Depends on: members 1 and 2.
5. `plausible-operations/kept-clean`: Bins hold litter and overflow onto the paving. A removed bin's litter spills there by member 1's ledger rule. Each staff room's fixed crew collects from the bins and paving that best repay the walk, weighing litter against route distance from the staff room, its store holds the garbage, and it sends the garbage over backstage paths to the nearest depot, which disposes of it. It publishes crew trips and the staff room's record. It remakes the day parks with crews at work and adds no-staff-room.park. Depends on: members 1 through 4.
6. `legible-simulation/explained-litter`: Litter on the paving, bins' fill, and janitors from crew trips are drawn. A litter overlay has hover naming the amount and the nearest staff room by route distance. The staff room inspector shows its crew, its store, and why it can't send garbage, and the guest inspector shows tolerance and the litter term. The departures panel counts litter. Depends on: members 1 through 5.

## Out of scope

- Garbage leaving the park. supply-lines sends it out through service entrances.
- Money: wages, and hiring janitors one by one.
- Cleaning standards for each staff room.
- Staff breaks, tiredness, and morale.
- Other staff jobs: cleaning toilets, serving in shops, and maintaining rides.
- Vomit and other mess.
- Gates the player places for staff to cross (decision 0040 leaves them open).
- Guests who go out of their way to find a bin.
- A tolerance for each need, and other personality traits.
- An overlay of how far it is to the next bin.
- Litter that attracts more litter, as people litter more where litter already lies.

## Open questions

None.

## Research notes

Planet Coaster 2 and Parkitect limit janitors with zones the player draws, which principle 9 rules out. Here, where the player puts a staff room limits where its janitors work. People carry litter only about 30 steps, so bin spacing is the player's main lever. Details are in RESEARCH.md.
