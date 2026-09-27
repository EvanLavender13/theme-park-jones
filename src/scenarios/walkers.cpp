#include "scenarios/synthetic.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/sim_math.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

// A walker drifts toward home by keyed softmax picks, and retires after its lifetime.
struct Walker {
  double Position = 0.0;
  double Energy = 0.0;
  uint32_t Age = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Walker &walker) {
  visitor.field("position", walker.Position);
  visitor.field("energy", walker.Energy);
  visitor.field("age", walker.Age);
}

constexpr uint64_t STEP_PURPOSE = hashName("walker-step");
constexpr uint64_t REST_PURPOSE = hashName("walker-rest");
constexpr uint64_t SPAWN_PURPOSE = hashName("walker-spawn");
constexpr double HOME = 8.0;
constexpr uint32_t LIFETIME = 400;
constexpr uint64_t SPAWN_INTERVAL = 25;
constexpr int START_WALKERS = 16;
constexpr std::array<double, 3> STEPS = {-0.5, 0.0, 0.5};

// Each walker picks a step by softmax over how close it would end to home, weighted by its
// energy, then its energy relaxes toward a drawn rest.
void stepWalkers(World &world) {
  for (const EntityKey key : world.keys()) {
    auto *walker = world.Registry.try_get<Walker>(world.findEntity(key));
    if (walker == nullptr) {
      continue;
    }
    std::array<double, 3> scores{};
    for (size_t i = 0; i < STEPS.size(); ++i) {
      scores[i] = -std::fabs(walker->Position + STEPS[i] - HOME) * walker->Energy;
    }
    const double highest = *std::max_element(scores.begin(), scores.end());
    std::array<double, 3> weights{};
    for (size_t i = 0; i < STEPS.size(); ++i) {
      weights[i] = simExp(scores[i] - highest);
    }
    const size_t pick =
        drawPick(drawKey(world, key, STEP_PURPOSE, 0), std::span<const double>(weights));
    walker->Position += STEPS[pick];
    walker->Energy = walker->Energy * 0.99 +
                     0.05 * simLog(1.0 + drawUniform(drawKey(world, key, REST_PURPOSE, 0)));
    walker->Age += 1;
  }
}

// Walkers retire at their lifetime, and every SPAWN_INTERVAL ticks one joins at a drawn position.
void turnOverWalkers(World &world) {
  std::vector<EntityKey> retiring;
  for (const EntityKey key : world.keys()) {
    const auto *walker = world.Registry.try_get<Walker>(world.findEntity(key));
    if (walker != nullptr && walker->Age >= LIFETIME) {
      retiring.push_back(key);
    }
  }
  for (const EntityKey key : retiring) {
    world.destroyEntity(key);
  }
  if (world.Tick % SPAWN_INTERVAL == 0) {
    const double place = 16.0 * drawUniform(drawKey(world, NULL_KEY, SPAWN_PURPOSE, 0));
    const EntityKey key = world.createEntity();
    world.Registry.emplace<Walker>(world.findEntity(key),
                                   Walker{.Position = place, .Energy = 1.0, .Age = 0});
  }
}

} // namespace

Scenario walkersScenario() {
  return Scenario{
      .Name = "walkers",
      .Seed = 1001,
      .MakeSchema = []() -> std::shared_ptr<const WorldSchema> {
        auto schema = std::make_shared<WorldSchema>();
        schema->addComponent<Walker>("walker", DataKind::State);
        schema->addSystem(stepWalkers);
        schema->addSystem(turnOverWalkers);
        return schema;
      },
      .Populate =
          [](World &world) {
            for (int i = 0; i < START_WALKERS; ++i) {
              const EntityKey key = world.createEntity();
              world.Registry.emplace<Walker>(
                  world.findEntity(key),
                  Walker{.Position = 0.5 * i, .Energy = 1.0 + 0.125 * i, .Age = 0});
            }
          },
      .QueueCommands = nullptr,
  };
}

} // namespace tpj
