# Research: park-files

## How do SDL3's file dialogs hand a path to the main loop?

SDL_ShowOpenFileDialog and SDL_ShowSaveFileDialog must be called from the main thread, and they return at once. The callback gets the chosen paths as a list: null on an error, which SDL_GetError describes, an empty list when the player cancels, and otherwise the paths, which SDL frees after the callback returns. On Windows, SDL runs the dialog on a thread of its own and calls back from there, so the callback copies the first path into a slot the main loop reads under a lock, and does nothing else. The callback may also run on the calling thread before the show call returns, so the caller must not hold that lock while it calls. The userdata pointer has to outlive a dialog the player leaves open while the app quits, so the slot lives for the whole program. Filters are pairs of a name and a semicolon-separated list of extensions without dots. The Windows backend sets no default extension, so a name typed without one is saved without one, and the open dialog's .park filter would then hide it. The app therefore adds .park to a save path whose file name has no extension.

SDL_LoadFile and SDL_SaveFile read and write a whole file in one call and report failures through SDL_GetError, which the app's --park path already uses for its messages. Keeping openParkFile and saveParkFile in a small library apart from the window lets the app's tests check the round trip without a display.

Rejected: reading or writing the file inside the callback, which would touch the world from another thread (principle 10). Polling a flag without a lock, which is a data race under the sanitizers. SDL's event queue as the handoff, which works but needs a registered user event type for one string.

Sources: https://wiki.libsdl.org/SDL3/SDL_ShowOpenFileDialog — threading and the callback's list; https://wiki.libsdl.org/SDL3/SDL_DialogFileCallback — null, empty, and chosen lists; https://wiki.libsdl.org/SDL3/SDL_SaveFile — writing a whole file; SDL's src/dialog/windows/SDL_windowsdialog.c — the dialog thread and the missing default extension.
