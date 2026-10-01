# Research: composed-app

## Own loop or SDL3's main callbacks?

SDL3's main callbacks hand the loop to SDL: SDL_AppInit builds an appstate object, SDL_AppIterate runs a frame, SDL_AppEvent receives each event, and SDL_AppQuit tears down. SDL recommends them for platforms that demand event-driven control, such as the web and iOS. They also make the application one object rather than loop locals. Their cost here is threading: SDL_AppEvent can run concurrently with SDL_AppIterate for events pushed from other threads, and the app's file dialogs call back from other threads, so input state would need its own lock or queue. Teardown would move into SDL_AppQuit, and --hash would have to finish inside SDL_AppInit.

An Application object run by main's own loop gets the same single owner of the app's state without those costs. The frame order stays in one function that reads top to bottom, as 0027 asks, and RAII owners release on return in reverse order. The vision targets the Windows desktop, so the portability the callbacks buy is not needed. Evan chose the own loop.

Rejected: SDL3's main callbacks — concurrency between events and frames, and teardown and --hash moved into callbacks, for a portability the project does not target.

Sources: https://wiki.libsdl.org/SDL3/README-main-functions — the callbacks and why SDL recommends them; https://wiki.libsdl.org/SDL3/SDL_AppEvent — concurrency with SDL_AppIterate.

## How does replacing the world reach every derived cache through one path?

main.cpp's runLoop replaces the world in useFileRequest and then resets six things by hand: the command queue, the tool's hold, the drawn intent (which also triggers the camera framing), the guest mesh's tick, the kept preview, and the Inspector's subject. A new cache that misses this list is a silent bug, the hazard 0027's context names. A version counter is the general form of Nystrom's dirty flag: the source carries a number that changes on every change, and each cache stores the number it was built from. Here the source is the park session, its number a generation that changes exactly when the world is replaced, and each cache rebuilds or empties itself when its stored generation differs. The decision of what to rebuild then becomes a function of plain values (generation, tick, intent, preview, highlight, subject, overlay choice) that a test can call without a window or a GPU.

Two of the six are not caches but interaction state, the tool's hold and the Inspector's subject. They follow the same generation in the component that owns them, the interaction, and the session empties its own command queue as it replaces the world. The kept preview already states the rule its owner must keep: empty it when the world is replaced (legible/SPEC.md, Previews). The scene sync that owns it keeps the rule by comparing generations.

Rejected: an observer list the session calls on replacement — each listener is still registered by hand, and the order of calls becomes a hidden contract. Comparing world addresses — the world is replaced in place by assignment, so its address never changes.

Sources: https://gameprogrammingpatterns.com/dirty-flag.html — derived data as a cache, and invalidation; src/app/main.cpp runLoop — the six hand resets.

## How is app code tested without a window?

tpj_park_files set the precedent: a static library of app code apart from the window, linked by tpj_app_tests. The restructure extends it into a library of every app component that needs no window, with the executable adding only main.cpp and the platform edges. Each core takes plain values rather than calling SDL or ImGui itself:
- event mapping takes an SDL_Event, a plain union a test can fill without SDL_Init, and ImGui's capture flags as booleans;
- the cursor's normalized coordinates take the window size and mouse position;
- the frame clock takes performance counter readings and their frequency;
- the scene sync's rule takes the values above.

The SDL and ImGui calls that gather those values stay in thin shells.

Rejected: tests that open a hidden window — they need a display and a GPU, which ctest on the Linux build does not have, and they would test SDL rather than the app's rules.

Sources: src/app/CMakeLists.txt and tests/app/CMakeLists.txt — tpj_park_files and tpj_app_tests; https://functional-architecture.org/functional_core_imperative_shell/ — the core and shell split.

## How do platform resources get single owners?

main acquires SDL, then the window, then the ImGui context and its SDL3 backend, then the renderer, and releases them by hand in reverse. runLoopLogged catches what the loop throws, such as a world invariant the simulation checks, logs it as `Main loop: <what>`, and returns failure, so the release after it still runs. C++ destroys an object's members in the reverse of their declaration order, so an owner type per resource, held as members in acquisition order, gives the reverse release on every return path with no manual teardown. On an exception it does so only while the stack unwinds, and when no handler catches an exception, std::terminate runs and whether the stack unwinds first is implementation-defined; libstdc++ does not. So a handler stays: the Application's run catches, logs as today, and returns failure, inside the owners' lifetime, and the owners release as the stack unwinds to it. SDL_Window fits std::unique_ptr with SDL_DestroyWindow as its deleter. SDL itself, the ImGui context with its backend, and the renderer each need a small owner type whose destructor makes the matching call.

Rejected: keeping manual teardown after the catch — each new resource repeats the pattern, and the release order is a convention, not a guarantee. Dropping the catch and relying on the owners alone — an uncaught exception may terminate without unwinding, releasing nothing and losing the log line.

Sources: https://en.cppreference.com/w/cpp/language/destructor — destruction order of members; https://en.cppreference.com/w/cpp/error/terminate — std::terminate and unwinding when no handler is found; src/app/main.cpp main and runLoopLogged.

## Where do the Debug panel's numbers and the graph drawing go?

drawPanels gathers shop lines, the guest count, mean hunger, waiting guests, and meals eaten from shopRecord, guestRecord, parkGuests, parkBoxes, and unitsConsumed: all queries of what the park publishes, needing no window. That is legible's role, so a park summary in legible gives the numbers, tested in tpj_legible_tests as inspect.h is, and the Debug panel adds the frame rate and the camera and draws them. drawGraph is only ImGui draw-list calls over render's buildGraphOverlay, which already holds the logic, so it is an app component at the platform edge. Evan chose this placement.

Rejected: keeping the summary in app — a summary of the simulation would sit where only the app can use or test it, beside platform code.

Sources: src/app/main.cpp drawPanels and drawGraph; src/legible/SPEC.md — legible's scope.
