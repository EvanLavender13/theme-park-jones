# Research: stepped-fields

## How does a resolution know which sources' resolved entries it changed?

The layer rule clears a source's stepped entries when a resolution changes its resolved entries. That needs the entries from before the resolution and the entries after every producer has published. The field's resolver runs first, so it can set the old entries aside, but nothing in the cycle runs after the last resolver.

Compilers and build systems that recompute derived data face the same question. Salsa's red-green algorithm and Bazel's Skyframe recompute a derived value and compare it with the old one by value. When they are equal, everything downstream is marked unchanged, which is called early cutoff. The comparison happens only after the value has been recomputed, and the old value is kept until then. A game engine's frame has the same shape: late update and end-of-frame phases exist because some work can only run once every other system has written.

What this suggests: the field's resolver keeps the previous resolution's entries, a phase after every resolver compares them with the new entries, source by source, and then clears the stepped entries of the sources that differ. The comparison should use the walk's words, which compare doubles by their bits, so "changed" means exactly what world equality means. A resolution with no previous entries, the first one after construction or a load, compares nothing, and so a loaded world resolves to the world that was saved.

Rejected: Comparing at sample time and masking stepped entries instead of clearing them — the old entries are derived, so they are gone after a load and a loaded world would sample differently from the one saved. Clearing a source's stepped entries whenever it republishes — every resolution republishes, so every command would clear every source, and the milestone asks for clearing only on change. Clearing all stepped entries on every resolution after a command — simpler, but an unrelated command would blank every offer and footfall entry for a tick. A resolver registered last by convention — nothing enforces it, and every later module would have to register before it.

Sources: https://salsa-rs.github.io/salsa/reference/algorithm.html — the red-green algorithm and early cutoff by value comparison; https://bazel.build/reference/skyframe — change pruning after re-evaluation; https://docs.unity3d.com/Manual/ExecutionOrder.html — LateUpdate as a phase after every Update.
