// tpj_scenarios: runs the registered scenarios and the given park files, printing per-tick hashes
// and the simulation's math and draw outputs for the cross-build check (decision 0022).
//
//   tpj_scenarios [--ticks N] [FILE...]
//   tpj_scenarios --compare LEFT RIGHT
//   tpj_scenarios --slice-parks FED WARM CUT
//
// Timings go to standard error, so they never enter the compared output.

#include "scenarios/runner.h"
#include "scenarios/scenarios.h"
#include "scenarios/slice_parks.h"
#include "sim/field_text.h"
#include "sim/park_schema.h"

#include <array>
#include <charconv>
#include <chrono>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr uint64_t DEFAULT_TICKS = 3000;

std::optional<std::string> readFile(const std::string &path) {
  const std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream text;
  text << file.rdbuf();
  if (file.bad()) {
    return std::nullopt;
  }
  return text.str();
}

int compareOutputs(const std::vector<std::string> &arguments) {
  if (arguments.size() != 3) {
    std::cerr << "tpj_scenarios: --compare takes two files\n";
    return 2;
  }
  const std::string &leftPath = arguments[1];
  const std::string &rightPath = arguments[2];
  const std::optional<std::string> left = readFile(leftPath);
  if (!left) {
    std::cerr << "tpj_scenarios: cannot read " << leftPath << '\n';
    return 2;
  }
  const std::optional<std::string> right = readFile(rightPath);
  if (!right) {
    std::cerr << "tpj_scenarios: cannot read " << rightPath << '\n';
    return 2;
  }
  const std::optional<tpj::LineDifference> difference = tpj::firstDifference(*left, *right);
  if (!difference) {
    return 0;
  }
  std::cout << leftPath << " and " << rightPath << " first differ at line " << difference->Line
            << ":\n"
            << leftPath << ": " << difference->Left.value_or("(no line)") << '\n'
            << rightPath << ": " << difference->Right.value_or("(no line)") << '\n';
  return 1;
}

// Writes '<source>: <n> ticks in <s> s, <r> ticks per second' to standard error.
void reportTiming(std::string_view source, uint64_t ticks,
                  std::chrono::steady_clock::duration elapsed) {
  const double seconds = std::chrono::duration<double>(elapsed).count();
  const double rate = seconds > 0.0 ? static_cast<double>(ticks) / seconds : 0.0;
  std::cerr << source << ": " << ticks << " ticks in " << std::fixed << std::setprecision(3)
            << seconds << " s, " << std::setprecision(0) << rate << " ticks per second\n";
}

int runAll(uint64_t ticks, const std::vector<std::string> &paths) {
  for (const tpj::Scenario &scenario : tpj::registeredScenarios()) {
    const auto start = std::chrono::steady_clock::now();
    tpj::runScenario(scenario, ticks, std::cout);
    reportTiming("scenario " + std::string(scenario.Name), ticks,
                 std::chrono::steady_clock::now() - start);
  }
  for (const std::string &path : paths) {
    const std::optional<std::string> text = readFile(path);
    if (!text) {
      std::cerr << "tpj_scenarios: cannot read " << path << '\n';
      return 1;
    }
    const auto start = std::chrono::steady_clock::now();
    try {
      tpj::runSave(path, tpj::makeParkSchema(), *text, ticks, std::cout);
    } catch (const tpj::LoadError &error) {
      std::cerr << "tpj_scenarios: " << path << ": " << error.what() << '\n';
      return 1;
    }
    reportTiming("file " + path, ticks, std::chrono::steady_clock::now() - start);
  }
  tpj::writeSimMathLines(std::cout);
  tpj::writeDrawLines(std::cout);
  return 0;
}

// Writes the text to the path in binary mode, so every line ends with a line feed on every build.
bool writeFile(const std::string &path, const std::string &text) {
  std::ofstream file(path, std::ios::binary);
  file << text;
  file.close();
  return !file.fail();
}

// Makes the slice's parks from FED's text and writes them to FED, WARM, and CUT.
int writeSliceParks(const std::vector<std::string> &arguments) {
  if (arguments.size() != 4) {
    std::cerr << "tpj_scenarios: --slice-parks takes three files\n";
    return 1;
  }
  const std::string &fedPath = arguments[1];
  const std::optional<std::string> fed = readFile(fedPath);
  if (!fed) {
    std::cerr << "tpj_scenarios: cannot read " << fedPath << '\n';
    return 1;
  }
  tpj::SliceParks parks;
  try {
    parks = tpj::makeSliceParks(*fed, tpj::WARM_TICKS);
  } catch (const tpj::LoadError &error) {
    std::cerr << "tpj_scenarios: " << fedPath << ": " << error.what() << '\n';
    return 1;
  }
  const std::array<const std::string *, 3> texts{&parks.Fed, &parks.Warm, &parks.Cut};
  for (size_t i = 0; i < texts.size(); ++i) {
    const std::string &path = arguments[i + 1];
    if (!writeFile(path, *texts[i])) {
      std::cerr << "tpj_scenarios: cannot write " << path << '\n';
      return 1;
    }
  }
  return 0;
}

int run(const std::vector<std::string> &arguments) {
  if (!arguments.empty() && arguments[0] == "--compare") {
    return compareOutputs(arguments);
  }
  if (!arguments.empty() && arguments[0] == "--slice-parks") {
    return writeSliceParks(arguments);
  }
  uint64_t ticks = DEFAULT_TICKS;
  std::vector<std::string> paths;
  for (size_t i = 0; i < arguments.size(); ++i) {
    const std::string &argument = arguments[i];
    if (argument == "--ticks") {
      if (i + 1 == arguments.size()) {
        std::cerr << "tpj_scenarios: --ticks takes a decimal count\n";
        return 1;
      }
      const std::string &value = arguments[++i];
      const char *end = value.data() + value.size();
      const auto result = std::from_chars(value.data(), end, ticks);
      if (value.empty() || result.ec != std::errc() || result.ptr != end) {
        std::cerr << "tpj_scenarios: --ticks takes a decimal count\n";
        return 1;
      }
    } else if (argument.starts_with("--")) {
      std::cerr << "tpj_scenarios: unknown option " << argument << '\n';
      return 1;
    } else {
      paths.push_back(argument);
    }
  }
  return runAll(ticks, paths);
}

} // namespace

int main(int argc, char **argv) {
  try {
    return run(std::vector<std::string>(argv + 1, argv + argc));
  } catch (const std::exception &error) {
    std::cerr << "tpj_scenarios: " << error.what() << '\n';
    return 1;
  }
}
