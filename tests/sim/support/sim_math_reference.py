#!/usr/bin/env python3
"""Writes tests/sim/support/sim_math_reference.h: correctly rounded values of e^x and ln x.

Python's decimal module computes exp and ln correctly rounded to the context's precision. At 80
significant digits, rounding that value to the nearest double gives the correctly rounded double
unless the true value lies within 10^-79 of a midpoint between two doubles, and the script checks
every entry against that, so every value in the table is the correctly rounded result.

The arguments are drawn from a fixed seed. musl's exp reads a 128-entry table indexed by
round(x * 128 / ln 2) mod 128, and its log a 128-entry table indexed by the top seven bits of the
mantissa, so the domain samples take one argument per index, spread at random over each function's
finite domain, and a mis-ported table entry cannot go unchecked. Separate groups cover the ranges
the functions handle apart from the rest. Regenerate from the repository root with:

    python3 tests/sim/support/sim_math_reference.py > tests/sim/support/sim_math_reference.h
"""

import decimal
import math
import random
import struct
import sys

PRECISION = 80
CONTEXT = decimal.Context(prec=PRECISION, Emax=999999, Emin=-999999,
                          rounding=decimal.ROUND_HALF_EVEN)
# Wide enough to hold any double, and the midpoint of any two, exactly.
EXACT = decimal.Context(prec=2000, Emax=999999, Emin=-999999)

DBL_MAX = sys.float_info.max
DENORM_MIN = math.ldexp(1.0, -1074)
LN2 = decimal.Decimal(2).ln(CONTEXT)


def from_bits(bits):
    return struct.unpack("<d", struct.pack("<Q", bits))[0]


def correctly_rounded(value):
    """The double nearest value, after checking value is far enough from a midpoint to be sure."""
    nearest = float(value)  # Python converts decimal strings to doubles correctly rounded
    exact = decimal.Decimal(nearest)
    if math.isinf(nearest):
        below = decimal.Decimal(DBL_MAX)
        midpoint = EXACT.add(below, EXACT.power(decimal.Decimal(2), 970))
        ulp = EXACT.power(decimal.Decimal(2), 971)
    else:
        neighbour = math.nextafter(nearest, math.inf if value >= exact else -math.inf)
        if math.isinf(neighbour):
            midpoint = EXACT.add(exact, EXACT.power(decimal.Decimal(2), 970))
            ulp = EXACT.power(decimal.Decimal(2), 971)
        else:
            other = decimal.Decimal(neighbour)
            midpoint = EXACT.divide(EXACT.add(exact, other), decimal.Decimal(2))
            ulp = EXACT.abs(EXACT.subtract(other, exact))
    distance = EXACT.abs(EXACT.subtract(value, midpoint))
    if distance <= EXACT.multiply(ulp, decimal.Decimal("1e-40")):
        raise AssertionError(f"{value} is too near a midpoint to round with confidence")
    return nearest


def reference_exp(x):
    return correctly_rounded(CONTEXT.exp(decimal.Decimal(x)))


def reference_log(x):
    return correctly_rounded(CONTEXT.ln(decimal.Decimal(x)))


def nearest_double(value):
    return float(value)


def around(value, steps=2):
    """The double nearest value and its neighbours, steps each way."""
    centre = nearest_double(value)
    below = [centre]
    for _ in range(steps):
        below.append(math.nextafter(below[-1], -math.inf))
    above = [centre]
    for _ in range(steps):
        above.append(math.nextafter(above[-1], math.inf))
    return sorted(set(below + above))


def random_in_binade(rng, exponent):
    """A double in [2^exponent, 2^(exponent + 1)) with a random mantissa."""
    return math.ldexp(1.0 + rng.getrandbits(52) * 2.0**-52, exponent)


def exp_groups(rng):
    groups = []

    # One argument per table index, anywhere in the domain where e^x is neither 0 nor infinite.
    lowest = math.ceil(-745.1 * 128 / math.log(2))
    highest = math.floor(709.7 * 128 / math.log(2))
    domain = []
    for index in range(128):
        k = rng.randrange(lowest + index, highest, 128)
        # Keep round(x * 128 / ln 2) on k, away from the edges of its interval.
        offset = rng.uniform(-0.4, 0.4)
        domain.append((k + offset) * math.log(2) / 128)
    groups.append(("the finite domain, one argument per table index", domain))

    # e^x rounds to 1 below 2^-54 in magnitude; the binades straddle that and reach the tiniest.
    near_zero = []
    for exponent in (-1, -4, -8, -16, -32, -53, -54, -55, -100, -1022):
        magnitude = random_in_binade(rng, exponent)
        near_zero += [magnitude, -magnitude]
    near_zero += [DENORM_MIN, -DENORM_MIN]
    groups.append(("arguments near 0", near_zero))

    # Above ln(DBL_MAX + half an ulp), e^x rounds to infinity.
    overflow = CONTEXT.ln(decimal.Decimal(DBL_MAX) + decimal.Decimal(2) ** 970)
    groups.append(("around the overflow threshold", around(overflow)))

    # Below -1022 ln 2, e^x is subnormal.
    subnormal = CONTEXT.multiply(decimal.Decimal(-1022), LN2)
    groups.append(("around the threshold where the result turns subnormal", around(subnormal)))
    lower = float(CONTEXT.multiply(decimal.Decimal(-1075), LN2))
    upper = float(subnormal)
    groups.append(("results inside the subnormal range",
                   [rng.uniform(lower, upper) for _ in range(4)]))

    # Below -1075 ln 2, e^x is under half the smallest subnormal and rounds to zero.
    zero = CONTEXT.multiply(decimal.Decimal(-1075), LN2)
    groups.append(("around the threshold where the result rounds to zero", around(zero)))

    return [(name, [(x, reference_exp(x)) for x in args]) for name, args in groups]


def log_groups(rng):
    groups = []

    # One argument per value of the mantissa's top seven bits, with a random exponent over the
    # normal range and random low bits.
    domain = []
    for index in range(128):
        exponent = rng.randint(-1022, 1023)
        bits = ((exponent + 1023) << 52) | (index << 45) | rng.getrandbits(45)
        domain.append(from_bits(bits))
    domain += [math.ldexp(1.0, -1022), DBL_MAX]
    groups.append(("the finite domain, one argument per table index, and its normal ends",
                   domain))

    # ln x loses relative accuracy to cancellation as x nears 1, where it is handled apart.
    near_one = [math.nextafter(1.0, 0.0), math.nextafter(1.0, 2.0)]
    for exponent in (-2, -4, -5, -8, -16, -32, -52):
        delta = random_in_binade(rng, exponent)
        near_one += [1.0 + delta, 1.0 - delta]
    groups.append(("arguments near 1", near_one))

    subnormals = [DENORM_MIN, math.ldexp(1.0, -1022) - DENORM_MIN]
    for width in (1, 13, 30, 51):
        subnormals.append(from_bits((1 << width) | rng.getrandbits(width)))
    groups.append(("subnormal arguments", subnormals))

    return [(name, [(x, reference_log(x)) for x in args]) for name, args in groups]


def literal(value):
    if math.isinf(value):
        return "INF" if value > 0 else "-INF"
    return value.hex()


def print_table(name, groups):
    count = sum(len(entries) for _, entries in groups)
    print(f"inline constexpr std::array<SimMathReference, {count}> {name} = {{{{")
    for reason, entries in groups:
        print(f"    // {reason[0].upper()}{reason[1:]}.")
        for argument, expected in entries:
            print(f"    {{.Argument = {literal(argument)}, .Expected = {literal(expected)}}},")
    print("}};")


def main():
    rng = random.Random(20260927)
    exp = exp_groups(rng)
    log = log_groups(rng)

    print("// Generated by tests/sim/support/sim_math_reference.py: correctly rounded e^x and ln x.")
    print("// Do not edit; regenerate from the repository root with:")
    print("// python3 tests/sim/support/sim_math_reference.py > tests/sim/support/sim_math_reference.h")
    print("#ifndef TPJ_TESTS_SIM_SUPPORT_SIM_MATH_REFERENCE_H")
    print("#define TPJ_TESTS_SIM_SUPPORT_SIM_MATH_REFERENCE_H")
    print()
    print("#include <array>")
    print("#include <limits>")
    print()
    print("namespace tpj::test {")
    print()
    print("struct SimMathReference {")
    print("  double Argument = 0.0;")
    print("  double Expected = 0.0;")
    print("};")
    print()
    print("inline constexpr double INF = std::numeric_limits<double>::infinity();")
    print()
    print_table("EXP_REFERENCE", exp)
    print()
    print_table("LOG_REFERENCE", log)
    print()
    print("} // namespace tpj::test")
    print()
    print("#endif")


if __name__ == "__main__":
    main()
