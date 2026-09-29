# operations

Operations: the shops and depots that keep the park running behind what the player sees (principle 7). It is part of tpj_sim and follows its contract (src/sim/SPEC.md). Its shops and depots meet only through the medium, the flow ledger and route distance, and neither reads the other's state (principles 3 and 6).

## Registration

addOperations registers the flow kinds SupplyOrders, named supply-orders, Supplies, named supplies, GuestVisits, named guest-visits, and Meals, named meals, in that order, and then the systems stepShops and stepDepots, in that order, so shops step before depots. makeParkSchema calls it after addRoutes. GuestVisits and Meals are declared in operations.h for believable-guests, which sends visits and receives meals. The module registers no component type: everything a shop or depot knows between cycles is in the ledgers, intent, and the fields.

## Supply routes

Every shop box of parkBoxes is a shop and every depot box a depot. supplyRouteLength(world, at, source) is the least Distance among the entries of the source that sampleField of backstage-route-distance on parkNetwork(world, Backstage) gives at the nodePlace of each node anchored to at, or none when there is none. nearestDepot(world, shop) is, among the depot boxes with a supplyRouteLength at the shop, the one with the least, ties to the lower key, as a DepotRoute of its key and that length, or none. A shop with a nearest depot is supplied, and one without is starved: a shop with no backstage connector, or whose connector reaches no depot, is placed and simply gets no supplies (principle 5).

shipmentDelay(distance) is a shipment's delay in ticks: ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS)), computed in that order, with SUPPLY_SPEED 2 m/s. A result below 1, or a NaN, gives 1, and a result above the largest uint32_t gives the largest uint32_t.

## Orders and supplies

A shop's orders, and the supplies shipped for them, carry the shop's key as their handle, so the units addressed to it are what it has on order. inventoryPosition(world, shop) adds, from addressedTo with the shop's key, the supplies stocks the shop holds, the supplies packets whose destination is the shop, the supply-orders packets whose destination is a depot box, and the supply-orders stocks held by depot boxes. So orders on their way to a depot, or held by one, count, and orders on their way back, back at the shop, or held by or heading to a deleted depot do not. The shop reorders from another depot as soon as its depot is deleted. Orders to a depot it can no longer reach still count until that depot sends them back, since a route restored before they arrive fills them, so the position never exceeds ORDER_UP_TO.

Each cycle, stepShops takes each shop in ascending key order. The shop consumes every order unit it holds under its own handle with the cause unfilled. Then, when it has a nearest depot and its inventoryPosition is REORDER_POINT, 8, or below, it creates ORDER_UP_TO, 24, minus that position order units under its own handle, and sends them to the nearest depot with its own handle and the delay ORDER_DELAY, 30 ticks. A starved shop sends no orders.

Then stepDepots takes each depot in ascending key order. The depot consumes every supplies unit it holds, which only a returned shipment gives it, with the cause returned. Then, for each handle it holds order units under, in ascending order, it acts on all of them. When the handle names no shop box, it consumes them with the cause cancelled. When supplyRouteLength(world, depot, handle) is none, it sends them back to the handle with that handle and the delay ORDER_DELAY. Otherwise it consumes them with the cause fulfilled, creates as many supplies under the handle, and sends them to the handle with that handle and the delay shipmentDelay of that length. A depot is an unlimited source, and its supplies are created as it ships.

Every ledger operation changes only its own endpoint's stock, and shops step before depots, so each decision is a function of the world as it stood between cycles: its ledgers, intent, and fields. An order sent while stepping tick t reaches its depot's stock at t + ORDER_DELAY, the depot ships it while stepping that tick, and the supplies reach the shop's stock at t + ORDER_DELAY plus the shipment's delay.

The ledger settles what the systems leave. A deleted depot's held orders go to their shop, and orders in transit to it return to their shop, which consumes both as unfilled. A deleted shop's supplies are discarded, and shipments in transit to it return to their depot, which consumes them as returned, or are consumed as undeliverable when the depot is gone too. So every consumed order unit was consumed as fulfilled, unfilled, cancelled, undeliverable, or discarded, every consumed supplies unit as returned, undeliverable, or discarded, and the supplies created equal the orders consumed as fulfilled.
