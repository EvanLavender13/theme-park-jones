#include "support/synthetic_fields.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <memory>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::addSource;
using test::addStepper;
using test::addStepsComponents;
using test::CARRIER_A;
using test::CARRIER_B;
using test::Emits;
using test::entryAt;
using test::Footfall;
using test::Layout;
using test::LAYOUT_KEY;
using test::makeFieldWorld;
using test::makeSteppedFieldSchema;
using test::networkOf;
using test::PublishOrder;
using test::PublishOrderScope;
using test::publishSteppedSources;
using test::publishSteppedSourcesWhere;
using test::Reach;
using test::reachCalls;
using test::setSteps;
using test::standardLayout;
using test::Steps;

using Sampled = std::vector<std::pair<EntityKey, double>>;
using SampledBits = std::vector<std::pair<EntityKey, uint64_t>>;

constexpr Place place(EntityKey carrier, double distance) {
  return {.Carrier = carrier, .Distance = distance};
}

// Sampled on the world's own network.
template <typename F> Sampled sampled(const World &world, const Place &at) {
  Sampled result;
  for (const SampledEntry<double> &entry : sampleField<F>(world, networkOf(world), at)) {
    result.emplace_back(entry.Source, entry.Value);
  }
  return result;
}

template <typename F> SampledBits sampledBits(const World &world, const Place &at) {
  SampledBits result;
  for (const auto &[source, value] : sampled<F>(world, at)) {
    result.emplace_back(source, std::bit_cast<uint64_t>(value));
  }
  return result;
}

void requireSameValue(const World &one, const World &other) {
  REQUIRE(worldsEqual(one, other));
  REQUIRE(worldsEqual(other, one));
  REQUIRE(hashWorld(one) == hashWorld(other));
}

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

// What recording systems, swaps, and resolvers saw. They take only the world, so the record lives
// outside it.
std::vector<Thrown> &outcomes() {
  static std::vector<Thrown> recorded;
  return recorded;
}

std::vector<Sampled> &observed() {
  static std::vector<Sampled> recorded;
  return recorded;
}

const ComponentType *componentNamed(const WorldSchema &schema, std::string_view name) {
  const auto &components = schema.components();
  const auto found = std::ranges::find_if(
      components, [name](const ComponentType &type) { return type.Name == name; });
  return found == components.end() ? nullptr : &*found;
}

struct Occupant {
  int64_t Count = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Occupant &occupant) {
  visitor.field("count", occupant.Count);
}

// Commands that change the sources' resolved entries and the layout.
struct SetFootfall {
  EntityKey Source = NULL_KEY;
  std::vector<PlacedEntry<double>> Entries;
};

[[maybe_unused]] void applyCommand(World &world, const SetFootfall &command) {
  world.Registry.emplace_or_replace<Emits<Footfall>>(world.findEntity(command.Source),
                                                     Emits<Footfall>{.Entries = command.Entries});
}

struct DropFootfall {
  EntityKey Source = NULL_KEY;
};

[[maybe_unused]] void applyCommand(World &world, const DropFootfall &command) {
  world.Registry.remove<Emits<Footfall>>(world.findEntity(command.Source));
}

struct ReplaceLayout {
  Layout Replacement;
};

[[maybe_unused]] void applyCommand(World &world, const ReplaceLayout &command) {
  world.Registry.get<Layout>(world.findEntity(LAYOUT_KEY)) = command.Replacement;
}

std::shared_ptr<WorldSchema> makeCommandedSchema() {
  auto schema = makeSteppedFieldSchema();
  schema->addCommand<SetFootfall>();
  schema->addCommand<DropFootfall>();
  schema->addCommand<ReplaceLayout>();
  return schema;
}

constexpr Place JUNCTION = place(CARRIER_A, 4);

TEST_CASE("addField registers the field's state component as <name>-stepped") {
  const auto schema = makeSteppedFieldSchema();

  for (const std::string_view name : {"footfall", "reach"}) {
    CAPTURE(name);
    const ComponentType *stepped = componentNamed(*schema, std::string(name) + "-stepped");
    REQUIRE(stepped != nullptr);
    REQUIRE(stepped->Kind == DataKind::State);
  }
}

TEST_CASE("addField throws std::invalid_argument when <name>-stepped is already registered") {
  WorldSchema schema;
  schema.addComponent<Occupant>("footfall-stepped", DataKind::State);

  REQUIRE(thrownBy([&] { addField<Footfall>(schema); }) == Thrown::InvalidArgument);
}

// Registered after the stepping sources' systems, so it runs in the same tick, after they publish.
void observeJunction(World &world) { observed().push_back(sampled<Footfall>(world, JUNCTION)); }

TEST_CASE("entries published while stepping a tick are sampled from the swap that ends it until "
          "the swap that ends the next, which replaces them with that tick's, if any") {
  auto schema = makeSteppedFieldSchema();
  schema->addSystem(observeJunction);
  World world = makeFieldWorld(schema);
  const EntityKey source = addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
  resolveWorld(world);
  REQUIRE(sampled<Footfall>(world, JUNCTION).empty());

  observed().clear();
  // Tick 0 publishes 1.0, and tick 1 publishes 2.0.
  stepWorld(world);
  REQUIRE(observed().back().empty());
  REQUIRE(sampled<Footfall>(world, JUNCTION) == Sampled{{source, 1.0}});

  stepWorld(world);
  REQUIRE(observed().back() == Sampled{{source, 1.0}});
  REQUIRE(sampled<Footfall>(world, JUNCTION) == Sampled{{source, 2.0}});

  // The source stops publishing, so the swap that ends tick 2 leaves it nothing.
  world.Registry.remove<Steps<Footfall>>(world.findEntity(source));
  stepWorld(world);
  REQUIRE(observed().back() == Sampled{{source, 2.0}});
  REQUIRE(sampled<Footfall>(world, JUNCTION).empty());
}

void publishOutsideStepping(World &world) {
  outcomes().push_back(
      thrownBy([&] { publishStepped<Footfall>(world, LAYOUT_KEY, {entryAt(CARRIER_A, 4, 1.0)}); }));
}

TEST_CASE("publishStepped throws std::logic_error when the world is not stepping") {
  auto schema = std::make_shared<WorldSchema>();
  addField<Footfall>(*schema);

  SECTION("outside any cycle") {
    World world(schema, 0);
    world.createEntity();
    resolveWorld(world);
    outcomes().clear();
    publishOutsideStepping(world);
  }
  SECTION("in a resolver") {
    schema->addResolver("resolving-publisher", publishOutsideStepping, {"footfall-field"});
    World world(schema, 0);
    world.createEntity();
    outcomes().clear();
    resolveWorld(world);
  }
  SECTION("in a swap") {
    schema->addSwap(publishOutsideStepping);
    World world(schema, 0);
    world.createEntity();
    resolveWorld(world);
    outcomes().clear();
    stepWorld(world);
  }

  REQUIRE(outcomes() == std::vector<Thrown>{Thrown::OtherLogicError});
}

TEST_CASE("publishStepped throws std::logic_error when the world holds no stepped entries for the "
          "field") {
  // The field is not registered, so no resolver ever creates its stepped entries.
  auto schema = std::make_shared<WorldSchema>();
  schema->addSystem([](World &world) {
    outcomes().push_back(thrownBy(
        [&] { publishStepped<Footfall>(world, LAYOUT_KEY, {entryAt(CARRIER_A, 4, 1.0)}); }));
  });
  World world(schema, 0);
  world.createEntity();

  outcomes().clear();
  stepWorld(world);

  REQUIRE(outcomes() == std::vector<Thrown>{Thrown::OtherLogicError});
}

TEST_CASE("publishStepped throws std::invalid_argument for the null source key") {
  auto schema = std::make_shared<WorldSchema>();
  addField<Footfall>(*schema);
  schema->addSystem([](World &world) {
    outcomes().push_back(
        thrownBy([&] { publishStepped<Footfall>(world, NULL_KEY, {entryAt(CARRIER_A, 4, 1.0)}); }));
  });
  World world(schema, 0);

  outcomes().clear();
  stepWorld(world);

  REQUIRE(outcomes() == std::vector<Thrown>{Thrown::InvalidArgument});
}

// Publishes into footfall for the layout's key twice, the first time with the given entries.
template <size_t FirstCount> void publishSteppedTwice(World &world) {
  std::vector<PlacedEntry<double>> first;
  if constexpr (FirstCount > 0) {
    first.push_back(entryAt(CARRIER_A, 4, 1.0));
  }
  outcomes().push_back(
      thrownBy([&] { publishStepped<Footfall>(world, LAYOUT_KEY, std::move(first)); }));
  outcomes().push_back(
      thrownBy([&] { publishStepped<Footfall>(world, LAYOUT_KEY, {entryAt(CARRIER_A, 4, 2.0)}); }));
}

// The refusal is per tick: the next tick accepts the source's first publication again.
TEST_CASE("publishStepped throws std::invalid_argument for a source that has already published "
          "into the field in the same tick, even with no entries") {
  auto schema = std::make_shared<WorldSchema>();
  addField<Footfall>(*schema);
  SECTION("a first publication with entries") { schema->addSystem(publishSteppedTwice<1>); }
  SECTION("a first publication with no entries") { schema->addSystem(publishSteppedTwice<0>); }
  World world(schema, 0);
  world.createEntity();

  outcomes().clear();
  stepWorld(world);
  stepWorld(world);

  REQUIRE(outcomes() == std::vector<Thrown>{Thrown::Nothing, Thrown::InvalidArgument,
                                            Thrown::Nothing, Thrown::InvalidArgument});
}

TEST_CASE("a publishStepped that throws leaves the field's entries unchanged") {
  SECTION("a repeated source keeps its first entries, and a null source adds none") {
    auto schema = std::make_shared<WorldSchema>();
    test::addSyntheticNetwork(*schema);
    addField<Footfall>(*schema);
    schema->addSystem([](World &world) {
      publishSteppedTwice<1>(world);
      outcomes().push_back(thrownBy(
          [&] { publishStepped<Footfall>(world, NULL_KEY, {entryAt(CARRIER_A, 4, 3.0)}); }));
    });
    World world = makeFieldWorld(schema);

    outcomes().clear();
    stepWorld(world);

    REQUIRE(outcomes() ==
            std::vector<Thrown>{Thrown::Nothing, Thrown::InvalidArgument, Thrown::InvalidArgument});
    REQUIRE(sampled<Footfall>(world, JUNCTION) == Sampled{{LAYOUT_KEY, 1.0}});
  }
  SECTION("outside stepping, the world is unchanged") {
    World world = makeFieldWorld(makeSteppedFieldSchema());
    const EntityKey source = addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
    stepWorld(world);
    const World before = copyWorld(world);

    REQUIRE(thrownBy([&] {
              publishStepped<Footfall>(world, source, {entryAt(CARRIER_A, 4, 2.0)});
            }) == Thrown::OtherLogicError);
    requireSameValue(world, before);
  }
}

TEST_CASE("a source with readable stepped entries, even an empty list, is sampled by them, and "
          "otherwise by its resolved entries") {
  World world = makeFieldWorld(makeSteppedFieldSchema());
  const EntityKey both = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
  setSteps<Footfall>(world, both, {entryAt(CARRIER_A, 4, 10.0)});
  const EntityKey resolvedOnly = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 2.0)});
  const EntityKey emptyStepped = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 3.0)});
  setSteps<Footfall>(world, emptyStepped, {});
  // Its stepped entries are elsewhere, so it has none at the junction, in either layer.
  const EntityKey steppedElsewhere = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 4.0)});
  setSteps<Footfall>(world, steppedElsewhere, {entryAt(CARRIER_A, 2, 40.0)});
  resolveWorld(world);
  REQUIRE(sampled<Footfall>(world, JUNCTION) ==
          Sampled{{both, 1.0}, {resolvedOnly, 2.0}, {emptyStepped, 3.0}, {steppedElsewhere, 4.0}});

  stepWorld(world);

  REQUIRE(sampled<Footfall>(world, JUNCTION) == Sampled{{both, 10.0}, {resolvedOnly, 2.0}});
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 2)) == Sampled{{steppedElsewhere, 40.0}});
}

TEST_CASE("sources are sampled in ascending key order across both layers, each source's stepped "
          "entries in the order it gave them, whatever order they were published in") {
  for (const PublishOrder order :
       {PublishOrder::Ascending, PublishOrder::Descending, PublishOrder::Rotated}) {
    CAPTURE(order);
    const PublishOrderScope scope(order);
    World world = makeFieldWorld(makeSteppedFieldSchema());
    // Keys 2 to 5 alternate between the layers.
    addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 21.0), entryAt(CARRIER_B, 0, 20.0)});
    addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 30.0)});
    addStepper<Footfall>(world, {entryAt(CARRIER_B, 0, 41.0), entryAt(CARRIER_A, 4, 40.0)});
    addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 50.0)});
    resolveWorld(world);
    stepWorld(world);

    REQUIRE(sampled<Footfall>(world, JUNCTION) == Sampled{{EntityKey{2}, 21.0},
                                                          {EntityKey{2}, 20.0},
                                                          {EntityKey{3}, 30.0},
                                                          {EntityKey{4}, 41.0},
                                                          {EntityKey{4}, 40.0},
                                                          {EntityKey{5}, 50.0}});
  }
}

TEST_CASE("a stepped entry whose place does not resolve on the network is not sampled") {
  World world = makeFieldWorld(makeSteppedFieldSchema());
  // Just beyond each end of A, and on a carrier the network lacks.
  addStepper<Footfall>(world, {entryAt(CARRIER_A, -1, 1.0), entryAt(CARRIER_A, 11, 2.0),
                               entryAt(EntityKey{99}, 0, 3.0)});
  addStepper<Reach>(world, {entryAt(CARRIER_A, -1, 4.0), entryAt(EntityKey{99}, 0, 5.0),
                            entryAt(CARRIER_A, 3, 7.0)});
  resolveWorld(world);
  stepWorld(world);

  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 0)).empty());
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 10)).empty());

  reachCalls().clear();
  static_cast<void>(sampleField<Reach>(world, networkOf(world), place(CARRIER_A, 2)));
  REQUIRE(reachCalls().size() == 1);
  const EdgeSample<double> &sample = reachCalls().front().Sample;
  REQUIRE(sample.AtFrom.empty());
  REQUIRE(sample.AtTo.empty());
  REQUIRE(sample.Along.size() == 1);
  REQUIRE(sample.Along.front().Value == 7.0);
}

TEST_CASE("a resolution after a command clears the readable stepped entries of exactly the "
          "sources whose resolved entries it changed") {
  World world = makeFieldWorld(makeCommandedSchema());
  const EntityKey kept = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
  setSteps<Footfall>(world, kept, {entryAt(CARRIER_A, 4, 10.0)});
  // Its reach entries are stepped only, and its resolved reach entries do not change.
  const EntityKey changed = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 2.0)});
  setSteps<Footfall>(world, changed, {entryAt(CARRIER_A, 4, 20.0)});
  setSteps<Reach>(world, changed, {entryAt(CARRIER_A, 4, 25.0)});
  const EntityKey removed = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 4.0)});
  setSteps<Footfall>(world, removed, {entryAt(CARRIER_A, 4, 40.0)});
  const EntityKey first = addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 50.0)});
  const EntityKey steppedOnly = addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 60.0)});
  // A command rewrites its intent with the same entries.
  const EntityKey rewritten = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 7.0)});
  setSteps<Footfall>(world, rewritten, {entryAt(CARRIER_A, 4, 70.0)});
  resolveWorld(world);
  stepWorld(world);

  // The cycle's tick 1 makes each stepped value readable raised by 1 before the commands apply.
  CommandQueue queue;
  queue.push(SetFootfall{.Source = changed, .Entries = {entryAt(CARRIER_A, 4, 3.0)}});
  queue.push(DropFootfall{.Source = removed});
  queue.push(SetFootfall{.Source = first, .Entries = {entryAt(CARRIER_A, 4, 5.0)}});
  queue.push(SetFootfall{.Source = rewritten, .Entries = {entryAt(CARRIER_A, 4, 7.0)}});
  stepWorld(world, queue);

  REQUIRE(
      sampled<Footfall>(world, JUNCTION) ==
      Sampled{{kept, 11.0}, {changed, 3.0}, {first, 5.0}, {steppedOnly, 61.0}, {rewritten, 71.0}});
  REQUIRE(sampled<Reach>(world, JUNCTION) == Sampled{{changed, 26.0}});
}

TEST_CASE("resolved entries count as changed exactly when the walk emits different words for "
          "them") {
  World world = makeFieldWorld(makeCommandedSchema());
  // 0.0 and -0.0 compare equal as numbers but differ in their bits.
  const EntityKey signFlipped = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 0.0)});
  setSteps<Footfall>(world, signFlipped, {entryAt(CARRIER_A, 4, 10.0)});
  // A source that publishes an empty list and one that does not publish both have an empty list.
  const EntityKey emptied = addSource<Footfall>(world, {});
  setSteps<Footfall>(world, emptied, {entryAt(CARRIER_A, 4, 20.0)});
  resolveWorld(world);
  stepWorld(world);

  CommandQueue queue;
  queue.push(SetFootfall{.Source = signFlipped, .Entries = {entryAt(CARRIER_A, 4, -0.0)}});
  queue.push(DropFootfall{.Source = emptied});
  stepWorld(world, queue);

  REQUIRE(sampledBits<Footfall>(world, JUNCTION) ==
          SampledBits{{signFlipped, std::bit_cast<uint64_t>(-0.0)},
                      {emptied, std::bit_cast<uint64_t>(21.0)}});
}

// Two sources with both layers in both fields, stepped twice so that the readable stepped entries
// are not the first tick's.
World bothLayersInFlight(std::shared_ptr<const WorldSchema> schema) {
  World world = makeFieldWorld(std::move(schema));
  const EntityKey walker =
      addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0), entryAt(CARRIER_A, 2, 2.0)});
  setSteps<Footfall>(world, walker, {entryAt(CARRIER_A, 4, 10.0)});
  const EntityKey reacher = addSource<Reach>(world, {entryAt(CARRIER_A, 7, 3.0)});
  setSteps<Reach>(world, reacher, {entryAt(CARRIER_A, 4, 30.0), entryAt(CARRIER_B, 3, 31.0)});
  addSource<Footfall>(world, {entryAt(CARRIER_B, 0, 4.0)});
  resolveWorld(world);
  stepWorld(world);
  stepWorld(world);
  return world;
}

TEST_CASE("a save holds stepped entries, and loading the save of a resolved world with both layers "
          "in flight and resolving gives the world saved") {
  const auto schema = makeSteppedFieldSchema();
  const World world = bothLayersInFlight(schema);
  REQUIRE(sampled<Footfall>(world, JUNCTION) == Sampled{{EntityKey{2}, 11.0}, {EntityKey{4}, 4.0}});

  const std::string text = saveWorld(world);
  REQUIRE(text.find("[footfall-stepped]") != std::string::npos);
  REQUIRE(text.find("[reach-stepped]") != std::string::npos);

  World loaded = loadWorld(schema, text);
  resolveWorld(loaded);
  requireSameValue(loaded, world);
}

TEST_CASE("changing any stepped entry changes the world's hash") {
  const World world = bothLayersInFlight(makeSteppedFieldSchema());
  World changed = copyWorld(world);
  const entt::entity holder = changed.findEntity(fieldKey("footfall"));
  REQUIRE(holder != entt::null);
  auto *held = changed.Registry.try_get<SteppedEntries<Footfall>>(holder);
  REQUIRE(held != nullptr);
  SteppedEntries<Footfall> &stepped = *held;
  REQUIRE_FALSE(stepped.Readable.empty());
  REQUIRE_FALSE(stepped.Readable.front().Entries.empty());

  SECTION("a readable entry's value") { stepped.Readable.front().Entries.front().Value += 1.0; }
  SECTION("a readable entry's place") {
    stepped.Readable.front().Entries.front().At.Distance += 1.0;
  }
  SECTION("an entry published but not yet readable") {
    stepped.Pending.push_back({.Source = EntityKey{2}, .Entries = {entryAt(CARRIER_A, 4, 1.0)}});
  }

  REQUIRE_FALSE(worldsEqual(changed, world));
  REQUIRE(hashWorld(changed) != hashWorld(world));
}

TEST_CASE("a copy stepped forward equals the original stepped forward") {
  World original = bothLayersInFlight(makeSteppedFieldSchema());
  World copy = copyWorld(original);

  for (int tick = 0; tick < 3; ++tick) {
    CAPTURE(tick);
    stepWorld(original);
    stepWorld(copy);
    requireSameValue(copy, original);
  }
}

bool evenKey(EntityKey key) { return static_cast<uint64_t>(key) % 2 == 0; }
bool oddKey(EntityKey key) { return static_cast<uint64_t>(key) % 2 == 1; }
void publishEvenFootfall(World &world) { publishSteppedSourcesWhere<Footfall>(world, evenKey); }
void publishOddFootfall(World &world) { publishSteppedSourcesWhere<Footfall>(world, oddKey); }

// Three systems: two publish alternate sources into footfall, and one publishes into reach.
std::shared_ptr<const WorldSchema> makeSystemsSchema(const std::vector<WorldFunction> &systems) {
  auto schema = test::makeFieldSchema();
  addStepsComponents(*schema);
  for (const WorldFunction system : systems) {
    schema->addSystem(system);
  }
  return schema;
}

World publishingWorld(std::shared_ptr<const WorldSchema> schema) {
  World world = makeFieldWorld(std::move(schema));
  addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
  addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 2.0), entryAt(CARRIER_B, 3, 3.0)});
  addStepper<Footfall>(world, {entryAt(CARRIER_A, 4, 4.0)});
  addStepper<Reach>(world, {entryAt(CARRIER_A, 7, 5.0)});
  return world;
}

TEST_CASE("systems that publish into fields give the same world and the same hash after every "
          "tick whatever order they are registered in") {
  World forward = publishingWorld(
      makeSystemsSchema({publishEvenFootfall, publishOddFootfall, publishSteppedSources<Reach>}));
  World backward = publishingWorld(
      makeSystemsSchema({publishSteppedSources<Reach>, publishOddFootfall, publishEvenFootfall}));

  for (int tick = 0; tick < 3; ++tick) {
    CAPTURE(tick);
    stepWorld(forward);
    stepWorld(backward);
    REQUIRE(sampled<Footfall>(forward, JUNCTION).size() == 3);
    requireSameValue(forward, backward);
  }
}

// A preview is exact only if a candidate samples as the world that commits its commands.
TEST_CASE("a candidate made with makeCandidate samples every field, both layers, as the world that "
          "commits the same commands") {
  World base = makeFieldWorld(makeCommandedSchema());
  // Its resolved entries change, so the resolution clears its stepped entries.
  const EntityKey walker = addSource<Footfall>(base, {entryAt(CARRIER_A, 4, 1.0)});
  setSteps<Footfall>(base, walker, {entryAt(CARRIER_A, 4, 10.0), entryAt(CARRIER_A, 8.5, 11.0)});
  // Its resolved entries do not change, so it keeps its stepped entries.
  const EntityKey steady = addSource<Footfall>(base, {entryAt(CARRIER_A, 2, 2.0)});
  setSteps<Footfall>(base, steady, {entryAt(CARRIER_A, 4, 20.0)});
  // Stepped reach entries inside A's second edge, one where the command adds a node.
  addStepper<Reach>(base, {entryAt(CARRIER_A, 7, 30.0), entryAt(CARRIER_A, 10, 31.0)});
  addSource<Reach>(base, {entryAt(CARRIER_A, 4, 40.0)});
  resolveWorld(base);
  stepWorld(base);

  // A new node on A at 7 cuts A's second edge.
  Layout cut = standardLayout();
  cut.Carriers.front().Stops = {{.Distance = 0, .Node = 0},
                                {.Distance = 4, .Node = 1},
                                {.Distance = 7, .Node = 4},
                                {.Distance = 10, .Node = 2}};
  cut.NodeCount = 5;
  auto fill = [&](CommandQueue &queue) {
    queue.push(ReplaceLayout{.Replacement = cut});
    queue.push(SetFootfall{.Source = walker, .Entries = {entryAt(CARRIER_A, 4, 5.0)}});
  };

  World previewed = copyWorld(base);
  stepWorld(previewed);
  CommandQueue tentative;
  fill(tentative);
  const World candidate = makeCandidate(previewed, tentative);

  World committed = copyWorld(base);
  CommandQueue queued;
  fill(queued);
  stepWorld(committed, queued);

  // The commands changed what is sampled, and stepped entries survive them, so the comparison below
  // is not vacuous.
  REQUIRE(sampled<Footfall>(candidate, JUNCTION) != sampled<Footfall>(previewed, JUNCTION));
  REQUIRE(sampled<Footfall>(candidate, JUNCTION).size() == 2);
  REQUIRE_FALSE(sampled<Reach>(candidate, place(CARRIER_A, 7)).empty());

  // A node, a place inside an unchanged edge, the new node, and a place inside a cut edge.
  for (const Place at :
       {place(CARRIER_A, 4), place(CARRIER_A, 2), place(CARRIER_A, 7), place(CARRIER_A, 8.5)}) {
    CAPTURE(at.Distance);
    REQUIRE(sampledBits<Footfall>(candidate, at) == sampledBits<Footfall>(committed, at));
    REQUIRE(std::bit_cast<uint64_t>(fieldValue<Footfall>(candidate, networkOf(candidate), at)) ==
            std::bit_cast<uint64_t>(fieldValue<Footfall>(committed, networkOf(committed), at)));
    REQUIRE(sampledBits<Reach>(candidate, at) == sampledBits<Reach>(committed, at));
  }
}

// A field is sampled without being consumed.
TEST_CASE("sampleField takes the world by const reference") {
  using Sampler =
      std::vector<SampledEntry<double>> (*)(const World &, const Network &, const Place &);
  STATIC_REQUIRE(std::is_same_v<decltype(&sampleField<Footfall>), Sampler>);
}

} // namespace
} // namespace tpj
