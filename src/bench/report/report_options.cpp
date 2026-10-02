#include "bench/report/report_options.h"

#include <stddef.h>

namespace tpj {

std::optional<ReportOptions> parseReportOptions(std::span<const std::string> arguments,
                                                std::string &error) {
  if (arguments.size() < 2) {
    error = "no command given";
    return std::nullopt;
  }
  const std::string &command = arguments[1];
  ReportOptions options;
  size_t files = 0;
  if (command == "summarize") {
    options.Command = ReportCommandKind::Summarize;
    files = 1;
  } else if (command == "compare") {
    options.Command = ReportCommandKind::Compare;
    files = 2;
  } else {
    error = "unknown command " + command;
    return std::nullopt;
  }
  options.Paths.assign(arguments.begin() + 2, arguments.end());
  if (options.Paths.size() != files) {
    error = command + (files == 1 ? " takes one file" : " takes two files");
    return std::nullopt;
  }
  return options;
}

} // namespace tpj
