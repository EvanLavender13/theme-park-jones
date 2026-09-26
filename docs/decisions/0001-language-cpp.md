# 0001. C++ as the implementation language

Status: Accepted, 2026-09-26

## Context

The game needs tight control over memory layout and performance for a simulation and a custom renderer, and Evan is an experienced C++ developer.

## Decision

The project is written in C++20.

## Consequences

Tooling is CMake, GCC, and clang-tidy (see 0008). Language subset and conventions are still an open question.
