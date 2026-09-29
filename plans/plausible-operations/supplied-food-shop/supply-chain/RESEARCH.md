# Research: supply-chain

## How does a shop know what it has on order without reading its depot?

In inventory control, the (s, S) policy compares the reorder point with the inventory position: stock on hand, plus what is on order, minus backorders. What is on order includes the orders the supplier has not shipped yet as well as the shipments in transit. A shop that counts only its stock orders the same shortfall again every review until the shipment lands.

The ledger already gives each unit a handle, the key of the entity the unit is addressed to. The ledger uses that handle to send a removed endpoint's units on. If a shop's orders and the supplies shipped for them both carry the shop's key as their handle, then everything the shop has on order is exactly the units addressed to it. That covers orders in transit to a depot, orders a depot holds, and supplies in transit to the shop. A query listing a kind's packets and stocks by handle lets the shop count its position without reading a depot's state, and without keeping a counter of its own that a save would have to carry. Only orders heading to or held by a depot box count. Orders heading back to the shop, sitting in its stock as returned orders, or held by or heading to a deleted depot will not become supplies, so the shop reorders at once. Orders to a depot the shop can no longer reach still count until the depot sends them back, which takes up to ORDER_DELAY plus one cycle. A route restored before they arrive would still fill them, and counting them keeps the position from ever exceeding ORDER_UP_TO.

Handles also settle deletions without new rules. When a depot is deleted, the ledger sends the orders it holds to their handle, the shop. When a shop is deleted, its supplies have a handle that names no live entity, so the ledger discards them.

Rejected: a counter of outstanding orders in the shop's own state, updated from what it sees arrive. It duplicates what the ledger already knows, needs state that saves must carry, and has to infer shipments from changes in its stock. Rejected too: asking each depot box for the stock it holds under the shop's handle. It reads the same ledger entries that addressedTo gives, since stocks under a handle are the shared medium and not a depot's internals, but the shop would have to enumerate every depot to find them.

Sources: https://smartcorp.com/inventory-control/inventory-control-policies-software/, on the (s, S) policy and the inventory position; https://en.wikipedia.org/wiki/Reorder_point, on reordering against stock plus orders outstanding.

## In what order should shops and depots step?

Every ledger operation changes only the acting endpoint's stock, and packets it sends arrive no earlier than the next swap. Shops step before depots. So a shop decides from the world as it stood between cycles: its units addressed to it, the fields, and intent. A depot decides from the orders it held then, which no shop's step changes, since shops create packets rather than changing a depot's stock. So both decisions are functions of the world between cycles, which tests can observe with stepWorld alone. The lead time from an order to its shipment's arrival is exactly ORDER_DELAY plus the shipment's delay.

Rejected: depots first. A shop would then see orders a depot sent back earlier in the same cycle, so its decision would depend on another entity's step within the cycle, and a test could not compute it from the world it observes.

Sources: none; this follows from src/sim/SPEC.md's cycle and src/sim/medium/SPEC.md's ledger rules.
