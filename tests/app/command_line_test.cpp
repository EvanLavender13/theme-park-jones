#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace tpj {
namespace {

using Catch::Matchers::ContainsSubstring;

struct ProcessResult {
  int Status = -1;
  std::string Out;
  std::string Err;
};

std::filesystem::path scratchDirectory(std::string_view test) {
  const std::filesystem::path directory = std::filesystem::path(TPJ_APP_SCRATCH) / test;
  std::filesystem::create_directories(directory);
  return directory;
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string writeFile(const std::filesystem::path &path, std::string_view text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
  return path.string();
}

// Standard output is in text mode, which on Windows writes each line feed as a carriage return and
// line feed.
std::string withoutCarriageReturns(std::string text) {
  std::erase(text, '\r');
  return text;
}

std::string shellQuoted(std::string_view text) { return "\"" + std::string(text) + "\""; }

// Runs the executable with the arguments, capturing its standard output and error in the scratch
// directory.
ProcessResult run(std::string_view executable, const std::filesystem::path &directory,
                  const std::vector<std::string> &arguments) {
  const std::filesystem::path outPath = directory / "stdout.txt";
  const std::filesystem::path errPath = directory / "stderr.txt";
  std::string command = shellQuoted(executable);
  for (const std::string &argument : arguments) {
    command += " " + shellQuoted(argument);
  }
  command += " > " + shellQuoted(outPath.string()) + " 2> " + shellQuoted(errPath.string());
#ifdef _WIN32
  // cmd.exe strips the outer quotes of a command line that starts with one.
  command = "\"" + command + "\"";
#endif
  // The test runs one child at a time, and its environment is not changed.
  const int raw = std::system(command.c_str()); // NOLINT(cert-env33-c,concurrency-mt-unsafe)
  ProcessResult result;
#ifdef _WIN32
  result.Status = raw;
#else
  result.Status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
#endif
  result.Out = withoutCarriageReturns(readFile(outPath));
  result.Err = withoutCarriageReturns(readFile(errPath));
  return result;
}

ProcessResult runApp(const std::filesystem::path &directory,
                     const std::vector<std::string> &arguments) {
  return run(TPJ_APP_EXECUTABLE, directory, arguments);
}

// `tick <t> hash <h>` from the last line tpj_scenarios writes for the file after the ticks, or none
// when it writes no line for the file.
std::optional<std::string> scenariosLine(const std::filesystem::path &directory,
                                         const std::string &file, const std::string &ticks) {
  std::filesystem::create_directories(directory / "scenarios");
  const ProcessResult result =
      run(TPJ_SCENARIOS_EXECUTABLE, directory / "scenarios", {"--ticks", ticks, file});
  REQUIRE(result.Status == 0);
  const std::string prefix = "file " + file + " ";
  std::optional<std::string> last;
  std::istringstream lines(result.Out);
  for (std::string line; std::getline(lines, line);) {
    if (line.starts_with(prefix)) {
      last = line.substr(prefix.size());
    }
  }
  return last;
}

bool hasHashLine(const std::string &out) {
  static const std::regex HASH_LINE("(^|\n)tick [0-9]+ hash ");
  return std::regex_search(out, HASH_LINE);
}

// The single line the app writes with --hash, checked for its form.
std::string requireHashLine(const ProcessResult &result) {
  INFO("standard error: " << result.Err);
  REQUIRE(result.Status == 0);
  static const std::regex FORM("tick [0-9]+ hash [0-9a-f]{16}\n");
  REQUIRE(std::regex_match(result.Out, FORM));
  return result.Out.substr(0, result.Out.size() - 1);
}

std::string parkPath(std::string_view name) {
  return (std::filesystem::path(TPJ_PARKS_DIR) / name).string();
}

// Starts at a tick other than zero, so the tick written is the world's, not the count stepped.
constexpr std::string_view LATER_PARK = "tpj-park 1\nseed 5\ntick 20\nnext-key 3\n"
                                        "\n[entrance]\n"
                                        "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
                                        "\n[path]\n"
                                        "2 kind=backstage points=[{x=0 z=123} {x=10 z=110}]\n";

TEST_CASE("The app's hash line after --park and --ticks equals tpj_scenarios' for the file") {
  struct Case {
    std::string Name;
    std::string File;
    std::string Ticks;
  };
  const auto directory = scratchDirectory("park-hash");
  const std::vector<Case> cases = {
      {"the sketch park after 30 ticks", parkPath("sketch.park"), "30"},
      // No ticks at all, so the line is the loaded world's once resolved.
      {"a park saved at tick 20, after no ticks", writeFile(directory / "later.park", LATER_PARK),
       "0"}};
  for (const Case &test : cases) {
    INFO(test.Name);
    const std::optional<std::string> expected = scenariosLine(directory, test.File, test.Ticks);
    REQUIRE(expected.has_value());
    const ProcessResult result =
        runApp(directory, {"--park", test.File, "--ticks", test.Ticks, "--hash"});
    CHECK(requireHashLine(result) == expected.value_or(""));
  }
}

TEST_CASE("Without --park, --hash writes the line tpj_scenarios writes for tests/parks/new.park") {
  const auto directory = scratchDirectory("template-hash");
  const std::optional<std::string> expected = scenariosLine(directory, parkPath("new.park"), "10");
  REQUIRE(expected.has_value());
  const ProcessResult result = runApp(directory, {"--ticks", "10", "--hash"});
  CHECK(requireHashLine(result) == expected.value_or(""));
}

TEST_CASE("A command line the app does not accept exits with a nonzero status and no hash line") {
  const auto directory = scratchDirectory("bad-command-line");
  const std::string capture = (directory / "out.bmp").string();
  // Each asks for a hash, so an app that accepted it would write one and open no window.
  const std::vector<std::vector<std::string>> cases = {
      {"--hash", "--ticks", "0", "--frobnicate"},
      {"--hash", "--ticks"},
      {"--hash", "--ticks", "0", "--park"},
      // Trailing text a partial parse would accept, and a sign a count cannot have.
      {"--hash", "--ticks", "12x"},
      {"--hash", "--ticks", "-1"},
      {"--hash", "--ticks", "0", "--frames", "5"},
      {"--hash", "--ticks", "0", "--capture", capture}};
  for (const std::vector<std::string> &arguments : cases) {
    std::string shown;
    for (const std::string &argument : arguments) {
      shown += " " + argument;
    }
    INFO("arguments" << shown);
    const ProcessResult result = runApp(directory, arguments);
    CHECK(result.Status != 0);
    CHECK_FALSE(hasHashLine(result.Out));
  }
}

TEST_CASE("A park file the app cannot read exits with a nonzero status and no hash line, naming "
          "the file") {
  const auto directory = scratchDirectory("unreadable-park");
  const std::string missing = (directory / "missing.park").string();
  std::filesystem::remove(missing);
  const ProcessResult result = runApp(directory, {"--park", missing, "--ticks", "0", "--hash"});
  CHECK(result.Status != 0);
  CHECK_FALSE(hasHashLine(result.Out));
  CHECK_THAT(result.Out + result.Err, ContainsSubstring(missing));
}

TEST_CASE("A park file the app cannot load exits with a nonzero status and no hash line, naming "
          "the file and the load error") {
  const auto directory = scratchDirectory("unloadable-park");
  // Key 5 is not below next-key 3, an error past the header.
  constexpr std::string_view BROKEN =
      "tpj-park 1\nseed 1\ntick 0\nnext-key 3\n\n[entities]\n1\n5\n";
  std::string error;
  try {
    loadWorld(makeParkSchema(), BROKEN);
  } catch (const LoadError &loadError) {
    error = loadError.what();
  }
  REQUIRE_FALSE(error.empty());

  const std::string broken = writeFile(directory / "broken.park", BROKEN);
  const ProcessResult result = runApp(directory, {"--park", broken, "--ticks", "0", "--hash"});
  CHECK(result.Status != 0);
  CHECK_FALSE(hasHashLine(result.Out));
  CHECK_THAT(result.Out + result.Err, ContainsSubstring(broken));
  CHECK_THAT(result.Out + result.Err, ContainsSubstring(error));
}

} // namespace
} // namespace tpj
