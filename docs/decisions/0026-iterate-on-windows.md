# 0026. Iterate on Windows, verify on Linux at the end

Status: Superseded by 0030

## Context

Decision 0008 made linux-debug the verification build, and all building and testing while working went through it: AddressSanitizer and UBSan slow every test run, clang-tidy ran on every file each build compiled, and the build directory sits on the Windows drive, which WSL reaches slowly. A one-file rebuild took minutes, and a feature's test pass half an hour. Windows is the game's target, and its build is native, uninstrumented, and several times faster to rebuild.

## Decision

Work iterates on windows-debug: the implementer and the test-writer build with cmake.exe and run the Windows test executables directly. linux-debug keeps AddressSanitizer and UBSan but runs no clang-tidy; scripts/tidy.sh runs clang-tidy over every project source, in parallel, warnings as errors. The Linux build, its tests, and tidy run once work is done, in implementing-features' confirmation step before review, and pre-push runs them, so nothing unchecked leaves the machine. The cross-build check is unchanged.

## Consequences

A sanitizer or tidy finding surfaces at the end of a feature rather than at the change that caused it, so fixing it may touch code written earlier in the feature. Both builds treat warnings as errors, so compiler warnings still stop an iteration. Decision 0008's roles for the two builds otherwise stand.
