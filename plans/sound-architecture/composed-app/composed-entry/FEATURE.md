# Feature: Composed Entry

## Summary

composed-entry finishes composed-app's restructure of src/app, leaving main.cpp only composing. The options component, parseOptions in app/options.h, reads the command line into Options in tpj_app_core, tested without a window. The platform owners, in app/platform.h, each own one platform resource: SDL's video subsystem, the window, the ImGui context with its SDL3 backend, and the renderer. Each acquires only when the one before it did and releases only what it acquired. The Application, in app/application.h, holds the owners first and the components after them, states the order a frame runs in, and catches what the loop throws inside the owners' lifetime. The tooling UI's build gives the frame's ImGui draw data, so the Application calls no ImGui function. The app's behavior is unchanged.

## Acceptance criteria

- For a command line parseOptions accepts, its Options hold what each given option names: --frames N a FrameLimit of N, --capture and --park their paths, --ticks its count, and --hash, --graph, and --overlay food their flags. Each option not given leaves its member at its default: a FrameLimit of 0, null paths, 0 ticks, and false flags. The one exception is the next criterion's.
- A command line with --capture, and either no --frames or a --frames value that is not positive, gives a FrameLimit of 3.
- parseOptions gives no Options for exactly the command lines that src/app/SPEC.md's Command line section says print the usage.
- main.cpp only composes. It parses the options through parseOptions, writes the hash and returns on --hash, and otherwise builds the Application and runs it. It holds no concern from decision 0027's context list.
- The platform resources each have one owner in platform.h that releases them: SDL's video, the window, the ImGui context with its backend, and the renderer. The Application holds them as its first members, in that order, so they are released in reverse after every component, whether the app returns or the loop throws. Each acquires only when the one before it did. A throw from the loop is caught in the Application's run, inside the owners' lifetime, logged as `Main loop: <what>`, and turned into a nonzero exit status.
- The app's behavior is unchanged:
  - Every existing test passes with its source unchanged.
  - A --capture of a park with --graph and --overlay food shows the same scene and the same Debug panel lines.
  - The layer check and the private header check pass.

The platform owners, the Application, and main.cpp need SDL, a window, and the GPU, which tpj_app_tests cannot reach, so the fourth and fifth criteria are checked by review and by hand, and the sixth by the full test runs and the capture. The usage goes to SDL's log, which tests/app/command_line_test.cpp already reads through the executable, so the options' tests check only that a refused command line gives no Options.

## Medium

None. The feature moves the app's entry, its platform setup and teardown, and its frame loop, which read the park through the session and the components as before, and emits nothing.

## Principle checks

None of its own. The feature changes no simulation code, and the --hash path that principle 10's tests run through the executable keeps those tests in tests/app/command_line_test.cpp, which pass unchanged.

## Spec changes

src/app/SPEC.md, "## Main loop": the first sentence, "The app owns the Dear ImGui context and its SDL3 platform backend, created before the renderer and destroyed after it.", becomes:

> main.cpp only composes: it reads the command line with parseOptions (Command line), writes the hash and returns on --hash, and otherwise builds the Application, Application in application.h, and runs it. The Application holds the platform owners, in platform.h, each owning one resource: SDL's video subsystem, the window, the Dear ImGui context with its SDL3 platform backend, and the renderer, which needs the ImGui context for its GPU backend. They are acquired in that order, each only when the one before it was, and released in the reverse order as the Application is destroyed, after every component it holds, whether the app returns or the loop throws. The Application states the order a frame runs in, each step a call into a component. It catches what the loop throws, such as a world invariant the simulation checks, inside the owners' lifetime, logs it as `Main loop: <what>`, and the app exits with a nonzero status.

src/app/SPEC.md, "## Tooling UI", after the sentence ending "so the scene sync reads the Food overlay checkbox from it.":

> Its build begins and ends the frame's ImGui frame and gives the frame's draw data, which the renderer draws over the scene.

src/app/SPEC.md, "## Command line", a new last paragraph:

> The options component, parseOptions in options.h, reads the command line, its first argument being the program's name, into Options, or gives none after logging the usage.

## Files affected

- Modify: src/app/SPEC.md
- Create: src/app/options.h, src/app/options.cpp
- Create: src/app/platform.h, src/app/platform.cpp
- Create: src/app/application.h, src/app/application.cpp
- Modify: src/app/tooling_ui.h, src/app/tooling_ui.cpp
- Modify: src/app/main.cpp
- Modify: src/app/CMakeLists.txt
- Create (test pass): tests/app/options_test.cpp
- Modify (test pass): tests/app/CMakeLists.txt

## Dependencies

- park-session, scene-sync, frame-input, and tooling-ui, whose components the Application holds and calls: met.
- render's createRenderer and destroyRenderer, which releases only what a failed create made: met.

## Out of scope

- Refusing a --frames value that is not a count. The spec states no rule for it, and the restructure changes no behavior.
- A park size the simulation publishes, in place of the app's constant for the terrain and the camera's bounds.
- src/scenarios/main.cpp, a deepening candidate of sound-architecture.

## Open questions

- Three command lines the Command line section does not name keep their behavior from before the restructure and get no test: a --ticks value of decimal digits too large for 64 bits, which is refused; an argument that does not start with --, which is refused as an unknown option; and a --frames value that is not positive without --capture, which runs until the player quits. Evan decides whether the section should name them, should one matter.
