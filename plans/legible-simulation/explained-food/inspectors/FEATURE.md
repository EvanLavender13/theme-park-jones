# Feature: Inspectors

## Summary

With the Look tool, clicking a guest or a shop opens the Inspector, which explains it from its inspection record and refreshes every frame. tpj_render's picking gains cursorRay, the ray groundAtCursor already follows, rayEntry, where a ray first meets an upright box as appendBox draws it, and entityAtCursor, the entrance, box, or guest the cursor's ray first meets as the park and guest meshes draw them. tpj_legible gains inspect.h: inspectorSubject, which says whether a key is a guest or a shop an inspector can show, and inspectSubject, which builds the inspector's title and rows as text from guestRecord or shopRecord, with a guest's last choice as a table, or says its entity is gone. guest_mesh.h gains guestPose and appendGuestEntity, so a guest is picked and marked where it is drawn. The app picks on a Look press, before the frame's ticks, keeps one subject, which each pick of a guest or shop replaces, draws it in a new app component, the Inspector window, until its close button is pressed, and marks the subject in HIGHLIGHT_TINT in the ghost mesh.

## Acceptance criteria

1. cursorRay(view, aspect, x, y) has Origin view.Eye and the Direction render/SPEC.md gives, so drawFrame's projection maps every point Origin + Direction * t, t > 0, to the cursor's normalized device coordinates (x, y). groundAtCursor gives none when Origin.Y is not above 0 or Direction.Y is not below 0, and otherwise the x and z of Origin + Direction * t at t = -Origin.Y / Direction.Y.
2. rayEntry(ray, pose, size, height) is the least t at least 0 at which Origin + Direction * t lies over footprintOf(pose, size), edges included, at a height from 0 to the height, and none when there is no such t or the pose has no footprint.
3. entityAtCursor(world, view, aspect, x, y) gives the key with the least rayEntry of cursorRay(view, aspect, x, y) among each entrance of parkEntrances at its pose with ENTRANCE_SIZE and ENTRANCE_HEIGHT, each box of parkBoxes with boxSize and boxHeight of its kind, and each guest of parkGuests whose guestRecord has a Position, at guestPose of it with GUEST_SIZE and GUEST_HEIGHT. Of equal entries, the first in that order wins, each query's in the order it gives, and there is none when no entry exists.
4. guestPose(point) is the Pose at the point's x and z with the default facing. appendGuestEntity(mesh, world, key, color) adds exactly what appendBox adds for guestPose of the Position of the key's guestRecord, GUEST_SIZE, GUEST_HEIGHT, and the color, and adds nothing when guestRecord gives none or its record has no Position.
5. inspectorSubject(world, key) is {key, Guest} when guestRecord(world, key) gives a record, {key, Shop} when shopRecord(world, key) does, and none otherwise. pickSubject(subject, world, key) leaves the subject equal to inspectorSubject(world, key) when the key is given and that gives one, and unchanged otherwise.
6. For a guest subject whose guestRecord gives a record, inspectSubject gives the Title `Guest <key>`, Gone false, and the Rows Activity, Hunger, Target, Stay, Meals eaten, Last meal, and Last choice, in that order, each Value formatted from the record and the world's Tick as legible/SPEC.md, Inspectors, gives it.
7. For such a guest, Choices has one ChoiceRow for each option of the record's LastChoice, in order, and none when it has no LastChoice. A row's Option is `shop <Shop>` for an Offer and its kind's display name otherwise; its Relief, Distance, Wait, and Commitment are the option's terms with two decimals for an Offer and empty otherwise; its Score and Probability are the option's with two decimals; and exactly the row at LastChoice's Picked index has Picked true.
8. For a shop subject whose shopRecord gives a record, inspectSubject gives the Title `Shop <key>`, Gone false, the Rows Stock, Queue, On order, Limit, and Starved from the record as legible/SPEC.md gives them, and no Choices.
9. For a subject whose record the world does not give, inspectSubject gives its Title, Gone true, and no Rows or Choices: a guest that has left the park, or a shop whose box is deleted.
10. In the running app, with the Look tool, clicking a guest or shop opens the Inspector with its rows, following every tick, and marks it in HIGHLIGHT_TINT, the mark moving with a guest. Clicking another guest or shop replaces the subject, and clicking empty ground, a path, a depot, or the entrance leaves the Inspector as it was. A depot or the entrance in front of a guest takes the click. The Inspector stays open when another tool is selected, says `No longer in the park` once its guest leaves or its shop is deleted, and closes with its close button, which removes the mark. This is checked by hand, as slice criterion 10 is.

## Medium

The feature produces nothing the simulation consumes. It reads:

- Inspection records (believable-guests, plausible-operations, decision 0025): guestRecord and shopRecord, for the rows, the subject's kind, and each guest's Position, where it is drawn and picked.
- Park intent (effortless-building): parkEntrances and parkBoxes, for the solids a click can meet.
- parkGuests (believable-guests): the guests a click can meet.
- The world's Tick: for how long ago a meal or choice was, and how long a stay has left.

## Principle checks

- Principle 1: inspectorSubject, inspectSubject, and entityAtCursor change nothing: the world's hashWorld is the same before and after.
- Principle 2: a subject whose entity is gone gives an Inspection with Gone true, never an error (criterion 9), and a guest whose place does not resolve is inspected like any other while it cannot be picked (criteria 3 and 6).
- Principle 8: the choice table shows every option the guest weighed, each term of its score, its probability, and the one it picked, as its record holds them (criterion 7).

## Spec changes

- src/legible/SPEC.md: an Inspectors section defines InspectorSubject, inspectorSubject, Inspection, InspectorRow, ChoiceRow, and inspectSubject, with every row's text.
- src/render/SPEC.md: the picking paragraph defines cursorRay, groundAtCursor by it, rayEntry, and entityAtCursor; Guests adds guestPose and appendGuestEntity.
- src/app/SPEC.md: Park, the subject forgotten when the world is replaced; Tools, the Look press's pick and the subject's mark in the ghost mesh; Tooling UI, the Inspector window.

The exact text is in PLAN.md, Tasks 1 to 3.

## Files affected

- Modify: src/legible/SPEC.md, src/legible/CMakeLists.txt
- Create: src/legible/inspect.h, src/legible/inspect.cpp
- Modify: src/render/SPEC.md, src/render/picking.h, src/render/picking.cpp, src/render/guest_mesh.h, src/render/guest_mesh.cpp
- Modify: src/app/SPEC.md, src/app/CMakeLists.txt, src/app/main.cpp
- Create: src/app/inspector_window.h, src/app/inspector_window.cpp
- Tests, from the test pass: tests/legible/, tests/render/

## Dependencies

- guestRecord and parkGuests (believable-guests), shopRecord and limitingFactorName (plausible-operations), all merged.
- The Look tool, groundAtCursor, appendBox, the ghost mesh, and HIGHLIGHT_TINT (effortless-building, render), merged.
- candidate-previews, merged: the ghost mesh is rebuilt when the kept preview is made again, at every tick, or what it was built with changes.

## Out of scope

- The milestone's deepening candidates: hovering a choice option to highlight its shop and route, and the camera following an inspected guest.
- Inspecting depots, the entrance, or paths.
- Picking a shop by its starved mark.
- More than one Inspector at a time.

## Open questions

None.
