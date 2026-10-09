# Research: shop-stock

## Where should a shop's stock live, and how do other modules read it?

Box intent is one component, saved as one line per box. The milestone keeps the stock in a component of its own, so every park saved before it loads unchanged. The open choices are how the stock reaches readers and undo, and when the component is held.

The history records boxes as ParkBox values, compares them field by field, and puts them back through RestoreIntent's box records. If ParkBox carries the stock, the history notes a stock change as a changed box, an undo sets it back, and undoing a deletion brings the stock back with the box, all with no change to the history's code. parkBoxes is already how routes, operations, render, and the scene read boxes, so served-needs can read the stock there too.

The component is held only when the stock is drink or both. Setting a shop back to food removes it. This keeps one form for each state: a shop set to drink and back to food is the same world as one never set, and saves to the same text. Undo then gives back exactly the world before the edit, and every park saved before the feature saves again with no new section.

Rejected: a separate query for the stock, such as shopStock(world, key) — the history and RestoreIntent would need their own stock records beside the box records, and an undo of a deletion would have to restore two records for one box. A component held for every shop, even at food — a park saved before the feature would save again with a new section, and setting a shop to drink and back would leave a world unequal to the one before. Restricting the stock to shops in RestoreIntent — history records come only from real boxes, so a non-shop record never holds anything but food, and the extra rule would check nothing play can reach.

## How should the Inspector hold a choice before it is applied?

The ghost and the preview come from one tentative edit each frame, which today is the tool's. The Inspector's choice is a second source of a tentative edit. The interaction already holds the tool and the Inspector's subject, so it can hold the chosen stock and give its SetShopStock as the tentative edit when the tool gives none. With the Look tool, which is the only tool that picks a subject, the tool never gives an edit. The choice belongs to its subject, so picking another subject, closing the Inspector, or replacing the park drops it.

Rejected: applying the stock as soon as a radio button is pressed — the slice previews every policy change before it is committed (principle 8). A choice the Inspector window keeps itself — the window is drawn after the frame's preview is made, so the preview could not see it without reaching into the window.

Sources: src/tools/history.cpp — how edits are recorded and reversed; src/sim/park/edits.cpp — RestoreIntent's box records; src/app/application.cpp — the order of tentative edit, preview, and panels in a frame.
