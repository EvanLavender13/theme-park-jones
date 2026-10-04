# 0035. clang-tidy checks the Windows build

Status: Accepted, 2026-10-03

## Context

Decision 0030 kept scripts/tidy.sh reading linux-debug's compile commands, which decision 0008 had made the build clang-tidy ran in. Since decision 0026, nothing configures linux-debug in the course of work, so its database listed the sources as of whenever it was last configured. A test file added since was never checked, and tidy reported clean; a file deleted since failed the run with no diagnostic. It also meant tidy read the code as Linux compiles it, with Linux's system headers and without the `_WIN32` branches, rather than as the build that ships.

MSYS2's clang-tidy.exe, the same LLVM 18 as WSL's clang-tidy-18, reads windows-debug's compile database directly, and that database is rewritten whenever windows-debug configures, which every change's build does when a CMake file changed.

## Decision

scripts/tidy.sh runs clang-tidy.exe over the project sources in windows-debug's compile database, configuring windows-debug first when a CMake file is newer than the database. linux-debug is no longer needed for tidy; it is the release build of decision 0030 alone.

## Consequences

tidy checks the code the game is built from, `_WIN32` branches included, and a source added or removed through a CMake file is never missed. Shown before adoption: a planted oversized function in a source and a planted `return 0` for a pointer in a project header were each reported, and nothing else. A full run takes about as long as the Linux one did. The pre-push hook no longer configures linux-debug.
