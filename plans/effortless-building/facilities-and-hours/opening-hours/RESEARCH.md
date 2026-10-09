# Research: opening-hours

## Where should the park's hours live, and how do undo and redo reach them?

The milestone keeps the hours in a component of their own, not on the entrance, and reads a park with none as 9:00 to 21:00. Park intent lives in components on entities, and a save writes one section for each component type some entity holds. So the hours sit on one entity of their own, which SetOpeningHours creates the first time it sets hours other than the default. When the hours are set back to the default, the entity goes. This keeps one form for each state, as shop-stock does: a park set to other hours and back is the same intent as one never set, and every park saved before the feature saves again with no new section.

The hours are stored as the second of the day the park opens and the second it closes, the same unit as the clock's ParkTime. a-day-out compares them with parkTime(tick).Second with no conversion.

The history records path points, segments, and boxes by key and compares them field by field. The hours have no key that other modules see, since the entity is created and removed as they change. So the history's records gain the park's hours as one value, beside the keyed lists. An edit that changes them holds them before and after, and its reversal is a RestoreIntent that sets them back. The hours entity is created after RestoreIntent's added records, so the keys those records take are the ones the history expects.

Rejected: a world-level field outside the entities — every save would gain a line, and the save format would need a new kind of section. Hours on the entrance — the milestone's research rejects it, since the hours belong to the park. A keyed hours record in the history, like a box's — the hours entity's key changes each time the hours return to the default and leave it, so the history would have to rename it for no reason a player can see. Keeping the hours entity at the default hours — a park set to other hours and back would save an hours line, and an undo would not give back the world before the edit.

## How should the player choose the hours?

Planet Coaster sets the opening and closing times in the park management panel, and the change takes effect at once. Here the change is a tentative edit with a preview and Apply, like the shop's stock (principle 8), and explained-needs will later show the arrival curve it gives in the same panel.

Two times with - and + buttons in quarter-hour steps can't be mistyped, need no text field, and show a valid time at every step. A slider over the day was considered, but ImGui's slider shows a number, not a clock time, and a drag across 96 steps is harder to stop on the wanted time than a few clicks. The panel keeps closing after opening by moving the other time along when a step would meet it. The command itself accepts any two times of day, since hours past midnight are a choice a player could make (principle 5), and only the panel limits itself to the hours within one day.

The choice is held in the app's Interaction, beside the Inspector's chosen stock, because the frame's preview is made from the Interaction's tentative edit before the panels are drawn.

Rejected: applying a time the moment a button is pressed — the slice previews every policy change before it is committed. Disabling a button when its time would meet the other — moving both times later would then need the closing time moved first, which is a step the player shouldn't have to know. A text field for each time — it can hold a time that isn't one, which the panel would have to refuse.

Sources: plans/effortless-building/facilities-and-hours/RESEARCH.md — Planet Coaster's hours and the component per setting; src/tools/history.cpp — how edits are recorded and reversed; src/sim/park/edits.cpp — RestoreIntent and the order its adds take keys; src/app/application.cpp — the order of tentative edit, preview, and panels in a frame.
