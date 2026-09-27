#include "scenarios/synthetic.h"
#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <cmath>
#include <memory>
#include <stdint.h>

namespace tpj {
namespace {

// Intent: a beacon the command placed.
struct Beacon {
  double X = 0.0;
  uint32_t Strength = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Beacon &beacon) {
  visitor.field("x", beacon.X);
  visitor.field("strength", beacon.Strength);
}

// Derived: one glow per unit of a beacon's strength, on entities keyed from the beacon.
struct Glow {
  double Level = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Glow &glow) {
  visitor.field("level", glow.Level);
}

// State: the running sum of glow levels over time.
struct Tally {
  double Total = 0.0;
  uint64_t Glows = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Tally &tally) {
  visitor.field("total", tally.Total);
  visitor.field("glows", tally.Glows);
}

struct PlaceBeacon {
  double X = 0.0;
  uint32_t Strength = 0;
};

void applyCommand(World &world, const PlaceBeacon &command) {
  const EntityKey key = world.createEntity();
  world.Registry.emplace<Beacon>(world.findEntity(key),
                                 Beacon{.X = command.X, .Strength = command.Strength});
}

constexpr uint64_t GLOW_PURPOSE = hashName("beacon-glow");

// Gives each beacon one glow per unit of strength, fading with the glow's index.
void resolveGlows(World &world) {
  for (const EntityKey key : world.keys()) {
    const auto *found = world.Registry.try_get<Beacon>(world.findEntity(key));
    if (found == nullptr) {
      continue;
    }
    // Creating entities may move the registry's storage, so the beacon is copied first.
    const Beacon beacon = *found;
    for (uint32_t i = 0; i < beacon.Strength; ++i) {
      const EntityKey glow = world.createDerivedEntity(key, GLOW_PURPOSE, i);
      world.Registry.emplace_or_replace<Glow>(
          world.findEntity(glow),
          Glow{.Level = std::sqrt(beacon.X * beacon.X + 1.0) / static_cast<double>(i + 1)});
    }
  }
}

// Adds this tick's glow to the tally, summed in key order.
void tallyGlows(World &world) {
  double sum = 0.0;
  uint64_t count = 0;
  Tally *tally = nullptr;
  for (const EntityKey key : world.keys()) {
    const entt::entity entity = world.findEntity(key);
    if (const auto *glow = world.Registry.try_get<Glow>(entity)) {
      sum += glow->Level * SIM_TICK_SECONDS;
      ++count;
    }
    if (auto *found = world.Registry.try_get<Tally>(entity)) {
      tally = found;
    }
  }
  if (tally != nullptr) {
    tally->Total += sum;
    tally->Glows = count;
  }
}

} // namespace

Scenario beaconsScenario() {
  return Scenario{
      .Name = "beacons",
      .Seed = 2002,
      .MakeSchema = []() -> std::shared_ptr<const WorldSchema> {
        auto schema = std::make_shared<WorldSchema>();
        schema->addComponent<Beacon>("beacon", DataKind::Intent);
        schema->addComponent<Glow>("glow", DataKind::Derived);
        schema->addComponent<Tally>("tally", DataKind::State);
        schema->addSystem(tallyGlows);
        schema->addResolver("glows", resolveGlows);
        schema->addCommand<PlaceBeacon>();
        return schema;
      },
      .Populate =
          [](World &world) {
            const EntityKey tally = world.createEntity();
            world.Registry.emplace<Tally>(world.findEntity(tally));
            const EntityKey beacon = world.createEntity();
            world.Registry.emplace<Beacon>(world.findEntity(beacon),
                                           Beacon{.X = 1.5, .Strength = 2});
          },
      .QueueCommands =
          [](const World &world, CommandQueue &commands) {
            if (world.Tick % 100 == 50) {
              commands.push(
                  PlaceBeacon{.X = static_cast<double>(world.Tick) / 7.0,
                              .Strength = static_cast<uint32_t>(1 + (world.Tick / 100) % 4)});
            }
          },
  };
}

} // namespace tpj
