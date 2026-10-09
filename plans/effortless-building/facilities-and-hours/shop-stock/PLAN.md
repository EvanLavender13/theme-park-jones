# Implementation Plan: Shop Stock

## Goal

Each shop has a saved stock of food, drink, or both, which the player previews, applies, undoes, and redoes from the shop's Inspector, with every existing park unchanged.

## Approach

A new private component, shop-stock, holds a shop's stock only when it is drink or both, so a shop at food has none and every older park saves as before. ParkBox carries the stock, which brings it into the history's box records and RestoreIntent with no change to the history's code. A new command, SetShopStock, sets it. The app's Interaction holds the Inspector's chosen stock and gives it as the tentative edit while the tool gives none, and the Inspector draws the radio buttons and Apply. This feature's RESEARCH.md gives the reasons.

## Placement

Decision 0027 places each behavior this feature adds:

- A shop's stock as intent, its save, and its reading through parkBoxes: sim/park, src/sim/park/intent.h with intent.cpp and internal/components.h. It owns box intent and its private components.
- SetShopStock, and RestoreIntent's stock: sim/park, src/sim/park/edits.h with edits.cpp, which owns every command that changes intent.
- The Inspector's chosen stock and its tentative edit: app input, src/app/input/interaction.h with interaction.cpp. It already holds the tool and the Inspector's subject, and gives the frame's tentative edit.
- The Sells radio buttons and Apply: app UI, src/app/ui/inspector_window.h with inspector_window.cpp, which draws the Inspector.
- Passing the shop's stock to the Inspector and the player's choice back to the interaction: app UI, src/app/ui/tooling_ui.cpp, which already builds the Inspector each frame. It adds calls only.
- A park that holds stocks: tests/parks/facilities.park, which the park world checks open.

## Tasks

### Task 1: Describe the stock in the specs

Files:
- Modify: `src/sim/park/SPEC.md` (Boundary, Intent, Commands, ParkEdit)
- Modify: `src/tools/SPEC.md` (History)
- Modify: `src/app/SPEC.md` (Tools and the ghost)
- Modify: `plans/effortless-building/facilities-and-hours/MILESTONE.md` (Medium)

Step 1: Make each edit FEATURE.md's Spec changes gives.

Step 2: In MILESTONE.md's Medium, change "parkBoxes, with the new kinds, the shop stock query, and the hours query" to "parkBoxes, with the new kinds and each box's stock, and the hours query".

### Task 2: Declare the stock and SetShopStock

Files:
- Modify: `src/sim/park/intent.h:39-43` (after BoxKind's enumNames), `src/sim/park/intent.h:145-152` (ParkBox), and the addParkIntent comment
- Modify: `src/sim/park/edits.h` (after SetTaperLength, addParkEdits' comment, ParkEdit, isAccepted, applyCommand)
- Modify: `src/sim/park/edits.cpp` (stub definitions)

Step 1: In intent.h, add after BoxKind's enumNames:

    // What a shop sells.
    enum class ShopStock : uint8_t { Food, Drink, Both };

with enumNames giving food, drink, and both. Give ParkBox a last member `ShopStock Stock = ShopStock::Food;`, and change its comment to "A box's intent, as other modules read it: its kind, its pose, and its stock, which only a shop's sets." Change addParkIntent's comment to name entrance, path-point, path-segment, box, and shop-stock.

Step 2: In edits.h, after SetTaperLength, add:

    // The shop's stock: what it sells.
    struct SetShopStock {
      EntityKey Shop = NULL_KEY;
      ShopStock Stock = ShopStock::Food;

      bool operator==(const SetShopStock &) const = default;
    };

Add SetShopStock to ParkEdit between SetTaperLength and RestoreIntent, and to addParkEdits' comment in the same place. Change ParkEdit's comment to "one of the fourteen commands". Declare `bool isAccepted(const World &world, const SetShopStock &command);` and `void applyCommand(World &world, const SetShopStock &command);` after SetTaperLength's.

Step 3: In edits.cpp, define both as stubs: isAccepted returns false, and applyCommand does nothing. Add `std::optional<EditPlan> planOf(const World & /*world*/, const SetShopStock & /*command*/)` returning std::nullopt, after SetTaperLength's planOf, and register the command in addParkEdits after SetTaperLength.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds with no warnings.

### Task 3: Write the test

Files: none by hand.

Step 1: Dispatch the test-writer as implementing-features says, with FEATURE.md, src/sim/park/SPEC.md and src/tools/SPEC.md, and the public headers src/sim/park/intent.h and src/sim/park/edits.h.

Step 2: Build and run the world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: the new chosen edit fails because the stub refuses it, and nothing fails to build.

### Task 4: Hold the stock as intent

Files:
- Modify: `src/sim/park/internal/components.h` (after BoxIntent's visitFields)
- Modify: `src/sim/park/intent.cpp:27-32` (addParkIntent), `src/sim/park/intent.cpp:49-52` (parkBoxes)

Step 1: In components.h, add after BoxIntent's visitFields:

    // A shop's stock, held only when it is not food.
    struct ShopStockIntent {
      ShopStock Stock = ShopStock::Food;

      bool operator==(const ShopStockIntent &) const = default;
    };

with visitFields writing one field, "stock".

Step 2: In addParkIntent, register `ShopStockIntent` as "shop-stock", DataKind::Intent, after box.

Step 3: In parkBoxes, give each box the stock its entity's ShopStockIntent holds, or ShopStock::Food when it holds none.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: the build succeeds with no warnings.

### Task 5: Apply SetShopStock and restore stocks

Files:
- Modify: `src/sim/park/edits.cpp` (a helper in the anonymous namespace, isBoxValid, isAccepted and applyCommand for SetShopStock, applyCommand for RestoreIntent)

Step 1: Add a helper in the anonymous namespace, `void setStock(World &world, EntityKey box, ShopStock stock)`, with the comment "Gives the box the stock: removes its ShopStockIntent for food, so a shop at food saves as one never set, and holds the stock otherwise." It removes the entity's ShopStockIntent when the stock is Food, and otherwise emplaces or replaces it.

Step 2: isAccepted for SetShopStock: true when findIntent<BoxIntent> finds a box at command.Shop whose Kind is BoxKind::Shop, and isKnown(command.Stock). applyCommand: when accepted, setStock(world, command.Shop, command.Stock).

Step 3: In isBoxValid, also require isKnown(box.Stock).

Step 4: In applyCommand for RestoreIntent, call setStock for each record in Set.Boxes with its key, after setting its BoxIntent, and for each record in Add.Boxes with the key just created for it.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 6: Hold the Inspector's choice

Files:
- Modify: `src/app/input/interaction.h`
- Modify: `src/app/input/interaction.cpp`

Step 1: Give Interaction a private `std::optional<ShopStock> StockChoice;`, and change the class comment to "The player's hold on the park: the tool, the guest or shop the Inspector shows, and the stock chosen for that shop and not yet applied. It follows the park session's generation, so a replaced world drops the tool's hold, the subject, and the choice."

Step 2: Add these public members, each with its comment:

- `[[nodiscard]] const std::optional<ShopStock> &stockChoice() const { return StockChoice; }` — "The stock chosen in the Inspector and not yet applied."
- `void chooseStock(ShopStock stock);` — "Chooses the stock for the Inspector's shop. It is applied only by applyStock."
- `void applyStock(CommandQueue &commands);` — "Queues a SetShopStock of the subject to the chosen stock, when the subject is a shop and a stock is chosen. The choice stays until settleStock finds the shop has it."
- `void settleStock(const World &world);` — "Drops the chosen stock once the Inspector's shop has it, so the choice stays marked until its SetShopStock applies."

Step 3: In interaction.cpp, follow and forgetSubject drop the choice, and pick drops it when the subject it leaves differs from the one before. Move forgetSubject's body into interaction.cpp.

Step 4: tentativeEdit gives the tool's edit when it has one. Otherwise, when the subject is a shop, a stock is chosen, and parkBoxes gives the subject's box with another stock, it gives SetShopStock{subject, choice}. Otherwise none. Change its declaration's comment to "The tool's edit, or else the Inspector's chosen stock as a SetShopStock while it differs from the shop's."

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 7: Draw Sells and Apply

Files:
- Modify: `src/app/ui/inspector_window.h`
- Modify: `src/app/ui/inspector_window.cpp`
- Modify: `src/app/ui/tooling_ui.cpp:68-71`
- Modify: `src/app/ui/tooling_ui.h` (build's comment)

Step 1: In inspector_window.h, add:

    // The Inspector's stock controls for a shop: the shop's stock, and the one marked.
    struct StockControls {
      ShopStock Current = ShopStock::Food;
      ShopStock Chosen = ShopStock::Food;
    };

    // What the player did in the Inspector: whether it is still open, the stock they chose this
    // frame, and whether they pressed Apply.
    struct InspectorUse {
      bool Open = true;
      std::optional<ShopStock> Chosen;
      bool Apply = false;
    };

Change drawInspector to `InspectorUse drawInspector(const Inspection &inspection, const std::optional<StockControls> &stock);`, and add to its comment: "With stock controls, and while the entity is there, it also draws a Sells row of radio buttons, Food, Drink, and Both, with Chosen marked, and an Apply button, disabled while Chosen is Current."

Step 2: In inspector_window.cpp, after the rows and before the choices, draw the controls as the comment says, on one line: the text "Sells", then the three radio buttons on the same line, then Apply on the same line. A press of a radio button other than the marked one sets Chosen, and a press of Apply sets Apply.

Step 3: In tooling_ui.cpp, call interaction.settleStock(world) before drawing the Inspector. Then, when the interaction has a subject: if it is a shop and parkBoxes gives its box, make StockControls with Current the box's stock and Chosen the interaction's stockChoice, or Current when none. Call drawInspector with them, or with none otherwise. Forget the subject when Open is false. Otherwise call chooseStock with Chosen when set, and applyStock with session.commands() when Apply is set. Add to ToolingUi::build's comment in tooling_ui.h: "The Inspector's stock controls set the interaction's chosen stock and apply it."

Run: `cmake.exe --build --preset windows-debug --target tpj_app`
Expected: the build succeeds with no warnings.

### Task 8: Add stocked shops to facilities.park

Files:
- Modify: `tests/parks/facilities.park`

Step 1: Add two shops along the guest segment, clear of the bench, which spans z 112 to 114 on the east, and the toilet block, which spans z 109 to 117 on the west. Each faces the path with its front face 1 m off the paving. Set next-key to 9, add the two box lines after the toilet block's, and add a shop-stock section after the box section:

    7 kind=shop x=5.5 z=107.5 facing-x=-1 facing-z=0
    8 kind=shop x=-5.5 z=104.5 facing-x=1 facing-z=0

    [shop-stock]
    7 stock=drink
    8 stock=both

Step 2: Run the world checks.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 9: Confirm the remade parks are unchanged

Files:
- Create, scratch: `build/shop-stock/fed.park`, `warm.park`, `cut.park`, `warm-main-street.park`

Step 1: Remake the slice parks with this feature's tpj_scenarios into a scratch directory, from a scratch copy of fed.park, which it writes back as loaded and saved again, and compare them all with the checked-in ones.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/shop-stock && cp tests/parks/fed.park build/shop-stock/fed.park && cd build/shop-stock && ../windows-debug/tpj_scenarios.exe --slice-parks fed.park warm.park cut.park ../../tests/parks/main-street.park warm-main-street.park && for f in fed warm cut warm-main-street; do cmp $f.park ../../tests/parks/$f.park && echo "$f same"; done; cd ../..`
Expected: `fed same`, `warm same`, `cut same`, and `warm-main-street same`.

### Task 10: Play the stock

Files: none.

Step 1: Play warm-main-street.park.

Run: `build/windows-debug/ThemeParkJones.exe --park tests/parks/warm-main-street.park`
Expected: With Look, clicking a shop shows Sells with Food marked and Apply disabled. Choosing Drink marks it and enables Apply. Apply keeps Drink marked, with no flicker back to Food at 1x, and Apply is disabled once it applies. Ctrl+Z puts Food back and Ctrl+Y Drink again. Moving the shop keeps Drink. Deleting it and pressing Ctrl+Z brings it back, and clicking it shows Drink. Choosing Both and then clicking another shop drops the choice: going back to the first shop shows Drink. Pause with Space and repeat the Apply, which shows at once. Saving the park and opening the file shows each shop's stock as it was. Report what was seen, and leave the full check to the user's play.

### Task 11: Confirm the feature

Files: none.

Step 1: Run the full build, the tests, and tidy.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: the build has no warnings, ctest reports 100% tests passed, and tidy reports `tidy: clean.`

### Task 12: Commit and merge

Files: none.

Step 1: Stage only this feature's paths: src/sim/park/, src/tools/SPEC.md, src/app/input/interaction.h, src/app/input/interaction.cpp, src/app/ui/inspector_window.h, src/app/ui/inspector_window.cpp, src/app/ui/tooling_ui.h, src/app/ui/tooling_ui.cpp, src/app/SPEC.md, tests/parks/facilities.park, tests/sim/park_world_checks_test.cpp, plans/effortless-building/facilities-and-hours/MILESTONE.md, and plans/effortless-building/facilities-and-hours/shop-stock/. Commit them by pathspec with the commit-hygiene skill, subject `Sim: Let each shop stock food, drink, or both`. If the format hook rewraps a file after the commit, add it and amend the commit before merging. Merge into main as implementing-features says, and rebuild windows-release.
