#ifndef TPJ_SCENARIOS_RUNNER_H
#define TPJ_SCENARIOS_RUNNER_H

#include "scenarios/scenarios.h"
#include "sim/schema.h"

#include <iosfwd>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string_view>

namespace tpj {

// Makes, populates, and resolves the scenario's world, then writes its hash line and, for each of
// ticks cycles, steps it with the commands the scenario queues and writes the line again:
//   scenario <name> tick <t> hash <h>
void runScenario(const Scenario &scenario, uint64_t ticks, std::ostream &out);

// The same for a save: loads text with schema, resolves, and steps with no commands, writing
//   file <label> tick <t> hash <h>
// Throws LoadError, as loadWorld does.
void runSave(std::string_view label, std::shared_ptr<const WorldSchema> schema,
             std::string_view text, uint64_t ticks, std::ostream &out);

// simExp and simLog for every argument of sim-math's reference tables, as bits:
//   exp argument <a> result <r>
//   log argument <a> result <r>
void writeSimMathLines(std::ostream &out);

// The draws for every key of keyed-draws' expected-draw table:
//   draw <i> bits <b> uniform <u> integer-pick <p> double-pick <q>
void writeDrawLines(std::ostream &out);

// Where two outputs first differ: a 1-based line number, and each output's line there, or none
// after its end.
struct LineDifference {
  size_t Line = 0;
  std::optional<std::string_view> Left;
  std::optional<std::string_view> Right;
};

// Empty when both texts hold the same lines. A line ends at a line feed, and a last line may have
// none. Lines are compared byte for byte.
std::optional<LineDifference> firstDifference(std::string_view left, std::string_view right);

} // namespace tpj

#endif
