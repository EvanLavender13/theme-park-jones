# 0013. Tracy as the profiler, off by default

Status: Accepted, 2026-09-26

## Context

Profiling will matter once the simulation has real work, but a profiler should cost nothing until then.

## Decision

Tracy, behind the CMake option TPJ_TRACY (off by default). Code includes core/profile.h and uses its TPJ_PROFILE_ macros, never Tracy directly. With the option off, Tracy is not fetched and the macros expand to nothing.

## Consequences

The main loop marks frames and the simulation step has a zone. Enabling profiling means configuring with -DTPJ_TRACY=ON and running the Tracy server.
