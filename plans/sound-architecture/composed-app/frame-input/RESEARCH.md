# Research: frame-input

## How is input mapped without a window?

main.cpp's gatherInput mixes three things: draining SDL's queue and handing each event to ImGui, deciding what each event means for the camera, the left button, and quitting, and reading the held keys. Only the first needs SDL to be running. SDL_Event is a plain union that a test can fill without SDL_Init, and SDL_GetKeyboardState gives an array of booleans indexed by scancode with its length, so the meaning of an event and of the held keys becomes two functions of plain values: the event or the key array, and whether ImGui wants the mouse or the keyboard. ImGui sets WantCaptureMouse and WantCaptureKeyboard in NewFrame, not while it processes events, so the flags are the same for every event of one frame's drain, and passing them in as booleans gives exactly what reading them inside the loop gave. The header forward-declares SDL_Event, as renderer.h does SDL's GPU types, so the core's interface names no SDL header and tpj_app_core keeps SDL private. The thin shell left in the executable polls, hands each event to ImGui, and calls the core.

The key array's length comes with it, so a scancode past its end reads as not held instead of reading past the array, which the old call to SDL_GetKeyboardState(nullptr) could not check.

Rejected: an event queue the core drains itself — it would call SDL_PollEvent and need SDL running. Reading ImGui's flags inside the core — it would need an ImGui context in every test.

Sources: src/app/main.cpp gatherInput, addMouseInput, and keyAxis; .cpm-cache SDL3 SDL_events.h — SDL_Event as a union typedef; SDL_keyboard.h — SDL_GetKeyboardState's numkeys; imgui.cpp NewFrame — where the capture flags are set.

## Where does PointerButtons belong?

scene-sync moved PointerButtons into interaction.h because the interaction consumes it. Input mapping now produces it, and a producer owning its output type keeps the dependency pointing from consumer to producer: the interaction includes the input's header, and input mapping needs nothing of the tool. The struct moves to input_map.h, and interaction.h includes it, so every existing use still compiles.

Rejected: input_map.h including interaction.h for the struct — input would depend on the owner of the tool and the Inspector, which it has no use for.

Sources: src/app/interaction.h; plans/sound-architecture/composed-app/scene-sync/PLAN.md Placement.

## How is the frame clock tested?

runLoop reads SDL's performance counter each frame, divides the difference by the counter's frequency, clamps it to 0.25 s, adds it to an accumulator, and steps one tick per whole SIM_TICK_SECONDS it holds. Given the readings and the frequency as integers, that is a small class with no SDL in it, and a test feeds it readings it chooses. Keeping the arithmetic as it is, the same division, clamp, and repeated subtraction, keeps the tick count of every frame identical to today's. The clock gives the count rather than stepping the session itself, so it knows nothing of the world, and the loop steps the session that many times.

Rejected: the clock calling SDL_GetPerformanceCounter — tests would depend on real time. The clock stepping the session — the clock would own a dependency on the world for a loop of one line.

Sources: src/app/main.cpp runLoop; src/sim/world.h SIM_TICK_SECONDS; https://gafferongames.com/post/fix_your_timestep/ — the accumulator and the clamp against a spiral of ticks.

## Does the orbit camera join the library?

The cursor's ground and entity take the camera's view, and input mapping fills the orbit camera's CameraInput. The orbit camera needs no window but builds into the executable, and main.cpp's cameraView helper turns it into a CameraView. The milestone puts every window-free app component in tpj_app_core, so the orbit camera moves there now and the helper becomes orbitCameraView beside it. Evan chose this.

Rejected: leaving the orbit camera in the executable — the input and cursor would depend on a header whose code the library cannot link, and cameraView would stay in main.cpp.

Sources: src/app/orbit_camera.h and orbit_camera.cpp; src/app/main.cpp cameraView.
