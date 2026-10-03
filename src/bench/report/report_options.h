#ifndef TPJ_BENCH_REPORT_REPORT_OPTIONS_H
#define TPJ_BENCH_REPORT_REPORT_OPTIONS_H

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tpj {

// What tpj_bench_report is asked to do.
enum class ReportCommandKind { Summarize, Compare, Show, Frames };

// tpj_bench_report's command and the files it reads.
struct ReportOptions {
  ReportCommandKind Command = ReportCommandKind::Summarize;
  std::vector<std::string> Paths;

  bool operator==(const ReportOptions &) const = default;
};

// Reads tpj_bench_report summarize LAUNCHES, compare BEFORE AFTER, show REPORT, or frames PARK
// OUTPUT, the first argument being the program's name. None, with error naming the problem, for no
// command, an unknown command, or a count of arguments other than one for summarize and show and
// two for compare and frames.
std::optional<ReportOptions> parseReportOptions(std::span<const std::string> arguments,
                                                std::string &error);

} // namespace tpj

#endif
