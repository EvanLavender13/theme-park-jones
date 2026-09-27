#include "support/synthetic_types.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::ContainsSubstring;
using test::Cached;
using test::componentOf;
using test::Probe;
using test::SLOT_PURPOSE;

void requireSameValue(const World &one, const World &other) {
  REQUIRE(worldsEqual(one, other));
  REQUIRE(worldsEqual(other, one));
  REQUIRE(hashWorld(one) == hashWorld(other));
}

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
  schema->addSystem([](World &world) { record("system 1", world); });
  schema->addSystem([](World &world) { record("system 2", world); });
  schema->addSwap([](World &world) { record("swap 1", world); });
  schema->addSwap([](World &world) { record("swap 2", world); });
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
  world.Registry.emplace<Plan>(world.findEntity(key), Plan{.Size = command.Size});
  world.Registry.emplace<Probe>(world.findEntity(key));
}

struct ResizePlan {
  EntityKey Key = NULL_KEY;
  int64_t Size = 0;
};

[[maybe_unused]] void applyCommand(World &world, const ResizePlan &command) {
  componentOf<Plan>(world, command.Key).Size = command.Size;
}

void countUp(World &world) {
  world.Registry.view<Probe>().each([](Probe &probe) { probe.Count += 1; });
}

void publishCounts(World &world) {
  world.Registry.view<Probe>().each(
      [](Probe &probe) { probe.Small = static_cast<uint16_t>(probe.Count); });
}

// Each plan derives an entity holding twice its size.
void deriveTotals(World &world) {
  std::vector<std::pair<EntityKey, int64_t>> plans;
  world.Registry.view<Plan>().each([&](entt::entity entity, const Plan &plan) {
    plans.emplace_back(world.keyOf(entity), plan.Size);
  });
  for (const auto &[owner, size] : plans) {
    const EntityKey key = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
    world.Registry.emplace_or_replace<Cached>(world.findEntity(key), Cached{.Total = 2 * size});
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
  world.Registry.emplace<Probe>(world.findEntity(key));
  world.Registry.emplace<Plan>(world.findEntity(key), Plan{.Size = 3});
  return world;
}

// A command no schema registers.
struct Straggler {
  int Id = 0;
};

// The message of the std::invalid_argument the call throws, or nothing if it throws none.
template <typename Call> std::optional<std::string> invalidArgument(Call call) {
  try {
    call();
  } catch (const std::invalid_argument &error) {
    return std::string(error.what());
  }
  return std::nullopt;
}

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

TEST_CASE("stepWorld without a queue runs the same cycle as with an empty queue") {
  const auto schema = makeRecordingSchema();
  World withQueue(schema, 0);
  World withoutQueue(schema, 0);

  journal().clear();
  CommandQueue empty;
  stepWorld(withQueue, empty);
  const std::vector<std::string> queued = journal();

  journal().clear();
  stepWorld(withoutQueue);

  REQUIRE(journal() == queued);
  requireSameValue(withoutQueue, withQueue);
}

TEST_CASE("a new world is pending until resolveWorld calls every resolver once, in registration "
          "order") {
  World world(makeRecordingSchema(), 0);
  REQUIRE(world.isResolvePending());

  journal().clear();
  resolveWorld(world);

  REQUIRE(journal() == std::vector<std::string>{"resolver zones @0", "resolver access @0"});
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

TEST_CASE("a queue holding an unregistered command type is refused, naming the type, with no "
          "change to the world or the queue") {
  // A pending world, so that even the cycle's first resolution would change it.
  World world = buildPark(makeParkSchema());
  const World before = copyWorld(world);
  const uint64_t hashBefore = hashWorld(world);

  // A registered command that would change the world comes first.
  CommandQueue queue;
  queue.push(ResizePlan{.Key = FIRST, .Size = 9});
  queue.push(Straggler{.Id = 1});

  std::optional<std::string> message;
  SECTION("stepWorld") {
    message = invalidArgument([&] { stepWorld(world, queue); });
  }
  SECTION("makeCandidate") {
    message = invalidArgument([&] { static_cast<void>(makeCandidate(world, queue)); });
  }

  REQUIRE(message.has_value());
  CHECK_THAT(message.value_or(""), ContainsSubstring("Straggler"));
  requireSameValue(world, before);
  REQUIRE(hashWorld(world) == hashBefore);
  REQUIRE(queue.commands().size() == 2);
  REQUIRE(queue.commands()[0].TypeId == entt::type_id<ResizePlan>().hash());
  REQUIRE(queue.commands()[1].TypeId == entt::type_id<Straggler>().hash());
}

// A preview is exact only if the candidate is the world the same commands would give when queued.
TEST_CASE("a candidate made just after a cycle equals the world that queuing its commands for "
          "that cycle gives, and leaves its source unchanged") {
  World resolved = buildPark(makeParkSchema());
  resolveWorld(resolved);
  World first = copyWorld(resolved);
  World second = copyWorld(resolved);

  // One command creates an entity from the key counter, and one changes existing intent.
  auto fill = [](CommandQueue &queue) {
    queue.push(AddPlan{.Size = 5});
    queue.push(ResizePlan{.Key = FIRST, .Size = 8});
  };

  stepWorld(first);
  const World firstAfterCycle = copyWorld(first);
  const uint64_t firstHash = hashWorld(first);
  CommandQueue tentative;
  fill(tentative);
  const World candidate = makeCandidate(first, tentative);

  CommandQueue queued;
  fill(queued);
  stepWorld(second, queued);

  requireSameValue(candidate, second);
  REQUIRE(hashWorld(first) == firstHash);
  requireSameValue(first, firstAfterCycle);
  // The commands did change something, so the equality above is not vacuous.
  REQUIRE_FALSE(worldsEqual(candidate, first));
}

TEST_CASE("makeCandidate applies the commands in submission order and resolves, without "
          "stepping") {
  World world(makeRecordingSchema(), 0);
  world.Tick = 4;
  resolveWorld(world);
  CommandQueue queue;
  queue.push(Note{.Id = 1});
  queue.push(Mark{.Id = 2});
  queue.push(Note{.Id = 3});

  journal().clear();
  const World candidate = makeCandidate(world, queue);

  REQUIRE(journal() == std::vector<std::string>{
                           "note 1 @4",
                           "mark 2 @4",
                           "note 3 @4",
                           "resolver zones @4",
                           "resolver access @4",
                       });
  REQUIRE(candidate.Tick == 4);
  REQUIRE_FALSE(candidate.isResolvePending());
}

TEST_CASE("makeCandidate resolves its copy only when resolution is pending") {
  const CommandQueue none;

  SECTION("a resolved source gives an equal copy and calls no resolver") {
    World world(makeRecordingSchema(), 0);
    resolveWorld(world);
    journal().clear();
    const World candidate = makeCandidate(world, none);
    REQUIRE(journal().empty());
    requireSameValue(candidate, world);
  }
  SECTION("a pending source gives a resolved copy and stays pending") {
    const World world(makeRecordingSchema(), 0);
    journal().clear();
    const World candidate = makeCandidate(world, none);
    REQUIRE(journal() == std::vector<std::string>{"resolver zones @0", "resolver access @0"});
    REQUIRE_FALSE(candidate.isResolvePending());
    REQUIRE(world.isResolvePending());
  }
}

TEST_CASE("worlds built by the same calls and cycled with the same commands at the same ticks "
          "stay equal after every cycle") {
  const auto schema = makeParkSchema();
  World left = buildPark(schema);
  World right = buildPark(schema);

  for (int cycle = 0; cycle < 6; ++cycle) {
    for (World *world : {&left, &right}) {
      CommandQueue queue;
      if (cycle == 2) {
        queue.push(AddPlan{.Size = 4});
      }
      if (cycle == 4) {
        queue.push(ResizePlan{.Key = FIRST, .Size = 6});
      }
      stepWorld(*world, queue);
    }
    CAPTURE(cycle);
    requireSameValue(left, right);
  }
}

TEST_CASE("a resolved world with nothing registered cycles by advancing only its tick") {
  World world(std::make_shared<WorldSchema>(), 3);
  world.createEntity();
  world.createEntity();
  resolveWorld(world);
  World expected = copyWorld(world);
  expected.Tick += 1;

  stepWorld(world);

  requireSameValue(world, expected);
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

// Derived keys depend only on their arguments, so resolving again finds the same entities rather
// than adding new ones.
TEST_CASE("createDerivedEntity works during resolution, and resolving again recreates the same "
          "keys") {
  auto schema = std::make_shared<WorldSchema>();
  schema->addResolver("slots", [](World &world) {
    world.createDerivedEntity(FIRST, SLOT_PURPOSE, 0);
    world.createDerivedEntity(FIRST, SLOT_PURPOSE, 1);
  });
  World world(schema, 0);
  world.createEntity();

  REQUIRE_NOTHROW(resolveWorld(world));
  std::vector<EntityKey> expected = {FIRST, deriveKey(FIRST, SLOT_PURPOSE, 0),
                                     deriveKey(FIRST, SLOT_PURPOSE, 1)};
  std::ranges::sort(expected);
  REQUIRE(world.keys() == expected);

  resolveWorld(world);
  REQUIRE(world.keys() == expected);
}

// A command that records whether the world is stepping when it is applied.
struct Look {
  int Id = 0;
};

void recordStepping(const std::string &stage, const World &world) {
  journal().push_back(stage + (world.isStepping() ? " stepping" : " not stepping"));
}

[[maybe_unused]] void applyCommand(World &world, const Look & /*look*/) {
  recordStepping("command", world);
}

TEST_CASE("isStepping is true while a system runs and false otherwise") {
  auto schema = std::make_shared<WorldSchema>();
  schema->addSystem([](World &world) { recordStepping("system 1", world); });
  schema->addSystem([](World &world) { recordStepping("system 2", world); });
  schema->addSwap([](World &world) { recordStepping("swap", world); });
  schema->addResolver("watch", [](World &world) { recordStepping("resolver", world); });
  schema->addFinisher([](World &world) { recordStepping("finisher", world); });
  schema->addCommand<Look>();
  World world(schema, 0);
  REQUIRE_FALSE(world.isStepping());

  journal().clear();
  CommandQueue queue;
  queue.push(Look{});
  stepWorld(world, queue);

  REQUIRE(journal() == std::vector<std::string>{
                           "resolver not stepping",
                           "finisher not stepping",
                           "system 1 stepping",
                           "system 2 stepping",
                           "swap not stepping",
                           "command not stepping",
                           "resolver not stepping",
                           "finisher not stepping",
                       });
  REQUIRE_FALSE(world.isStepping());
}

// Finishers are registered between resolvers, so that only running them after every resolver
// explains the calls.
std::shared_ptr<const WorldSchema> makeFinishingSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addFinisher([](World &world) { record("finisher 1", world); });
  schema->addResolver("zones", [](World &world) { record("resolver zones", world); });
  schema->addFinisher([](World &world) { record("finisher 2", world); });
  schema->addResolver("access", [](World &world) { record("resolver access", world); }, {"zones"});
  schema->addSystem([](World &world) { record("system", world); });
  schema->addCommand<Note>();
  return schema;
}

TEST_CASE("every resolution runs each finisher once, after all of its resolvers, in registration "
          "order") {
  const auto schema = makeFinishingSchema();
  const std::vector<std::string> resolution = {"resolver zones @0", "resolver access @0",
                                               "finisher 1 @0", "finisher 2 @0"};

  SECTION("resolveWorld") {
    World world(schema, 0);
    journal().clear();
    resolveWorld(world);
    REQUIRE(journal() == resolution);
  }
  SECTION("a cycle's resolutions, before stepping and after a command") {
    World world(schema, 0);
    CommandQueue queue;
    queue.push(Note{.Id = 1});
    journal().clear();
    stepWorld(world, queue);
    REQUIRE(journal() == std::vector<std::string>{
                             "resolver zones @0",
                             "resolver access @0",
                             "finisher 1 @0",
                             "finisher 2 @0",
                             "system @0",
                             "note 1 @1",
                             "resolver zones @1",
                             "resolver access @1",
                             "finisher 1 @1",
                             "finisher 2 @1",
                         });
  }
  SECTION("a candidate's") {
    World world(schema, 0);
    resolveWorld(world);
    CommandQueue queue;
    queue.push(Note{.Id = 1});
    journal().clear();
    static_cast<void>(makeCandidate(world, queue));
    REQUIRE(journal() == std::vector<std::string>{"note 1 @0", "resolver zones @0",
                                                  "resolver access @0", "finisher 1 @0",
                                                  "finisher 2 @0"});
  }
  SECTION("a loaded world's") {
    World world(schema, 0);
    resolveWorld(world);
    World loaded = loadWorld(schema, saveWorld(world));
    journal().clear();
    resolveWorld(loaded);
    REQUIRE(journal() == resolution);
  }
}

} // namespace
} // namespace tpj
