#ifndef TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_FLOWS_H
#define TPJ_TESTS_SIM_SUPPORT_SYNTHETIC_FLOWS_H

#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/medium/flow.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <memory>
#include <stdint.h>
#include <string_view>
#include <utility>
#include <vector>

// Synthetic flow kinds owned by the tests, the endpoints that trade in them, and the systems and
// command that drive randomized runs, all registered through the public schema.
namespace tpj::test {

struct Meals {
  static constexpr std::string_view Name = "meals";
};

struct Supplies {
  static constexpr std::string_view Name = "supplies";
};

// Marks an entity as an endpoint the trading systems act for.
struct Trader {};

inline std::vector<EntityKey> traderKeys(const World &world) {
  std::vector<EntityKey> traders;
  for (const EntityKey key : world.keys()) {
    if (world.Registry.all_of<Trader>(world.findEntity(key))) {
      traders.push_back(key);
    }
  }
  return traders;
}

// A key the world has issued from its counter, drawn from the bits, whether or not it is still
// live. The world must have issued at least one.
inline EntityKey issuedKey(const World &world, uint64_t bits) {
  return EntityKey{1 + (bits % (world.nextKey() - 1))};
}

// One tick of an endpoint's trade in K, driven by draws keyed by the endpoint. It creates units
// addressed to no one, to itself, or to any issued key, sends part of a holding to any issued key,
// live or not, and sometimes consumes a unit. It reads only its own stock and the world's keys, so
// its operations do not depend on what other endpoints do in the same tick.
template <FlowDefinition K> void tradeFor(World &world, EntityKey endpoint) {
  const uint64_t purpose = hashName(K::Name);
  auto draw = [&](uint64_t index) { return drawBits(drawKey(world, endpoint, purpose, index)); };

  const uint64_t handleChoice = draw(0) % 3;
  EntityKey handle = NULL_KEY;
  if (handleChoice == 1) {
    handle = endpoint;
  } else if (handleChoice == 2) {
    handle = issuedKey(world, draw(1));
  }
  createUnits<K>(world, endpoint, handle, static_cast<int64_t>(1 + (draw(2) % 3)));

  std::vector<FlowHolding> holdings = stockOf<K>(world, endpoint);
  const FlowHolding sent = holdings.empty() ? FlowHolding{} : holdings[draw(3) % holdings.size()];
  if (sent.Units > 0) {
    const auto units = static_cast<int64_t>(1 + (draw(4) % static_cast<uint64_t>(sent.Units)));
    const auto delay = static_cast<uint32_t>(1 + (draw(5) % 4));
    sendUnits<K>(world, endpoint, issuedKey(world, draw(6)), sent.Handle, units, delay);
  }

  holdings = stockOf<K>(world, endpoint);
  if (!holdings.empty() && draw(7) % 2 == 0) {
    consumeUnits<K>(world, endpoint, holdings[draw(8) % holdings.size()].Handle, 1, "used");
  }
}

// A system trading in K for each trader whose key Select selects, in ascending key order.
template <FlowDefinition K, bool (*Select)(EntityKey)> void tradeWhere(World &world) {
  for (const EntityKey trader : traderKeys(world)) {
    if (Select(trader)) {
      tradeFor<K>(world, trader);
    }
  }
}

inline bool anyKey(EntityKey /*key*/) { return true; }
inline bool evenKey(EntityKey key) { return static_cast<uint64_t>(key) % 2 == 0; }
inline bool oddKey(EntityKey key) { return static_cast<uint64_t>(key) % 2 == 1; }

// A system trading in K for every trader.
template <FlowDefinition K> void tradeAll(World &world) { tradeWhere<K, anyKey>(world); }

// Removes a drawn trader and adds new ones, so endpoints come and go with units held by them,
// addressed to them, and in transit to and from them.
struct Churn {};

inline void applyCommand(World &world, const Churn & /*command*/) {
  constexpr uint64_t PURPOSE = hashName("churn");
  auto draw = [&](uint64_t index) { return drawBits(drawKey(world, NULL_KEY, PURPOSE, index)); };
  const std::vector<EntityKey> traders = traderKeys(world);
  if (traders.size() > 1 && draw(0) % 4 == 0) {
    world.destroyEntity(traders[draw(1) % traders.size()]);
  }
  if (traders.size() < 3 || draw(2) % 4 == 0) {
    world.Registry.emplace<Trader>(world.findEntity(world.createEntity()));
  }
}

// Registers trader (state), meals and supplies, and Churn, with no systems.
inline std::shared_ptr<WorldSchema> makeFlowSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Trader>("trader", DataKind::State);
  addFlow<Meals>(*schema);
  addFlow<Supplies>(*schema);
  schema->addCommand<Churn>();
  return schema;
}

// The flow schema with the given systems, in the order given.
inline std::shared_ptr<const WorldSchema>
makeTradeSchema(const std::vector<WorldFunction> &systems) {
  auto schema = makeFlowSchema();
  for (const WorldFunction system : systems) {
    schema->addSystem(system);
  }
  return schema;
}

// A resolved world of the given number of traders, keyed from 1.
inline World makeTradeWorld(std::shared_ptr<const WorldSchema> schema, uint64_t seed, int traders) {
  World world(std::move(schema), seed);
  for (int index = 0; index < traders; ++index) {
    world.Registry.emplace<Trader>(world.findEntity(world.createEntity()));
  }
  resolveWorld(world);
  return world;
}

// One cycle with a Churn command.
inline void stepWithChurn(World &world) {
  CommandQueue commands;
  commands.push(Churn{});
  stepWorld(world, commands);
}

} // namespace tpj::test

#endif
