# Research: Two Goods

## How does one queue tell a guest's food visit from its drink visit?

A shop's queue is saved state: shop-service holds one guest key per visit, in arrival order. warm.park, cut.park, warm-main-street.park, and full.park all hold it. If the queue's entries gained a good, every one of those lines would change, and the milestone requires that parks whose shops all stock food stay as they are.

A guest's visits are already held by kind. The shop holds food visits under guest-visits and drink visits under drink-visits, each by the guest's handle. So the queue can stay a list of guest keys if a rule says which of a guest's entries is which: a guest's entries take its visits in the order of the goods, food first, as many of each as it holds. A guest with one visit, which is every guest so far, is never in doubt. A guest with both at one shop is one person at the counter, and taking its food first changes nothing for anyone behind it. The rule adds no state, so a food-only park saves the same text.

Rejected: a good on each queue entry — changes the saved queue of every warm park. A second component listing each entry's good — new saved state that no checked park would hold until a-day-out, and a second list to keep in step with the first. A queue for each good — loses the arrival order between goods, which one counter needs.

Sources: src/sim/operations/internal/shop_service.h — the queue; src/sim/operations/operations.cpp — reconciledQueue rebuilds the queue from the visits held.

## How does the sell-off offer survive the resolution that a stock change starts?

When a command changes a source's resolved entries, the field's finisher clears that source's stepped entries. A shop switched away from drink loses its resolved drink offer, since resolution publishes an offer only for goods the setting names. So the finisher would clear the stepped drink offer too, and the preview and the committed world would show no drink while the shop still sells it off.

The finisher can ask the field's owner. If a field's type gives a keepStepped rule, the finisher keeps a changed source's stepped entries when the rule says so. For a shop's offers, the rule keeps the offer of a good the shop's setting no longer names while the shop holds or awaits that good. The finisher may read state, since it changes only state its module's spec says a resolution invalidates. The resolved layer still comes from intent alone. In a world's first resolution, the finisher compares nothing, so a save loads and resolves equal mid-sell-off. This follows the field's existing optional rule, sampleEdge.

Rejected: a resolver that reads the ledger to publish a sell-off offer — makes resolved data depend on state, which breaks the load rule. Keeping every switched shop's stepped offer — the preview would show an offer for a good the shop no longer has. A finisher in operations that restores the offer — the medium's finisher has already cleared it, and operations can't reach the field's storage.

Sources: src/sim/medium/field.h — settleSteppedEntries, HasEdgeRule; src/sim/SPEC.md — Resolution.

## How much can a shop still serve of a good it no longer orders?

A starved shop serves from what it holds and from shipments already on their way, and returns the queued guests beyond those. A good the shop no longer orders works the same way: its cover is the units held plus the units shipped. Orders still at or on their way to the depot ship later and add to the cover when they do. The offer's wait stays exact, because every unit it counts has a known tick. If the next guest for the good would be beyond the cover, the offer says none, as a starved shop's does. Once the shop neither holds nor awaits the good, it publishes no offer for it.

Rejected: counting unshipped orders in the cover — their ship tick isn't known while they travel, so the wait would be a guess. Cancelling unshipped orders at the switch — the shop would read the depot's holdings, and switching back at once would lose stock.

Sources: src/sim/operations/operations.cpp — supplyCover, returnUncovered, nextWait.

## What does stocked.park hold, and how is it made?

facilities.park has a drink shop, a shop selling both, a bench, and a toilet block on one guest path, but no depot. The generator adds a depot near the both shop and a backstage path that runs past both shops' backs. The both shop's route to the depot is short, and the drink shop's is long. Both shops order in their first cycle. The orders reach the depot together and ship together, so the both shop's food and drink arrive several ticks before the drink shop's drink. The generator steps until a shop holds food, a shop holds drink, and drink is still in transit, which then holds for several ticks. If that never happens within a bound, it throws, naming what it waited for.

The park opens at 10:00 and play starts at 9:00, so no guest arrives while it warms up. Drink visits come only from synthetic guests until a-day-out, so stocked.park holds no queued drink visit. The new state needs none: the drink ledgers and the drink offer's stepped entries are held whether or not a visit is queued.

Rejected: a hand-written stocked.park — it would not follow the simulation's rules when they change. A fixed number of warm-up ticks — a change to the route lengths would silently stop it holding a shipment in transit.

Sources: tests/parks/facilities.park; src/scenarios/slice_parks.cpp; src/sim/routes/networks.h — CONNECTION_REACH.
