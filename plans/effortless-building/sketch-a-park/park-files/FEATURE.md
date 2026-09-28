# Feature: Park Files

## Summary

The player starts a new park, opens a park file, and saves one from the Tools panel. Open and save go through SDL's native dialogs, whose callback only hands the chosen path to the main loop. A small library in src/app, tpj_park_files, reads and writes park files. openParkFile loads a file with makeParkSchema and resolves it, saveParkFile writes saveWorld's text, and withParkExtension adds .park to a save path without an extension. The --park option uses the same openParkFile. At the start of a frame, before any input reaches the tool, the loop acts on a request. New park and a successful open replace the world, empty the command queue, drop the tool's hold and drawn points, and frame the camera on the new park. A failed open or save leaves the world as it was and shows why.

## Acceptance criteria

1. For a resolved world, saveParkFile writes exactly saveWorld's text to the path, replacing any file there, and gives an empty message.
2. openParkFile of a file saveParkFile wrote for a resolved world gives a world whose save is identical to that file and whose hashWorld equals the saved world's, so a park saved and reopened is unchanged.
3. openParkFile of a path it cannot read gives no world and a message naming the path. Of a file whose text loadWorld refuses, it gives no world and a message naming the path and holding the LoadError's message.
4. saveParkFile to a path it cannot write gives a message naming the path.
5. withParkExtension gives a path unchanged when its file name, the part after its last / or \, holds a '.', and otherwise the path followed by .park.
6. In the running app, New park, Open park, and Save park in the Tools panel work: new and open replace the park and reframe the camera, a tool's hold or half-drawn path is dropped, a cancelled dialog changes nothing, saving over an existing park under a name typed without .park asks first, a failed open or save shows a message box and leaves the park, and a park saved from the app and reopened is unchanged. Checked by hand (manual).

## Medium

None. A park file is saveWorld's text, the encoding deterministic-simulation owns, and the app reads and writes it whole. The feature samples, emits, draws, and supplies nothing.

## Principle checks

- Principle 1: a park file holds only saveWorld's text, the world's intent and state and nothing derived (criterion 1), and an opened park derives the rest by resolving (criterion 2).
- Principle 2: an opened or new park is resolved before its first tick, so the loop never runs a world with derived data missing. Criterion 2's equal hash shows the opened world resolved.
- Principle 10: the dialog's callback never touches the world. The loop replaces or saves it only at the start of a frame, between ticks, and commands queued for the old world are dropped, never applied to the new one (criterion 6, manual).

## Spec changes

src/app/SPEC.md, in "## Park", the first sentence becomes:

    The app starts from makeNewPark(1), resolved, or from the park file --park names, opened with openParkFile.

src/app/SPEC.md, after "## Tools", a new section:

    ## Park files

    The library tpj_park_files, in park_file.h, reads and writes park files apart from the window, so the app's tests call it. openParkFile reads a file whole, loads its text with makeParkSchema, and resolves the world. When the file cannot be read it gives no world and the message `Cannot read <path>: <reason>`, with SDL's reason, and when loadWorld refuses the text, `Cannot load <path>: <message>`, with the LoadError's message. saveParkFile writes saveWorld's text to a path, replacing any file there, and gives an empty message, or `Cannot save <path>: <reason>` when it cannot. withParkExtension gives a path unchanged when its file name, the part after its last / or \, holds a '.', and otherwise the path followed by .park.

    The Tools panel has New park, Open park, and Save park buttons, which do nothing while a dialog is showing. Open park and Save park show SDL's open and save dialogs for the window, filtered to .park files. A dialog's callback, which may run on another thread, only hands the first chosen path to the main loop under a lock. A cancelled dialog hands nothing, and a failed one logs SDL's error. New park and a chosen path are acted on at the start of the next frame, before its buttons reach the tool. Saving writes the world as it is to withParkExtension of the path, so an edit queued but not yet applied is not in the file. When that added the extension and a file already exists there, the app first asks whether to replace it, since the dialog asked only about the name as typed, and saves nothing unless the player chooses Replace. New park and a successful open replace the world with makeNewPark(1), resolved, or openParkFile's world. They empty the command queue, select the current tool again so it drops any hold and drawn points, and frame the camera on the new park's mesh as at start. A failed open or save leaves the world as it was, and shows its message in an error message box and the log.

## Files affected

- Create: `src/app/park_file.h`
- Create: `src/app/park_file.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/SPEC.md`
- Modify: `src/app/tool_panel.h`
- Modify: `src/app/tool_panel.cpp`
- Modify: `src/app/main.cpp`
- Modify: `tests/app/CMakeLists.txt`
- Tests from the test pass, under `tests/app/`.

## Dependencies

- world-as-value: saveWorld, loadWorld, LoadError, resolveWorld, and hashWorld.
- park-intent: makeParkSchema and makeNewPark.
- box-tools and path-tool: the Tools panel, ToolState, and selectTool.
- SDL3's file dialogs, SDL_LoadFile, SDL_SaveFile, and SDL_ShowSimpleMessageBox, in the linked SDL.

## Out of scope

- Asking to save unsaved changes before New park or Open park, which needs tracking whether the park changed since it was last saved.
- Save without a dialog to the last path, a recent-files list, and a remembered folder.
- Choosing a new park's seed. New park uses seed 1, as the app's start does.
- Saving queued edits that have not yet applied.
- Automated checks of the dialogs, the message box, and the loop's wiring.

## Open questions

None.
