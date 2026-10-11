# Milestone: Sim Components

Slice: none

Status: planned

## Summary

sim-components restructures the three largest sim files under decision 0027, so each concern they hold is a component behind its own internal header. On 2026-10-11, src/sim/park/edits.cpp is 2137 lines and holds about six concerns, from overlap checks to undo. src/sim/guests/guests.cpp is 1041 lines, and src/sim/park/surface.cpp is 1367. Their public headers stay as they are, and the simulation doesn't change. It comes now because these files keep growing: since 2026-09-25, edits.cpp and guests.cpp each changed in 16 commits and surface.cpp in 8. Each new park command adds three more overloads to edits.cpp. If a concern has no home of its own, the next feature grows these files again.

## Acceptance criteria

- The functions that edits.h declares, and the dispatch over the commands, are all that is left in the file that defines them. Each of these concerns has its own component behind an internal header:
  - the overlap and bounds checks between solids, lines, and paving;
  - planning a path edit, including where new and existing segments meet;
  - applying a plan to the world or to a layout;
  - finding the segments and points an edit changes;
  - checking and applying a restore.
- src/sim/guests/guests.cpp keeps the tick, walking, admitting guests, and the queries guests.h declares. Each of these concerns has its own component behind an internal header:
  - the offers for each need and the scoring of a guest's choices;
  - carrying guests to their new places when the network changes.
- The file that defines the functions surface.h declares keeps building the paved surface and those functions. Each of these concerns has its own component behind an internal header:
  - turning an outline into triangles;
  - shaping the paving where segments meet at a point;
  - hiding paving where pieces join.
- The edit components sit in src/sim/park/edits/, beside the file of edits.h's functions, and their headers sit in src/sim/park/edits/internal/. The surface components sit the same way in src/sim/park/surface/. edits.h and surface.h stay in src/sim/park.
- The guest components sit in src/sim/guests/, and their headers in src/sim/guests/internal/.
- If work that lands before this milestone adds a concern to one of the three files, that concern gets its own component too. Each feature's plan lists the concerns from the file as it stands then.
- The includes among the new components form no cycle.
- edits.h, guests.h, and surface.h don't change, and no SPEC.md changes.
- The private header check and the layer check pass.
- The simulation is unchanged. For each feature, `tpj_scenarios --compare` on windows-debug output from before and after shows no difference, and scripts/cross-build-check.sh passes.
- The existing tests pass unchanged, and the milestone adds no tests.
- scripts/runtime-report.sh runs before and after each feature, and the comparison is reported to the user. If a stage is marked slower, the user decides before the merge whether it ships. Timings aren't gated.

## Medium

This milestone adds, samples, and publishes no fields or flows. It moves code that the park and guests modules own, and it changes neither module's public contract.

## Dependencies

- Decision 0027 accepted: met.
- The layer check and the private header check from layered-dependencies: met.
- The somewhere-to-sit slice closed, including the retirement of the bench box. Its members change edits.cpp, surface.cpp, and guests.cpp, and the user has ordered the whole slice first, so that its plans stay valid as written: unmet.
- composed-app restructures src/app only, so this milestone doesn't depend on it.

## Core feature

`edit-components` comes first. edits.cpp is the worst case: it is the largest source file by 770 lines and holds the most concerns. It sets the pattern surface-components follows: a subdirectory, internal headers, and a file of entry points. guest-components keeps src/sim/guests's existing layout, with headers in its internal/ directory.

## Features

1. `edit-components`: Split src/sim/park/edits.cpp into the overlap checks, edit planning, plan application, the changed region, and restore, in src/sim/park/edits/, with the entry points left in a file of their own. Depends on: none.
2. `guest-components`: Move need offers with choice scoring, and carrying guests across a network change, out of src/sim/guests/guests.cpp into their own components. Depends on: none.
3. `surface-components`: Move triangulation, the paving at a point, and the hiding of joined paving out of src/sim/park/surface.cpp into their own components. Depends on: feature 1, whose pattern it follows.

## Deepening candidates

- Audit src/sim/operations/operations.cpp, at 750 lines, for concerns that deserve their own components. Gated on: an audit showing it holds more than one.

## Open questions

None.

## Research notes

- Each concern becomes a source file with its own internal header, and the public header stays the interface, as in Lakos's components and BDE's layout. If two files share a helper, declare it once in an internal header and define it in one source file, as park/internal/curves.h does. A shared template goes in the internal header, because it can't be defined once in a source file.
- Moving code between files can't change the simulation's results, because tpj_sim builds with -ffp-contract=off, SSE arithmetic, and no LTO or fast-math. `tpj_scenarios --compare` and cross-build-check.sh confirm it.

The details are in RESEARCH.md.
