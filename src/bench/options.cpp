#include "bench/options.h"

#include <charconv>
#include <system_error>
#include <vector>

namespace tpj {

std::optional<BenchOptions> parseBenchOptions(std::span<const std::string> arguments,
                                              std::string &error) {
  BenchOptions options;
  std::vector<std::string> files;
  for (size_t i = 1; i < arguments.size(); ++i) {
    const std::string &argument = arguments[i];
    if (argument == "--ticks") {
      if (i + 1 == arguments.size()) {
        error = "--ticks needs a count";
        return std::nullopt;
      }
      const std::string &value = arguments[++i];
      uint64_t ticks = 0;
      const char *end = value.data() + value.size();
      const auto result = std::from_chars(value.data(), end, ticks);
      if (value.empty() || result.ec != std::errc() || result.ptr != end || ticks == 0) {
        error = "--ticks takes a positive decimal count, not '" + value + "'";
        return std::nullopt;
      }
      options.Ticks = ticks;
    } else if (argument.starts_with("--")) {
      error = "unknown option " + argument;
      return std::nullopt;
    } else {
      files.push_back(argument);
    }
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
