#ifndef TPJ_TESTS_BENCH_SUPPORT_BENCH_PARKS_H
#define TPJ_TESTS_BENCH_SUPPORT_BENCH_PARKS_H

#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

// The parks the runner's tests time, and loading them as tpj_bench does.
namespace tpj::test {

// The path of the named file of tests/parks/.
inline std::string parkPath(std::string_view name) {
  return (std::filesystem::path(TPJ_PARKS_DIR) / std::string(name)).string();
}

// The named file of tests/parks/, which is required to be there.
inline std::string parkText(std::string_view name) {
  std::ifstream file(std::filesystem::path(parkPath(name)), std::ios::binary);
  std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  return text;
}

// A park's text loaded with the park's schema, resolution pending, as tpj_bench loads it.
inline World loadPark(std::string_view text) { return loadWorld(makeParkSchema(), text); }

// The key of the lower-keyed of OVERLAPPING_SHOPS' two shops.
inline constexpr std::string_view OVERLAPPING_LOWER_SHOP = "37";

// A park no edit could make: two shops two meters apart, so their footprints overlap, and moving
// either to its own pose is refused, since the other still overlaps it. The keys are ones no other
// part of a message is likely to hold.
inline constexpr std::string_view OVERLAPPING_SHOPS =
    "tpj-park 1\nseed 1\ntick 0\nnext-key 53\n"
    "\n[entrance]\n"
    "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
    "\n[path]\n"
    "2 kind=guest points=[{x=0 z=123} {x=0 z=103}]\n"
    "\n[box]\n"
    "37 kind=shop x=40 z=0 facing-x=0 facing-z=-1\n"
    "52 kind=shop x=42 z=0 facing-x=0 facing-z=-1\n";

} // namespace tpj::test

#endif
