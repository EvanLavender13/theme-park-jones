#include "bench/options.h"

#include "bench/stages.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::ContainsSubstring;

// The program's name, which parseBenchOptions reads past.
constexpr const char *PROGRAM = "tpj_bench";

struct Parsed {
  std::optional<BenchOptions> Options;
  std::string Error;
};

Parsed parse(const std::vector<std::string> &arguments) {
  std::vector<std::string> all{PROGRAM};
  all.insert(all.end(), arguments.begin(), arguments.end());
  Parsed parsed;
  parsed.Options = parseBenchOptions(all, parsed.Error);
  return parsed;
}

std::string shown(const std::vector<std::string> &arguments) {
  std::string text = PROGRAM;
  for (const std::string &argument : arguments) {
    text += " " + argument;
  }
  return text;
}

TEST_CASE("parseBenchOptions reads the file and --ticks N, with DEFAULT_TICKS without --ticks") {
  struct Case {
    std::vector<std::string> Arguments;
    BenchOptions Expected;
  };
  const std::vector<Case> cases = {
      {{"winding-path.park"}, {DEFAULT_TICKS, "winding-path.park"}},
      {{"--ticks", "7", "a.park"}, {7, "a.park"}},
      // An option may follow the file, and its value is not taken for a second file.
      {{"a.park", "--ticks", "12"}, {12, "a.park"}},
      // A count past 32 bits, which the 64-bit tick count holds.
      {{"--ticks", "4294967296", "b.park"}, {4'294'967'296ULL, "b.park"}},
  };
  for (const Case &example : cases) {
    INFO(shown(example.Arguments));
    const Parsed parsed = parse(example.Arguments);
    INFO("error: " << parsed.Error);
    REQUIRE(parsed.Options.has_value());
    CHECK(parsed.Options.value_or(BenchOptions{}) == example.Expected);
  }
}

TEST_CASE("parseBenchOptions refuses an unknown option with a message naming it") {
  const Parsed parsed = parse({"--frobnicate", "a.park"});
  CHECK_FALSE(parsed.Options.has_value());
  CHECK_THAT(parsed.Error, ContainsSubstring("--frobnicate"));
}

TEST_CASE("parseBenchOptions refuses --ticks with no value or one that is not a positive decimal "
          "count") {
  const std::vector<std::vector<std::string>> cases = {
      {"a.park", "--ticks"},
      // Zero is a count but not a positive one.
      {"--ticks", "0", "a.park"},
      // A sign, trailing text a partial parse would accept, a fraction, and no digits at all.
      {"--ticks", "-3", "a.park"},
      {"--ticks", "12x", "a.park"},
      {"--ticks", "1.5", "a.park"},
      {"--ticks", "many", "a.park"},
  };
  for (const std::vector<std::string> &arguments : cases) {
    INFO(shown(arguments));
    const Parsed parsed = parse(arguments);
    CHECK_FALSE(parsed.Options.has_value());
    CHECK_FALSE(parsed.Error.empty());
  }
}

TEST_CASE("parseBenchOptions refuses a command line with no file or more than one") {
  const std::vector<std::vector<std::string>> cases = {
      {},
      {"--ticks", "5"},
      {"a.park", "b.park"},
  };
  for (const std::vector<std::string> &arguments : cases) {
    INFO(shown(arguments));
    const Parsed parsed = parse(arguments);
    CHECK_FALSE(parsed.Options.has_value());
    CHECK_FALSE(parsed.Error.empty());
  }
}

// A message names its problem, so problems of different kinds are never given the same message.
TEST_CASE("parseBenchOptions gives each kind of refusal its own message") {
  const std::vector<std::vector<std::string>> cases = {
      {"--frobnicate", "a.park"},
      {"--ticks", "0", "a.park"},
      {},
      {"a.park", "b.park"},
  };
  std::set<std::string> messages;
  for (const std::vector<std::string> &arguments : cases) {
    messages.insert(parse(arguments).Error);
  }
  CHECK(messages.size() == cases.size());
}

} // namespace
} // namespace tpj
