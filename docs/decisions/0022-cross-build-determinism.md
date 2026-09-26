# 0022. The simulation is bit-identical across builds

Status: Accepted, 2026-09-26

## Context

Principle 10 makes the simulation deterministic, and decision 0008 splits playing (Windows, MSYS2 UCRT) from verification (Linux, glibc). The two C runtimes' math functions can differ in the last bit, and the compilers may contract multiply-adds differently. Without cross-build determinism, Linux tests would not be evidence for what players see, and saves and replays would not be portable.

## Decision

A park simulates bit-identically on every supported build. The simulation uses its own implementations of transcendental functions (exp, log, pow, trigonometry) instead of the C runtime's, and tpj_sim builds with -ffp-contract=off and without fast-math or target-specific instruction sets.

## Consequences

A cross-build check records the state hash of a scripted run on Windows and compares it with the same run on Linux; it is added once the simulation has state worth hashing. Rendering and tooling may use the C runtime freely, since they never feed back into the simulation.
