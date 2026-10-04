// Draw keys and weights tpj_scenarios writes its draw lines for, so the cross-build check compares
// keyed draws between builds. Each key's comment says why it was chosen.
#ifndef TPJ_TESTS_SIM_SUPPORT_EXPECTED_DRAWS_H
#define TPJ_TESTS_SIM_SUPPORT_EXPECTED_DRAWS_H

#include "sim/draw.h"
#include "sim/entity_key.h"

#include <array>
#include <stdint.h>

namespace tpj::test {

struct ExpectedDraw {
  DrawKey Key;
};

inline constexpr std::array<uint64_t, 5> EXPECTED_DRAW_INTEGER_WEIGHTS = {7, 0, 12, 3, 9};
inline constexpr std::array<double, 4> EXPECTED_DRAW_DOUBLE_WEIGHTS = {0.1, 0.0, 0.7, 0.2};

inline constexpr std::array<ExpectedDraw, 10> EXPECTED_DRAWS = {{
    // Every word zero.
    {.Key = {.Seed = 0x0000000000000000ULL,
             .Entity = EntityKey{0x0000000000000000ULL},
             .Purpose = 0x0000000000000000ULL,
             .Tick = 0x0000000000000000ULL,
             .Index = 0x0000000000000000ULL}},
    // A typical key.
    {.Key = {.Seed = 0x0000000000003039ULL,
             .Entity = EntityKey{0x0000000000000007ULL},
             .Purpose = 0x0123456789abcdefULL,
             .Tick = 0x0000000000000bb8ULL,
             .Index = 0x0000000000000000ULL}},
    // The seed changed.
    {.Key = {.Seed = 0x000000000000303aULL,
             .Entity = EntityKey{0x0000000000000007ULL},
             .Purpose = 0x0123456789abcdefULL,
             .Tick = 0x0000000000000bb8ULL,
             .Index = 0x0000000000000000ULL}},
    // The entity changed to a derived-range key.
    {.Key = {.Seed = 0x0000000000003039ULL,
             .Entity = EntityKey{0x8000000000000007ULL},
             .Purpose = 0x0123456789abcdefULL,
             .Tick = 0x0000000000000bb8ULL,
             .Index = 0x0000000000000000ULL}},
    // The purpose changed.
    {.Key = {.Seed = 0x0000000000003039ULL,
             .Entity = EntityKey{0x0000000000000007ULL},
             .Purpose = 0xfedcba9876543210ULL,
             .Tick = 0x0000000000000bb8ULL,
             .Index = 0x0000000000000000ULL}},
    // The tick changed.
    {.Key = {.Seed = 0x0000000000003039ULL,
             .Entity = EntityKey{0x0000000000000007ULL},
             .Purpose = 0x0123456789abcdefULL,
             .Tick = 0x0000000000000bb9ULL,
             .Index = 0x0000000000000000ULL}},
    // The index changed.
    {.Key = {.Seed = 0x0000000000003039ULL,
             .Entity = EntityKey{0x0000000000000007ULL},
             .Purpose = 0x0123456789abcdefULL,
             .Tick = 0x0000000000000bb8ULL,
             .Index = 0x0000000000000001ULL}},
    // Every word at its maximum.
    {.Key = {.Seed = 0xffffffffffffffffULL,
             .Entity = EntityKey{0xffffffffffffffffULL},
             .Purpose = 0xffffffffffffffffULL,
             .Tick = 0xffffffffffffffffULL,
             .Index = 0xffffffffffffffffULL}},
    // A value in the first word only.
    {.Key = {.Seed = 0x0000000000003039ULL,
             .Entity = EntityKey{0x0000000000000000ULL},
             .Purpose = 0x0000000000000000ULL,
             .Tick = 0x0000000000000000ULL,
             .Index = 0x0000000000000000ULL}},
    // The same value in the last word only.
    {.Key = {.Seed = 0x0000000000000000ULL,
             .Entity = EntityKey{0x0000000000000000ULL},
             .Purpose = 0x0000000000000000ULL,
             .Tick = 0x0000000000000000ULL,
             .Index = 0x0000000000003039ULL}},
}};

} // namespace tpj::test

#endif
