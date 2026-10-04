#include "support/synthetic_flows.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/flow.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

// So that a failed comparison of labeled outcomes prints them.
#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::Meals;
using test::Supplies;

// What a call threw. std::invalid_argument is itself a std::logic_error, so the two are told apart.
enum class Thrown : uint8_t { Nothing, InvalidArgument, OtherLogicError, Other };

template <typename Call> Thrown thrownBy(Call call) {
  try {
    call();
  } catch (const std::invalid_argument &) {
    return Thrown::InvalidArgument;
  } catch (const std::logic_error &) {
    return Thrown::OtherLogicError;
  } catch (...) {
    return Thrown::Other;
  }
  return Thrown::Nothing;
}

void requireSameValue(const World &one, const World &other) {
  REQUIRE(worldsEqual(one, other));
  REQUIRE(worldsEqual(other, one));
  REQUIRE(hashWorld(one) == hashWorld(other));
}

// Scripted actions, run by the scripted system in the tick they are keyed by. Systems take only
// the world, so the script lives outside it.
using Action = std::function<void(World &)>;

std::map<uint64_t, std::vector<Action>> &script() {
  static std::map<uint64_t, std::vector<Action>> actions;
  return actions;
}

void at(uint64_t tick, Action action) { script()[tick].push_back(std::move(action)); }

void runScript(World &world) {
  const auto found = script().find(world.Tick);
  if (found == script().end()) {
    return;
  }
  const std::vector<Action> actions = found->second;
  for (const Action &action : actions) {
    action(world);
  }
}

// Clears the script for a test, and after it.
class ScriptScope {
public:
  ScriptScope() { script().clear(); }
  ScriptScope(const ScriptScope &) = delete;
  ScriptScope &operator=(const ScriptScope &) = delete;
  ScriptScope(ScriptScope &&) = delete;
  ScriptScope &operator=(ScriptScope &&) = delete;
  ~ScriptScope() { script().clear(); }
};

struct RemoveEntity {
  EntityKey Key = NULL_KEY;
};

[[maybe_unused]] void applyCommand(World &world, const RemoveEntity &command) {
  world.destroyEntity(command.Key);
}

// A command whose resolution leaves every endpoint live.
struct AddBystander {};

[[maybe_unused]] void applyCommand(World &world, const AddBystander & /*command*/) {
  world.createEntity();
}

// The scripted world's entities, keyed from the counter in this order. DEAD is removed before the
// first cycle, and NOWHERE is never issued.
constexpr EntityKey ENDPOINT{1};
constexpr EntityKey OTHER{2};
constexpr EntityKey HANDLE{3};
constexpr EntityKey DEAD{4};
constexpr EntityKey NOWHERE = deriveKey(NULL_KEY, hashName("nowhere"), 0);

std::shared_ptr<WorldSchema> makeScriptedSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addFlow<Meals>(*schema);
  schema->addCommand<RemoveEntity>();
  schema->addCommand<AddBystander>();
  schema->addSystem(runScript);
  return schema;
}

World makeScriptedWorld(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 7);
  for (const EntityKey expected : {ENDPOINT, OTHER, HANDLE, DEAD}) {
    REQUIRE(world.createEntity() == expected);
  }
  world.destroyEntity(DEAD);
  resolveWorld(world);
  return world;
}

void stepWith(World &world, const std::vector<EntityKey> &removals) {
  CommandQueue commands;
  for (const EntityKey key : removals) {
    commands.push(RemoveEntity{.Key = key});
  }
  stepWorld(world, commands);
}

// The kind's ledger component, or null when the world holds none.
template <FlowDefinition K> Ledger *ledgerOf(World &world) {
  const entt::entity holder = world.findEntity(flowKey(K::Name));
  return holder == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(holder);
}

using Holdings = std::vector<std::pair<EntityKey, int64_t>>;

template <FlowDefinition K> Holdings holdingsOf(const World &world, EntityKey endpoint) {
  Holdings holdings;
  for (const FlowHolding &holding : stockOf<K>(world, endpoint)) {
    holdings.emplace_back(holding.Handle, holding.Units);
  }
  return holdings;
}

// The meals ledger as the scripted tests see it: the two endpoints' stocks, and the kind's counts
// by name.
struct LedgerView {
  Holdings Actor;
  Holdings Other;
  std::map<std::string, int64_t> Counts;
};

LedgerView viewOf(const World &world) {
  return {.Actor = holdingsOf<Meals>(world, ENDPOINT),
          .Other = holdingsOf<Meals>(world, OTHER),
          .Counts = {{"created", unitsCreated<Meals>(world)},
                     {"in transit", unitsInTransit<Meals>(world)},
                     {"consumed", unitsConsumed<Meals>(world)},
                     {"eaten", unitsConsumed<Meals>(world, "eaten")},
                     {"spoiled", unitsConsumed<Meals>(world, "spoiled")},
                     {"never-used", unitsConsumed<Meals>(world, "never-used")}}};
}

void requireView(const LedgerView &actual, const LedgerView &expected) {
  CHECK(actual.Actor == expected.Actor);
  CHECK(actual.Other == expected.Other);
  CHECK(actual.Counts == expected.Counts);
}

// Tick 0 of the operation tests: the endpoint holds 2 units addressed to no one and 5 addressed to
// HANDLE, and the other endpoint holds 3 addressed to HANDLE, after eating 1.
void stockUp(World &world) {
  createUnits<Meals>(world, ENDPOINT, HANDLE, 5);
  createUnits<Meals>(world, ENDPOINT, NULL_KEY, 2);
  createUnits<Meals>(world, OTHER, HANDLE, 4);
  consumeUnits<Meals>(world, OTHER, HANDLE, 1, "eaten");
}

const LedgerView STOCKED{.Actor = {{NULL_KEY, 2}, {HANDLE, 5}},
                         .Other = {{HANDLE, 3}},
                         .Counts = {{"created", 11},
                                    {"in transit", 0},
                                    {"consumed", 1},
                                    {"eaten", 1},
                                    {"spoiled", 0},
                                    {"never-used", 0}}};

// Steps the scripted world through tick 0, which stocks it up, and tick 1, which applies the
// operation, and gives the ledger as it was just before and just after the operation.
std::pair<LedgerView, LedgerView> viewsAround(const Action &operation) {
  const ScriptScope scope;
  World world = makeScriptedWorld(makeScriptedSchema());
  LedgerView before;
  LedgerView after;
  at(0, stockUp);
  at(1, [&](World &stepping) {
    before = viewOf(stepping);
    operation(stepping);
    after = viewOf(stepping);
  });
  stepWorld(world);
  stepWorld(world);
  return {before, after};
}

const ComponentType *componentNamed(const WorldSchema &schema, std::string_view name) {
  const auto &components = schema.components();
  const auto found = std::ranges::find_if(
      components, [name](const ComponentType &type) { return type.Name == name; });
  return found == components.end() ? nullptr : &*found;
}

bool hasResolver(const WorldSchema &schema, std::string_view name) {
  return std::ranges::any_of(schema.resolvers(),
                             [name](const ResolverType &type) { return type.Name == name; });
}

TEST_CASE("addFlow registers the kind's state component as <name>-ledger, its resolver as "
          "<name>-flow, and a swap") {
  WorldSchema schema;
  addFlow<Meals>(schema);
  const size_t swaps = schema.swaps().size();
  addFlow<Supplies>(schema);

  for (const std::string_view name : {"meals", "supplies"}) {
    CAPTURE(name);
    const ComponentType *ledger = componentNamed(schema, std::string(name) + "-ledger");
    REQUIRE(ledger != nullptr);
    REQUIRE(ledger->Kind == DataKind::State);
    REQUIRE(hasResolver(schema, std::string(name) + "-flow"));
  }
  REQUIRE(schema.swaps().size() == swaps + 1);
}

void requireEmptyAnswers(const World &world) {
  REQUIRE(unitsHeld<Meals>(world, ENDPOINT, NULL_KEY) == 0);
  REQUIRE(unitsHeld<Meals>(world) == 0);
  REQUIRE(stockOf<Meals>(world, ENDPOINT).empty());
  REQUIRE(unitsInTransit<Meals>(world) == 0);
  REQUIRE(unitsCreated<Meals>(world) == 0);
  REQUIRE(unitsConsumed<Meals>(world) == 0);
  REQUIRE(unitsConsumed<Meals>(world, DISCARDED_CAUSE) == 0);
}

TEST_CASE("every query gives 0, or an empty list, for a world holding no ledger for the kind") {
  SECTION("the kind is not registered, though another is") {
    auto schema = std::make_shared<WorldSchema>();
    addFlow<Supplies>(*schema);
    World world(schema, 0);
    world.createEntity();
    resolveWorld(world);
    requireEmptyAnswers(world);
  }
  SECTION("the kind is registered, but its resolver has not yet run") {
    World world(makeScriptedSchema(), 0);
    world.createEntity();
    requireEmptyAnswers(world);
  }
}

TEST_CASE("createUnits adds the units to the acting endpoint's stock under the handle, and to the "
          "created count, and changes nothing else") {
  const auto [before, after] =
      viewsAround([](World &world) { createUnits<Meals>(world, ENDPOINT, HANDLE, 3); });

  requireView(before, STOCKED);
  LedgerView expected = STOCKED;
  expected.Actor = {{NULL_KEY, 2}, {HANDLE, 8}};
  expected.Counts["created"] += 3;
  requireView(after, expected);
}

TEST_CASE("sendUnits takes the units from the acting endpoint's stock under the handle and puts "
          "them in transit, and changes nothing else") {
  const auto [before, after] =
      viewsAround([](World &world) { sendUnits<Meals>(world, ENDPOINT, OTHER, HANDLE, 3, 2); });

  requireView(before, STOCKED);
  LedgerView expected = STOCKED;
  expected.Actor = {{NULL_KEY, 2}, {HANDLE, 2}};
  expected.Counts["in transit"] += 3;
  requireView(after, expected);
}

TEST_CASE("consumeUnits takes the units from the acting endpoint's stock under the handle and "
          "counts them consumed with the cause, and changes nothing else") {
  const auto [before, after] =
      viewsAround([](World &world) { consumeUnits<Meals>(world, ENDPOINT, HANDLE, 3, "spoiled"); });

  requireView(before, STOCKED);
  LedgerView expected = STOCKED;
  expected.Actor = {{NULL_KEY, 2}, {HANDLE, 2}};
  expected.Counts["consumed"] += 3;
  expected.Counts["spoiled"] += 3;
  requireView(after, expected);
}

TEST_CASE("stockOf gives an endpoint's holdings with handles ascending and no empty holdings") {
  const ScriptScope scope;
  World world = makeScriptedWorld(makeScriptedSchema());
  at(0, [](World &stepping) {
    createUnits<Meals>(stepping, ENDPOINT, HANDLE, 1);
    createUnits<Meals>(stepping, ENDPOINT, OTHER, 2);
    createUnits<Meals>(stepping, ENDPOINT, NOWHERE, 3);
    createUnits<Meals>(stepping, ENDPOINT, NULL_KEY, 4);
    consumeUnits<Meals>(stepping, ENDPOINT, OTHER, 2, "eaten");
  });
  stepWorld(world);

  REQUIRE(holdingsOf<Meals>(world, ENDPOINT) == Holdings{{NULL_KEY, 4}, {HANDLE, 1}, {NOWHERE, 3}});
  REQUIRE(unitsHeld<Meals>(world, ENDPOINT, OTHER) == 0);
}

// A stock as its endpoint, handle, and units, since FlowStock has no comparison.
using StockTuple = std::tuple<EntityKey, EntityKey, int64_t>;

std::vector<StockTuple> stockTuples(const std::vector<FlowStock> &stocks) {
  std::vector<StockTuple> tuples;
  tuples.reserve(stocks.size());
  for (const FlowStock &stock : stocks) {
    tuples.emplace_back(stock.Endpoint, stock.Handle, stock.Units);
  }
  return tuples;
}

TEST_CASE("addressedTo gives exactly the kind's packets and stocks whose handle is the key, each "
          "in the ledger's order") {
  const ScriptScope scope;
  World world = makeScriptedWorld(makeScriptedSchema());
  at(0, [](World &stepping) {
    createUnits<Meals>(stepping, ENDPOINT, HANDLE, 6);
    createUnits<Meals>(stepping, ENDPOINT, NULL_KEY, 2);
    createUnits<Meals>(stepping, OTHER, HANDLE, 4);
    createUnits<Meals>(stepping, OTHER, OTHER, 3);
    createUnits<Meals>(stepping, HANDLE, HANDLE, 1);
    sendUnits<Meals>(stepping, ENDPOINT, OTHER, HANDLE, 2, 5);
    sendUnits<Meals>(stepping, OTHER, ENDPOINT, HANDLE, 1, 3);
    // DEAD is gone at its arrival, so it comes back as a returning packet, still addressed.
    sendUnits<Meals>(stepping, ENDPOINT, DEAD, HANDLE, 1, 1);
    sendUnits<Meals>(stepping, ENDPOINT, OTHER, NULL_KEY, 1, 2);
    sendUnits<Meals>(stepping, OTHER, ENDPOINT, OTHER, 1, 4);
  });
  stepWorld(world);

  const FlowAddressed addressed = addressedTo<Meals>(world, HANDLE);
  CHECK(addressed.Packets == std::vector<FlowPacket>{{.Arrival = 2,
                                                      .From = DEAD,
                                                      .To = ENDPOINT,
                                                      .Handle = HANDLE,
                                                      .Units = 1,
                                                      .Delay = 1,
                                                      .Returning = true},
                                                     {.Arrival = 3,
                                                      .From = OTHER,
                                                      .To = ENDPOINT,
                                                      .Handle = HANDLE,
                                                      .Units = 1,
                                                      .Delay = 3,
                                                      .Returning = false},
                                                     {.Arrival = 5,
                                                      .From = ENDPOINT,
                                                      .To = OTHER,
                                                      .Handle = HANDLE,
                                                      .Units = 2,
                                                      .Delay = 5,
                                                      .Returning = false}});
  CHECK(stockTuples(addressed.Stocks) ==
        std::vector<StockTuple>{{ENDPOINT, HANDLE, 3}, {OTHER, HANDLE, 3}, {HANDLE, HANDLE, 1}});
}

// Each operation, with every argument valid for a scripted world stocked up in tick 0.
std::vector<std::pair<std::string, Action>> validOperations() {
  return {
      {"createUnits", [](World &world) { createUnits<Meals>(world, ENDPOINT, HANDLE, 1); }},
      {"sendUnits", [](World &world) { sendUnits<Meals>(world, ENDPOINT, OTHER, HANDLE, 1, 1); }},
      {"consumeUnits",
       [](World &world) { consumeUnits<Meals>(world, ENDPOINT, HANDLE, 1, "spoiled"); }},
  };
}

// Supplies has a ledger, and meals has none.
std::shared_ptr<const WorldSchema> makeUnledgeredSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addFlow<Supplies>(*schema);
  schema->addSystem(runScript);
  return schema;
}

using Rejections = std::vector<std::pair<std::string, Action>>;

Rejections endpointAndUnitRejections();
Rejections sendRejections();
Rejections consumeRejections();
std::vector<std::string> labelsOf(const Rejections &calls);

// Calls every operation rejects for its endpoint or units, in a world stocked up in tick 0.
Rejections endpointAndUnitRejections() {
  Rejections rejections;
  for (const EntityKey endpoint : {NULL_KEY, DEAD}) {
    const std::string who = endpoint == NULL_KEY ? " for a null endpoint" : " for a dead endpoint";
    rejections.emplace_back("createUnits" + who, [endpoint](World &world) {
      createUnits<Meals>(world, endpoint, HANDLE, 1);
    });
    rejections.emplace_back("sendUnits" + who, [endpoint](World &world) {
      sendUnits<Meals>(world, endpoint, OTHER, HANDLE, 1, 1);
    });
    rejections.emplace_back("consumeUnits" + who, [endpoint](World &world) {
      consumeUnits<Meals>(world, endpoint, HANDLE, 1, "spoiled");
    });
  }
  for (const int64_t units : {int64_t{0}, int64_t{-1}}) {
    const std::string what = " of " + std::to_string(units) + " units";
    rejections.emplace_back("createUnits" + what, [units](World &world) {
      createUnits<Meals>(world, ENDPOINT, HANDLE, units);
    });
    rejections.emplace_back("sendUnits" + what, [units](World &world) {
      sendUnits<Meals>(world, ENDPOINT, OTHER, HANDLE, units, 1);
    });
    rejections.emplace_back("consumeUnits" + what, [units](World &world) {
      consumeUnits<Meals>(world, ENDPOINT, HANDLE, units, "spoiled");
    });
  }
  return rejections;
}

// The endpoint holds 5 units addressed to HANDLE, and none addressed to OTHER.
Rejections sendRejections() {
  return {
      {"to a null destination",
       [](World &world) { sendUnits<Meals>(world, ENDPOINT, NULL_KEY, HANDLE, 1, 1); }},
      {"with a delay of 0",
       [](World &world) { sendUnits<Meals>(world, ENDPOINT, OTHER, HANDLE, 1, 0); }},
      {"of more units than held",
       [](World &world) { sendUnits<Meals>(world, ENDPOINT, OTHER, HANDLE, 6, 1); }},
      {"of a handle not held",
       [](World &world) { sendUnits<Meals>(world, ENDPOINT, OTHER, OTHER, 1, 1); }},
  };
}

Rejections consumeRejections() {
  return {
      {"with an uppercase cause",
       [](World &world) { consumeUnits<Meals>(world, ENDPOINT, HANDLE, 1, "Spoiled"); }},
      {"with an empty cause",
       [](World &world) { consumeUnits<Meals>(world, ENDPOINT, HANDLE, 1, ""); }},
      {"with a cause holding a space",
       [](World &world) { consumeUnits<Meals>(world, ENDPOINT, HANDLE, 1, "went off"); }},
      {"of more units than held",
       [](World &world) { consumeUnits<Meals>(world, ENDPOINT, HANDLE, 6, "spoiled"); }},
      {"of a handle not held",
       [](World &world) { consumeUnits<Meals>(world, ENDPOINT, OTHER, 1, "spoiled"); }},
  };
}

std::vector<std::string> labelsOf(const Rejections &calls) {
  std::vector<std::string> labels;
  for (const auto &[label, call] : calls) {
    labels.push_back(label);
  }
  return labels;
}

constexpr int64_t LARGEST = std::numeric_limits<int64_t>::max();

// After tick 0 the created count is set 7 below the largest int64_t. The ledger is a public medium
// component, so a test may set it directly.
World nearlyFullWorld() {
  World world = makeScriptedWorld(makeScriptedSchema());
  at(0, stockUp);
  stepWorld(world);
  Ledger *ledger = ledgerOf<Meals>(world);
  REQUIRE(ledger != nullptr);
  ledger->Created = LARGEST - 7;
  return world;
}

std::vector<std::pair<std::string, bool>> &unchanged() {
  static std::vector<std::pair<std::string, bool>> recorded;
  return recorded;
}

// Records, for each call, whether the world's hash is the same after it threw as before.
void recordUnchanged(World &world, const Rejections &calls) {
  for (const auto &labeled : calls) {
    const Action &call = labeled.second;
    const uint64_t before = hashWorld(world);
    const Thrown thrown = thrownBy([&] { call(world); });
    unchanged().emplace_back(labeled.first,
                             thrown != Thrown::Nothing && hashWorld(world) == before);
  }
}

std::vector<std::pair<std::string, bool>> allUnchanged(const std::vector<std::string> &labels) {
  std::vector<std::pair<std::string, bool>> labeled;
  labeled.reserve(labels.size());
  for (const std::string &label : labels) {
    labeled.emplace_back(label, true);
  }
  return labeled;
}

TEST_CASE("an operation that throws changes nothing") {
  const ScriptScope scope;
  unchanged().clear();

  SECTION("rejected for its arguments while stepping") {
    Rejections calls = endpointAndUnitRejections();
    std::ranges::move(sendRejections(), std::back_inserter(calls));
    std::ranges::move(consumeRejections(), std::back_inserter(calls));
    calls.emplace_back("createUnits beyond the largest created count",
                       [](World &world) { createUnits<Meals>(world, ENDPOINT, HANDLE, 8); });
    World world = nearlyFullWorld();
    at(1, [&calls](World &stepping) { recordUnchanged(stepping, calls); });
    stepWorld(world);
    REQUIRE(unchanged() == allUnchanged(labelsOf(calls)));
  }
  SECTION("rejected for want of a ledger") {
    World world = makeScriptedWorld(makeUnledgeredSchema());
    const Rejections calls = validOperations();
    at(0, [&calls](World &stepping) { recordUnchanged(stepping, calls); });
    stepWorld(world);
    REQUIRE(unchanged() == allUnchanged(labelsOf(calls)));
  }
  SECTION("rejected outside stepping") {
    World world = makeScriptedWorld(makeScriptedSchema());
    at(0, stockUp);
    stepWorld(world);
    const World before = copyWorld(world);
    for (const auto &operation : validOperations()) {
      const Action &call = operation.second;
      CAPTURE(operation.first);
      REQUIRE(thrownBy([&] { call(world); }) != Thrown::Nothing);
      requireSameValue(world, before);
    }
  }
}

TEST_CASE("a packet sent while stepping tick t with delay d to a destination live at its arrival "
          "enters the destination's stock under its handle at the swap that reaches tick t + d, "
          "and at no earlier swap") {
  const ScriptScope scope;
  // Commands between sending and arrival make the ledger's resolver run again.
  bool withCommands = false;
  SECTION("with no commands in between") { withCommands = false; }
  SECTION("with a command and a resolution in every cycle in between") { withCommands = true; }

  // The shortest delay, and a longer one.
  for (const uint32_t delay : {1U, 3U}) {
    CAPTURE(delay);
    script().clear();
    World world = makeScriptedWorld(makeScriptedSchema());
    at(1, [delay](World &stepping) {
      createUnits<Meals>(stepping, ENDPOINT, HANDLE, 4);
      sendUnits<Meals>(stepping, ENDPOINT, OTHER, HANDLE, 4, delay);
    });
    stepWorld(world);

    const uint64_t arrival = 1 + delay;
    while (world.Tick <= arrival) {
      CommandQueue commands;
      if (withCommands) {
        commands.push(AddBystander{});
      }
      stepWorld(world, commands);
      CAPTURE(world.Tick);
      const bool arrived = world.Tick >= arrival;
      REQUIRE(holdingsOf<Meals>(world, OTHER) == (arrived ? Holdings{{HANDLE, 4}} : Holdings{}));
      REQUIRE(unitsInTransit<Meals>(world) == (arrived ? 0 : 4));
    }
  }
}

TEST_CASE("a packet whose destination is not live at its arrival goes back to its sender under "
          "the same handle, which holds the units again from tick t + 2d") {
  const ScriptScope scope;
  EntityKey destination = NULL_KEY;
  std::vector<EntityKey> removedInFirstCycle;
  SECTION("a destination removed before the packet arrives") {
    destination = OTHER;
    removedInFirstCycle = {OTHER};
  }
  SECTION("a destination never live") { destination = NOWHERE; }

  // Sent while stepping tick 0 with delay 2.
  World world = makeScriptedWorld(makeScriptedSchema());
  at(0, [destination](World &stepping) {
    createUnits<Meals>(stepping, ENDPOINT, HANDLE, 4);
    sendUnits<Meals>(stepping, ENDPOINT, destination, HANDLE, 4, 2);
  });
  stepWith(world, removedInFirstCycle);

  while (world.Tick < 5) {
    CAPTURE(world.Tick);
    const bool returned = world.Tick >= 4;
    REQUIRE(holdingsOf<Meals>(world, ENDPOINT) == (returned ? Holdings{{HANDLE, 4}} : Holdings{}));
    REQUIRE(unitsInTransit<Meals>(world) == (returned ? 0 : 4));
    REQUIRE(unitsHeld<Meals>(world) == (returned ? 4 : 0));
    REQUIRE(unitsConsumed<Meals>(world) == 0);
    stepWorld(world);
  }
}

TEST_CASE("a packet is consumed as undeliverable when its sender is not live either at its "
          "arrival or at its return's") {
  const ScriptScope scope;
  // Sent while stepping tick 0 with delay 2, so it arrives at tick 2 and would return at tick 4.
  World world = makeScriptedWorld(makeScriptedSchema());
  at(0, [](World &stepping) {
    createUnits<Meals>(stepping, ENDPOINT, HANDLE, 4);
    sendUnits<Meals>(stepping, ENDPOINT, OTHER, HANDLE, 4, 2);
  });

  uint64_t consumedAt = 0;
  SECTION("a sender removed before the packet arrives") {
    consumedAt = 2;
    stepWith(world, {OTHER, ENDPOINT});
  }
  SECTION("a sender removed before the return arrives") {
    consumedAt = 4;
    stepWith(world, {OTHER});
    stepWorld(world);
    // Applied after the swap that reaches tick 3.
    stepWith(world, {ENDPOINT});
  }

  while (world.Tick <= consumedAt) {
    CAPTURE(world.Tick);
    const bool consumed = world.Tick >= consumedAt;
    REQUIRE(unitsInTransit<Meals>(world) == (consumed ? 0 : 4));
    REQUIRE(unitsConsumed<Meals>(world, UNDELIVERABLE_CAUSE) == (consumed ? 4 : 0));
    REQUIRE(unitsConsumed<Meals>(world) == (consumed ? 4 : 0));
    REQUIRE(unitsHeld<Meals>(world) == 0);
    stepWorld(world);
  }
}

// The endpoint holds 5 units addressed to HANDLE, which stays live, 3 addressed to no one, and 2
// addressed to itself.
void stockForRemoval(World &world) {
  createUnits<Meals>(world, ENDPOINT, HANDLE, 5);
  createUnits<Meals>(world, ENDPOINT, NULL_KEY, 3);
  createUnits<Meals>(world, ENDPOINT, ENDPOINT, 2);
}

const Holdings HELD_FOR_REMOVAL = {{NULL_KEY, 3}, {ENDPOINT, 2}, {HANDLE, 5}};

TEST_CASE("at the first swap after an endpoint is removed, each handle's units go to the handle's "
          "entity under the same handle when it is live and are consumed as discarded otherwise, "
          "and until then they are still held") {
  const ScriptScope scope;
  World world = makeScriptedWorld(makeScriptedSchema());
  at(0, stockForRemoval);

  SECTION("removed by a command") {
    stepWith(world, {ENDPOINT});
    REQUIRE(world.findEntity(ENDPOINT) == entt::null);
    REQUIRE(holdingsOf<Meals>(world, ENDPOINT) == HELD_FOR_REMOVAL);
    REQUIRE(unitsHeld<Meals>(world) == 10);
    REQUIRE(unitsConsumed<Meals>(world) == 0);
    stepWorld(world);
  }
  SECTION("removed by a system, before the swap of the same cycle") {
    std::vector<Holdings> seen;
    at(1, [&seen](World &stepping) {
      stepping.destroyEntity(ENDPOINT);
      seen.push_back(holdingsOf<Meals>(stepping, ENDPOINT));
    });
    stepWorld(world);
    stepWorld(world);
    REQUIRE(seen == std::vector<Holdings>{HELD_FOR_REMOVAL});
  }

  REQUIRE(holdingsOf<Meals>(world, ENDPOINT).empty());
  REQUIRE(holdingsOf<Meals>(world, HANDLE) == Holdings{{HANDLE, 5}});
  REQUIRE(unitsHeld<Meals>(world) == 5);
  REQUIRE(unitsConsumed<Meals>(world, DISCARDED_CAUSE) == 5);
  REQUIRE(unitsConsumed<Meals>(world) == 5);
}

std::shared_ptr<const WorldSchema> tradeSchema() {
  static const auto schema =
      test::makeTradeSchema({test::tradeAll<Meals>, test::tradeAll<Supplies>});
  return schema;
}

template <FlowDefinition K> bool conserved(const World &world) {
  return unitsCreated<K>(world) ==
         unitsInTransit<K>(world) + unitsHeld<K>(world) + unitsConsumed<K>(world);
}

TEST_CASE("for each kind, units created equal units in transit plus held plus consumed after "
          "every tick of randomized runs") {
  const auto schema = tradeSchema();
  // Whether the runs reached every way units leave a stock, so the identity is not checked
  // vacuously.
  bool sawTransit = false;
  bool sawUsed = false;
  bool sawUndeliverable = false;
  bool sawDiscarded = false;

  for (const uint64_t seed : {1U, 2U, 3U}) {
    CAPTURE(seed);
    World world = test::makeTradeWorld(schema, seed, 4);
    for (int cycle = 0; cycle < 200; ++cycle) {
      test::stepWithChurn(world);
      CAPTURE(world.Tick);
      REQUIRE(conserved<Meals>(world));
      REQUIRE(conserved<Supplies>(world));
      sawTransit = sawTransit || unitsInTransit<Meals>(world) > 0;
    }
    sawUsed = sawUsed || unitsConsumed<Meals>(world, "used") > 0;
    sawUndeliverable = sawUndeliverable || unitsConsumed<Meals>(world, UNDELIVERABLE_CAUSE) > 0;
    sawDiscarded = sawDiscarded || unitsConsumed<Meals>(world, DISCARDED_CAUSE) > 0;
  }

  CHECK(sawTransit);
  CHECK(sawUsed);
  CHECK(sawUndeliverable);
  CHECK(sawDiscarded);
}

TEST_CASE("systems whose ledger operations act for different endpoints give the same world and "
          "the same hash after every tick whatever order they are registered in") {
  const WorldFunction evenMeals = test::tradeWhere<Meals, test::evenKey>;
  const WorldFunction oddMeals = test::tradeWhere<Meals, test::oddKey>;
  const WorldFunction supplies = test::tradeAll<Supplies>;
  World forward =
      test::makeTradeWorld(test::makeTradeSchema({evenMeals, oddMeals, supplies}), 5, 6);
  World backward =
      test::makeTradeWorld(test::makeTradeSchema({supplies, oddMeals, evenMeals}), 5, 6);

  bool sawTransit = false;
  for (int cycle = 0; cycle < 10; ++cycle) {
    CAPTURE(cycle);
    stepWorld(forward);
    stepWorld(backward);
    requireSameValue(forward, backward);
    sawTransit = sawTransit || unitsInTransit<Meals>(forward) > 0;
  }
  CHECK(sawTransit);
}

} // namespace
} // namespace tpj
