# Milestone: Composed App

Slice: none

## Summary

composed-app restructures src/app under decision 0027, so main.cpp only composes and each concern it now orchestrates has its own component. A park session owns the world, the command queue, and the park file flow, and is the one place the world is replaced. A scene sync owns every cache derived from the world, rebuilt by a rule tested without a window and reset by the session's generation alone. Input, the cursor, the frame clock, the panels, and the platform resources each get their own component, and an Application states the frame order in one place. It comes now because the rules from layered-dependencies are in force and main.cpp is their worst case: it grew from 635 to 742 lines after 0027 was accepted, and replacing the world resets six things by hand. If 0027 cannot produce a sound application layer here, the record needs rework.

## Acceptance criteria

- src/app/main.cpp only composes. It parses the options through the options component, writes the hash and returns on --hash, builds the Application, and runs it. It holds no concern from 0027's context list.
- Each concern main.cpp orchestrates today has its own component behind its own header:
  - the options;
  - the park session, owning the world, the command queue, and the park file flow;
  - the SDL file dialogs and message boxes, as the session's platform edge;
  - the scene sync, owning every mesh built from the world, the kept preview, and the camera framing on a new park;
  - the interaction, owning the tool state, the step that hands the frame's presses and releases to the tool and queues the edit a release commits, the Look press's pick, and the Inspector's subject, which the Inspector's close button forgets through it;
  - input mapping;
  - the cursor, meaning its normalized coordinates and the ground and entity under it;
  - the frame clock;
  - the graph view;
  - the tooling UI that builds the panels;
  - the platform owners;
  - the Application, which states the frame order.
- The world is replaced only in the park session, and a generation it gives changes exactly when the world is replaced. The session empties its own command queue as it replaces the world. Everything else that today is reset by hand on replacement follows the generation instead, with no reset anywhere else: in the scene sync, every derived mesh with the camera framing and the kept preview; in the interaction, the tool's hold and the Inspector's subject. A test without a window shows that a replaced world rebuilds every mesh and empties the kept preview.
- What the scene sync rebuilds is decided by a function of plain values, tested without a window: the park mesh when intent or the generation changes, the guest mesh when the tick or the generation changes, and the ghost and overlay meshes when the preview is made again or the highlight, the subject, or the overlay choice changes.
- The Debug panel's numbers about the park come from a park summary in legible, tested in tpj_legible_tests. The graph view is an app component that draws render's buildGraphOverlay.
- Every platform resource has one owner that releases it: SDL, the window, the ImGui context with its backend, and the renderer. They are released in the reverse of the order they were acquired, on every return and when the loop throws. A throw from the loop is caught inside the owners' lifetime, logged as `Main loop: <what>` as today, and turned into a failing exit, so the owners' destructors run as the stack unwinds.
- Every app component that needs no window is in a library that tpj_app_tests links, and its rules are tested through its header: options parsing, the file flow, the session's replacement, the scene sync's rule, input mapping, the cursor's coordinates, and the frame clock.
- src/app groups its components by concern into session/, input/, scene/, and ui/, with the Application, the platform owners, the options, and the frame clock at its top, and tests/app mirrors the grouping. cmake/layers.txt declares the four as layers inside app, session and input below scene and ui, below the top, so the layer check enforces their order.
- The restructure changes no behavior. Every existing test passes with its source unchanged, apart from where its file lives, the paths of the app headers it includes, and the layer check's expected table, though tests/app/CMakeLists.txt may link the new library in place of tpj_park_files, and a --capture of a park with --graph and --overlay food shows the same scene as before. src/app/SPEC.md changes only where it describes structure, and src/legible/SPEC.md gains the park summary. The layer check and the private header check pass.

## Medium

This milestone introduces, samples, and emits no fields or flows. It moves app code that reads the park through the public queries it already uses, and the park summary reads only what the park publishes, as the rest of legible does.

## Dependencies

- layered-dependencies, whose layer check and placement step govern every feature here: met.
- tpj_park_files and tpj_app_tests, the precedent for app code tested without a window: met.
- legible's KeptPreview, whose rule that it is emptied when the world is replaced the scene sync keeps: met.

## Core feature

park-session is the core. It makes the world's replacement one call in one component, with a generation every later component reads, and it moves the file flow's request handling, which today lives in a function-local static shared with dialog threads, behind a header where it is tested without a window. On its own it removes the largest stateful concern from main.cpp and gives the scene sync the one signal it needs.

## Features

1. `park-session`: the park session, owning the world, the command queue, the starting world, and the park file flow's requests, with the generation that changes when the world is replaced. It also makes the library of window-free app components that tpj_app_tests links, and moves the SDL dialogs and message boxes into their own platform-edge component. Depends on: none.
2. `scene-sync`: the scene sync, owning the park, guest, ghost, and overlay meshes, the kept preview, and the camera framing on a new park, with its rebuild rule tested without a window; and the interaction, owning the tool state, the buttons' step to the tool and the command queue, the Look press's pick, and the Inspector's subject. Both reset by the generation alone, so no reset on replacement is left in main.cpp. Depends on: park-session.
3. `frame-input`: input mapping from SDL events to camera input and the left button, the cursor's coordinates and the ground and entity under it, and the frame clock with its clamp and tick accumulator, each tested without a window. Depends on: park-session, for the library.
4. `tooling-ui`: the park summary in legible, the graph view, and the tooling UI component that builds the Debug, Tools, and Inspector panels and the tooltips. Depends on: park-session, for the library.
5. `composed-entry`: the options component, the platform owners, and the Application stating the frame order, leaving main.cpp only composing. src/app/SPEC.md describes the components. Depends on: features 1 to 4.
6. `grouped-app`: src/app and tests/app grouped by concern into session/, input/, scene/, and ui/, declared as layers inside app, so the components the restructure made do not sit in one flat directory. Depends on: feature 5.

## Deepening candidates

- A pixel comparison of captures before and after a restructure, so "the same scene" is checked mechanically. Gated on: captures that are byte-stable, which needs the frame rate kept out of the captured panel.
- The tooling UI split per panel, should it grow past one concern as panels are added.

## Open questions

None. Evan chose the own loop over SDL3's main callbacks and placed the park summary in legible and the graph view in app (RESEARCH.md).

## Research notes

- An Application run by main's own loop gives one owner of the app's state without SDL_AppEvent's concurrency with frames.
- A generation owned by the park session, stored by each cache, replaces the six hand resets with one path.
- Window-free cores take plain values (an SDL_Event, window size, counter readings), and thin shells call SDL and ImGui.
- Owners held as members in acquisition order release in reverse, provided a handler inside their lifetime catches what the loop throws, since an uncaught exception need not unwind.

Depth is in RESEARCH.md.
