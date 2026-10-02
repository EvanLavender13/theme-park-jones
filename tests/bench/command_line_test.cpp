#include "bench/options.h"
#include "bench/stages.h"
#include "bench/timing.h"
#include "sim/field_text.h"
#include "sim/world.h"
#include "support/bench_parks.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <regex>
#include <sstream>
#include <stddef.h>
#include <stdint.h>
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
  const std::filesystem::path directory = std::filesystem::path(TPJ_BENCH_SCRATCH) / test;
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

// Runs tpj_bench with the arguments, capturing its standard output and error in the directory.
ProcessResult runBench(const std::filesystem::path &directory,
                       const std::vector<std::string> &arguments) {
  const std::filesystem::path outPath = directory / "stdout.txt";
  const std::filesystem::path errPath = directory / "stderr.txt";
  std::string command = shellQuoted(TPJ_BENCH_EXECUTABLE);
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

std::vector<std::string> linesOf(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  for (std::string line; std::getline(stream, line);) {
    lines.push_back(line);
  }
  return lines;
}

TEST_CASE("tpj_bench FILE writes its park line, then the line of each stage of benchPark in its "
          "order, and exits with status 0") {
  const auto directory = scratchDirectory("stage-lines");
  const std::string file = test::parkPath("fed.park");
  constexpr uint64_t ticks = 2;
  const std::vector<StageResult> stages =
      benchPark(test::loadPark(test::parkText("fed.park")), ticks);

  const ProcessResult result = runBench(directory, {"--ticks", std::to_string(ticks), file});

  INFO("standard error: " << result.Err);
  REQUIRE(result.Status == 0);
  REQUIRE(result.Out.ends_with('\n'));
  const std::vector<std::string> lines = linesOf(result.Out);
  REQUIRE(lines.size() == 1 + stages.size());
  CHECK(lines[0] == parkLine(file, ticks));
  // Times differ from run to run, so each line is compared with the line of benchPark's stage
  // holding the times the line itself gives.
  static const std::regex TIMES(
      "stage [^ ]+ count [0-9]+ median ([0-9]+) least ([0-9]+) greatest ([0-9]+) [^ ]+ [^ ]+");
  for (size_t i = 0; i < stages.size(); ++i) {
    const std::string &line = lines[i + 1];
    INFO("line: " << line);
    std::smatch times;
    REQUIRE(std::regex_match(line, times, TIMES));
    StageResult shown = stages[i];
    shown.Times.Median = static_cast<int64_t>(std::stoll(times[1].str()));
    shown.Times.Least = static_cast<int64_t>(std::stoll(times[2].str()));
    shown.Times.Greatest = static_cast<int64_t>(std::stoll(times[3].str()));
    CHECK(line == stageLine(shown));
  }
}

// The message parseBenchOptions gives for tpj_bench's arguments, or nothing when it accepts them.
std::string optionsError(const std::vector<std::string> &arguments) {
  std::vector<std::string> all{"tpj_bench"};
  all.insert(all.end(), arguments.begin(), arguments.end());
  std::string error;
  (void)parseBenchOptions(all, error);
  REQUIRE_FALSE(error.empty());
  return error;
}

std::string loadErrorOf(std::string_view text) {
  try {
    (void)test::loadPark(text);
  } catch (const LoadError &error) {
    return error.what();
  }
  FAIL("the park loaded");
  return {};
}

std::string benchErrorOf(std::string_view text, uint64_t ticks) {
  try {
    (void)benchPark(test::loadPark(text), ticks);
  } catch (const std::exception &error) {
    return error.what();
  }
  FAIL("benchPark threw nothing");
  return {};
}

TEST_CASE("tpj_bench writes what it cannot do to standard error, nothing to standard output, and "
          "exits with a nonzero status") {
  const auto directory = scratchDirectory("failures");
  const std::string usage = "usage: tpj_bench [--ticks N] FILE";
  const std::string fed = test::parkPath("fed.park");

  const std::filesystem::path absentPath = directory / "absent.park";
  std::filesystem::remove(absentPath);
  const std::string absent = absentPath.string();

  constexpr std::string_view BROKEN_TEXT = "tpj-park 1\nnot a park\n";
  const std::string broken = writeFile(directory / "broken.park", BROKEN_TEXT);
  const std::string overlapping =
      writeFile(directory / "overlapping.park", test::OVERLAPPING_SHOPS);

  struct Case {
    std::string Name;
    std::vector<std::string> Arguments;
    std::vector<std::string> Messages;
  };
  const std::vector<Case> cases = {
      {"an unknown option", {"--frobnicate", fed}, {optionsError({"--frobnicate", fed}), usage}},
      {"no file", {}, {optionsError({}), usage}},
      {"a file it cannot read", {absent}, {"tpj_bench: cannot read " + absent}},
      {"a file loadWorld refuses",
       {broken},
       {"tpj_bench: cannot load " + broken + ": " + loadErrorOf(BROKEN_TEXT)}},
      {"a park benchPark throws for",
       {"--ticks", "1", overlapping},
       {"tpj_bench: " + benchErrorOf(test::OVERLAPPING_SHOPS, 1)}},
  };
  for (const Case &failure : cases) {
    INFO(failure.Name);
    const ProcessResult result = runBench(directory, failure.Arguments);
    INFO("standard error: " << result.Err);
    CHECK(result.Status != 0);
    CHECK(result.Out.empty());
    for (const std::string &message : failure.Messages) {
      CHECK_THAT(result.Err, ContainsSubstring(message));
    }
  }
}

} // namespace
} // namespace tpj
