# Research: composed-entry

## What does the options component give, and who prints the usage?

main.cpp's parseOptions fills an Options from argv, refuses an unknown option, a missing value, a --ticks value that is not a decimal count, an --overlay value other than food, and --hash beside a window option, logs the usage line with SDL_Log on refusal, and gives --capture a three-frame limit when --frames gives none that is positive. All of it is a function of the arguments, so it moves into tpj_app_core as options.h, where tpj_app_tests calls it. It takes argc and argv as `const char *const *`, which main's `char **` converts to with no cast and which a test fills from string literals, and gives `std::optional<Options>`, none on refusal. FramesGiven, which only the --hash rule reads, becomes a local of the parse. The paths stay pointers into argv, which outlive the app.

The options component logs the usage itself, as it does today, since the usage is the command line's to describe and main then only returns on none. SDL_Log needs no SDL_Init, and tpj_app_core already links SDL privately.

Rejected: returning the usage text for main to print — main would carry the options' output. Validating --frames' value — the spec states no rule for it, and the restructure changes no behavior. std::string paths — they would copy what argv already holds for the program's lifetime, and drawFrame and startingPark take pointers.

Sources: src/app/main.cpp parseOptions and parseCount; src/app/SPEC.md, Command line; tests/app/command_line_test.cpp, which checks the same rules through the executable.

## How do the platform owners acquire in order and report a failure?

main acquires SDL's video, the window, the ImGui context with its SDL3 backend, and the renderer, each only when the one before succeeded, logs SDL's error for the first two, and releases what it acquired in reverse (milestone RESEARCH.md, single owners). Four owner types in platform.h, each taking the owner before it by const reference, keep both rules in the types: an owner whose predecessor did not acquire acquires nothing, and its destructor releases only what it acquired. Held as the Application's first members, in acquisition order, they are built in that order and destroyed in reverse after every other member, on every return and as a throw unwinds to the Application's catch. The Application's run reports the failure by returning false when the last owner, the renderer, is not ready, so no constructor throws and main stays one call.

The renderer's owner calls destroyRenderer even after a failed createRenderer, which already releases only what was created. The ImGui owner keeps today's calls, InitForSDLGPU's result ignored as today.

Rejected: a factory giving a null Application on failure — the owners would need to be movable or allocated, to be built step by step before the Application exists. Constructors that throw on failure — main would need its own catch for a failure that is expected and already logged. A unique_ptr with SDL_DestroyWindow as its deleter for the window — it fits, but the window's owner also opens the window only after SDL's video, and the four owners read alike as classes.

Sources: src/app/main.cpp main; src/render/renderer.cpp createRenderer and destroyRenderer; https://en.cppreference.com/w/cpp/language/destructor — members destroyed in reverse of declaration.

## Where do the ImGui frame's begin and end live?

ToolingUi::build begins the ImGui frame (beginUiFrame, the SDL3 backend's NewFrame, ImGui::NewFrame) and ends it with ImGui::Render, and the loop then reads ImGui::GetDrawData for drawFrame. The frame is the tooling UI's, the only component that builds ImGui windows, so build keeps the bracket and gives the draw data as its result. The Application then passes it to drawFrame and calls no ImGui function, and the data's path from the panels to the renderer shows in the frame order.

Rejected: the Application bracketing the build — it would call ImGui and the GPU backend's frame start itself, platform calls the tooling UI already owns.

Sources: src/app/tooling_ui.cpp build; plans/sound-architecture/composed-app/tooling-ui/RESEARCH.md — the bracket left to this feature.

## Where does the park's size live?

PARK_SIZE_METERS sizes the renderer's terrain and bounds the camera's focus to half of it. The Application is the one place that hands it to both, so it is a constant in application.cpp. A park size the simulation publishes would be its proper source, but the sim has none, and adding one is a behavior change, not a restructure.

Rejected: orbit_camera.h — the renderer's terrain would then depend on the camera's header.

Sources: src/app/main.cpp PARK_SIZE_METERS and its two uses.
