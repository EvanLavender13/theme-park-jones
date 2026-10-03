#include "bench/report/internal/columns.h"

#include <algorithm>
#include <format>
#include <stddef.h>

namespace tpj {

std::string microseconds(int64_t nanoseconds) {
  return std::format("{:.1f}", static_cast<double>(nanoseconds) / 1000.0);
}

void appendTimes(std::vector<std::string> &fields, const TimeSummary &times) {
  fields.push_back(microseconds(times.Median));
  fields.push_back("[" + microseconds(times.Least));
  fields.push_back(microseconds(times.Greatest) + "]");
}

std::string alignedText(std::span<const std::vector<std::string>> lines) {
  std::vector<size_t> widths;
  for (const std::vector<std::string> &fields : lines) {
    widths.resize(std::max(widths.size(), fields.size()), 0);
    for (size_t i = 0; i < fields.size(); ++i) {
      widths[i] = std::max(widths[i], fields[i].size());
    }
  }
  std::string text;
  for (const std::vector<std::string> &fields : lines) {
    for (size_t i = 0; i < fields.size(); ++i) {
      text += fields[i];
      if (i + 1 < fields.size()) {
        text += std::string(widths[i] - fields[i].size() + 1, ' ');
      }
    }
    text += '\n';
  }
  return text;
}

} // namespace tpj
