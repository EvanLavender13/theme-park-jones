# Research: world-as-value

## What does porting musl's exp and log involve?

musl's exp and log are Arm's optimized routines (MIT, Arm Limited, 2018). Each is one source file plus a data file of tables and polynomial coefficients: exp.c with exp_data.c, and log.c with log_data.c. Both use 128-entry tables (EXP_TABLE_BITS and LOG_TABLE_BITS are 7). Their helpers come from musl's internal libm.h, and each one has a direct replacement in C++20:

- asuint64 and asdouble become std::bit_cast.
- eval_as_double, fp_barrier, fp_force_eval, and predict_false exist to control excess precision, force floating-point exceptions, and hint branches. On x86-64 SSE2 with contraction off they change no results, so they become plain expressions.
- __math_oflow, __math_uflow, __math_divzero, and __math_invalid compute results that raise floating-point exceptions and set errno. The port returns the same values (infinity, zero, negative infinity, NaN) directly, since the simulation never reads exception flags or errno.

The conditional paths settle themselves on this project's builds. TOINT_INTRINSICS is off on x86-64, so exp uses its portable shift-based rounding. __FP_FAST_FMA is undefined without -mfma, so log takes its path without fused multiply-add, which is the path contraction-off requires anyway. WANT_ROUNDING only guards non-default rounding modes. Those are excluded by asserting the default mode, so the port can drop them.

Arm documents the error bounds as about 0.51 ULP for exp and 0.52 ULP for log. The routines are therefore not correctly rounded. A reference table of correctly rounded values, generated once at high precision and checked in, can test them within 1 ULP. Bit-identity across builds comes from the cross-build script comparing the port's own outputs, not from the table.

Rejected:

- fdlibm's older exp and log: smaller tables, but slower and less accurate, with no offsetting benefit, since table size does not matter here.
- Keeping musl's exception-raising helpers: nothing reads the flags, and they need compiler barriers that add no determinism.

Sources:

- https://git.musl-libc.org/cgit/musl/tree/src/math/exp.c: the exp routine, its helpers, and its conditional paths.
- https://git.musl-libc.org/cgit/musl/tree/src/math/log.c: the log routine, its tables, and its FMA path.
- https://github.com/ARM-software/optimized-routines: the upstream source and its stated error bounds.

## How is the no-transcendentals rule checked mechanically?

`nm -u` on the static archive libtpj_sim.a lists each object's undefined symbols. A call to the C runtime's exp, including one made through std::exp, shows up as an undefined exp. The check fails on a denylist of transcendental names: exp, exp2, expm1, log, log2, log10, log1p, pow, sin, cos, tan, their inverse and hyperbolic forms, atan2, cbrt, hypot, erf, erfc, tgamma, and lgamma, each with its f and l variants, plus glibc's older __exp_finite-style aliases.

It has to be a denylist, not an allowlist of everything libm exports. GCC emits real calls to some exactly rounded functions. std::sqrt compiles to the sqrtsd instruction with a fallback call to sqrt for negative inputs, because of errno. floor and ceil become calls on baseline x86-64 without SSE4.1. Those functions are exact in every runtime, so they are safe.

The check runs on the Linux build as a test. The sources are shared, and tpj_sim has no platform branches, so the symbols are the same on both builds. The planted violation is a small object compiled with a call to exp, which the check must reject.

Rejected:

- Grepping the sources for exp( and similar: it misses calls through templates and std:: overloads, and flags comments.
- Linking tpj_sim with -nostdlib or a stub libm: it breaks the exact functions GCC calls legitimately, and sanitizer runtimes need libm.

Sources:

- https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html: -fmath-errno, and why sqrt keeps a library call.
- https://sourceware.org/binutils/docs/binutils/nm.html: nm's undefined-symbol listing.

## Which mixing function serves the state hash and the keyed draws?

Both need a fast, portable 64-bit function whose output depends on every input bit. Neither needs cryptographic strength. The state hash detects accidental change, and the draws need good statistical quality.

SplitMix64's finalizer, a multiply-xorshift mix from Steele, Lea, and Flood that Vigna uses to seed his xoshiro generators, passes BigCrush when applied to a counter. A draw can chain it over the key's words: seed, entity key, purpose, tick, and index, mixing after each. The output is 64 bits, so a uniform double in [0, 1) takes the top 53 bits exactly. The state hash can be the same running mix over the words the walk produces. Each registered type's hash function feeds its fields as 64-bit words: integers widened, and doubles as their bit patterns.

Hashing doubles by their bits makes -0.0 and 0.0 differ, which is correct for bit identity. NaN payloads do not survive text saves, since std::to_chars writes any NaN as "nan". NaN in simulation state is therefore treated as an error, caught in debug by the walk.

Rejected:

- Philox: its quality is unmatched, but it needs 128-bit counters and more code than a keyed draw needs.
- Squirrel Eiserloh's noise functions: 32-bit output, too narrow for a 53-bit double from one draw.
- A third-party hash library such as xxHash: good, but it adds a dependency for a few lines of code.
- FNV-1a: weak mixing of low bits, which makes near-identical worlds more likely to collide.

Sources:

- https://prng.di.unimi.it/splitmix64.c: the SplitMix64 reference implementation and its constants.
- https://dl.acm.org/doi/10.1145/2714064.2660195: Steele, Lea, and Flood, "Fast splittable pseudorandom number generators".

## How does the walk copy an EnTT registry and detect unregistered types?

registry.storage() iterates every storage the registry holds, each with a type_info. A storage whose type is not registered with the walk, and is not EnTT's own entity storage, means an unregistered component. The walk checks this in debug builds, before a copy, hash, or save.

A copy creates each entity in the destination and copies each registered type's components, visiting types in registration order and entities in key order. registry.create(hint) can recreate an entity with a given identifier. Copies need not rely on it, though: entities are identified by their stable keys, and a component that refers to another entity holds its key, never an entt::entity. So the copy, the original, and a loaded save all agree on every reference, whatever identifiers EnTT hands out.

Registration must happen in a fixed order. C++ leaves the order of static initializers across translation units unspecified, so self-registering types would give an order that depends on link order. Registration is an explicit function that calls each module's registration in a written order.

Rejected:

- Storing entt::entity in components as references: identifiers are recycled with versions and are not preserved by every load path, so references would break across saves.
- Static self-registration: its order depends on the link.

Sources:

- https://skypjack.github.io/entt/md_docs_2md_2entity.html: storage iteration, type_info, and create with a hint.
- https://en.cppreference.com/w/cpp/language/siof: the static initialization order problem.

## How does a WSL script drive the Windows build and gate a push?

From WSL, cmake.exe configures and builds the Windows preset in the same repository on the Windows drive, as CLAUDE.md already does, and Windows executables run directly through WSL interop. Their text output uses CRLF line endings, so the script strips carriage returns before comparing. Any path passed to the Windows executable goes through wslpath -w.

git feeds pre-push one line per ref on stdin: local ref, local sha, remote ref, remote sha. For an update, the pushed commits are remote..local. For a new branch, the remote sha is all zeros, and the commits not yet on the remote are local minus the remote's existing refs (git rev-list local --not --remotes). A deleted ref has an all-zero local sha and pushes nothing. `git diff --name-only` over the pushed commits, limited to tpj_sim's build inputs, decides whether the cross-build script runs.

Rejected:

- Running the check on every push: building the Windows side takes minutes, which is wasted on pushes that touch only plans or rendering.
- Diffing against the working tree: the hook must judge what is being pushed, not what is on disk.

Sources:

- https://git-scm.com/docs/githooks#_pre_push: the pre-push stdin format.
- https://learn.microsoft.com/en-us/windows/wsl/filesystems: running Windows executables from WSL, and wslpath.
