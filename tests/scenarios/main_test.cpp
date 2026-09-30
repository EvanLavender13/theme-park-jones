#include "scenarios/runner.h"
#include "scenarios/scenarios.h"
#include "scenarios/slice_parks.h"
#include "sim/field_text.h"
#include "sim/park_schema.h"
#include "sim/save.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <regex>
#include <span>
#include <sstream>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>
#else
#include <cstdio>
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
  const std::filesystem::path directory = std::filesystem::path(TPJ_SCENARIOS_SCRATCH) / test;
  std::filesystem::create_directories(directory);
  return directory;
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Standard output is in text mode, which on Windows writes each line feed as a carriage return and
// line feed, so its lines are compared with those carriage returns removed.
std::string withoutCarriageReturns(std::string text) {
  std::erase(text, '\r');
  return text;
}

std::string writeFile(const std::filesystem::path &path, std::string_view text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
  return path.string();
}

std::string shellQuoted(std::string_view text) { return "\"" + std::string(text) + "\""; }

// Runs tpj_scenarios with the arguments, capturing its standard output and error in the test's
// scratch directory.
ProcessResult runScenarios(const std::filesystem::path &directory,
                           const std::vector<std::string> &arguments) {
  const std::filesystem::path outPath = directory / "stdout.txt";
  const std::filesystem::path errPath = directory / "stderr.txt";
  std::string command = shellQuoted(TPJ_SCENARIOS_EXECUTABLE);
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
  result.Out = readFile(outPath);
  result.Err = readFile(errPath);
  return result;
}

// Runs tpj_scenarios with no arguments, reading what it writes to standard output, carriage returns
// removed, until a line that does not start with the prefix, and then stops it. Gives the lines
// read, that one included. Its standard error goes to the test's scratch directory.
std::vector<std::string> linesWhilePrefixed(const std::filesystem::path &directory,
                                            std::string_view prefix) {
  std::vector<std::string> lines;
  std::string line;
  // Takes one character of output, and gives whether to read on.
  const auto take = [&](char character) {
    if (character == '\r') {
      return true;
    }
    if (character != '\n') {
      line += character;
      return true;
    }
    lines.push_back(line);
    line.clear();
    return lines.back().starts_with(prefix);
  };
  const std::string errPath = (directory / "stderr.txt").string();
#ifdef _WIN32
  SECURITY_ATTRIBUTES inherited{.nLength = sizeof(SECURITY_ATTRIBUTES),
                                .lpSecurityDescriptor = nullptr,
                                .bInheritHandle = TRUE};
  HANDLE readEnd = nullptr;
  HANDLE writeEnd = nullptr;
  REQUIRE(CreatePipe(&readEnd, &writeEnd, &inherited, 0));
  SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
  HANDLE errFile = CreateFileA(errPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherited,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = writeEnd;
  startup.hStdError = errFile;
  PROCESS_INFORMATION process{};
  std::string commandLine = shellQuoted(TPJ_SCENARIOS_EXECUTABLE);
  const BOOL started = CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0,
                                      nullptr, nullptr, &startup, &process);
  CloseHandle(writeEnd);
  CloseHandle(errFile);
  REQUIRE(started);
  std::array<char, 4096> buffer{};
  bool reading = true;
  DWORD got = 0;
  while (reading &&
         ReadFile(readEnd, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr) != 0 &&
         got > 0) {
    for (DWORD index = 0; index < got && reading; ++index) {
      reading = take(buffer.at(index));
    }
  }
  TerminateProcess(process.hProcess, 0);
  WaitForSingleObject(process.hProcess, INFINITE);
  CloseHandle(process.hProcess);
  CloseHandle(process.hThread);
  CloseHandle(readEnd);
#else
  const std::string command = shellQuoted(TPJ_SCENARIOS_EXECUTABLE) + " 2> " + shellQuoted(errPath);
  // The test runs one child at a time, and its environment is not changed.
  FILE *pipe = popen(command.c_str(), "r"); // NOLINT(cert-env33-c,concurrency-mt-unsafe)
  REQUIRE(pipe != nullptr);
  bool reading = true;
  for (int character = std::fgetc(pipe); reading && character != EOF;
       character = std::fgetc(pipe)) {
    reading = take(static_cast<char>(character));
  }
  // The child's next write to the closed pipe stops it.
  pclose(pipe);
#endif
  return lines;
}

// What the runner's library writes for the registered scenarios and the files, as (path, text).
std::string libraryOutput(uint64_t ticks,
                          const std::vector<std::pair<std::string, std::string>> &files) {
  std::ostringstream out;
  for (const Scenario &scenario : registeredScenarios()) {
    runScenario(scenario, ticks, out);
  }
  for (const auto &[path, text] : files) {
    runSave(path, makeParkSchema(), text, ticks, out);
  }
  writeSimMathLines(out);
  writeDrawLines(out);
  return out.str();
}

std::vector<std::string> linesOf(const std::string &text) {
  std::vector<std::string> lines;
  std::istringstream stream(text);
  for (std::string line; std::getline(stream, line);) {
    lines.push_back(line);
  }
  return lines;
}

// Saves holding no intent or state, which makeParkSchema loads: a header alone, and one listing
// entities.
constexpr std::string_view EMPTY_PARK = "tpj-park 1\nseed 7\ntick 0\nnext-key 1\n";
constexpr std::string_view ENTITIES_PARK =
    "tpj-park 1\nseed 8\ntick 20\nnext-key 3\n\n[entities]\n1\n2\n";

TEST_CASE("tpj_scenarios writes the scenarios', files', math, and draw lines, in that order") {
  const auto directory = scratchDirectory("writes-lines");
  writeFile(directory / "a.park", EMPTY_PARK);
  writeFile(directory / "b.park", ENTITIES_PARK);
  // Files run in the order given, not by name, and are labeled by the path as given, unnormalized.
  const std::string second = (directory / "." / "b.park").string();
  const std::string first = (directory / "a.park").string();

  const ProcessResult result = runScenarios(directory, {"--ticks", "2", second, first});
  CHECK(result.Status == 0);
  CHECK(withoutCarriageReturns(result.Out) ==
        libraryOutput(2, {{second, std::string(ENTITIES_PARK)}, {first, std::string(EMPTY_PARK)}}));
}

TEST_CASE("tpj_scenarios runs 3000 ticks when --ticks is not given") {
  // Every scenario runs the same count, so the first one's lines, ending where the second's begin,
  // show it, and the run is stopped there.
  const std::span<const Scenario> scenarios = registeredScenarios();
  REQUIRE(scenarios.size() >= 2);
  std::ostringstream first;
  runScenario(scenarios[0], 3000, first);
  const std::vector<std::string> expected = linesOf(first.str());

  const auto directory = scratchDirectory("default-ticks");
  const std::vector<std::string> lines =
      linesWhilePrefixed(directory, "scenario " + std::string(scenarios[0].Name) + " ");
  REQUIRE(lines.size() == expected.size() + 1);
  CHECK(std::ranges::equal(std::span(lines).first(expected.size()), expected));
  CHECK(lines.back().starts_with("scenario " + std::string(scenarios[1].Name) + " tick 0 hash "));
}

TEST_CASE(
    "tpj_scenarios writes one ticks-per-second line per scenario and file to standard error") {
  const auto directory = scratchDirectory("timings");
  const std::string park = writeFile(directory / "a.park", EMPTY_PARK);
  const ProcessResult result = runScenarios(directory, {"--ticks", "2", park});
  REQUIRE(result.Status == 0);
  std::vector<std::string> sources;
  for (const Scenario &scenario : registeredScenarios()) {
    sources.push_back("scenario " + std::string(scenario.Name));
  }
  sources.push_back("file " + park);

  const std::vector<std::string> lines = linesOf(withoutCarriageReturns(result.Err));
  CHECK(lines.size() == sources.size());
  for (const std::string &source : sources) {
    INFO("source " << source);
    const auto isSourceLine = [&](const std::string &line) {
      return line.starts_with(source + ": 2 ticks in ") && line.ends_with(" ticks per second");
    };
    CHECK(std::ranges::count_if(lines, isSourceLine) == 1);
  }
}

TEST_CASE("tpj_scenarios fails, naming the option, for an option it does not recognize") {
  const auto directory = scratchDirectory("unknown-option");
  const ProcessResult result = runScenarios(directory, {"--frobnicate"});
  CHECK(result.Status != 0);
  CHECK_THAT(result.Err, ContainsSubstring("--frobnicate"));
}

TEST_CASE("tpj_scenarios fails, naming --ticks, for a --ticks with no value or not a count") {
  // Trailing text a partial parse would accept, a sign a count cannot have, and no value at all.
  const std::vector<std::vector<std::string>> cases = {
      {"--ticks", "12x"}, {"--ticks", "-1"}, {"--ticks"}};
  for (const std::vector<std::string> &arguments : cases) {
    INFO("arguments " << arguments.size() << ", last " << arguments.back());
    const auto directory = scratchDirectory("bad-ticks");
    const ProcessResult result = runScenarios(directory, arguments);
    CHECK(result.Status != 0);
    CHECK_THAT(result.Err, ContainsSubstring("--ticks"));
  }
}

TEST_CASE("tpj_scenarios fails, naming the file, for a file it cannot read") {
  const auto directory = scratchDirectory("unreadable-file");
  const std::string missing = (directory / "missing.park").string();
  std::filesystem::remove(missing);
  const ProcessResult result = runScenarios(directory, {"--ticks", "1", missing});
  CHECK(result.Status != 0);
  CHECK_THAT(result.Err, ContainsSubstring(missing));
}

TEST_CASE("tpj_scenarios fails, naming the file and the line, for a file it cannot load") {
  const auto directory = scratchDirectory("unloadable-file");
  // Key 5 is not below next-key 3, an error well past the header.
  constexpr std::string_view BROKEN =
      "tpj-park 1\nseed 1\ntick 0\nnext-key 3\n\n[entities]\n1\n5\n";
  std::optional<size_t> errorLine;
  try {
    loadWorld(makeParkSchema(), BROKEN);
  } catch (const LoadError &error) {
    errorLine = error.line();
  }
  REQUIRE(errorLine.has_value());

  const std::string broken = writeFile(directory / "broken.park", BROKEN);
  const ProcessResult result = runScenarios(directory, {"--ticks", "1", broken});
  CHECK(result.Status != 0);
  CHECK_THAT(result.Err, ContainsSubstring(broken));
  // The line number as a whole number, somewhere other than in the path.
  std::string rest = result.Err;
  for (size_t at = rest.find(broken); at != std::string::npos; at = rest.find(broken)) {
    rest.erase(at, broken.size());
  }
  const std::regex lineNumber("(^|[^0-9])" + std::to_string(errorLine.value_or(0)) + "([^0-9]|$)");
  CHECK(std::regex_search(rest, lineNumber));
}

TEST_CASE("tpj_scenarios --compare exits 0 for outputs holding the same lines") {
  const auto directory = scratchDirectory("compare-same");
  // The same lines, one output lacking its last line feed, so a byte comparison would differ.
  const std::string left = writeFile(directory / "left.txt", "scenario a tick 0 hash 00\nend\n");
  const std::string right = writeFile(directory / "right.txt", "scenario a tick 0 hash 00\nend");
  const ProcessResult result = runScenarios(directory, {"--compare", left, right});
  CHECK(result.Status == 0);
}

TEST_CASE("tpj_scenarios --compare exits 1 and prints the first differing line of each output") {
  const auto directory = scratchDirectory("compare-differ");
  const std::string left =
      writeFile(directory / "left.txt", "scenario a tick 0 hash 00000000000000aa\n"
                                        "scenario a tick 1 hash 00000000000000bb\n"
                                        "scenario a tick 2 hash 00000000000000cc\n");
  const std::string right =
      writeFile(directory / "right.txt", "scenario a tick 0 hash 00000000000000aa\n"
                                         "scenario a tick 1 hash 00000000000000dd\n"
                                         "scenario a tick 2 hash 00000000000000ee\n");
  const ProcessResult result = runScenarios(directory, {"--compare", left, right});
  CHECK(result.Status == 1);
  const std::string report = withoutCarriageReturns(result.Out);
  CHECK_THAT(report, ContainsSubstring(left + " and " + right + " first differ at line 2:"));
  CHECK_THAT(report, ContainsSubstring(left + ": scenario a tick 1 hash 00000000000000bb"));
  CHECK_THAT(report, ContainsSubstring(right + ": scenario a tick 1 hash 00000000000000dd"));
}

TEST_CASE("tpj_scenarios --compare prints (no line) for an output that has ended") {
  const auto directory = scratchDirectory("compare-prefix");
  const std::string left = writeFile(directory / "left.txt", "scenario a tick 0 hash 00\n");
  const std::string right =
      writeFile(directory / "right.txt", "scenario a tick 0 hash 00\nscenario a tick 1 hash 11\n");
  const ProcessResult result = runScenarios(directory, {"--compare", left, right});
  CHECK(result.Status == 1);
  const std::string report = withoutCarriageReturns(result.Out);
  CHECK_THAT(report, ContainsSubstring(left + " and " + right + " first differ at line 2:"));
  CHECK_THAT(report, ContainsSubstring(left + ": (no line)"));
  CHECK_THAT(report, ContainsSubstring(right + ": scenario a tick 1 hash 11"));
}

TEST_CASE("tpj_scenarios --compare exits 2 when it cannot read a file") {
  const auto directory = scratchDirectory("compare-unreadable");
  const std::string left = writeFile(directory / "left.txt", "scenario a tick 0 hash 00\n");
  const std::string missing = (directory / "missing.txt").string();
  std::filesystem::remove(missing);
  const ProcessResult result = runScenarios(directory, {"--compare", left, missing});
  CHECK(result.Status == 2);
}

TEST_CASE("tpj_scenarios --slice-parks writes makeSliceParks of the first file with WARM_TICKS to "
          "the three files") {
  const auto directory = scratchDirectory("slice-parks-writes");
  const std::string fed = writeFile(directory / "fed.park", EMPTY_PARK);
  const std::string warm = (directory / "warm.park").string();
  const std::string cut = (directory / "cut.park").string();
  std::filesystem::remove(warm);
  std::filesystem::remove(cut);

  const ProcessResult result = runScenarios(directory, {"--slice-parks", fed, warm, cut});
  CHECK(result.Status == 0);
  // Compared byte for byte, so a carriage return before any line feed fails.
  const SliceParks parks = makeSliceParks(EMPTY_PARK, WARM_TICKS);
  CHECK(readFile(fed) == parks.Fed);
  CHECK(readFile(warm) == parks.Warm);
  CHECK(readFile(cut) == parks.Cut);
}

TEST_CASE("tpj_scenarios --slice-parks fails, writing no file, when not given three files") {
  const std::vector<size_t> counts = {2, 4};
  for (const size_t count : counts) {
    INFO("files " << count);
    const auto directory = scratchDirectory("slice-parks-count");
    const std::string fed = writeFile(directory / "fed.park", EMPTY_PARK);
    std::vector<std::string> arguments = {"--slice-parks", fed};
    std::vector<std::string> others;
    for (size_t index = 1; index < count; ++index) {
      others.push_back((directory / ("out" + std::to_string(index) + ".park")).string());
      std::filesystem::remove(others.back());
      arguments.push_back(others.back());
    }
    const ProcessResult result = runScenarios(directory, arguments);
    CHECK(result.Status != 0);
    CHECK_FALSE(result.Err.empty());
    CHECK(readFile(fed) == EMPTY_PARK);
    for (const std::string &other : others) {
      CHECK_FALSE(std::filesystem::exists(other));
    }
  }
}

TEST_CASE(
    "tpj_scenarios --slice-parks fails, writing no file, when it cannot read its first file") {
  const auto directory = scratchDirectory("slice-parks-unreadable");
  const std::string missing = (directory / "missing.park").string();
  const std::string warm = (directory / "warm.park").string();
  const std::string cut = (directory / "cut.park").string();
  for (const std::string &path : {missing, warm, cut}) {
    std::filesystem::remove(path);
  }
  const ProcessResult result = runScenarios(directory, {"--slice-parks", missing, warm, cut});
  CHECK(result.Status != 0);
  CHECK_FALSE(result.Err.empty());
  CHECK_FALSE(std::filesystem::exists(missing));
  CHECK_FALSE(std::filesystem::exists(warm));
  CHECK_FALSE(std::filesystem::exists(cut));
}

TEST_CASE("tpj_scenarios --slice-parks fails, writing no file and naming the line, when it cannot "
          "load its first file") {
  const auto directory = scratchDirectory("slice-parks-unloadable");
  // Key 5 is not below next-key 3, an error well past the header.
  constexpr std::string_view BROKEN =
      "tpj-park 1\nseed 1\ntick 0\nnext-key 3\n\n[entities]\n1\n5\n";
  std::optional<size_t> errorLine;
  try {
    loadWorld(makeParkSchema(), BROKEN);
  } catch (const LoadError &error) {
    errorLine = error.line();
  }
  REQUIRE(errorLine.has_value());

  const std::string fed = writeFile(directory / "fed.park", BROKEN);
  const std::string warm = (directory / "warm.park").string();
  const std::string cut = (directory / "cut.park").string();
  std::filesystem::remove(warm);
  std::filesystem::remove(cut);
  const ProcessResult result = runScenarios(directory, {"--slice-parks", fed, warm, cut});
  CHECK(result.Status != 0);
  CHECK(readFile(fed) == BROKEN);
  CHECK_FALSE(std::filesystem::exists(warm));
  CHECK_FALSE(std::filesystem::exists(cut));
  // The line number as a whole number, somewhere other than in the paths.
  std::string rest = result.Err;
  for (const std::string &path : {fed, warm, cut}) {
    for (size_t at = rest.find(path); at != std::string::npos; at = rest.find(path)) {
      rest.erase(at, path.size());
    }
  }
  const std::regex lineNumber("(^|[^0-9])" + std::to_string(errorLine.value_or(0)) + "([^0-9]|$)");
  CHECK(std::regex_search(rest, lineNumber));
}

TEST_CASE("tpj_scenarios --slice-parks fails, naming the file, when it cannot write one") {
  const auto directory = scratchDirectory("slice-parks-unwritable");
  const std::string fed = writeFile(directory / "fed.park", EMPTY_PARK);
  // A file in a directory that does not exist cannot be opened for writing.
  std::filesystem::remove_all(directory / "missing");
  const std::string warm = (directory / "missing" / "warm.park").string();
  const std::string cut = (directory / "cut.park").string();
  const ProcessResult result = runScenarios(directory, {"--slice-parks", fed, warm, cut});
  CHECK(result.Status != 0);
  CHECK_THAT(result.Err, ContainsSubstring(warm));
}

} // namespace
} // namespace tpj
