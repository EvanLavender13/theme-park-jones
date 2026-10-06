# Implementation Plan: Undo Redo

## Goal

Let the player undo and redo every committed edit with Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z, and two panel buttons, through a new RestoreIntent command and a history in the tools.

## Approach

sim/park gains RestoreIntent, which removes, sets, and adds path points, segments, and boxes. It checks validity the way the path commands do, on a copy of the path layout. The tools gain an EditHistory. After each tick that applied commands, it compares the published intent with what it saw last, and records the difference as an edit. An undo or redo gives the RestoreIntent that reverses an edit. The app's ParkSession applies that command alone in its tick, then lets the history pair the records it added with the keys the tick gave them.

## Placement

Decision 0027 places each behavior this feature adds:

- RestoreIntent, its acceptance, and its application: sim/park, src/sim/park/edits.h and edits.cpp. They own every intent command and the validity check it reuses.
- Recording edits as intent changes, building the reversing command, and renaming restored keys: tools, a new component src/tools/history.h and history.cpp. The milestone places the undo history in tpj_tools as session state. It is one concern apart from the tool state machine, as snapping and frontage are, so it gets its own file. It has its own public header because the app's session uses it and the tools' state machine doesn't.
- Holding the history, applying an undo or redo alone in its tick, noting each tick's edits, and clearing on New and Open: app/session, ParkSession in park_session.h. It is the one place the world is stepped and replaced, and it owns the command queue.
- Dropping a held press or drawn path instead of undoing: app/input, Interaction in interaction.h. It owns the tool state.
- Reading Ctrl+Z, Ctrl+Y, and Ctrl+Shift+Z: app/input, input_map.h, which maps every other key and event.
- The Undo and Redo buttons, and acting on them or the keys: app/ui, tool_panel.h and tooling_ui.h. The panel draws the buttons, as it draws the park buttons. ToolingUi already acts on the panel's choices, and it is the lowest layer that sees both the Interaction and the ParkSession.
- application.cpp passes the session and the frame's keys to ToolingUi::build. This is composition only.

## Tasks

### Task 1: Save the scenarios baseline

Before any change, with the windows-debug build of main at d8734d7 or later in build/windows-debug.

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null > build/undo-base.txt; wc -l < build/undo-base.txt`
Expected: `15336`

### Task 2: Update the specs

Files:
- Modify: `src/sim/park/SPEC.md`, `src/tools/SPEC.md`, `src/app/SPEC.md`
- Modify: `plans/effortless-building/path-network/MILESTONE.md`

Step 1: Make each change listed under Spec changes in FEATURE.md, with that wording.

Step 2: In MILESTONE.md's Deepening candidates, add two items at the end:

- "Undo while a path is half drawn removes only its last stop, as Backspace does in Inkscape's pen tool."
- "RestoreIntent among tpj_scenarios' random edits, so the scenarios try it on worlds no undo would reach."

### Task 3: Declare the command

Files:
- Modify: `src/sim/park/edits.h`
- Modify: `src/sim/park/edits.cpp`

Step 1: In edits.h, after SetTaperLength, add:

```cpp
// A path segment's intent as RestoreIntent puts it: a ParkSegment without its ground line.
struct SegmentRecord {
  EntityKey Key = NULL_KEY;
  PathKind Kind = PathKind::Guest;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  double Width = 0.0;
  double FromEdge = DEFAULT_EDGE_RADIUS;
  double ToEdge = DEFAULT_EDGE_RADIUS;

  bool operator==(const SegmentRecord &) const = default;
};

// Path points, segments, and boxes, each list in ascending key order unless its user says
// otherwise.
struct IntentRecords {
  std::vector<ParkPathPoint> Points;
  std::vector<SegmentRecord> Segments;
  std::vector<ParkBox> Boxes;

  bool operator==(const IntentRecords &) const = default;
};

// Intent put back as an undo or redo finds it. The path points, segments, and boxes Remove names
// go. Each record in Set replaces the values of the one holding its key. Each record in Add is
// added with a key from the counter, in the order listed, points first, then segments, then boxes.
// Add's keys are names only: a segment that names one as From or To ends at the point added under
// it.
struct RestoreIntent {
  std::vector<EntityKey> Remove;
  IntentRecords Set;
  IntentRecords Add;

  bool operator==(const RestoreIntent &) const = default;
};
```

Step 2: Update addParkEdits's comment to list RestoreIntent last. Add RestoreIntent at the end of the ParkEdit variant, and change its comment to "one of the thirteen commands". Declare `bool isAccepted(const World &world, const RestoreIntent &command);` after the SetTaperLength one, and `void applyCommand(World &world, const RestoreIntent &command);` after its SetTaperLength one. Update the comment on editedSegments and editedSurface to say they give none for RestoreIntent, which shows no ghost.

Step 3: In edits.cpp:

- Add `schema.addCommand<RestoreIntent>();` last in addParkEdits.
- Add `std::optional<EditPlan> planOf(const World & /*world*/, const RestoreIntent & /*command*/) { return std::nullopt; }` after the SetTaperLength planOf, so editedOf gives nothing for it. Give it the comment that RestoreIntent has no plan and shows no ghost.
- Add stubs: isAccepted returns false, and applyCommand does nothing. Leave their parameters unnamed, because -Wextra makes an unused parameter an error.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim`
Expected: builds with no warnings.

### Task 4: Declare the history

Files:
- Create: `src/tools/history.h`
- Create: `src/tools/history.cpp`
- Modify: `src/tools/CMakeLists.txt`

Step 1: Create src/tools/history.h, guard `TPJ_TOOLS_HISTORY_H`, including sim/entity_key.h, sim/park/edits.h, sim/world.h, `<map>`, `<optional>`, `<stdint.h>`, and `<vector>`:

```cpp
// One edit as the history holds it: the path points, segments, and boxes it removed and added, and
// those it changed, as they were before it and after it, each list in ascending key order.
struct IntentChange {
  IntentRecords Removed;
  IntentRecords Added;
  IntentRecords Before;
  IntentRecords After;
};

// Which command the history waits to note: none, an undo's, or a redo's.
enum class HistoryStep : uint8_t { None, Undo, Redo };

// The edits to undo and to redo, each list oldest first. Change it only through clearHistory,
// noteEdits, undoEdit, and redoEdit.
struct EditHistory {
  // The world's path points, segments, and boxes when the history last looked.
  IntentRecords Seen;
  std::vector<IntentChange> Undos;
  std::vector<IntentChange> Redos;
  // While an undo's or redo's command waits to be noted: which it is, the edit it took off its
  // list, and the command.
  HistoryStep Waiting = HistoryStep::None;
  IntentChange Taken;
  RestoreIntent Sent;
  // Each key whose record an undo or redo added back, and the key it took.
  std::map<EntityKey, EntityKey> Renamed;
};

// Empties both lists and takes the world's intent as the intent the history saw last.
void clearHistory(EditHistory &history, const World &world);
// Records what changed in the world's intent since the history last looked, as tools/SPEC.md's
// History states. Call it after each cycle that applied commands.
void noteEdits(EditHistory &history, const World &world);
// The command that reverses the last edit on the undo list, or on the redo list. None when that
// list is empty or a command waits to be noted. Apply it alone in its cycle.
std::optional<RestoreIntent> undoEdit(EditHistory &history);
std::optional<RestoreIntent> redoEdit(EditHistory &history);
// Whether undoEdit or redoEdit would give a command.
bool canUndo(const EditHistory &history);
bool canRedo(const EditHistory &history);
```

Step 2: Create src/tools/history.cpp, including tools/history.h, with stubs: clearHistory and noteEdits do nothing, undoEdit and redoEdit return std::nullopt, and canUndo and canRedo return false. Leave the parameters unnamed.

Step 3: Add history.cpp to tpj_tools's sources in src/tools/CMakeLists.txt, between frontage.cpp and snapping.cpp.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 5: Run the test pass

Dispatch the test-writer as implementing-features describes, with FEATURE.md, src/sim/park/SPEC.md, and the public headers src/sim/park/edits.h and src/sim/park/intent.h. Its chosen edit fails until Task 6 is done, because the stub refuses RestoreIntent.

### Task 6: Accept and apply RestoreIntent

Files:
- Modify: `src/sim/park/edits.cpp`

Step 1: Split editedBy so it can compare any two layouts. Move its body after `applyPlan(store, plan);` into a new function `Edited editedBetween(const PathLayout &before, const PathLayout &after, std::vector<EntityKey> points, Wanted wanted)`. `points` holds extra points whose paving can change though no segment at them does. Add them to what pointsChanged gives, then sort and remove duplicates, before withFarEnds. editedBy builds before and after as today, and calls editedBetween with `{plan.Tapered}` when Tapered is not NULL_KEY, and with no points otherwise.

Step 2: Change piecesNear to take `const PathLayout &layout` instead of the world, and build its ground from that layout. Its callers pass `layoutOf(world)`.

Step 3: In the anonymous namespace before isAccepted, add the restore's helpers:

1. `bool namesValid(const World &world, const RestoreIntent &command)`. It is true when all of these hold:
    - Each key in Remove holds a PathPointIntent, PathSegmentIntent, or BoxIntent.
    - Each Set record's key holds intent of the record's type, and is not in Remove.
    - Each Add record's key holds no entity (world.findEntity gives entt::null), and no two Add records share a key.
    - Each Set or Add segment's kind is known, and its From and To differ. Each of From and To is an Add point's key, or holds a PathPointIntent and is not in Remove.
    - Each Set or Add box's kind is known.
    - Each segment the world holds that the command neither removes nor sets has From and To that the command doesn't remove.
    - Every number in Set and Add is finite. Every point's radius and taper, and every segment's FromEdge and ToEdge, is at least 0, and every Width is above 0.
2. `PathLayout restoredLayout(const World &world, const RestoreIntent &command, const PathLayout &before)`. Copy before. With a LayoutStore on the copy and world.nextKey(): destroy each Remove key, add each Add point in order and keep a map from its name to its new key, add each Add segment with From and To mapped through that map when they name an Add point, then set each Set point's and segment's PathPointIntent or PathSegmentIntent, mapping From and To the same way. Add segments come after Add points, so the layout's lists stay in ascending key order.
3. `std::vector<Solid> restoredSolids(const World &world, const RestoreIntent &command, uint64_t firstBoxKey)`. Start from solidsOf(world). Drop each solid whose key is in Remove. Replace the pose and size of each Set box's solid. Append each Add box's solid, keyed firstBoxKey, firstBoxKey + 1, and so on, so no two share a key.
4. `bool isRestoreAccepted(const World &world, const RestoreIntent &command)`:
    - It is false unless namesValid holds.
    - Make before = layoutOf(world) and after = restoredLayout. The first Add box key is world.nextKey() plus the counts of Add points and Add segments.
    - Find what changes with `editedBetween(before, after, points, Wanted::Pieces)`, where points holds the key of every Set point and every Add point's new key, in ascending order. The i-th Add point's new key is world.nextKey() plus i.
    - Every edited segment must have a ground line that stays in the park, and every edited piece must stay in the park and fit restoredSolids, as acceptedPlan checks.
    - Each Set and Add box's solid must exist and fit the restored solids and `piecesNear(after, solid)`. fits skips a solid with the same key, so build the Set box's solid with its own key, and the Add box's with its new key.

Step 4: Replace the stubs. isAccepted returns isRestoreAccepted. applyCommand returns at once unless isRestoreAccepted. Otherwise, with a WorldStore on the world, it destroys each Remove key, adds each Add point, then each Add segment with its points mapped, sets each Set point and segment, mapped the same way, then sets each Set box's BoxIntent, and adds each Add box with createEntity and a BoxIntent. That order takes the keys the spec gives: points, then segments, then boxes.

Run: `cmake.exe --build --preset windows-debug --target tpj_sim_tests && build/windows-debug/tpj_sim_tests.exe -# "[#park_world_checks_test]"`
Expected: `All tests passed`.

### Task 7: Record and reverse edits

Files:
- Modify: `src/tools/history.cpp`

Step 1: In an anonymous namespace, add:

1. `IntentRecords recordsOf(const World &world)`: parkPathPoints and parkBoxes as they come, and each ParkSegment of parkSegments as a SegmentRecord without its Line. All three are in key order.
2. `IntentChange changeBetween(const IntentRecords &was, const IntentRecords &is)`. For each of the three lists, walk both in key order. A key only in was goes in Removed, a key only in is goes in Added, and a key in both whose records differ goes in Before with was's record and in After with is's.
3. `bool isEmpty(const IntentChange &change)`: all twelve lists are empty.
4. `EntityKey current(const EditHistory &history, EntityKey key)`: follow Renamed from the key until a key has no entry.
5. `RestoreIntent reversal(const EditHistory &history, const IntentChange &change)`:
    - Remove holds current() of each key in change.Added's three lists.
    - Set holds change.Before's records with their keys through current().
    - Add holds change.Removed's records with their keys unchanged.
    - In every Set and Add segment, From and To stay unchanged when they name a point in change.Removed, and go through current() otherwise.
6. `void rename(EditHistory &history, const IntentChange &seen)`. For each of the three lists, pair the i-th record of Sent.Add with the i-th record of seen.Added, for as many as both have, and set Renamed[sent key] to the seen key.

Step 2: Implement the public functions:

- clearHistory resets the history to a default EditHistory and sets Seen to recordsOf(world).
- noteEdits reads `now = recordsOf(world)` and `change = changeBetween(Seen, now)`, then sets Seen to now. It then acts on Waiting:
    - If Waiting is None: when the change isn't empty, push it on Undos and clear Redos.
    - If Waiting is Undo or Redo and the change is empty: push Taken back on the list it came from.
    - Otherwise: call rename(history, change), and push the change on Redos for an undo, or on Undos for a redo.
    - In every case, set Waiting to None.
- undoEdit gives none when Waiting is not None or Undos is empty. Otherwise it moves Undos' last entry into Taken, sets Sent to reversal(history, Taken), sets Waiting to Undo, and gives Sent. redoEdit does the same with Redos and Redo.
- canUndo is `Waiting == None && !Undos.empty()`, and canRedo is the same for Redos.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools`
Expected: builds with no warnings.

### Task 8: Step undos alone in the session

Files:
- Modify: `src/app/session/park_session.h`
- Modify: `src/app/session/park_session.cpp`

Step 1: Include tools/history.h and `<optional>`. Give ParkSession two private members after Commands: `EditHistory History;` and `std::optional<RestoreIntent> Restore;`, the command waiting to apply alone.

Step 2: Add public members:

```cpp
  // Asks for an undo, or a redo: the history's command waits to apply alone in the next cycle.
  // Nothing happens while a command is queued or waits, or when there is nothing to undo or redo.
  void undo();
  void redo();
  // Whether undo or redo would do anything now.
  [[nodiscard]] bool canUndo() const;
  [[nodiscard]] bool canRedo() const;
```

Update step's comment: one cycle with the waiting undo or redo alone, when one waits, or else with the queued commands, which it empties. Either way, the history then notes what the cycle changed.

Step 3: Implement them:

- The constructor calls clearHistory(History, Current) after moving the world in.
- step: if Restore has a value, make a local CommandQueue, push *Restore onto it, call stepWorld(Current, thatQueue), reset Restore, and call noteEdits(History, Current). Otherwise, note whether Commands is empty, call stepWorld(Current, Commands), and call noteEdits only when Commands was not empty.
- undo: when Commands is empty and Restore has no value, set Restore to undoEdit(History). redo does the same with redoEdit.
- canUndo is `Commands.empty() && !Restore && tpj::canUndo(History)`, and canRedo the same with canRedo.
- replace resets Restore and calls clearHistory(History, Current) after replacing the world.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: builds with no warnings.

### Task 9: Read the keys and drop held edits

Files:
- Modify: `src/app/input/input_map.h`, `src/app/input/input_map.cpp`
- Modify: `src/app/input/platform_input.cpp`
- Modify: `src/app/input/interaction.h`, `src/app/input/interaction.cpp`

Step 1: In input_map.h, add before FrameInput:

```cpp
// What the player asked of the edit history this frame.
enum class HistoryAction : uint8_t { None, Undo, Redo };
```

Include `<stdint.h>`. Give FrameInput a member `HistoryAction History = HistoryAction::None;` after SnapsOff, commented "Whether the player pressed Ctrl+Z to undo, or Ctrl+Y or Ctrl+Shift+Z to redo."

Step 2: Give mapEvent a `bool keyboardWanted` parameter after mouseWanted, and extend its comment: while ImGui doesn't want the keyboard, a key press of Z with Ctrl held asks to undo, or to redo with Shift held too, and a key press of Y with Ctrl held asks to redo. In mapEvent, before the `!mouseWanted` branch, add a branch for `event.type == SDL_EVENT_KEY_DOWN && !keyboardWanted && (event.key.mod & SDL_KMOD_CTRL) != 0`. In it, set History to Redo for `event.key.key == SDLK_Y`, and for SDLK_Z set it to Redo when `(event.key.mod & SDL_KMOD_SHIFT) != 0` and to Undo otherwise. Include `<SDL3/SDL_keyboard.h>` and `<SDL3/SDL_keycode.h>`. Key repeats count, so holding the keys repeats the action.

Step 3: In platform_input.cpp, pass io.WantCaptureKeyboard to mapEvent.

Step 4: In Interaction, add:

```cpp
  // Drops what the tool holds or has drawn, as selecting it again does, and gives whether there was
  // anything: a held press or a drawn stop.
  bool dropHeld();
```

It returns false when `!Tool.Holding && Tool.Drawn.empty()`. Otherwise it calls tpj::selectTool(Tool, Tool.Kind) and returns true.

Run: `cmake.exe --build --preset windows-debug --target tpj_app_core`
Expected: builds with no warnings.

### Task 10: Add the buttons and act on them

Files:
- Modify: `src/app/ui/tool_panel.h`, `src/app/ui/tool_panel.cpp`
- Modify: `src/app/ui/tooling_ui.h`, `src/app/ui/tooling_ui.cpp`
- Modify: `src/app/application.cpp`

Step 1: In tool_panel.h, include app/input/input_map.h. Give ToolPanelChoice a member `HistoryAction History = HistoryAction::None;`. Give drawToolPanel two parameters after drawing, `bool canUndo, bool canRedo`, and say in its comment that it draws Undo and Redo buttons, each disabled while it can't act.

Step 2: In drawToolPanel, after the park buttons' EndDisabled, draw `ImGui::Separator()`, then an Undo button inside `ImGui::BeginDisabled(!canUndo)` and `ImGui::EndDisabled()`, then `ImGui::SameLine()`, then a Redo button disabled by `!canRedo`. A press sets choice.History to Undo or Redo.

Step 3: In ToolingUi::build, replace the `const World &world` parameter with `ParkSession &session`, and add a last parameter `HistoryAction keys`. Include app/session/park_session.h. Use `session.world()` where build and drawPanels used world. Pass session to drawPanels instead of the world. In drawPanels, pass session.canUndo() and session.canRedo() to drawToolPanel. After it acts on choice.Tool and choice.Park, take the action from choice.History, or from keys when that is None. If it is not None and interaction.dropHeld() returns false, call session.undo() for Undo or session.redo() for Redo. Extend build's comment: it also undoes or redoes as the Tools panel's buttons or the frame's keys ask, unless the tool held or had drawn something, which it drops instead.

Step 4: In application.cpp, call `Ui.build(Window.get(), Dialogs, Session, Camera, Interactions, Scene.preview(), input.History)`.

Run: `cmake.exe --build --preset windows-debug`
Expected: builds with no warnings.

### Task 11: Check the history

Files: none in the repository. Write a scratch check under build/undo-check, which git ignores.

Step 1: Write build/undo-check/check.cpp, a small program that links tpj_tools. It opens tests/parks/warm.park as the app opens it, with loadWorld and resolveWorld, and steps it once. It keeps an EditHistory, clears it on the world, and applies commands as ParkSession does: one edit per cycle, through a CommandQueue and stepWorld, then noteEdits; each undoEdit or redoEdit command alone in its cycle, then noteEdits. Before each command it saves the intent apart from keys: the sorted list of each point's position, radius, and taper; of each segment's kind, width, edge radii, and its two points' positions; and of each box's kind and pose.

Step 2: Apply one accepted edit of each of the twelve commands, built from warm.park's segments, points, and boxes found through the public queries, with stepWorld cycles between them so guests take keys. Then undo all twelve, then redo all twelve, then undo three and apply a new AddBox. Check, and count a failure for each miss:

- each edit and each undo and redo changes the world;
- after each undo, the intent apart from keys equals the intent before its edit;
- after each redo, it equals the intent after its edit;
- every key a restore added is at least the world's nextKey() before its cycle;
- after twelve undos, canUndo is false, and after twelve redos, canRedo is false;
- after the new AddBox, canRedo is false, and the next undo removes the box.

The program prints each failure, then a last line `failures: N`.

Step 3: Copy build/frontage-check/build.sh to build/undo-check/build.sh.

Run: `cmake.exe --build --preset windows-debug --target tpj_tools tpj_sim && build/undo-check/build.sh check && build/undo-check/check.exe | tail -1`
Expected: `failures: 0`

### Task 12: Confirm the criteria

Step 1: Build, test, and tidy.

Run: `cmake.exe --build --preset windows-debug 2>&1 | grep -c -i warning; ctest.exe --preset windows-debug 2>&1 | tail -3; scripts/tidy.sh 2>&1 | tail -3`
Expected: `0`, then `100% tests passed, 0 tests failed out of 107`, then `tidy: clean.`

Step 2: Compare the scenarios against the baseline (criterion 7).

Run: `build/windows-debug/tpj_scenarios.exe 2> /dev/null > build/undo-branch.txt; build/windows-debug/tpj_scenarios.exe --compare build/undo-base.txt build/undo-branch.txt; echo $?`
Expected: `0`

Step 3: Build windows-release and play criteria 1 to 4 in build/windows-release/ThemeParkJones.exe by hand, as each one says.

Run: `cmake.exe --preset windows-release && cmake.exe --build --preset windows-release`
Expected: builds with no warnings.

### Task 13: Review and commit

Step 1: Stage only this feature's paths: src/sim/park/edits.h, src/sim/park/edits.cpp, src/sim/park/SPEC.md, src/tools/history.h, src/tools/history.cpp, src/tools/CMakeLists.txt, src/tools/SPEC.md, the app files under Files affected in FEATURE.md, src/app/SPEC.md, tests/sim/park_world_checks_test.cpp, plans/effortless-building/path-network/MILESTONE.md, and the undo-redo plan directory. Dispatch the reviewer with the reviewing skill, and present its findings.

Step 2: Commit once with the commit-hygiene skill, subject `Tools: Undo and redo every committed edit`.
