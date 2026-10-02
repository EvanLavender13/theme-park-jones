#include "bench/report/report.h"

#include "bench/report/internal/lines.h"
#include "bench/report/report_error.h"
#include "bench/timing.h"

#include <algorithm>
#include <utility>

namespace tpj {

namespace {

[[noreturn]] void refuseGroup(const Launch &launch, const std::string &problem) {
  throw ReportError("launches of " + launch.Park + " on " + launch.Build + " " + problem);
}

// Refuses the launches unless every one did the work the first did.
void checkSameWork(const std::vector<const Launch *> &group) {
  const Launch &first = *group.front();
  for (const Launch *launch : group) {
    if (launch->Ticks != first.Ticks || launch->WarmUps != first.WarmUps ||
        launch->Repetitions != first.Repetitions) {
      refuseGroup(first, "differ in their ticks, warm-ups, or repetitions");
    }
    if (launch->Stages.size() != first.Stages.size()) {
      refuseGroup(first, "differ in their stages");
    }
    for (size_t s = 0; s < first.Stages.size(); ++s) {
      const StageResult &stage = launch->Stages[s];
      const StageResult &firstStage = first.Stages[s];
      if (stage.Name != firstStage.Name) {
        refuseGroup(first, "differ in their stages");
      }
      if (stage.Kind != firstStage.Kind || stage.Result != firstStage.Result) {
        refuseGroup(first, "differ in the result of stage " + firstStage.Name);
      }
    }
  }
}

ReportPark summarizeGroup(const std::vector<const Launch *> &group) {
  checkSameWork(group);
  const Launch &first = *group.front();
  ReportPark park{first.Build,       first.Park, group.size(), first.Ticks, first.WarmUps,
                  first.Repetitions, {}};
  for (size_t s = 0; s < first.Stages.size(); ++s) {
    std::vector<int64_t> medians;
    medians.reserve(group.size());
    for (const Launch *launch : group) {
      medians.push_back(launch->Stages[s].Times.Median);
    }
    StageResult stage = first.Stages[s];
    stage.Times = summarizeTimes(medians);
    park.Stages.push_back(std::move(stage));
  }
  return park;
}

ReportPark readReportParkLine(const TextLine &line) {
  if (line.Fields.size() != 11) {
    refuseLine(line, "a report's park line has 11 fields");
  }
  expectWord(line, 3, "launches");
  expectWord(line, 5, "ticks");
  expectWord(line, 7, "warm-ups");
  expectWord(line, 9, "repetitions");
  return ReportPark{std::string(line.Fields[1]),
                    std::string(line.Fields[2]),
                    static_cast<size_t>(readCount(line, 4)),
                    readCount(line, 6),
                    static_cast<size_t>(readCount(line, 8)),
                    static_cast<size_t>(readCount(line, 10)),
                    {}};
}

} // namespace

std::vector<ReportPark> summarizeLaunches(std::span<const Launch> launches) {
  std::vector<std::vector<const Launch *>> groups;
  for (const Launch &launch : launches) {
    const auto group =
        std::ranges::find_if(groups, [&launch](const std::vector<const Launch *> &members) {
          return members.front()->Build == launch.Build && members.front()->Park == launch.Park;
        });
    if (group == groups.end()) {
      groups.push_back({&launch});
    } else {
      group->push_back(&launch);
    }
  }
  std::vector<ReportPark> report;
  report.reserve(groups.size());
  for (const std::vector<const Launch *> &group : groups) {
    report.push_back(summarizeGroup(group));
  }
  return report;
}

std::string reportText(std::span<const ReportPark> report) {
  std::string text;
  for (const ReportPark &park : report) {
    text += "park " + park.Build + " " + park.Park + " launches " + std::to_string(park.Launches) +
            " ticks " + std::to_string(park.Ticks) + " warm-ups " + std::to_string(park.WarmUps) +
            " repetitions " + std::to_string(park.Repetitions) + "\n";
    for (const StageResult &stage : park.Stages) {
      text += stageLine(stage) + "\n";
    }
  }
  return text;
}

std::vector<ReportPark> parseReport(std::string_view text) {
  std::vector<ReportPark> report;
  for (const TextLine &line : readLines(text)) {
    if (line.Fields.front() == "park") {
      ReportPark park = readReportParkLine(line);
      const bool isRepeated = std::ranges::any_of(report, [&park](const ReportPark &read) {
        return read.Build == park.Build && read.Park == park.Park;
      });
      if (isRepeated) {
        refuseLine(line, "repeats the park " + park.Park + " on " + park.Build);
      }
      report.push_back(std::move(park));
    } else if (line.Fields.front() == "stage" && !report.empty()) {
      StageResult stage = readStageLine(line);
      std::vector<StageResult> &stages = report.back().Stages;
      if (std::ranges::any_of(
              stages, [&stage](const StageResult &read) { return read.Name == stage.Name; })) {
        refuseLine(line, "repeats the stage " + stage.Name);
      }
      stages.push_back(std::move(stage));
    } else {
      refuseLine(line,
                 report.empty() ? "expected a park line" : "expected a park line or a stage line");
    }
  }
  return report;
}

} // namespace tpj
