#include "app/options.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// The program's name, which parseOptions reads past.
constexpr const char *PROGRAM = "ThemeParkJones";

using Arguments = std::vector<const char *>;

std::optional<Options> parse(const Arguments &arguments) {
  Arguments argv{PROGRAM};
  argv.insert(argv.end(), arguments.begin(), arguments.end());
  return parseOptions(static_cast<int>(argv.size()), argv.data());
}

std::string shown(const Arguments &arguments) {
  std::string text = PROGRAM;
  for (const char *argument : arguments) {
    text += " ";
    text += argument;
  }
  return text;
}

bool samePath(const char *path, std::string_view expected) {
  return path != nullptr && std::string_view(path) == expected;
}

TEST_CASE("parseOptions holds what each given option names and leaves each option not given at "
          "its default") {
  SECTION("no options") {
    const std::optional<Options> options = parse({});
    REQUIRE(options.has_value());
    if (!options.has_value()) {
      return;
    }
    CHECK(options->FrameLimit == 0);
    CHECK(options->CapturePath == nullptr);
    CHECK(options->ParkPath == nullptr);
    CHECK(options->Ticks == 0);
    CHECK_FALSE(options->PrintHash);
    CHECK_FALSE(options->ShowGraph);
    CHECK_FALSE(options->ShowFoodOverlay);
  }
  SECTION("every option a window run takes") {
    const std::optional<Options> options =
        parse({"--frames", "7", "--capture", "out.bmp", "--park", "a.park", "--ticks", "42",
               "--graph", "--overlay", "food"});
    REQUIRE(options.has_value());
    if (!options.has_value()) {
      return;
    }
    CHECK(options->FrameLimit == 7);
    CHECK(samePath(options->CapturePath, "out.bmp"));
    CHECK(samePath(options->ParkPath, "a.park"));
    CHECK(options->Ticks == 42);
    CHECK_FALSE(options->PrintHash);
    CHECK(options->ShowGraph);
    CHECK(options->ShowFoodOverlay);
  }
  SECTION("a hash run") {
    // A count past 32 bits, which the 64-bit tick count holds.
    const std::optional<Options> options =
        parse({"--park", "b.park", "--ticks", "4294967296", "--hash"});
    REQUIRE(options.has_value());
    if (!options.has_value()) {
      return;
    }
    CHECK(options->PrintHash);
    CHECK(samePath(options->ParkPath, "b.park"));
    CHECK(options->Ticks == 4'294'967'296ULL);
    CHECK(options->FrameLimit == 0);
    CHECK(options->CapturePath == nullptr);
    CHECK_FALSE(options->ShowGraph);
    CHECK_FALSE(options->ShowFoodOverlay);
  }
  SECTION("--frames alone") {
    const std::optional<Options> options = parse({"--frames", "5"});
    REQUIRE(options.has_value());
    if (!options.has_value()) {
      return;
    }
    CHECK(options->FrameLimit == 5);
    CHECK(options->CapturePath == nullptr);
    CHECK(options->ParkPath == nullptr);
    CHECK(options->Ticks == 0);
    CHECK_FALSE(options->PrintHash);
    CHECK_FALSE(options->ShowGraph);
    CHECK_FALSE(options->ShowFoodOverlay);
  }
}

TEST_CASE("A command line with --capture gives a FrameLimit of 3 unless --frames gives a positive "
          "one") {
  struct Case {
    Arguments Given;
    int FrameLimit;
  };
  const std::vector<Case> cases = {
      {{"--capture", "out.bmp"}, 3},
      // Zero and a negative value are not positive, wherever --frames stands.
      {{"--capture", "out.bmp", "--frames", "0"}, 3},
      {{"--frames", "-2", "--capture", "out.bmp"}, 3},
      // The least positive value is kept.
      {{"--capture", "out.bmp", "--frames", "1"}, 1}};
  for (const Case &test : cases) {
    INFO(shown(test.Given));
    const std::optional<Options> options = parse(test.Given);
    REQUIRE(options.has_value());
    if (!options.has_value()) {
      continue;
    }
    CHECK(options->FrameLimit == test.FrameLimit);
    CHECK(samePath(options->CapturePath, "out.bmp"));
  }
}

TEST_CASE("parseOptions sets FrameTimes exactly when --frame-times is given with a positive "
          "--frames") {
  struct Case {
    Arguments Given;
    bool FrameTimes;
  };
  const std::vector<Case> cases = {{{"--frames", "5", "--frame-times"}, true},
                                   // The least positive value, with --frame-times before --frames.
                                   {{"--frame-times", "--frames", "1"}, true},
                                   {{"--frames", "5"}, false}};
  for (const Case &test : cases) {
    INFO(shown(test.Given));
    const std::optional<Options> options = parse(test.Given);
    REQUIRE(options.has_value());
    if (!options.has_value()) {
      continue;
    }
    CHECK(options->FrameTimes == test.FrameTimes);
  }
}

TEST_CASE("parseOptions gives no Options for a command line that prints the usage") {
  const std::vector<Arguments> cases = {
      // An unknown option.
      {"--frobnicate"},
      // An option missing its value, a count's and a path's.
      {"--ticks"},
      {"--graph", "--park"},
      {"--frames", "1", "--overlay"},
      // A --ticks value that is not a decimal count: trailing text a partial parse would accept,
      // and a sign a count cannot have.
      {"--ticks", "12x"},
      {"--ticks", "-1"},
      // An --overlay value other than food, as written.
      {"--overlay", "Food"},
      // --hash with each option it refuses, whatever their values, wherever each stands.
      {"--hash", "--frames", "0"},
      {"--hash", "--capture", "out.bmp"},
      {"--graph", "--hash"},
      {"--hash", "--overlay", "food"},
      // --frame-times without --frames, even where --capture gives a frame limit, and with a
      // --frames value that is zero or negative.
      {"--frame-times"},
      {"--capture", "out.bmp", "--frame-times"},
      {"--frame-times", "--frames", "0"},
      {"--frames", "-1", "--frame-times"}};
  for (const Arguments &arguments : cases) {
    INFO(shown(arguments));
    CHECK_FALSE(parse(arguments).has_value());
  }
}

} // namespace
} // namespace tpj
