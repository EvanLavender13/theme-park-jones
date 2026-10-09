# Feature: Shop Stock

## Summary

Each shop gets a stock setting: food, drink, or both, which says what it sells. The specs call it the stock setting, since a shop's stock already means the units it holds. The player sets it from the shop's Inspector with Food, Drink, and Both radio buttons and an Apply button. A choice not yet applied is a tentative edit, which the preview resolves on a candidate as it does a tool's edit. Apply commits it with a new SetShopStock command. The stock is saved park intent, undo and redo cover it, and a shop with no setting stocks food, so every existing park keeps its shops as they are. Nothing reads the stock yet. plausible-operations' served-needs makes shops sell by it.

## Acceptance criteria

1. When the Inspector shows a shop, it has a Sells row of radio buttons, Food, Drink, and Both, with the shop's stock setting marked, and an Apply button. Choosing another marks it and enables Apply. Apply commits it, and the Inspector keeps the new setting marked, with Apply disabled once the setting applies. Picking another subject, closing the Inspector, or opening or starting a park drops a choice not yet applied. (play)
2. A choice not yet applied is a tentative edit, a SetShopStock of the shop to the chosen stock, while the tool gives no edit of its own. The preview resolves it on a candidate as it does any edit (legible/SPEC.md, Previews). (review, and the park world checks' chosen edit)
3. SetShopStock sets a shop's stock setting and changes nothing else. It is refused unless its key holds a shop and its setting is food, drink, or both. A shop's stock setting is part of its box record, so undo and redo restore a change of it, Move box keeps it, and undoing a stocked shop's deletion brings the shop back with it. (play: set a shop to drink, undo, redo; move it; delete it and undo)
4. A shop with no stock setting stocks food, and parkBoxes gives it food. A shop set to food saves no shop-stock line, so every park saved before this feature loads, reads every shop as food, and saves again to its own text. The parks remade by `tpj_scenarios --slice-parks`, fed.park loaded and saved again among them, are the same text as before the feature, which shows the simulation unchanged over thousands of ticks. (the park world checks, and the remade parks compared with cmp)
5. tests/parks/facilities.park holds a shop set to drink and a shop set to both, so the park world checks cover the new state. (ctest)

## Tests

- The park world checks gain one chosen edit of warm.park, in tests/sim/park_world_checks_test.cpp: setting warm.park's shop, while guests wait at it and head to it, to sell drink with SetShopStock. Like the other chosen edits, its candidate must equal the world that commits it, and both are among the checked worlds.

## Medium

The feature adds no fields or flows. A shop's stock is park intent, which decision 0025 places outside principle 3.

- parkBoxes gives each box's stock setting. plausible-operations' served-needs will read it there. Nothing reads it in this feature.
- The app reads a shop's stock setting through parkBoxes for the Inspector and changes it only through SetShopStock.

## Spec changes

src/sim/park/SPEC.md:

- Boundary: "addParkIntent registers the intent types entrance, path-point, path-segment, and box." becomes "addParkIntent registers the intent types entrance, path-point, path-segment, box, and shop-stock." "addParkEdits registers the thirteen commands." becomes "addParkEdits registers the fourteen commands."
- Intent, after the box item: "- A shop has a stock setting: food, drink, or both, which says what it sells. A shop with no stock setting stocks food. parkBoxes gives each box's stock setting, food unless one is set."
- Intent, the save lines gain: "- a shop's stock setting, only when it is drink or both, in a shop-stock line;" and "Kinds are saved by name" gains the sentence "Stock settings are saved by name: food, drink, and both."
- Commands: "Thirteen commands" becomes "Fourteen commands", and the list gains SetShopStock after SetTaperLength. The paragraph naming SetCornerRadius's arguments gains "SetShopStock names a shop and a stock setting." RestoreIntent's box record becomes "a box's kind, pose, and stock setting". The acceptance paragraph gains "For SetShopStock, it means the key holds a box of kind shop and the setting is food, drink, or both." and RestoreIntent's "kinds are known" becomes "kinds and stock settings are known".
- Commands, the effects list: "MoveBox replaces a box's pose and keeps its key and kind." becomes "MoveBox replaces a box's pose and keeps its key, kind, and stock setting." It gains "- SetShopStock sets the shop's stock setting and changes nothing else. A shop set to food holds no shop-stock line, as if none had been set."
- "A ParkEdit holds any one of the thirteen commands." becomes "fourteen".

src/tools/SPEC.md, History: after "Each edit is the intent it changed: ..." add "A box's record holds its kind, pose, and stock setting."

src/app/SPEC.md, Tools and the ghost, after the paragraph on clicking with Look: "When the Inspector shows a shop, it has a Sells row of radio buttons, Food, Drink, and Both, with the shop's stock setting marked, and an Apply button. Choosing another setting marks it, and while the tool gives no edit of its own, the choice is the edit the ghost and the preview show, a SetShopStock. Apply commits it, and it applies at the next tick, or at once while paused. The choice stays marked until the shop has that setting, and Apply is disabled while the chosen setting is the shop's. Picking another subject, closing the Inspector, or replacing the world drops the choice."

Public interface:

- src/sim/park/intent.h: `enum class ShopStock : uint8_t { Food, Drink, Both };` with enumNames food, drink, and both. ParkBox gains `ShopStock Stock = ShopStock::Food;` after At. addParkIntent's comment names shop-stock.
- src/sim/park/edits.h: `struct SetShopStock { EntityKey Shop = NULL_KEY; ShopStock Stock = ShopStock::Food; };` with operator==, and isAccepted and applyCommand overloads for it. ParkEdit holds it between SetTaperLength and RestoreIntent.
- src/app/input/interaction.h: `stockChoice()`, `chooseStock(ShopStock)`, `applyStock(CommandQueue &)`, and `settleStock(const World &)` on Interaction.
- src/app/ui/inspector_window.h: drawInspector takes the shop's stock controls and returns what the player did.

## Files affected

- Modify: src/sim/park/intent.h
- Modify: src/sim/park/intent.cpp
- Modify: src/sim/park/internal/components.h
- Modify: src/sim/park/edits.h
- Modify: src/sim/park/edits.cpp
- Modify: src/sim/park/SPEC.md
- Modify: src/tools/SPEC.md
- Modify: src/app/input/interaction.h
- Modify: src/app/input/interaction.cpp
- Modify: src/app/ui/inspector_window.h
- Modify: src/app/ui/inspector_window.cpp
- Modify: src/app/ui/tooling_ui.cpp
- Modify: src/app/SPEC.md
- Modify: tests/parks/facilities.park
- Modify: tests/sim/park_world_checks_test.cpp (the test pass)
- Modify: plans/effortless-building/facilities-and-hours/MILESTONE.md (the Medium names the stock in parkBoxes)

## Dependencies

- benches-and-toilets: tests/parks/facilities.park. Merged.
- legible-simulation's Inspector, and the preview of a tentative edit. Met.

## Out of scope

- Selling by the stock, and a drink offer: plausible-operations' served-needs.
- Showing a shop's stock in the park itself, such as a sign or a color. The Inspector shows it.
- Stocks for kinds of shop beyond food and drink, and prices.
- Setting several shops' stock at once: the milestone's deepening candidates hold it.

## Open questions

None.
