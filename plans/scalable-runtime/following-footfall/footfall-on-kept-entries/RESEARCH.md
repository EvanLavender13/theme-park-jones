# Research: Footfall on Kept Entries

## Which rule decays an empty stretch's value?

Today an empty stretch's value takes one step of V + (0 - V) / FOOTFALL_TIME each tick, which multiplies it by 1 - 1/FOOTFALL_TIME up to rounding. With the value held at the tick it last changed, the decay owed after k idle ticks is that factor raised to k. Two ways compute it identically on every build. The closed form value * simExp(k * simLog(1 - 1/FOOTFALL_TIME)) uses tpj_sim's ports of musl's exp and log. Its cost is the same for every k, and it is stated in one sentence. Its error comes mostly from simLog's last bit, multiplied by k: after 100,000 idle ticks, about an hour of play, the relative error is around 1e-13. That is far below the two decimals the shop ghost's tooltip shows, and below anything else that reads footfall, since nothing else does. simExp of -0.0 is exactly 1, through musl's tiny-argument path, so the form gives the value unchanged at 0 ticks. So a stretch with guests on it every tick moves exactly as it does today, bit for bit. Once a value is small enough, simExp underflows to 0, after about 220,000 idle ticks, which is the right limit.

Rejected: repeated squaring of the factor. It is just as deterministic, and its accuracy is similar, about two roundings per bit of k. But its cost grows with log k, and the spec would have to state the loop's order of multiplications to fix its bits. Replaying k steps of today's rule on read. It matches today's bits exactly, but a read after an hour idle owes about 100,000 steps (the milestone's RESEARCH.md).

## How do values carried into one stretch combine?

A resolution carries each kept entry by carryOver, as carryFootfall carries Footfall's entries today. Afterwards a stretch can hold none, one, or several of them: several when stretches join, as when deleting a shop's connector removes the node between two spine stretches. Today the next step sums them, each at the value the last step gave it. With kept entries they carry different ticks, so adding them needs a common tick. The milestone fixes that: each is decayed to the resolution's tick and added, and the sum is held with that tick.

A stretch that one entry is carried into is the common case: every stretch an edit does not touch. Keeping that entry's value and tick, and moving it only to its stretch's midpoint, leaves those stretches exactly as they were. So an edit changes footfall's bits only where it joins stretches. Re-reading every carried entry at the resolution's tick would change, in its last bits, every stretch's value at every edit, for no gain.

Rejected: decaying and restamping every carried entry at each resolution — as above. Keeping every carried entry and summing a stretch's entries when it is read or stepped — the step would have to find and merge them, and the milestone holds one entry per stretch.

## How does a tick find the stretches guests are on without visiting the network?

stepFootfall must sum each stretch's guests' hunger in ascending key order, as today, and touch only those stretches. Taking each guest in key order and pairing its stretch's index with its hunger, then stable-sorting the pairs by stretch, groups each stretch's guests with key order kept within each group. The cost is the guests times a logarithm.

Rejected: an array of sums sized to the network's edges, as today. It is allocated and zeroed every tick, so the tick's work grows with the network, which decision 0031 forbids. A hash map from stretch to sum. Iterating it gives no fixed order, so it would need sorting anyway, and it allocates per entry.

Sources: src/sim/musl_exp.cpp and src/sim/musl_log.cpp — simExp's tiny-argument path and both ports' accuracy; src/sim/guests/footfall.cpp and src/sim/guests/SPEC.md — today's step, node mean, and carrying; tests/sim/guests/footfall_test.cpp — the joining case today's tests exercise; plans/scalable-runtime/following-footfall/RESEARCH.md — forward decay and the rejected replay.
