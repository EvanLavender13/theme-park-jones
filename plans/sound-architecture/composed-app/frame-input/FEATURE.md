# Feature: Frame Input

## Summary

frame-input moves the frame's input, the cursor, and the frame clock out of main.cpp into components tested without a window. Input mapping, in app/input_map.h, turns one SDL event and the held keys into the frame's camera input, the left button's press and release, and a request to quit, given whether ImGui wants the mouse and the keyboard. The cursor, in app/cursor.h, gives the cursor's normalized device coordinates and the window's aspect ratio from the window's size and the mouse position, and the ground and entity under it. The frame clock, in app/frame_clock.h, turns performance counter readings into each frame's elapsed time, clamped, and the whole simulation ticks it steps. platform_input.h, their platform edge in the executable, drains SDL's events through ImGui and reads the cursor. The orbit camera joins tpj_app_core, with orbitCameraView giving the view it sees. The app's behavior is unchanged.

## Acceptance criteria

- mapEvent records a quit event as a request to quit, and a left button release as a release whether or not ImGui wants the mouse. It records a left button press only while ImGui does not want the mouse. No other event changes the request to quit or the buttons.
- While ImGui does not want the mouse, mapEvent adds a motion event's relative pixels to the camera's orbit deltas when the event's own button state holds the right button, and otherwise to its pan deltas when it holds the middle button, and adds a wheel event's vertical steps, its y as SDL gives it whatever its direction, to its zoom, so a frame's events sum. While ImGui wants the mouse, no event changes the camera input.
- mapKeys sets each of the camera's move and rotate axes to its positive key's state minus its negative key's, a held key counting 1 and any other 0: W and S for MoveForward, D and A for MoveRight, Q and E for Rotate. It changes no other field of the camera input, so the mouse's deltas mapEvent gathered stay. A scancode past the end of the keys counts as not held. While ImGui wants the keyboard, mapKeys changes nothing.
- cursorNdc of a window width by height pixels and a mouse at (x, y) pixels from its top left gives X = 2x / width - 1, Y = 1 - 2y / height, and Aspect = width / height, and gives none when the width or the height is not positive.
- groundUnderCursor gives groundAtCursor of the view and the cursor's aspect and coordinates, and entityUnderCursor gives entityAtCursor of the world, the view, and the cursor's aspect and coordinates. Both give none without a cursor.
- FrameClock's advance gives as Dt the readings since the previous reading, or since the reading it was constructed with, divided by the frequency, and at most MAX_FRAME_SECONDS, which is 0.25.
- Over any run of advances, the ticks given so far are the whole SIM_TICK_SECONDS the Dts given so far hold, to within floating-point rounding, so the time a frame does not step carries to the next.
- orbitCameraView gives a CameraView whose Eye is orbitCameraEye of the camera, whose Target is its Focus, and whose other fields are a default CameraView's.
- The app's behavior is unchanged. Every existing test passes with its source unchanged, a --capture of a park with --graph and --overlay food shows the same scene, and main.cpp holds no event mapping, cursor arithmetic, or tick accumulator.

## Medium

None. The cursor reads the world only through render's entityAtCursor, as main.cpp did, and none of these components samples or emits a field or flow.

## Principle checks

- Principle 10: the simulation advances in fixed SIM_TICK_SECONDS ticks however the elapsed time is split into frames. The ticks given over a run of advances depend only on the summed Dt, which the tick criterion checks.

## Spec changes

src/app/SPEC.md:

- "## Main loop", after "Elapsed time per frame is clamped to 0.25 s so a stall does not trigger a burst of ticks.", add:

  > The frame clock, FrameClock in frame_clock.h, turns the performance counter's readings into each frame's elapsed time, clamped, and the whole ticks the time not yet stepped covers, carrying the rest to the next frame. Input mapping, in input_map.h, turns each event and the held keys into the frame's camera input, the left button's press and release, and a request to quit, given whether ImGui wants the mouse and the keyboard. platform_input.h, the input's platform edge, drains SDL's events, handing each to ImGui and then to input mapping, and reads the cursor.

- "## Camera", after its last paragraph, a new paragraph:

  > The camera and orbitCameraView, which gives the view from its eye toward its focus, are in orbit_camera.h. The cursor, in cursor.h, gives the cursor's normalized device coordinates and the window's aspect ratio from the window's size and the mouse position, none when the window has no size, and the ground and the entity under it. platform_input.h reads it from SDL, none while ImGui wants the mouse.

## Files affected

- Modify: src/app/SPEC.md
- Create: src/app/input_map.h, src/app/input_map.cpp
- Create: src/app/cursor.h, src/app/cursor.cpp
- Create: src/app/frame_clock.h, src/app/frame_clock.cpp
- Create: src/app/platform_input.h, src/app/platform_input.cpp
- Modify: src/app/orbit_camera.h, src/app/orbit_camera.cpp
- Modify: src/app/interaction.h
- Modify: src/app/main.cpp
- Modify: src/app/CMakeLists.txt
- Create (test pass): tests/app/input_map_test.cpp, tests/app/cursor_test.cpp, tests/app/frame_clock_test.cpp, tests/app/orbit_camera_test.cpp
- Modify (test pass): tests/app/CMakeLists.txt

## Dependencies

- park-session's tpj_app_core and tpj_app_tests: met.
- scene-sync's PointerButtons and Interaction: met.
- render's groundAtCursor and entityAtCursor, and sim's SIM_TICK_SECONDS: met.

## Out of scope

- Moving the panels, the graph view, and the Debug panel's numbers: tooling-ui.
- The options, the platform owners, and the Application: composed-entry.
- Tests of updateOrbitCamera and frameOrbitCamera, whose behavior this feature does not change.

## Open questions

None.
