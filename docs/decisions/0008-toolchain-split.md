# 0008. Windows build for playing, Linux build for verification

Status: Accepted, 2026-09-26. Where work is built and tested while iterating, and clang-tidy in the linux-debug build, are superseded by 0026.

## Context

Development happens in WSL on Windows. The native Windows toolchain used by SteelJones, MSYS2 UCRT64 GCC driven by cmake.exe, has no AddressSanitizer runtime and only a trap mode for UndefinedBehaviorSanitizer. MSVC has ASan but not UBSan. WSL's GCC has both, and clang-tidy is installed there.

## Decision

Two builds with different roles. The Windows build (presets windows-debug and windows-release, MSYS2 UCRT64 GCC with Ninja, built from WSL with cmake.exe) is for playing and GPU work. The Linux build (presets linux-debug and linux-release, WSL GCC) is the verification build: linux-debug runs AddressSanitizer, UndefinedBehaviorSanitizer, and clang-tidy, and tests are run there.

## Consequences

Code must build with both toolchains. Sanitizers apply only to project targets, not dependencies. The simulation builds without rendering (principle 10), so its tests never need a window. The app also runs under WSLg, rendering through lavapipe; leak reports from the X11 and Vulkan libraries are suppressed in the app only. SDL's XScreenSaver and XTest support is disabled in the Linux build because their development packages are not installed. Windows executables link the runtime statically so they run outside an MSYS2 shell.
