#ifndef TPJ_SCENARIOS_SCENARIOS_H
#define TPJ_SCENARIOS_SCENARIOS_H

#include "sim/command_queue.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <memory>
#include <span>
#include <stdint.h>
#include <string_view>

namespace tpj {

// A synthetic world for the scenario runner: its schema and seed, how to fill a new world, and the
// commands it queues before each cycle.
struct Scenario {
  // Lowercase letters, digits, and hyphens, distinct among the registered scenarios.
  std::string_view Name;
  uint64_t Seed = 0;
  std::shared_ptr<const WorldSchema> (*MakeSchema)() = nullptr;
  void (*Populate)(World &world) = nullptr;
  // Queues the commands for the cycle about to run on the world. Null when the scenario queues
  // none.
  void (*QueueCommands)(const World &world, CommandQueue &commands) = nullptr;
};

// The registered scenarios, in a written order.
std::span<const Scenario> registeredScenarios();

} // namespace tpj

#endif
