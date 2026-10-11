# Research: sim-components

## How should one large implementation file become several components?

Lakos's physical design treats a component as a header and a source file with the same root name. The source file includes its own header first. Components depend on each other only through headers, and those dependencies form no cycle. Bloomberg's BDE library applies this rule throughout, and it groups components into packages. The project's conventions already match the file-level rules: a .cpp includes its matching header first, and names match contents.

For this milestone, that means each concern in a large .cpp becomes its own .cpp with its own internal header. The public header stays the module's interface. The public functions in a split file become thin entry points that call into the components. Pick the split so the includes between the new components run one way. For example, the edit planner may include the overlap checks, but the overlap checks must not include the planner.

Helpers that sit in an anonymous namespace today stay there if only one new file uses them. If two files need a helper, declare it in an internal header in namespace tpj and define it once in a .cpp. Don't put an anonymous namespace in a header. Each file that includes it would get its own copy, and SEI CERT DCL59-CPP warns against it for that reason. park/internal/curves.h is the existing precedent: one internal header, one definition in park/curves.cpp. Templates are the exception. guests.cpp's forEachNeed takes callers' lambdas with template call operators, and edits.cpp's applyPlan runs on two store types. If two components share a template, define it in the internal header.

The project already has the mechanism for private headers. A header in a directory named internal is private to that directory's parent, and cmake/check_private_headers.cmake enforces it. A header in sim/park/edits/internal/ would be private to sim/park/edits, which is stricter than sim/park/internal/. Only files under sim/park/edits may include it, so the file that defines edits.h's functions must move into that subdirectory too. edits.h can stay in sim/park, because its path is all that callers see. The layer check counts a subdirectory as part of its module's unit, because sim/park is declared as a directory unit. A new subdirectory needs no change to cmake/layers.txt.

Rejected: keep one file and add section comments — 0027 asks for one concern per component, and comments neither stop growth nor give a header to test against. Move ordinary helpers into a header as inline or anonymous-namespace functions — this copies them into each including file, and it puts implementation in headers. Headers in sim/park/internal/ with sources in a subdirectory — it separates each component's header from its source. Split the public headers in the same work — the user chose to keep them; listing each module's public surface stays a separate capability candidate. Tooling that splits a translation unit automatically (a CppCon 2026 talk) — it serves compile time on generated code, not ownership of concerns.

Sources: https://github.com/bloomberg/bde/wiki/physical-code-organization — component, package, and package group rules; https://www.informit.com/store/large-scale-c-plus-plus-software-design-9780201633627 — Lakos's components and levelization; https://wiki.sei.cmu.edu/confluence/display/cplusplus/DCL59-CPP.+Do+not+define+an+unnamed+namespace+in+a+header+file — why helpers don't go in anonymous namespaces in headers; https://cppcon2026.sched.com/event/2RT6E — automated translation unit splitting, rejected.

## Can moving code between translation units change the simulation?

Moving a function to another file changes what the compiler can inline. In general, a different inlining context can change floating-point results. The main cause is contraction of a multiply and an add into one fused instruction, which skips a rounding step. GCC contracts by default.

tpj_sim compiles with -ffp-contract=off (decision 0022), so GCC fuses nothing in either context. The builds are 64-bit, so arithmetic uses SSE and has no x87 excess precision. No build uses -ffast-math or LTO. Under these flags, each operation rounds the same way wherever it is compiled. The project already relies on this: scripts/cross-build-check.sh shows that windows-debug, with no inlining, simulates exactly like the optimized release builds.

So `tpj_scenarios --compare` before and after each split is a sound check that behavior is unchanged. Run cross-build-check.sh too, because inlining differs most in the release builds.

Rejected: mark moved functions noinline to keep inlining as it was — the flags already make it irrelevant, and it would cost speed in the gameplay runtime.

Sources: https://gcc.gnu.org/ml/gcc-help/2015-01/msg00162.html — inlining context can change floating-point results without strict flags; https://inbox.sourceware.org/gcc-patches/ZBNJlxyrvv9nfswE@tucnak/T/ — GCC's -ffp-contract default and what off disables; https://lists.libreplanet.org:443/archive/html/bug-glpk/2020-07/msg00022.html — fused multiply-add giving different bits in practice.
