#include "support/scenario_world.h"

#include "tests/sim/support/expected_draws.h"
#include "tests/sim/support/sim_math_reference.h"

#include "scenarios/runner.h"
#include "scenarios/scenarios.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/field_text.h"
#include "sim/mix.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/sim_math.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpj {
namespace {

std::string hex16(uint64_t value) { return std::format("{:016x}", value); }

std::string hashLine(std::string_view prefix, std::string_view source, const World &world) {
  return std::format("{} {} tick {} hash {}\n", prefix, source, world.Tick,
                     hex16(hashWorld(world)));
}

// A save's world as runSave needs it: intent, state a system advances every tick, and derived data
// a resolver regenerates from the intent.
struct Plan {
  int64_t Size = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Plan &plan) {
  visitor.field("size", plan.Size);
}

struct Progress {
  int64_t Count = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Progress &progress) {
  visitor.field("count", progress.Count);
}

struct Echo {
  int64_t Size = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Echo &echo) {
  visitor.field("size", echo.Size);
}

constexpr uint64_t ECHO_PURPOSE = hashName("echo");

void advanceProgress(World &world) {
  world.Registry.view<Progress>().each([](Progress &progress) { ++progress.Count; });
}

void deriveEchoes(World &world) {
  std::vector<std::pair<EntityKey, int64_t>> plans;
  world.Registry.view<Plan>().each([&](entt::entity entity, const Plan &plan) {
    plans.emplace_back(world.keyOf(entity), plan.Size);
  });
  for (const auto &[owner, size] : plans) {
    const EntityKey key = world.createDerivedEntity(owner, ECHO_PURPOSE, 0);
    world.Registry.emplace_or_replace<Echo>(world.findEntity(key), Echo{.Size = size});
  }
}

std::shared_ptr<const WorldSchema> makeSaveSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Plan>("plan", DataKind::Intent);
  schema->addComponent<Progress>("progress", DataKind::State);
  schema->addComponent<Echo>("echo", DataKind::Derived);
  schema->addSystem(advanceProgress);
  schema->addResolver("echoes", deriveEchoes);
  return schema;
}

// Part-way through: the tick is not zero and the state has advanced.
constexpr std::string_view SAVE_TEXT = "tpj-park 1\n"
                                       "seed 99\n"
                                       "tick 5\n"
                                       "next-key 2\n"
                                       "\n"
                                       "[plan]\n"
                                       "1 size=3\n"
                                       "\n"
                                       "[progress]\n"
                                       "1 count=4\n";

TEST_CASE("runScenario writes the hash of the scenario's started world and of each cycle after") {
  // Zero ticks leaves only the started world's line; a few ticks show each cycle's.
  for (const uint64_t ticks : {uint64_t{0}, uint64_t{5}}) {
    for (const Scenario &scenario : registeredScenarios()) {
      INFO("scenario " << scenario.Name << ", " << ticks << " ticks");
      World world = test::startScenarioWorld(scenario, scenario.Seed);
      std::string expected = hashLine("scenario", scenario.Name, world);
      for (uint64_t tick = 0; tick < ticks; ++tick) {
        test::stepScenarioWorld(scenario, world);
        expected += hashLine("scenario", scenario.Name, world);
      }

      std::ostringstream out;
      runScenario(scenario, ticks, out);
      CHECK(out.str() == expected);
    }
  }
}

TEST_CASE("Running a scenario twice writes identical lines") {
  constexpr uint64_t TICKS = 100;
  for (const Scenario &scenario : registeredScenarios()) {
    INFO("scenario " << scenario.Name);
    std::ostringstream first;
    std::ostringstream second;
    runScenario(scenario, TICKS, first);
    runScenario(scenario, TICKS, second);
    CHECK(first.str() == second.str());
  }
}

// The first line is the resolved world, so the park's derived data is regenerated from the save,
// never read from it.
TEST_CASE("runSave writes the hash of the loaded, resolved world and of each cycle after") {
  const auto schema = makeSaveSchema();
  for (const uint64_t ticks : {uint64_t{0}, uint64_t{4}}) {
    INFO(ticks << " ticks");
    World world = loadWorld(schema, SAVE_TEXT);
    resolveWorld(world);
    std::string expected = hashLine("file", "parks/sample.park", world);
    for (uint64_t tick = 0; tick < ticks; ++tick) {
      stepWorld(world);
      expected += hashLine("file", "parks/sample.park", world);
    }

    std::ostringstream out;
    runSave("parks/sample.park", schema, SAVE_TEXT, ticks, out);
    CHECK(out.str() == expected);
  }
}

TEST_CASE("runSave throws loadWorld's LoadError for text that is not a save") {
  // The error is on line 3, where tick should be, so the line it names is not a default.
  constexpr std::string_view NOT_A_SAVE = "tpj-park 1\nseed 99\nbogus 5\n";
  const auto schema = makeSaveSchema();
  std::optional<size_t> loadLine;
  try {
    loadWorld(schema, NOT_A_SAVE);
  } catch (const LoadError &error) {
    loadLine = error.line();
  }
  REQUIRE(loadLine.has_value());

  std::optional<size_t> runLine;
  std::ostringstream out;
  try {
    runSave("parks/broken.park", schema, NOT_A_SAVE, 3, out);
  } catch (const LoadError &error) {
    runLine = error.line();
  }
  CHECK(runLine == loadLine);
}

TEST_CASE("writeSimMathLines writes simExp and simLog of each reference argument, as bits") {
  std::string expected;
  for (const test::SimMathReference &reference : test::EXP_REFERENCE) {
    expected += std::format("exp argument {} result {}\n",
                            hex16(std::bit_cast<uint64_t>(reference.Argument)),
                            hex16(std::bit_cast<uint64_t>(simExp(reference.Argument))));
  }
  for (const test::SimMathReference &reference : test::LOG_REFERENCE) {
    expected += std::format("log argument {} result {}\n",
                            hex16(std::bit_cast<uint64_t>(reference.Argument)),
                            hex16(std::bit_cast<uint64_t>(simLog(reference.Argument))));
  }

  std::ostringstream out;
  writeSimMathLines(out);
  CHECK(out.str() == expected);
}

TEST_CASE("writeDrawLines writes the draws for each key of the expected-draw table") {
  const std::span<const uint64_t> integerWeights(test::EXPECTED_DRAW_INTEGER_WEIGHTS);
  const std::span<const double> doubleWeights(test::EXPECTED_DRAW_DOUBLE_WEIGHTS);
  std::string expected;
  for (size_t index = 0; index < test::EXPECTED_DRAWS.size(); ++index) {
    const DrawKey &key = test::EXPECTED_DRAWS.at(index).Key;
    expected += std::format("draw {} bits {} uniform {} integer-pick {} double-pick {}\n", index,
                            hex16(drawBits(key)), hex16(std::bit_cast<uint64_t>(drawUniform(key))),
                            drawPick(key, integerWeights), drawPick(key, doubleWeights));
  }

  std::ostringstream out;
  writeDrawLines(out);
  CHECK(out.str() == expected);
}

// A difference as text, so that a failed check shows all of it: "same", or the line number and
// each side's line, quoted, or "none".
std::string describe(const std::optional<LineDifference> &difference) {
  if (!difference.has_value()) {
    return "same";
  }
  const auto side = [](const std::optional<std::string_view> &line) {
    return line.has_value() ? "\"" + std::string(*line) + "\"" : std::string("none");
  };
  return std::format("line {}, left {}, right {}", difference->Line, side(difference->Left),
                     side(difference->Right));
}

TEST_CASE("firstDifference is empty exactly for texts holding the same lines") {
  CHECK(describe(firstDifference("", "")) == "same");
  CHECK(describe(firstDifference("alpha\nbeta\n", "alpha\nbeta\n")) == "same");
  // A last line needs no line feed, so these hold the same two lines.
  CHECK(describe(firstDifference("alpha\nbeta", "alpha\nbeta\n")) == "same");
  CHECK(describe(firstDifference("alpha\nbeta\n", "alpha\nbeta")) == "same");
}

TEST_CASE("firstDifference gives the lowest differing line and each text's line there") {
  CHECK(describe(firstDifference("same\nleft two\nsame\nleft four\n",
                                 "same\nright two\nsame\nright four\n")) ==
        "line 2, left \"left two\", right \"right two\"");
}

TEST_CASE("firstDifference of a proper prefix is the line after its last, with none for it") {
  CHECK(describe(firstDifference("one\ntwo\n", "one\ntwo\nthree\n")) ==
        "line 3, left none, right \"three\"");
  CHECK(describe(firstDifference("one\ntwo\nthree\n", "one\ntwo\n")) ==
        "line 3, left \"three\", right none");
  CHECK(describe(firstDifference("", "one\n")) == "line 1, left none, right \"one\"");
}

// A Windows output whose carriage returns were not stripped must not pass as matching.
TEST_CASE("firstDifference compares lines byte for byte, carriage returns included") {
  CHECK(describe(firstDifference("one\r\ntwo\r\n", "one\ntwo\n")) ==
        "line 1, left \"one\r\", right \"one\"");
}

} // namespace
} // namespace tpj
