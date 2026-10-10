# Implementation Plan: Inspectors

## Goal

Open the Inspector on benches and toilet blocks, and cut the guest's choice table to six fixed rows, one for each need, carrying on, and heading home.

## Approach

The Look tool's picking already meets bench and toilet block boxes, and only inspectorSubject turns them away. SubjectKind gains Bench and ToiletBlock, inspectorSubject gives them for keys with a facilityRecord, and inspectSubject builds their rows from that record. choiceRows folds guestOptions into six rows: for each need, the highest-scoring offer of that need and the sum of its offers' probabilities, then carrying on and heading home. The Inspector window draws the same table with renamed columns.

## Placement

Decision 0027 places each behavior this feature adds:

- Picking benches and toilet blocks as subjects and building their rows: legible, inspect.h. It already turns keys into subjects and records into rows for guests and shops.
- Folding a guest's options into one row for each need: legible, inspect.h, in choiceRows. It already turns guestOptions into the table's text.
- The table's column headings: app, inspector_window.h. It already draws the table.

## Tasks

### Task 1: Record the simulation before the change

Files: none

Main has changed since need-overlays (commit 15f2117), so record a fresh baseline. tpj_scenarios runs headless and opens no window.

Step 1: Build and record.

Run: `cmake.exe --build --preset windows-debug --target tpj_scenarios && mkdir -p build/inspectors && build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/inspectors/scenarios-before.txt; echo $?`
Expected: the build has no warnings, then `0`.

### Task 2: Update the legible spec

Files:
- Modify: `src/legible/SPEC.md:55-63` (Inspectors)

Step 1: Make the three edits FEATURE.md's Spec changes give for src/legible/SPEC.md: the first two sentences of the first paragraph, the guest's second bullet, and the new sentence after the shop's.

### Task 3: Update the app spec

Files:
- Modify: `src/app/SPEC.md:39` (Tools and the ghost)
- Modify: `src/app/SPEC.md:75` (Tooling UI)

Step 1: Make the two edits FEATURE.md's Spec changes give for src/app/SPEC.md.

### Task 4: Change the public interface

Files:
- Modify: `src/legible/inspect.h`

Step 1: Make the three changes FEATURE.md's public interface gives: SubjectKind's new values and comment, ChoiceRow's renamed fields and new comment, and inspectorSubject's comment. Leave the rest of the header as it is.

### Task 5: Inspect benches and toilet blocks

Files:
- Modify: `src/legible/inspect.cpp`

Step 1: Add `#include "sim/operations/facilities.h"`.

Step 2: In the anonymous namespace, add `std::vector<InspectorRow> facilityRows(const FacilityRecord &record)`, which gives three rows: "Places", "Taken", and "Queue", each its count from the record as a whole number.

Step 3: In inspectorSubject, after the shop check, when `facilityRecord(world, key)` gives a record, find the box with that key in `parkBoxes(world)` and give `InspectorSubject{key, SubjectKind::Bench}` for a BoxKind::Bench and `SubjectKind::ToiletBlock` for a BoxKind::ToiletBlock.

Step 4: In inspectSubject, before the shop's lines, handle Bench and ToiletBlock. The title is `capitalized(displayName(BoxKind::Bench))` or `capitalized(displayName(BoxKind::ToiletBlock))`, a space, and the key, which gives "Bench 5" or "Toilet block 7". Gone is true when facilityRecord gives no record. Otherwise the rows are facilityRows of the record. Then return.

Don't build yet. choiceRows still sets Option and Need, which Task 4 renamed, so tpj_legible builds again after Task 6.

### Task 6: Fold the choice table into six rows

Files:
- Modify: `src/legible/inspect.cpp:76-101` (choiceRows)

Step 1: Move capitalized, with its comment, above choiceRows, because choiceRows now uses it.

Step 2: Rewrite choiceRows to give six rows in this order.

- For each need in NEEDS, the row's Choice is `capitalized(displayName(static_cast<HomeReason>(need)))`. HomeReason lists the needs in NEEDS order, so this gives "Hunger", "Thirst", "Tiredness", and "Toilet". Among the options of kind Offer with that Need, take the one with the highest Score, the first one on a tie. Its Target is `boxText(world, offer.Facility)`, and Relief, Distance, Wait, Commitment, and Score are its values with two decimals. Probability is the sum of the Probability of every Offer option with that Need, with two decimals. If there is no such option, Target is "no offer" and every other cell is empty.
- Carrying on's row: Choice is `capitalized(displayName(ChoiceKind::CarryOn))`, which gives "Carry on". Score and Probability are the CarryOn option's, with two decimals.
- Heading home's row: Choice is `capitalized(displayName(ChoiceKind::HeadHome))`, which gives "Head home". When a HeadHome option exists, Target is "home for " and `displayName(option.Reason)`, and Commitment, Score, and Probability are its values with two decimals. Otherwise Target is "no route home" and every other cell is empty.

Update the comment above choiceRows to: "The guest's choices as six rows: each need's best offer and the chance of any offer of it, then carrying on and heading home."

Step 3: Build.

Run: `cmake.exe --build --preset windows-debug --target tpj_legible`
Expected: builds with no warnings.

### Task 7: Draw the renamed columns

Files:
- Modify: `src/app/ui/inspector_window.cpp:25-46` (drawChoices)
- Modify: `src/app/ui/inspector_window.h:25-29` (drawInspector's comment)
- Modify: `src/app/scene/scene_uploads.h:17-21` (uploadPreviewMeshes's comment)

Step 1: Change the headings to `"Choice", "Target", "Relief", "Distance", "Wait", "Commitment", "Score", "Chance"`, and the cells to `&choice.Choice, &choice.Target`, then the other six as they are. Change the comment above drawChoices to "The guest's choices, one to a row." Change the text "Options now" to "Choices now".

Step 2: In src/app/ui/inspector_window.h, in drawInspector's comment, change "and the options a guest would weigh now as a table, one to a row" to "and a guest's choices as a table, one to a row".

Step 3: In src/app/scene/scene_uploads.h, in uploadPreviewMeshes's comment, change "its inspected guest or shop's mark" to "its inspected guest's or box's mark".

Step 4: Build.

Run: `cmake.exe --build --preset windows-debug`
Expected: builds with no warnings.

### Task 8: Play the criteria

Files: none

Step 1: Build the release.

Run: `cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

Step 2: Play criteria 1 and 2 on `build/windows-release/ThemeParkJones.exe --park tests/parks/noon.park`. With the Look tool, click the bench and the toilet block at 1x, and watch Taken and Queue change as guests use them. Click a depot and an entrance, and see the Inspector stay. Remove the bench and see "No longer in the park". Click a guest and check the six rows, a "no offer" row if one shows, and that the chances add to 1.00 give or take rounding.

Step 3: Play criterion 3 on `build/windows-release/ThemeParkJones.exe --park tests/parks/stress/full.park`. Click a guest and see six rows.

### Task 9: Confirm the checks

Files: none

Step 1: Run the full checks.

Run: `cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/tidy.sh`
Expected: no warnings, every test passes, and tidy reports no findings.

Step 2: Compare the simulation.

Run: `build/windows-debug/tpj_scenarios.exe --ticks 300 tests/parks/day.park tests/parks/noon.park tests/parks/evening.park > build/inspectors/scenarios-after.txt && build/windows-debug/tpj_scenarios.exe --compare build/inspectors/scenarios-before.txt build/inspectors/scenarios-after.txt; echo $?`
Expected: `0`

The feature changes no per-tick or per-frame cost of the stress parks, so it runs no runtime report.

### Task 10: Commit

Files: every file above, and this feature's plan files.

Step 1: Commit with the commit-hygiene skill, staging only this feature's paths. Subject: `Legible: Add facility inspectors and a six-row choice table`.
