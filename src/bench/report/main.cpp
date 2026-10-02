// tpj_bench_report: summarizes tpj_bench's launches into a report, and compares two reports.
//
//   tpj_bench_report summarize LAUNCHES
//   tpj_bench_report compare BEFORE AFTER

#include "bench/report/comparison.h"
#include "bench/report/launches.h"
#include "bench/report/report.h"
#include "bench/report/report_error.h"
#include "bench/report/report_options.h"
#include "bench/text_file.h"

#include <exception>
#include <iostream>
#include <optional>
#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char **argv) {
  const std::vector<std::string> arguments(argv, argv + argc);
  std::string error;
  const std::optional<tpj::ReportOptions> options = tpj::parseReportOptions(arguments, error);
  if (!options) {
    std::cerr << "tpj_bench_report: " << error
              << "\nusage: tpj_bench_report summarize LAUNCHES | compare BEFORE AFTER\n";
    return 2;
  }
  std::vector<std::string> texts;
  for (const std::string &path : options->Paths) {
    std::optional<std::string> text = tpj::readTextFile(path);
    if (!text) {
      std::cerr << "tpj_bench_report: cannot read " << path << '\n';
      return 1;
    }
    texts.push_back(std::move(*text));
  }
  // The file being read, which a ReportError's message names.
  size_t reading = 0;
  try {
    std::string output;
    if (options->Command == tpj::ReportCommandKind::Summarize) {
      output = tpj::reportText(tpj::summarizeLaunches(tpj::parseLaunches(texts[0])));
    } else {
      const std::vector<tpj::ReportPark> before = tpj::parseReport(texts[0]);
      reading = 1;
      const std::vector<tpj::ReportPark> after = tpj::parseReport(texts[1]);
      output = tpj::comparisonText(tpj::compareReports(before, after));
    }
    std::cout << output;
  } catch (const tpj::ReportError &failure) {
    std::cerr << "tpj_bench_report: " << options->Paths[reading] << ": " << failure.what() << '\n';
    return 1;
  } catch (const std::exception &failure) {
    std::cerr << "tpj_bench_report: " << failure.what() << '\n';
    return 1;
  }
  return 0;
}
