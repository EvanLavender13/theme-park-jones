#include "bench/options.h"

#include <charconv>
#include <system_error>
#include <vector>

namespace tpj {
namespace {

// The positive decimal count after the option at index i, moving i past it, or none with error
// naming the problem.
std::optional<uint64_t> readCount(std::span<const std::string> arguments, size_t &i,
                                  std::string &error) {
  const std::string &option = arguments[i];
  if (i + 1 == arguments.size()) {
    error = option + " needs a count";
    return std::nullopt;
  }
  const std::string &value = arguments[++i];
  uint64_t count = 0;
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(value.data(), end, count);
  if (value.empty() || result.ec != std::errc() || result.ptr != end || count == 0) {
    error = option + " takes a positive decimal count, not '" + value + "'";
    return std::nullopt;
  }
  return count;
}

} // namespace

std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> arguments,
                                              std::string &error) {
  BenchOptions options;
  std::vector<std::string> files;
  bool ticksGiven = false;
  bool warmTicksGiven = false;
  for (size_t i = 1; i < arguments.size(); ++i) {
    const std::string &argument = arguments[i];
    if (argument == "--ticks" || argument == "--warm-ticks") {
      const std::optional<uint64_t> count = readCount(arguments, i, error);
      if (!count) {
        return std::nullopt;
      }
      if (argument == "--ticks") {
        options.Ticks = *count;
        ticksGiven = true;
      } else {
        options.WarmTicks = *count;
        warmTicksGiven = true;
      }
    } else if (argument == "--full-park") {
      if (i + 1 == arguments.size()) {
        error = "--full-park needs a path";
        return std::nullopt;
      }
      options.FullPark = arguments[++i];
    } else if (argument.starts_with("--")) {
      error = "unknown option " + argument;
      return std::nullopt;
    } else {
      files.push_back(argument);
    }
  }
  if (!options.FullPark.empty()) {
    if (!files.empty()) {
      error = "--full-park takes no park file";
      return std::nullopt;
    }
    if (ticksGiven) {
      error = "--ticks does not apply to --full-park";
      return std::nullopt;
    }
    return options;
  }
  if (warmTicksGiven) {
    error = "--warm-ticks needs --full-park";
    return std::nullopt;
  }
  if (files.empty()) {
    error = "no park file given";
    return std::nullopt;
  }
  if (files.size() > 1) {
    error = "more than one park file given";
    return std::nullopt;
  }
  options.Park = files.front();
  return options;
}

} // namespace tpj
