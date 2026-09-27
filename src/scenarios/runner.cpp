#include "scenarios/runner.h"
#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/save.h"
#include "sim/sim_math.h"
#include "sim/world.h"
#include "tests/sim/support/expected_draws.h"
#include "tests/sim/support/sim_math_reference.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// Exactly 16 lowercase hexadecimal digits.
std::string hex16(uint64_t value) {
  std::array<char, 16> digits{};
  const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value, 16);
  const auto length = static_cast<size_t>(result.ptr - digits.data());
  return std::string(16 - length, '0') + std::string(digits.data(), length);
}

void writeHashLine(std::ostream &out, std::string_view source, std::string_view label,
                   const World &world) {
  out << source << ' ' << label << " tick " << world.Tick << " hash " << hex16(hashWorld(world))
      << '\n';
}

void writeMathLines(std::ostream &out, std::string_view name,
                    std::span<const test::SimMathReference> table, double (*function)(double)) {
  for (const test::SimMathReference &entry : table) {
    out << name << " argument " << hex16(std::bit_cast<uint64_t>(entry.Argument)) << " result "
        << hex16(std::bit_cast<uint64_t>(function(entry.Argument))) << '\n';
  }
}

// The pieces of text ending at each line feed, without it, and the remainder if not empty.
std::vector<std::string_view> splitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  while (!text.empty()) {
    const size_t end = text.find('\n');
    if (end == std::string_view::npos) {
      lines.push_back(text);
      break;
    }
    lines.push_back(text.substr(0, end));
    text.remove_prefix(end + 1);
  }
  return lines;
}

} // namespace

void runScenario(const Scenario &scenario, uint64_t ticks, std::ostream &out) {
  World world(scenario.MakeSchema(), scenario.Seed);
  scenario.Populate(world);
  resolveWorld(world);
  writeHashLine(out, "scenario", scenario.Name, world);
  for (uint64_t i = 0; i < ticks; ++i) {
    CommandQueue commands;
    if (scenario.QueueCommands != nullptr) {
      scenario.QueueCommands(world, commands);
    }
    stepWorld(world, commands);
    writeHashLine(out, "scenario", scenario.Name, world);
  }
}

void runSave(std::string_view label, std::shared_ptr<const WorldSchema> schema,
             std::string_view text, uint64_t ticks, std::ostream &out) {
  World world = loadWorld(std::move(schema), text);
  resolveWorld(world);
  writeHashLine(out, "file", label, world);
  for (uint64_t i = 0; i < ticks; ++i) {
    stepWorld(world);
    writeHashLine(out, "file", label, world);
  }
}

void writeSimMathLines(std::ostream &out) {
  writeMathLines(out, "exp", test::EXP_REFERENCE, simExp);
  writeMathLines(out, "log", test::LOG_REFERENCE, simLog);
}

void writeDrawLines(std::ostream &out) {
  for (size_t i = 0; i < test::EXPECTED_DRAWS.size(); ++i) {
    const DrawKey &key = test::EXPECTED_DRAWS[i].Key;
    out << "draw " << i << " bits " << hex16(drawBits(key)) << " uniform "
        << hex16(std::bit_cast<uint64_t>(drawUniform(key))) << " integer-pick "
        << drawPick(key, std::span<const uint64_t>(test::EXPECTED_DRAW_INTEGER_WEIGHTS))
        << " double-pick "
        << drawPick(key, std::span<const double>(test::EXPECTED_DRAW_DOUBLE_WEIGHTS)) << '\n';
  }
}

std::optional<LineDifference> firstDifference(std::string_view left, std::string_view right) {
  const std::vector<std::string_view> leftLines = splitLines(left);
  const std::vector<std::string_view> rightLines = splitLines(right);
  const size_t count = std::max(leftLines.size(), rightLines.size());
  for (size_t i = 0; i < count; ++i) {
    const bool hasLeft = i < leftLines.size();
    const bool hasRight = i < rightLines.size();
    if (hasLeft && hasRight && leftLines[i] == rightLines[i]) {
      continue;
    }
    return LineDifference{
        .Line = i + 1,
        .Left = hasLeft ? std::optional<std::string_view>(leftLines[i]) : std::nullopt,
        .Right = hasRight ? std::optional<std::string_view>(rightLines[i]) : std::nullopt};
  }
  return std::nullopt;
}

} // namespace tpj
