#include "support/synthetic_types.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::Cached;
using test::componentOf;
using test::Probe;
using test::SLOT_PURPOSE;

// What the recording systems, swaps, resolvers, and commands were called for, in call order, each
// with the tick it saw. Systems take only the world, so the tests' record has to live outside it.
std::vector<std::string> &journal() {
  static std::vector<std::string> entries;
  return entries;
}

void record(const std::string &event, const World &world) {
  journal().push_back(event + " @" + std::to_string(world.Tick));
}

// Two command types, so that interleaving them shows commands run in submission order rather than
// grouped by type.
struct Note {
  int Id = 0;
};

[[maybe_unused]] void applyCommand(World &world, const Note &note) {
  record("note " + std::to_string(note.Id), world);
}

struct Mark {
  int Id = 0;
};

[[maybe_unused]] void applyCommand(World &world, const Mark &mark) {
  record("mark " + std::to_string(mark.Id), world);
}

// Resolvers named against alphabetical order, so that only registration order explains the calls.
std::shared_ptr<const WorldSchema> makeRecordingSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addSystem([](World &world, TickBuffers & /*buffers*/) { record("system 1", world); });
  schema->addSystem([](World &world, TickBuffers & /*buffers*/) { record("system 2", world); });
  schema->addSwap([](World &world, TickBuffers & /*buffers*/) { record("swap 1", world); });
  schema->addSwap([](World &world, TickBuffers & /*buffers*/) { record("swap 2", world); });
  schema->addResolver("zones", [](World &world) { record("resolver zones", world); });
  schema->addResolver("access", [](World &world) { record("resolver access", world); }, {"zones"});
  schema->addCommand<Note>();
  schema->addCommand<Mark>();
  return schema;
}

// A small park whose cycle changes the world at every stage: intent that commands change, state a
// system steps and a swap publishes, and derived data a resolver computes from intent alone.
struct Plan {
  int64_t Size = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Plan &plan) {
  visitor.field("size", plan.Size);
}

struct AddPlan {
  int64_t Size = 0;
};

[[maybe_unused]] void applyCommand(World &world, const AddPlan &command) {
  const EntityKey key = world.createEntity();
  world.emplace<Plan>(key, Plan{.Size = command.Size});
  world.emplace<Probe>(key);
}

struct ResizePlan {
  EntityKey Key = NULL_KEY;
  int64_t Size = 0;
};

[[maybe_unused]] void applyCommand(World &world, const ResizePlan &command) {
  componentOf<Plan>(world, command.Key).Size = command.Size;
}

void countUp(World &world, TickBuffers & /*buffers*/) {
  for (const EntityKey key : world.keysWith<Probe>()) {
    world.get<Probe>(key).Count += 1;
  }
}

void publishCounts(World &world, TickBuffers & /*buffers*/) {
  for (const EntityKey key : world.keysWith<Probe>()) {
    Probe &probe = world.get<Probe>(key);
    probe.Small = static_cast<uint16_t>(probe.Count);
  }
}

// Each plan derives an entity holding twice its size.
void deriveTotals(World &world) {
  std::vector<std::pair<EntityKey, int64_t>> plans;
  for (const EntityKey key : world.keysWith<Plan>()) {
    plans.emplace_back(key, world.get<Plan>(key).Size);
  }
  for (const auto &[owner, size] : plans) {
    const EntityKey key = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
    world.emplaceOrReplace<Cached>(key, Cached{.Total = 2 * size});
  }
}

std::shared_ptr<const WorldSchema> makeParkSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Probe>("probe", DataKind::State);
  schema->addComponent<Plan>("plan", DataKind::Intent);
  schema->addComponent<Cached>("cached", DataKind::Derived);
  schema->addSystem(countUp);
  schema->addSwap(publishCounts);
  schema->addResolver("totals", deriveTotals);
  schema->addCommand<AddPlan>();
  schema->addCommand<ResizePlan>();
  return schema;
}

constexpr EntityKey FIRST{1};

// Counter key 1 holds a probe and a plan. Not yet resolved.
World buildPark(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 77);
  const EntityKey key = world.createEntity();
  world.emplace<Probe>(key);
  world.emplace<Plan>(key, Plan{.Size = 3});
  return world;
}

// A command no schema registers.
struct Straggler {
  int Id = 0;
};

TEST_CASE("a cycle resolves if pending, steps the systems, advances the tick, runs the swaps, "
          "applies the commands in submission order, and resolves again") {
  World world(makeRecordingSchema(), 0);
  // Not zero, so that a tick set from anything but an increment would show.
  world.Tick = 7;
  CommandQueue queue;
  queue.push(Note{.Id = 1});
  queue.push(Mark{.Id = 2});
  queue.push(Note{.Id = 3});

  journal().clear();
  stepWorld(world, queue);

  REQUIRE(journal() == std::vector<std::string>{
                           "resolver zones @7",
                           "resolver access @7",
                           "system 1 @7",
                           "system 2 @7",
                           "swap 1 @8",
                           "swap 2 @8",
                           "note 1 @8",
                           "mark 2 @8",
                           "note 3 @8",
                           "resolver zones @8",
                           "resolver access @8",
                       });
  REQUIRE(world.Tick == 8);
  REQUIRE(queue.empty());
  REQUIRE_FALSE(world.isResolvePending());
}

TEST_CASE("a cycle that applies no command to a resolved world calls no resolver") {
  World world(makeRecordingSchema(), 0);
  resolveWorld(world);

  journal().clear();
  stepWorld(world);

  REQUIRE(journal() ==
          std::vector<std::string>{"system 1 @0", "system 2 @0", "swap 1 @1", "swap 2 @1"});
  REQUIRE_FALSE(world.isResolvePending());
}

TEST_CASE("the walk covers whether resolution is pending") {
  // The recording resolvers change nothing in the world, so resolving changes only the pending
  // resolution.
  const auto schema = makeRecordingSchema();
  const World pending(schema, 5);
  World resolved(schema, 5);
  resolveWorld(resolved);

  REQUIRE_FALSE(worldsEqual(pending, resolved));
  REQUIRE_FALSE(worldsEqual(resolved, pending));
  REQUIRE(hashWorld(pending) != hashWorld(resolved));

  REQUIRE(copyWorld(pending).isResolvePending());
  REQUIRE_FALSE(copyWorld(resolved).isResolvePending());
}

TEST_CASE("a queue holding a command of an unregistered type is refused before anything "
          "changes") {
  // A pending world, so that even the cycle's first resolution would change it.
  World world = buildPark(makeParkSchema());
  const World before = copyWorld(world);

  // A registered command that would change the world comes first.
  CommandQueue queue;
  queue.push(ResizePlan{.Key = FIRST, .Size = 9});
  queue.push(Straggler{.Id = 1});

  REQUIRE_THROWS_AS(stepWorld(world, queue), std::invalid_argument);
  REQUIRE(worldsEqual(world, before));
  REQUIRE(queue.commands().size() == 2);
}

TEST_CASE("a candidate equals the world that commits the same commands, applied in submission "
          "order") {
  const auto schema = makeParkSchema();
  World committed = buildPark(schema);
  World world = buildPark(schema);
  stepWorld(world);

  // Two sizes for one plan, so that only the order they apply in decides which one it keeps.
  CommandQueue queue;
  queue.push(ResizePlan{.Key = FIRST, .Size = 5});
  queue.push(ResizePlan{.Key = FIRST, .Size = 9});

  World candidate = makeCandidate(world, queue);
  stepWorld(committed, queue);

  REQUIRE(worldsEqual(candidate, committed));
  REQUIRE(componentOf<Plan>(candidate, FIRST).Size == 9);
}

TEST_CASE("in debug builds, createEntity during resolution throws WorldInvariantError") {
  if (!WORLD_CHECKS) {
    SKIP("world checks run only in debug builds");
  }
  auto schema = std::make_shared<WorldSchema>();
  schema->addResolver("counter", [](World &world) { world.createEntity(); });
  World world(schema, 0);

  REQUIRE_THROWS_AS(resolveWorld(world), WorldInvariantError);
}

// The stage flags a world shows, named, or none.
std::string flagsOf(const World &world) {
  std::string flags;
  if (world.isStepping()) {
    flags += " stepping";
  }
  if (world.isResolving()) {
    flags += " resolving";
  }
  if (world.isFinishing()) {
    flags += " finishing";
  }
  return flags.empty() ? "none" : flags.substr(1);
}

// Every set of flags each stage saw, so that a stage that runs twice must see the same flags both
// times.
std::map<std::string, std::set<std::string>> &flagsSeen() {
  static std::map<std::string, std::set<std::string>> seen;
  return seen;
}

void recordFlags(const std::string &stage, const World &world) {
  flagsSeen()[stage].insert(flagsOf(world));
}

// A command that records the flags the world shows when it is applied.
struct Look {
  int Id = 0;
};

[[maybe_unused]] void applyCommand(World &world, const Look & /*look*/) {
  recordFlags("command", world);
}

TEST_CASE("isStepping, isResolving, and isFinishing are each true exactly while their stage runs") {
  auto schema = std::make_shared<WorldSchema>();
  schema->addSystem([](World &world, TickBuffers & /*buffers*/) { recordFlags("system", world); });
  schema->addSwap([](World &world, TickBuffers & /*buffers*/) { recordFlags("swap", world); });
  schema->addResolver("watch", [](World &world) { recordFlags("resolver", world); });
  schema->addFinisher([](World &world) { recordFlags("finisher", world); });
  schema->addCommand<Look>();
  World world(schema, 0);
  REQUIRE(flagsOf(world) == "none");

  // The command makes the cycle resolve twice, before stepping and after the command.
  CommandQueue queue;
  queue.push(Look{});
  flagsSeen().clear();
  stepWorld(world, queue);

  REQUIRE(flagsSeen() == std::map<std::string, std::set<std::string>>{
                             {"system", {"stepping"}},
                             {"swap", {"none"}},
                             {"command", {"none"}},
                             {"resolver", {"resolving"}},
                             {"finisher", {"finishing"}},
                         });
  REQUIRE(flagsOf(world) == "none");
}

// Finishers are registered between resolvers, so that only running them after every resolver
// explains the calls, and resolvers against alphabetical order, so that only registration order
// explains theirs.
std::shared_ptr<const WorldSchema> makeFinishingSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addFinisher([](World &world) { record("finisher 1", world); });
  schema->addResolver("zones", [](World &world) { record("resolver zones", world); });
  schema->addFinisher([](World &world) { record("finisher 2", world); });
  schema->addResolver("access", [](World &world) { record("resolver access", world); }, {"zones"});
  return schema;
}

TEST_CASE("a resolution runs every resolver, then every finisher, once each in registration "
          "order") {
  World world(makeFinishingSchema(), 0);

  journal().clear();
  resolveWorld(world);

  REQUIRE(journal() == std::vector<std::string>{"resolver zones @0", "resolver access @0",
                                                "finisher 1 @0", "finisher 2 @0"});
}

} // namespace
} // namespace tpj
