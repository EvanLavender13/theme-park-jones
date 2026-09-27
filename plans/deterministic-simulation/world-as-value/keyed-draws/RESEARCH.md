# Research: keyed-draws

## How does a 64-bit draw become a uniform double in [0, 1)?

Shift the draw right by 11 bits, keeping its top 53, and multiply by 2^-53. Every result is a multiple of 2^-53 in [0, 1 - 2^-53]; each is equally likely, and the conversion is exact, so it is identical on every build. Vigna recommends this form for his generators, and the top bits of a SplitMix64 output are as good as the low ones.

Rejected: filling a double's mantissa by setting the exponent bits for [1, 2) and subtracting 1 — it keeps only 52 bits, so the lowest bit of every result is zero. Drawing more than 53 bits to reach the smaller doubles near zero, as some libraries do — it needs more than one draw per value, and nothing here needs values below 2^-53.

Sources: https://prng.di.unimi.it/ — the recommended conversion to double; https://github.com/rust-random/rand/issues/426 — why the exponent-bit form loses a bit.

## How does a draw pick from integer weights?

Lemire's multiply-shift maps a 64-bit value x to an index in [0, s) as the high 64 bits of the 128-bit product x times s. It needs no division. Each result occurs either floor(2^64 / s) or ceil(2^64 / s) times over all x, so without rejection the bias toward any index is below s / 2^64, which for any total weight a park uses (well under 2^32) is below 2^-32. Lemire removes the bias by rejecting a small band of x and drawing again. A keyed draw has no next value to take, so rejection would have to spend further indices of the key. The pick then walks the weights in order and returns the first whose running total exceeds the mapped value. The weights' sum must fit in 64 bits.

The high half of a 64 by 64 bit product can be computed with unsigned __int128 on GCC for both x86-64 targets, but -Wpedantic rejects the type unless it is marked __extension__. Four 32-bit partial products give the same high half in standard C++ in a few lines.

Rejected: x modulo s — needs a division, and its bias toward small results is the same size. Rejection sampling over further indices — exact, but it makes one pick consume a variable number of draws, which the key does not model, for a bias far below anything a test over a million samples could see.

Sources: https://arxiv.org/abs/1805.10941 — Lemire, "Fast Random Integer Generation in an Interval", the multiply-shift map and its bias bound; https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/ — the map without rejection.

## How does a draw pick from double weights?

Sum the weights in order, draw a uniform double u, set the target to u times the total, and return the first index whose running sum exceeds the target. Because the running sum at the last index is computed by the same additions in the same order as the total, it equals the total exactly. For a normal total the target always rounds below the total: u is at most 1 - 2^-53, so the exact product lies at least half an ulp below the total, and exactly half an ulp only when the total is a power of two, where the product is representable. Some index is therefore always returned. A subnormal total breaks this: the spacing of doubles there is a fixed 2^-1074, the target rounds to a few coarse values, and the picks stop being proportional (two weights of 2^-1074 split about 25 to 75). Refusing a total below the smallest normal double avoids it. An index with zero weight never has a running sum greater than the one before it, so it is never the first to exceed the target and is never picked. The failure mode found in other libraries is weights whose sum overflows to infinity: the target becomes infinite or NaN and every pick falls through to the last element. Refusing an infinite total, and any negative, infinite, or NaN weight, closes it.

Rejected: normalizing the weights to sum to one first — it adds a division per weight and its own rounding, and a check that the sum is close to one. Alias tables — constant time per pick after a linear setup, worth it only for many picks from one fixed weight list, which softmax over changing scores is not.

Sources: https://github.com/arthurmaciel/ipe-lang/issues/2642 — an overflowing total biasing every pick to the last element; https://github.com/blwatkins/typescript-utils/issues/117 — the last element silently absorbing rounding error.

## How is a million uniform draws tested?

Pearson's chi-square test over equal-width bins. With a million samples and 1000 bins, each bin expects 1000 samples, far above the usual minimum of five, and the statistic has 999 degrees of freedom. Its upper 0.001 critical value is 1142.85, and its lower 0.001 critical value, below which the counts are suspiciously even, is 866.55 (computed from the regularized incomplete gamma function). The draws come from fixed keys, so the statistic is one fixed number: the significance level is the chance that a correct generator would have been unlucky for the keys chosen, not a flake rate.

## What tolerance fits weighted picks over a million draws?

Each option's count is binomial with mean n p and standard deviation sqrt(n p (1 - p)), at most 500 for n of a million. A bound of five standard deviations per option fails a correct pick with probability about 6 in 10 million per option, and still catches a bias of 0.25 percentage points in any option. A chi-square test over the options is the alternative when a single statistic is wanted; its critical value depends on the number of options.
