#ifndef TPJ_TESTS_SCENARIOS_SUPPORT_SCENARIO_WORLD_H
#define TPJ_TESTS_SCENARIOS_SUPPORT_SCENARIO_WORLD_H

#include "scenarios/scenarios.h"
#include "sim/command_queue.h"
#include "sim/world.h"

#include <stdint.h>

namespace tpj::test {

// A scenario's world as a run starts it: made from its schema and the given seed, populated, and
// resolved.
inline World startScenarioWorld(const Scenario &scenario, uint64_t seed) {
  World world(scenario.MakeSchema(), seed);
  if (scenario.Populate != nullptr) {
    scenario.Populate(world);
  }
  resolveWorld(world);
  return world;
}

// One cycle, with the commands the scenario queues for it.
inline void stepScenarioWorld(const Scenario &scenario, World &world) {
  CommandQueue commands;
  if (scenario.QueueCommands != nullptr) {
    scenario.QueueCommands(world, commands);
  }
  stepWorld(world, commands);
}

} // namespace tpj::test

#endif
