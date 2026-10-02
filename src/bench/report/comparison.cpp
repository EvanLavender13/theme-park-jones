#include "bench/report/comparison.h"

#include "bench/timing.h"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// The stage of that name in that park on that build, or null.
const StageResult *findStage(std::span<const ReportPark> report, const std::string &build,
                             const std::string &park, const std::string &stage) {
  for (const ReportPark &read : report) {
    if (read.Build == build && read.Park == park) {
      const auto found = std::ranges::find(read.Stages, stage, &StageResult::Name);
      return found == read.Stages.end() ? nullptr : &*found;
    }
  }
  return nullptr;
}

ChangeKind changeBetween(const TimeSummary &before, const TimeSummary &after) {
  if (after.Greatest < before.Least) {
    return ChangeKind::Faster;
  }
  if (after.Least > before.Greatest) {
    return ChangeKind::Slower;
  }
  return ChangeKind::Unclear;
}

std::string microseconds(int64_t nanoseconds) {
  return std::format("{:.1f}", static_cast<double>(nanoseconds) / 1000.0);
}

void appendTimes(std::vector<std::string> &fields, const TimeSummary &times) {
  fields.push_back(microseconds(times.Median));
  fields.push_back("[" + microseconds(times.Least));
  fields.push_back(microseconds(times.Greatest) + "]");
}

std::string changeText(const TimeSummary &before, const TimeSummary &after) {
  if (before.Median == 0) {
    return "n/a";
  }
  const double change = static_cast<double>(after.Median - before.Median) /
                        static_cast<double>(before.Median) * 100.0;
  return std::format("{:+.1f}%", change);
}

std::string_view changeWord(ChangeKind change) {
  switch (change) {
  case ChangeKind::Faster:
    return "faster";
  case ChangeKind::Slower:
    return "slower";
  case ChangeKind::Unclear:
    break;
  }
  return "unclear";
}

std::vector<std::string> comparisonFields(const StageComparison &comparison) {
  std::vector<std::string> fields{comparison.Build, comparison.Park, comparison.Stage};
  if (comparison.Before.has_value() && comparison.After.has_value()) {
    const TimeSummary &before = comparison.Before->Times;
    const TimeSummary &after = comparison.After->Times;
    fields.emplace_back(changeWord(comparison.Change));
    fields.emplace_back("before");
    appendTimes(fields, before);
    fields.emplace_back("after");
    appendTimes(fields, after);
    fields.push_back(changeText(before, after));
    if (comparison.ResultChanged) {
      fields.emplace_back("result-changed");
    }
  } else if (comparison.Before.has_value()) {
    const TimeSummary &before = comparison.Before->Times;
    fields.emplace_back("only-before");
    appendTimes(fields, before);
  } else if (comparison.After.has_value()) {
    const TimeSummary &after = comparison.After->Times;
    fields.emplace_back("only-after");
    appendTimes(fields, after);
  }
  return fields;
}

} // namespace

std::vector<StageComparison> compareReports(std::span<const ReportPark> before,
                                            std::span<const ReportPark> after) {
  std::vector<StageComparison> comparisons;
  for (const ReportPark &park : before) {
    for (const StageResult &stage : park.Stages) {
      StageComparison comparison{park.Build,   park.Park,           stage.Name, stage,
                                 std::nullopt, ChangeKind::Unclear, false};
      const StageResult *other = findStage(after, park.Build, park.Park, stage.Name);
      if (other != nullptr) {
        comparison.After = *other;
        comparison.Change = changeBetween(stage.Times, other->Times);
        comparison.ResultChanged = stage.Kind != other->Kind || stage.Result != other->Result;
      }
      comparisons.push_back(std::move(comparison));
    }
  }
  for (const ReportPark &park : after) {
    for (const StageResult &stage : park.Stages) {
      if (findStage(before, park.Build, park.Park, stage.Name) == nullptr) {
        comparisons.push_back(StageComparison{park.Build, park.Park, stage.Name, std::nullopt,
                                              stage, ChangeKind::Unclear, false});
      }
    }
  }
  return comparisons;
}

std::string comparisonText(std::span<const StageComparison> comparisons) {
  std::vector<std::vector<std::string>> lines;
  std::vector<size_t> widths;
  size_t compared = 0;
  size_t clear = 0;
  for (const StageComparison &comparison : comparisons) {
    if (comparison.Before.has_value() && comparison.After.has_value()) {
      ++compared;
      if (comparison.Change != ChangeKind::Unclear) {
        ++clear;
      }
    }
    lines.push_back(comparisonFields(comparison));
    const std::vector<std::string> &fields = lines.back();
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
  text += "clear " + std::to_string(clear) + " of " + std::to_string(compared) + "\n";
  return text;
}

} // namespace tpj
