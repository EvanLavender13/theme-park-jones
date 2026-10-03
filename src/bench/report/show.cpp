#include "bench/report/show.h"

#include "bench/report/internal/columns.h"

#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

std::string showText(std::span<const ReportPark> report) {
  std::vector<std::vector<std::string>> lines;
  size_t over = 0;
  for (const ReportPark &park : report) {
    for (const ReportStage &stage : park.Stages) {
      std::vector<std::string> fields{park.Build, park.Park, stage.Stage.Name};
      appendTimes(fields, stage.Stage.Times);
      fields.emplace_back("worst");
      fields.push_back(microseconds(stage.Worst));
      if (stage.Stage.Times.Median > STAGE_BUDGET_NANOSECONDS) {
        fields.emplace_back("over-budget");
        ++over;
      }
      lines.push_back(std::move(fields));
    }
  }
  return alignedText(lines) + "over-budget " + std::to_string(over) + " of " +
         std::to_string(lines.size()) + "\n";
}

} // namespace tpj
