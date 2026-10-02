#include "support/guest_parks.h"

#include "sim/guests/guests.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// The lines of the save's [guest] section.
std::vector<std::string> guestLines(const std::string &save) {
  constexpr std::string_view HEADER = "\n[guest]\n";
  std::vector<std::string> lines;
  std::size_t at = save.find(HEADER);
  if (at == std::string::npos) {
    return lines;
  }
  at += HEADER.size();
  while (at < save.size() && save[at] != '\n') {
    const std::size_t end = save.find('\n', at);
    lines.push_back(save.substr(at, end - at));
    at = end == std::string::npos ? save.size() : end + 1;
  }
  return lines;
}

// The names of a key line's fields, in order: each name before an '=' that follows a space outside
// any struct or vector.
std::vector<std::string> fieldNames(std::string_view line) {
  std::vector<std::string> names;
  int depth = 0;
  bool inName = false;
  std::string name;
  for (const char c : line) {
    if (inName) {
      if (c == '=') {
        names.push_back(name);
        inName = false;
      } else {
        name += c;
      }
    } else if (c == ' ' && depth == 0) {
      inName = true;
      name.clear();
    } else if (c == '{' || c == '[') {
      ++depth;
    } else if (c == '}' || c == ']') {
      --depth;
    }
  }
  return names;
}

TEST_CASE("A guest's line in a save holds exactly at, forward, activity, hunger, hunger-rate, "
          "stay-until, target, meal-relief, meals-eaten, and last-meal, in that order, once the "
          "guest has chosen") {
  // The first guest arrives at its entrance's node and chooses there in the next cycle.
  World world = test::legsWorld();
  test::stepUntil(world, test::FIRST_ARRIVAL + 2);
  const std::vector<std::string> lines = guestLines(saveWorld(world));
  REQUIRE_FALSE(lines.empty());
  const std::vector<std::string> expected{"at",          "forward",    "activity", "hunger",
                                          "hunger-rate", "stay-until", "target",   "meal-relief",
                                          "meals-eaten", "last-meal"};
  for (const std::string &line : lines) {
    CAPTURE(line);
    CHECK(fieldNames(line) == expected);
  }
}

} // namespace
} // namespace tpj
