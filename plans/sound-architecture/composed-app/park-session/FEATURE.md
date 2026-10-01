# Feature: Park Session

## Summary

park-session gives the world the app holds one owner. ParkSession, in app/park_session.h, holds the world and the command queue, steps them, and uses the park file requests, and it is the one place the world is replaced: it hands the world out only as const, replaces it only as useFileRequest asks, and gives a generation that changes exactly when it does. ParkFileRequests, in app/park_file_requests.h, is the mailbox the park buttons and the dialogs write to and the main loop takes from. startingPark gives the park the app starts from. All three live in tpj_app_core, the library of window-free app components that tpj_park_files becomes, so tpj_app_tests reaches them through their headers. The SDL dialogs and message boxes move to ParkDialogs, in app/park_dialogs.h, the flow's platform edge. main.cpp keeps its resets on replacement, now keyed on the generation, until scene-sync moves them. The app's behavior is unchanged.

## Acceptance criteria

- ParkFileRequests: with no dialog showing, pressing New records the request New with an empty path and shows no dialog, and pressing Open or Save returns that dialog, after which a dialog shows. While a dialog shows, or for None, a press returns no dialog and leaves the request as it was.
- ParkFileRequests: an answer leaves no dialog showing. With a chosen path, the request becomes the answering dialog's action with that path. With none, as when the dialog was cancelled or failed, the request stays as it was. take gives the request and leaves none: a take after it gives None with an empty path.
- ParkSession's generation changes exactly when the world is replaced: useFileRequest with New, or with Open of a file openParkFile opens, changes it to a value it has not had before. Constructing, stepping, saving, a failed open, and a None request do not change it.
- A session's world is the world it was constructed with, its queue empty. step gives the world stepWorld gives for the same world and queued commands, and empties the queue.
- useFileRequest with New makes the world equal to resolvedNewPark(), which is makeNewPark(1) resolved, and with Open of a file openParkFile opens, equal to that world. Either empties the queue. Open of a file it cannot open leaves the world and the queue as they were and reports openParkFile's message to the edge once.
- useFileRequest with Save writes saveWorld of the world to withParkExtension of the path, and leaves the world and the queue as they were, so an edit queued but not applied is not in the file. When that path added the extension and fileExists is true for it, it first asks the edge to confirm replacing it, naming that path, and saves nothing unless the edge confirms. It asks in no other case. A save that fails reports saveParkFile's message to the edge once.
- fileExists, in park_file.h, is true exactly when a file or directory exists at the path.
- startingPark with no path gives resolvedNewPark() stepped the given number of cycles with no commands, and with a path, openParkFile's world stepped the same. For a file openParkFile cannot open, it gives no world and openParkFile's message.
- The app's behavior is unchanged. Every existing test passes with its source unchanged, and tests/app/CMakeLists.txt links tpj_app_core in place of tpj_park_files.

## Medium

None. The session holds the world and steps it as main.cpp did, and reads and writes park files through park_file.h. It samples and emits no field or flow of its own.

## Principle checks

- Principle 1: a save holds saveWorld of the world as it is, with nothing queued or derived in it. The Save criterion's queued edit checks it.
- Principle 10: the session steps the world only through stepWorld with the queued commands, so a session's step equals stepWorld's for the same world and queue. The step criterion checks it.

## Spec changes

src/app/SPEC.md, "## Park files":

- The first sentence, "The library tpj_park_files, in park_file.h, reads and writes park files apart from the window, so the app's tests call it.", becomes: "The library tpj_app_core holds the app's components that need no window, so the app's tests call them through their headers. In it, park_file.h reads and writes park files."
- After "withParkExtension gives a path unchanged when its file name, the part after its last / or \, holds a '.', and otherwise the path followed by .park.", a sentence: "fileExists says whether a file or directory exists at a path."
- A new paragraph after the paragraph that paragraph opens:

  > The park session, ParkSession in park_session.h, holds the world and the command queue. It is the one place the world is replaced: it gives the world only as const, steps it with its queue, and replaces it only in useFileRequest, emptying the queue as it does. Its generation changes exactly when the world is replaced, to a value it has not had. startingPark gives the world the app starts from (Park). ParkFileRequests, in park_file_requests.h, is the mailbox the park buttons and the dialogs write to and the main loop takes from, taking a lock in every call. Its one instance lives for the whole program in ParkDialogs, in park_dialogs.h, the flow's platform edge, which shows the dialogs and message boxes, since a dialog left open at quit may still call back.

- "A dialog's callback, which may run on another thread, only hands the first chosen path to the main loop under a lock." becomes "A dialog's callback, which may run on another thread, only hands the first chosen path to ParkFileRequests."

## Files affected

- Modify: src/app/SPEC.md
- Modify: src/app/park_file.h, src/app/park_file.cpp
- Create: src/app/park_file_requests.h, src/app/park_file_requests.cpp
- Create: src/app/park_session.h, src/app/park_session.cpp
- Create: src/app/park_dialogs.h, src/app/park_dialogs.cpp
- Modify: src/app/tool_panel.h
- Modify: src/app/main.cpp
- Modify: src/app/CMakeLists.txt
- Modify: tests/app/CMakeLists.txt
- Create (test pass): tests/app/park_file_requests_test.cpp, tests/app/park_session_test.cpp
- Modify (test pass): tests/app/park_file_test.cpp

## Dependencies

- park_file.h's openParkFile, saveParkFile, and withParkExtension: met.
- layered-dependencies's placement step and layer check: met.

## Out of scope

- Keying the scene's meshes, the kept preview, the tool's hold, and the Inspector's subject on the generation inside their own components: scene-sync.
- The options, the platform owners, and the Application: composed-entry.

## Open questions

None.
