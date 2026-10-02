#ifndef TPJ_BENCH_REPORT_COMPARISON_H
#define TPJ_BENCH_REPORT_COMPARISON_H

#include "bench/report/report.h"
#include "bench/stages.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tpj {

// Whether a stage's launches in one report all lie beyond its launches in the other.
enum class ChangeKind { Faster, Slower, Unclear };

// One stage of one park on one build, in either report or both.
struct StageComparison {
  std::string Build;
  std::string Park;
  std::string Stage;
  std::optional<StageResult> Before;
  std::optional<StageResult> After;
  // Faster when After's Greatest is less than Before's Least, Slower when After's Least is greater
  // than Before's Greatest, and Unclear otherwise or when either is empty.
  ChangeKind Change = ChangeKind::Unclear;
  // Whether both are present and differ in Kind or Result.
  bool ResultChanged = false;

  bool operator==(const StageComparison &) const = default;
};

// One comparison per build, park, and stage name in either report: before's in its order, then
// those only in after, in its order.
std::vector<StageComparison> compareReports(std::span<const ReportPark> before,
                                            std::span<const ReportPark> after);

// One line per comparison, times in microseconds, columns aligned, then `clear <n> of <m>`.
std::string comparisonText(std::span<const StageComparison> comparisons);

} // namespace tpj

#endif
