# Research: deterministic-simulation

## What breaks floating-point determinism between the two builds, and where do exp and log come from?

Floating-point determinism breaks in five ways:

- Different instruction paths: the x87 unit keeps intermediates at 80 bits, while SSE rounds each operation to its declared width.
- Compilers reordering or contracting arithmetic, most often into fused multiply-add.
- Changes to the floating-point control state, such as the rounding mode or precision, made by some other library.
- Math libraries whose transcendental functions differ in the last bit.
- Differences between compilers and compiler versions.

IEEE 754 requires +, -, *, / and square root to be correctly rounded, so those give identical results everywhere once the instruction path and the order of operations are fixed. It only recommends correct rounding for exp, log, and trigonometry, and glibc and the Windows UCRT make different choices there.

For this project, both builds target x86-64, where SSE2 is the default for double arithmetic, and both use GCC. Decision 0022 already turns off contraction and fast-math. The remaining gap is the C runtime's transcendental functions. There are two ways to close it:

- Port a small, well-known implementation built only from basic operations, such as musl's exp and log (MIT, derived from fdlibm and ARM's optimized routines) or the smaller fdlibm originals. Compiled into tpj_sim under the same flags, the port gives bit-identical results on both builds without being correctly rounded, because every step is a correctly rounded basic operation in a fixed order.
- Use CORE-MATH's correctly rounded implementations (MIT, portable C). A correctly rounded function has exactly one right answer, so it agrees with every other correct implementation, including future ones, and with or without FMA. The cost is more code and some use of 128-bit integers, which MinGW GCC supports.

Two other conditions still apply. Long double must stay out of the simulation, because it is 80-bit on MinGW and on Linux alike, but printing and parsing of it differ. The rounding mode must be the default, which a check at startup can assert.

Rejected:

- Fixed-point arithmetic throughout: decision 0022 settled on doubles with controlled flags, and fixed-point would complicate every curve and score.
- Trusting the C runtime's functions and checking the hash: glibc and UCRT are known to differ in the last bit, so the check would fail with no fix in reach.

Sources:

- https://gafferongames.com/post/floating_point_determinism/: causes (x87 against SSE, contraction, control state, libraries) and mitigations.
- https://randomascii.wordpress.com/2013/07/16/floating-point-determinism/: which operations IEEE 754 requires to be correctly rounded, and the practical sources of divergence.
- https://core-math.gitlabpages.inria.fr/: correctly rounded exp and log in portable C under MIT.
- https://core-math.gitlabpages.inria.fr/faq.html: the scope and testing of CORE-MATH.

## How should keyed, stateless random draws work?

Counter-based generators apply a keyed mixing function to a counter. They need no stored state, run in parallel, and give any number in a stream by index. Philox, from Salmon and colleagues at D. E. Shaw, passes TestU01's BigCrush and offers at least 2^64 independent streams. NumPy ships it as a standard generator. Squirrel Eiserloh's noise-based RNG (GDC 2017) makes the same argument for games. A good integer hash of (position or index, seed) acts as a lookup into an infinite table of rolled numbers. That gives unordered access, easy reseeding, record and playback, and lock-free parallelism, and is simpler than a stateful generator. SplitMix64's finalizer is another widely used mixing function for this.

For this project, a draw is a hash of (world seed, stable entity key, purpose, tick, index). Distributions built on top, such as a uniform double in [0, 1) or a pick weighted by softmax, must be project code, because the standard library's distributions differ between implementations (decision 0019). Uniform doubles come from the top 53 bits of the hash, which is exact and portable.

Draws are keyed on an entity, so the entity key must be stable across copies, saves, and loads. EnTT's handles are recycled with a version, and they are kept intact by EnTT's snapshot loader but not by its continuous loader. A dedicated monotonic key, saved with the world, avoids depending on either.

Rejected:

- Stateful per-entity generators such as PCG: the state must be saved and copied, and a new purpose within an entity shifts its later draws.
- One world generator: every change in draw counts reshuffles everything after it, and results depend on system order.
- Cryptographic hashes such as MD5 or SHA: slower than needed, and no quality benefit for simulation.

Sources:

- https://www.thesalmons.org/john/random123/papers/random123sc11.pdf: counter-based generators, Philox, statistical quality.
- https://numpy.org/doc/stable/reference/random/bit_generators/philox.html: Philox as a standard, keyed generator.
- https://www.gdcvault.com/play/1024365/Math-for-Game-Programmers-Noise: noise-based RNG for games and its benefits.
- https://gist.github.com/kevinmoran/0198d8e9de0da7057abe8b8b34d50f86: the improved Squirrel noise function.

## How do deterministic games copy, hash, and save their state, and what does EnTT offer?

Factorio runs deterministic lockstep: every client simulates every tick identically, and only inputs cross the network. Desyncs are found by comparing CRCs of the game state. In development, a replay computes a CRC of the whole map every tick and compares it with the recorded run, and a mismatch triggers saves from both runs at that tick, tagged so they can be read. Because the check runs every tick, the difference is usually a single variable, and that is what makes desyncs debuggable. A cheaper heuristic CRC, covering only key state, runs during normal play. Determinism also makes replays possible, and makes tests and bug reports reproducible from their initial conditions.

For this project, the useful lessons are these:

- Hash every tick in tests, not just at the end, so a divergence is caught where it starts.
- Keep a way to dump the state readably when hashes differ.
- Make the hash cover everything, because an incomplete hash hides exactly the bugs it exists to find.

EnTT does not copy registries directly. An earlier clone function was removed. Copying means iterating each component storage and copying its contents, which is only possible for copyable component types. EnTT's snapshot facility serializes entities and components through a user-supplied archive and restores them in the same order. Its snapshot loader keeps entity identifiers intact, while its continuous loader remaps them.

That points to one registered walk over component types, with each module supplying copy, compare, hash, and encode functions for its own types without exposing their layout (principle 6). The walk visits types in a fixed registration order, and entities in a fixed key order, never in storage order (decision 0016).

Rejected:

- Relying on EnTT storage order for hashing or saving: storage order depends on the history of insertions and removals, not only on the current state.
- A heuristic partial hash as the main check: it misses divergence in whatever it leaves out.

Sources:

- https://www.factorio.com/blog/post/fff-47: CRC of the whole map every tick, and comparing saves when they differ.
- https://wiki.factorio.com/Desynchronization: lockstep and the heuristic CRC.
- https://github.com/skypjack/entt/issues/1020: cloning a registry after clone() was removed.
- https://skypjack.github.io/entt/md_docs_2md_2entity.html: snapshots, archives, and the two loaders.

## What makes a save file byte-stable?

Re-saving a loaded park must reproduce the file byte for byte, as the slice's criterion 6 requires. That needs five things:

- A fixed order for everything: sections, entities sorted by stable key, and fields.
- No derived data, which principle 1 already requires.
- A fixed encoding for numbers.
- A version header.
- No timestamps or other ambient data.

Numbers can be encoded in binary or in text. In binary, doubles are raw IEEE bits in a fixed byte order. In text, doubles are written in the shortest form that reads back to the same bits, which C++17's std::to_chars produces in libstdc++ on both builds. The shortest round-trip form is unique, so the two builds agree. Text files can be reviewed in diffs and edited by hand, which matters because the slice checks park files into tests/parks/. Binary files are smaller and faster, which matters only at a scale the park has not reached.

Rejected:

- printf-style formatting of doubles: its precision handling can lose bits, and the output can depend on locale.
- Serializing EnTT storage directly: its order depends on insertion history, not only on state.

Sources:

- https://en.cppreference.com/w/cpp/utility/to_chars: the shortest round-trip guarantee of std::to_chars.
