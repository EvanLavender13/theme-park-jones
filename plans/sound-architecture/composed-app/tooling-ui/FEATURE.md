# Feature: Tooling UI

## Summary

tooling-ui moves the frame's panels, the graph's drawing, and the Debug panel's park numbers out of main.cpp. The park summary, summarizePark in legible/park_summary.h, gives what the Debug panel shows about the park: its shops' records, its guests' count, mean hunger, and how many wait, and the meals eaten, tested in tpj_legible_tests. The graph view, in app/graph_view.h, draws render's buildGraphOverlay on ImGui's background draw list. The tooling UI, ToolingUi in app/tooling_ui.h, owns which views the Debug panel's checkboxes show and builds the frame's Debug, Tools, and Inspector panels, the graph, and the tooltips. The app's behavior is unchanged.

## Acceptance criteria

- summarizePark's Shops holds a ShopLine for each box of parkBoxes that has a shopRecord, in parkBoxes' order, each with the box's key and that record, and no other line.
- summarizePark's Guests is the number of parkGuests that have a guestRecord, its MeanHunger the mean of those records' Hunger, or 0 when there are none, and its Waiting the number of those records whose Activity is Waiting.
- summarizePark's MealsEaten is unitsConsumed of Meals with the cause EATEN_CAUSE.
- The app's behavior is unchanged. Every existing test passes with its source unchanged, a --capture of a park with --graph and --overlay food shows the same scene and the same Debug panel lines, and main.cpp builds no panel, draws no graph, and computes none of the Debug panel's numbers.

## Medium

None. The summary reads the shops' and guests' records and the meals flow's consumption through the queries the park publishes, as drawPanels did, and emits nothing.

## Principle checks

- Principle 10: the summary is a function of the world alone. Equal worlds give equal summaries, and summarizing a world changes nothing in it, so its hash after equals its hash before.

## Spec changes

src/legible/SPEC.md, a new section after "## Inspectors":

> ## Park summary
>
> summarizePark(world) gives a ParkSummary, what the Debug panel shows about the park (app/SPEC.md, Tooling UI). Shops holds a ShopLine for each box of parkBoxes(world) that has a shopRecord, in parkBoxes' order, with the box's key and that record. Guests is the number of parkGuests(world) that have a guestRecord, MeanHunger the mean of their records' Hunger, or 0 when there are none, and Waiting the number of those records whose Activity is Waiting. MealsEaten is the meals units consumed with the cause eaten, unitsConsumed of Meals with EATEN_CAUSE (sim/guests/SPEC.md).

src/app/SPEC.md, "## Tooling UI", a new paragraph after its first:

> The tooling UI, ToolingUi in tooling_ui.h, owns which views the Debug panel's checkboxes show, starting from --graph and --overlay food, and builds the frame's panels, the graph, and the tooltips each frame, so the scene sync reads the Food overlay checkbox from it. The Debug panel's shop, guest, and meal lines come from summarizePark (legible/SPEC.md, Park summary), and the graph view, graph_view.h, draws the graph.

## Files affected

- Modify: src/legible/SPEC.md, src/app/SPEC.md
- Create: src/legible/park_summary.h, src/legible/park_summary.cpp
- Modify: src/legible/CMakeLists.txt
- Create: src/app/graph_view.h, src/app/graph_view.cpp
- Create: src/app/tooling_ui.h, src/app/tooling_ui.cpp
- Modify: src/app/debug_panel.h, src/app/debug_panel.cpp
- Modify: src/app/main.cpp
- Modify: src/app/CMakeLists.txt
- Create (test pass): tests/legible/park_summary_test.cpp
- Modify (test pass): tests/legible/CMakeLists.txt

## Dependencies

- sim's parkBoxes, shopRecord, parkGuests, guestRecord, and unitsConsumed: met.
- render's buildGraphOverlay and its graph colors: met.
- scene-sync's Interaction and frame-input's cursor and orbitCameraView, which the tooling UI calls: met.

## Out of scope

- The options, the platform owners, the Application, and where the ImGui frame begins and ends: composed-entry.
- Splitting the tooling UI per panel: a milestone deepening candidate, should it grow past one concern.

## Open questions

None.
