# Research: tooling-ui

## What does the park summary hold, and what stays in the Debug panel?

main.cpp's drawPanels fills DebugStats with two kinds of number: the frame's own (the simulation tick, the camera's focus and distance, and the frame rate the panel reads from ImGui), and the park's (a line for each shop box with its shopRecord, the guest count, their mean hunger, how many wait, and the meals eaten). The park's numbers are computed from parkBoxes, shopRecord, parkGuests, guestRecord, and unitsConsumed, all queries of what the park publishes, so they are legible's to give, as the milestone settled. ParkSummary holds exactly those, and summarizePark computes them with the same loops drawPanels runs today, so the panel shows the same text. ShopLine moves with it, since the summary is what produces it. DebugStats keeps the tick and the camera and holds a ParkSummary, and drawDebugPanel's text is unchanged.

The tick stays out of the summary: it is the world's clock, not something the park publishes about its shops or guests, and the panel already shows it beside the camera.

Rejected: the summary computing the tick and the camera's numbers too — the camera is the app's, and legible links the simulation alone. Leaving ShopLine in debug_panel.h — legible would include an app header, against the layers.

Sources: src/app/main.cpp drawPanels; src/app/debug_panel.h; plans/sound-architecture/composed-app/RESEARCH.md — the summary in legible.

## Who owns the shown views?

ShownViews, whether the graph and the food overlay show, is written only by the Debug panel's checkboxes, and read by the graph and food tooltip that the tooling UI draws and by the scene sync's PreviewLook. Today it is a loop local passed by reference. A ToolingUi class that holds it, starts it from the options, and gives it out as const keeps one writer, as the interaction does for the tool. Building the frame's UI is a member, so the panels write the views it owns, and the loop reads them after the build for the overlay's look.

The ImGui frame's begin and end (beginUiFrame, the SDL3 backend's NewFrame, ImGui::NewFrame, and ImGui::Render) stay where buildUi has them, inside the build, so the order of calls is unchanged. Where the Application places them is composed-entry's to settle.

Rejected: ShownViews left as a loop local — its one writer would be a function that takes it by reference, which is the pattern 0027 replaces with owners. The scene sync owning it — the scene sync reads it but has no checkbox.

Sources: src/app/main.cpp buildUi, drawPanels, and runLoop; src/app/interaction.h — the owner pattern.

## Can the tooling UI be tested without a window?

Everything it does is ImGui calls, which need a context, and its decisions are the panels' own: drawToolPanel returns the choice, drawInspector whether it stayed open. Its testable rule, the Debug panel's park numbers, moves into legible's summary. The rest, like the dialogs component and the scene uploads, is a platform edge checked by the capture and by hand.

Rejected: an ImGui context in tpj_app_tests — it tests ImGui's widgets, not the app's rules, and the capture already shows the panels.

Sources: src/app/tool_panel.h; src/app/inspector_window.h; src/app/debug_panel.h.
