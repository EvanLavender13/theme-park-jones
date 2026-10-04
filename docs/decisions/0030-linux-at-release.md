# 0030. Check each change on Windows, and Linux at release

Status: Accepted, 2026-10-02; where tidy reads its compile commands superseded by 0035

## Context

Decision 0026 moved iteration to windows-debug but kept linux-debug, with AddressSanitizer and UBSan, as the check every feature ends with and every push runs. Decision 0021's pre-push hook built linux-debug and ran its tests, and decision 0022's cross-build check ran on pushes that touched the simulation. On the Windows drive, through WSL, with the sanitizers, that build is many times slower than Windows. Planning full-park showed what that costs: generating the full park's minute of play took 1 min 46 s on windows-release and had not finished after over an hour on linux-debug. Every feature and every push waited on the slowest build the project has, for checks that rarely found anything the Windows build and tidy had not.

## Decision

Each change is checked on Windows. A change is finished when windows-debug builds without warnings, its tests pass, and scripts/tidy.sh is clean. tidy keeps reading linux-debug's compile commands, which need linux-debug configured, not built. The pre-push hook runs the same three checks.

Linux is checked at release. scripts/release-check.sh builds linux-debug with AddressSanitizer and UBSan, warnings as errors, runs its tests, and runs the cross-build check (decision 0022). It is run before a release, and whenever Evan asks.

Work that must leave the simulation unchanged, as decision 0028 requires of engineering capabilities, shows it on Windows. It compares tpj_scenarios's windows-debug output over the scenarios and tests/parks/*.park, before and after, with tpj_scenarios --compare. The cross-build check, which compares Windows with Linux, runs at release.

This supersedes decision 0026, and decision 0021's pre-push check of linux-debug. Decision 0022's guarantee stands: a park simulates bit-identically on every supported build. Only when it is checked changes.

## Consequences

A memory error the sanitizers would catch, or a change that makes Windows and Linux simulate differently, surfaces at the next release check rather than at the change that caused it. Finding the cause may then mean bisecting the commits since the last release check. The Windows build cannot run the sanitizers, since MSYS2's GCC does not ship them, so between release checks nothing looks for out-of-bounds reads or use after free. In exchange, a feature's confirmation and a push take minutes, not the better part of an hour.
