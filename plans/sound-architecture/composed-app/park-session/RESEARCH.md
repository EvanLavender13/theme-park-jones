# Research: park-session

## Who owns the request mailbox the dialogs write to?

SDL's open and save dialogs return at once and call back later, possibly on another thread and possibly before the call that showed them returns (SDL_dialog.h). Whatever their callback writes to must outlive any dialog still open, and main.cpp keeps its FileRequests in a function-local static for that reason: a dialog left open at quit may still call back. The mailbox's rules, meaning what a button press records, what a dialog's answer records, and what the main loop takes, need no SDL and can be tested as a class. Its one instance keeps the program's lifetime and lives in the platform edge that shows the dialogs, since that lifetime is dictated by SDL's callbacks, not by the session.

Rejected: the session owning the mailbox — a dialog open at quit would call back into a destroyed session. A shared_ptr copy passed to each dialog as its userdata — a dialog left open at quit leaks its copy, which LeakSanitizer reports in the linux-debug build.

Sources: .cpm-cache/sdl3/45cb/include/SDL3/SDL_dialog.h — the callback's thread and timing; src/app/main.cpp fileRequests and handOver.

## How is a park file request used without a window?

useFileRequest needs the window for two things only: the message box asking before a save replaces a file whose extension the app added, and the message box showing a failed open or save. A small interface for those two calls, implemented by the dialogs component and by a test double, leaves the rest testable: choosing the path with withParkExtension, checking for an existing file, saving, opening, and replacing the world. The existing-file check, SDL_GetPathInfo, needs no window but is still a platform call, so it moves behind park_file.h as fileExists, beside the park file input and output that file already does through SDL, and the session calls no SDL at all.

Rejected: passing the SDL_Window — the session would call SDL's message boxes itself and could not be tested without a display. A pair of std::function callbacks — workable, but an interface names the edge and its two duties in one place, as WordSink does in the sim.

Sources: src/app/main.cpp useFileRequest, confirmReplace, and reportFileError; src/sim/schema.h WordSink — the interface pattern the code already uses.

## How is replacement made the only path?

The session hands out the world only as const, so nothing outside it can assign a new one, and replacement is a private step reached only through useFileRequest. Stepping is the session's step, so the one other mutation also stays inside. The tools, the picker, and the meshes already take a const World. The generation is a counter that increases at each replacement, so it never returns to an earlier value, and a reader that stored one knows the world it saw is gone. The world is replaced by move assignment into the same member, so a reference to it stays valid across replacements.

Rejected: a public mutable world — any caller could replace it and skip the generation. Comparing world addresses — they never change, since replacement assigns into the same object.

Sources: src/app/main.cpp runLoop and useFileRequest; src/sim/world.h — stepWorld empties the queue it is given.

## What becomes of tpj_park_files and ParkAction?

tpj_park_files was the first library of window-free app code. It becomes tpj_app_core, the milestone's library of every such component, holding park_file.cpp beside the new ones. tests/app links it in place of tpj_park_files, the one test-side change the milestone allows. ParkAction, the park button a player pressed, is today declared in tool_panel.h, which is ImGui code in the executable. The mailbox in the library needs it, so it moves to the mailbox's header, and tool_panel.h includes that.

Rejected: keeping tpj_park_files and adding a second library — two libraries of the same kind of code, with the next features left to choose between them.

Sources: src/app/CMakeLists.txt; tests/app/CMakeLists.txt; src/app/tool_panel.h.
