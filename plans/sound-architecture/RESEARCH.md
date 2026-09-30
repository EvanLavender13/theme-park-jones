# Research: sound-architecture

## How can layered dependencies be enforced mechanically in this build?

Lakos's physical design treats a header and its source as a component and requires the graph of components to be acyclic, so the components can be ordered into levels, each depending only on lower ones. Acyclic graphs are cheaper to test, understand, and reuse, and he catalogs techniques for removing a cycle. Two bear on park/intent.cpp's makeNewPark, which calls makeParkSchema, the sim root's registration of every submodule. Escalation moves the dependent piece up into the higher level, but makeNewPark places components private to sim/park, so moving it out would break the private header check. Passing the dependency in instead, with the composition level handing makeNewPark the schema, removes the upward include and keeps the private components where they are.

Chromium enforces the same idea with checkdeps: a DEPS file in a directory lists, with + and - prefixes, which directories its files may include, rules accumulate from parent directories, and a script scans the includes. That is the shape of cmake/check_private_headers.cmake already: scan every include, resolve it, and test it against a rule. A layer check is a second rule over the same scan, with the allowed dependencies declared once in the repository.

Linking does not settle it here. Every module's CMake target puts src/ on the include path, so a file can include any module's header, and an inline function or type from it compiles and links even when the target does not link that module.

Rejected: Clang's layering check (-fmodules-decluse with module maps) — only Bazel generates the module maps, CMake has no support, and GCC, the Windows compiler, has no module maps. ArchLintCpp and CppDepend — each adds an external tool, against the lightweight stack of decision 0002, and CppDepend is commercial; a script over includes covers the rule. include-what-you-use — it checks that each file includes what it uses, not which layers it may reach.

Sources: https://chromium.googlesource.com/chromium/src/+/main/buildtools/checkdeps/README.md — DEPS include_rules and how they accumulate; https://maskray.me/blog/2022-09-25-layering-check-with-clang — Clang's layering check and its Bazel-only build support; https://accu.org/bookreviews/2020/bruntlett_1953/ — summary of Lakos's levelization and its techniques; https://github.com/mao2009/ArchLintCpp — a compile-database architecture linter; https://github.com/include-what-you-use/include-what-you-use — what IWYU checks.

## How should a game executable's application layer be structured?

The composition root pattern puts the wiring of every component in one place, as near the entry point as it can be, and keeps the rest of the code unaware of how the whole is assembled. Components receive what they use rather than reaching for globals. This matches 0027's composing entry point.

Functional core, imperative shell splits code into pure logic that maps inputs to outputs and a thin shell that does the effects, with the shell calling the core and never the reverse. It scales by pairing a core with a shell per concern, such as input separate from game state. In this project the sim is already a deterministic core. The app's pieces that can be stated without a window, such as turning events into camera input, the park file request state, and the debug statistics, are cores, and the SDL and ImGui calls around them are shells.

SDL3 offers main callbacks as an alternative to writing the loop: SDL_AppInit, SDL_AppIterate, SDL_AppEvent, and SDL_AppQuit, with an appstate pointer SDL carries between them, so the application's state is one object rather than globals or loop locals. SDL recommends them because they work on platforms that require event-driven control, such as the web and iOS. SDL_AppEvent may run concurrently with SDL_AppIterate for events pushed from other threads, which matters here because the file dialogs' callbacks can run on other threads. A headless mode such as --hash would return from SDL_AppInit before creating a window.

A fixed timestep keeps the simulation independent of frame rate with an accumulator of real time drained in fixed steps, and optionally interpolates rendering by the remainder. The accumulator is a small piece of state with one rule, which suits a component of its own rather than locals in the frame.

Rejected: a service locator or singletons for the app's components — it hides what each depends on, which the composition root exists to make explicit. A dependency injection container — the app has a handful of components, wired by hand in one place. Modelling the app's parts as EnTT entities or a scene graph — the ECS belongs to the sim, and presentation state is a few owned objects.

Sources: https://freecontent.manning.com/dependency-injection-in-net-2nd-edition-understanding-the-composition-root/ — definition of the composition root; https://functional-architecture.org/functional_core_imperative_shell/ — the core and shell split; https://dev.to/mahush/when-one-shell-isnt-enough-scaling-the-functional-core-imperative-shell-pattern-with-actors-in-c-43f6 — several core-shell pairs in C++; https://wiki.libsdl.org/SDL3/README-main-functions — SDL3 main callbacks and why SDL recommends them; https://wiki.libsdl.org/SDL3/SDL_AppEvent — SDL_AppEvent's concurrency with SDL_AppIterate; https://gafferongames.com/post/fix_your_timestep/ — the fixed timestep accumulator.

## How should derived presentation caches stay in step with the world?

Nystrom's dirty flag pattern treats derived data as a cache of primary data and names invalidation as the hard part: a flag set when the primary changes, checked before the derived data is used. A version counter generalizes it: the source carries a number that increases on change, and each cache stores the number it was built from. The app today compares snapshots of intent, which is robust for edits, but replacing the world is signalled separately by hand-resetting each cache. A generation or identity that changes when the world is replaced, owned beside the caches, would reach every cache through one path, as 0027 asks.

Rejected: callbacks from the sim to the app when state changes — the sim would know about presentation, against principle 10's direction of dependency. Resetting each cache at each place the world is replaced — the current state, where a new cache that misses a reset is a silent bug.

Sources: https://gameprogrammingpatterns.com/dirty-flag.html — the dirty flag pattern and cache invalidation.
