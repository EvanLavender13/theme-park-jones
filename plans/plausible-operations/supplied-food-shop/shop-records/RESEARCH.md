# Research: shop-records

## How does a shop publish its record?

As a query, shopRecord(world, shop), computed from the world between cycles, like inventoryPosition. Decision 0025 lets tooling and tests read what an entity publishes about itself, and nothing in the park reads it, so the record needs no field and no stored state. Computed on demand, it is always the record of the world as it stands: a saved world holds nothing new, a candidate's record is its own, and nothing can go stale between a step and a read. The record's parts are those the capability names. Stock is the supplies the shop holds under its own handle. Queue is the guest-visits units it holds under the keys of live entities, the same count food-offer's wait starts from, so visits that arrived at the last swap count before the shop's next step reconciles its queue. On order is inventoryPosition less the stock: supplies on their way to it and orders on their way to or held by a depot. Starved is whether nearestDepot gives none.

Rejected: a state component the shop writes each step. It adds saved state that only repeats what the ledgers and fields already give, and a newly placed or candidate shop would have no record until it stepped. Rejected: an entry field. Fields are for what park entities sample, and no park entity reads the record (decision 0025).

Sources: docs/decisions/0025-outside-the-medium.md; src/sim/operations/SPEC.md, on inventoryPosition and the food offer's wait.

## What is the limiting factor?

The scarcest of the three things meal-service names, read from the record: with no supply route, that; otherwise demand when no guest is queued, supply when the shop holds fewer supplies than guests queued, and the service rate when it holds enough for every guest queued, since then only the one-guest-per-interval rule holds them. So the factor explains the shop's state without predicting, and it is what the slice's scenario asks for: a cut shop names its missing route, whatever its stock.

Rejected: supply only when the stock is empty. A shop with two units and five guests would report the service rate while three of those guests wait on supply. Rejected: counting shipments in transit as supply. That predicts arrivals, which the offer's wait already does, and the record would disagree with the stock it shows.

Sources: plans/plausible-operations/CAPABILITY.md, foundation criteria; plans/slices/boxes-and-tubes/SLICE.md, the scenario.

## How is a starved shop marked?

A cube floating over the shop's roof in STARVED_COLOR, as Parkitect marks an out-of-stock shop with an icon above it. Evan chose it over recoloring the box or its roof, since the box keeps the color that says it is a shop. The cube is appendBox's faces for a small square footprint at the shop's pose, raised above the roof, with a bottom face so it is solid from a low camera. Starved depends on intent alone in every world the app holds, since route distance is published only by resolution, so the park mesh, rebuilt when intent changes or a park is opened, always shows it.

The ghost appends its candidate world's marks at GHOST_ALPHA, after its walkways, as it does the candidate's walkways. A ghost mark lying on a committed one blends to the same color, so only the marks the edit adds stand out: hovering a backstage path with the delete tool shows a translucent mark over each shop it would starve, and placing or moving a shop where it has no supply route shows its mark on the ghost.

Rejected: marking in the ghost only the shops that change. It needs the committed world's records beside the candidate's, and the blend already hides the unchanged ones. Rejected: showing in the ghost that a starved shop would be supplied again. It needs a new mark kind, and the slice asks only for previewing starvation. It is a deepening candidate for later.

Sources: plans/plausible-operations/supplied-food-shop/MILESTONE.md, research notes; src/render/SPEC.md, Ghosts.

## What does the Debug panel show?

One line per shop box, in key order: its key, stock, queue, on order, and limiting factor by name. Starved needs no word of its own, since a shop is starved exactly when its factor is no supply route. Evan asked for it to be folded in, so the record is visible while playing before legible-simulation's inspector replaces it.

Sources: none; follows from the milestone's deepening candidate this feature draws.
