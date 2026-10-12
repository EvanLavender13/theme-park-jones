#include "support/synthetic_types.h"

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/field_text.h"
#include "sim/mix.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stddef.h>
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
using test::Mood;
using test::Offset;
using test::Probe;
using test::SLOT_PURPOSE;
using test::Tag;

// Intent with vectors of every element kind a save spells differently: structs, enums, and keys,
// beside the widest integers.
struct Route {
  std::vector<Offset> Points;
  std::vector<Mood> Moods;
  std::vector<EntityKey> Stops;
  int64_t Low = 0;
  uint64_t High = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Route &route) {
  visitor.field("points", route.Points);
  visitor.field("moods", route.Moods);
  visitor.field("stops", route.Stops);
  visitor.field("low", route.Low);
  visitor.field("high", route.High);
}

// A state process that takes several ticks: each probe's samples fill up to four, one per tick.
void advanceProbes(World &world, TickBuffers & /*buffers*/) {
  for (const EntityKey key : world.keysWith<Probe>()) {
    Probe &probe = world.get<Probe>(key);
    probe.Level += 0.1;
    if (probe.Samples.size() < 4) {
      probe.Samples.push_back(probe.Level);
    }
  }
}

// Each route derives an entity holding its number of points.
void deriveSlots(World &world) {
  std::vector<std::pair<EntityKey, int64_t>> routes;
  for (const EntityKey key : world.keysWith<Route>()) {
    routes.emplace_back(key, static_cast<int64_t>(world.get<Route>(key).Points.size()));
  }
  for (const auto &[owner, points] : routes) {
    const EntityKey key = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
    world.emplaceOrReplace<Cached>(key, Cached{.Total = points});
  }
}

// A derived type registered between saved ones, so that skipping it is not an accident of
// position.
std::shared_ptr<const WorldSchema> makeSaveSchema() {
  auto schema = std::make_shared<WorldSchema>();
  schema->addComponent<Probe>("probe", DataKind::State);
  schema->addComponent<Tag>("tag", DataKind::Intent);
  schema->addComponent<Cached>("cached", DataKind::Derived);
  schema->addComponent<Route>("route", DataKind::Intent);
  schema->addSystem(advanceProbes);
  schema->addResolver("slots", deriveSlots);
  return schema;
}

void requireSameValue(const World &one, const World &other) {
  REQUIRE(worldsEqual(one, other));
  REQUIRE(worldsEqual(other, one));
  REQUIRE(hashWorld(one) == hashWorld(other));
}

constexpr double INF = std::numeric_limits<double>::infinity();
constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();

// The world whose save the canonical form test spells out.
constexpr EntityKey FIRST{1};
constexpr EntityKey SECOND{2};
constexpr EntityKey BARE{3};
constexpr EntityKey ROUTED{4};
const EntityKey SLOT = deriveKey(ROUTED, SLOT_PURPOSE, 0);

Probe firstProbe() {
  Probe probe;
  probe.Flag = true;
  probe.Count = -7;
  probe.Small = 9;
  probe.Level = 1.5;
  probe.Feeling = Mood::Excited;
  probe.Target = SECOND;
  probe.Samples = {0.25, -3.0};
  probe.Place = {.X = 4.5, .Z = -2.0};
  return probe;
}

// Doubles whose shortest form is an exponent, an infinity, or a negative zero.
Probe slotProbe() {
  Probe probe;
  probe.Level = 1e300;
  probe.Target = ROUTED;
  probe.Samples = {INF};
  probe.Place = {.X = -0.0, .Z = -INF};
  return probe;
}

Route fourthRoute() {
  Route route;
  route.Points = {{.X = 1.0, .Z = 2.0}, {.X = -0.0, .Z = 0.1 + 0.2}};
  route.Moods = {Mood::Calm, Mood::Excited};
  route.Stops = {FIRST, BARE};
  route.Low = std::numeric_limits<int64_t>::min();
  route.High = std::numeric_limits<uint64_t>::max();
  return route;
}

// Counter keys 1 and 2 hold a probe and a tag, 3 holds only derived data, 4 holds a route, and 5
// was destroyed. The route's derived entity holds a probe. Not yet resolved.
World buildFormWorld(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 12345);
  world.Tick = 3000;
  for (int i = 0; i < 5; ++i) {
    world.createEntity();
  }
  world.destroyEntity(EntityKey{5});
  world.emplace<Probe>(FIRST, firstProbe());
  world.emplace<Tag>(FIRST);
  world.emplace<Probe>(SECOND);
  world.emplace<Tag>(SECOND);
  world.emplace<Cached>(BARE, Cached{.Total = 8});
  world.emplace<Route>(ROUTED, fourthRoute());
  world.createDerivedEntity(ROUTED, SLOT_PURPOSE, 0);
  world.emplace<Probe>(SLOT, slotProbe());
  return world;
}

// The same world by other calls: an entity created and destroyed first, the derived entity made
// before the counter entities, and components added in reverse.
World buildFormWorldReordered(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 12345);
  world.Tick = 3000;
  world.destroyEntity(world.createDerivedEntity(EntityKey{9}, SLOT_PURPOSE, 3));
  world.createDerivedEntity(ROUTED, SLOT_PURPOSE, 0);
  world.emplace<Probe>(SLOT, slotProbe());
  for (int i = 0; i < 5; ++i) {
    world.createEntity();
  }
  world.emplace<Route>(ROUTED, fourthRoute());
  world.emplace<Cached>(BARE, Cached{.Total = 8});
  world.emplace<Tag>(SECOND);
  world.emplace<Probe>(SECOND);
  world.emplace<Tag>(FIRST);
  world.emplace<Probe>(FIRST, firstProbe());
  world.destroyEntity(EntityKey{5});
  return world;
}

// Draws for a randomized world, so that each world is a function of its seed alone.
class Dice {
public:
  explicit Dice(uint64_t seed) : Seed(seed) {}

  uint64_t bits() { return drawBits(DrawKey{.Seed = Seed, .Index = Count++}); }
  bool coin() { return (bits() & 1U) != 0; }
  uint64_t below(uint64_t bound) { return bits() % bound; }
  // Any double but NaN, from its bits, so that every exponent is as likely as any other.
  double real() {
    double value = NOT_A_NUMBER;
    while (std::isnan(value)) {
      value = std::bit_cast<double>(bits());
    }
    return value;
  }
  EntityKey key() { return EntityKey{below(12)}; }

private:
  uint64_t Seed;
  uint64_t Count = 0;
};

// Samples hold at most one value, so that two ticks later none has finished filling up.
Probe randomProbe(Dice &dice) {
  Probe probe;
  probe.Flag = dice.coin();
  probe.Count = static_cast<int32_t>(dice.bits());
  probe.Small = static_cast<uint16_t>(dice.bits());
  probe.Level = dice.real();
  probe.Feeling = dice.coin() ? Mood::Excited : Mood::Calm;
  probe.Target = dice.key();
  if (dice.coin()) {
    probe.Samples.push_back(dice.real());
  }
  probe.Place = {.X = dice.real(), .Z = dice.real()};
  return probe;
}

Route randomRoute(Dice &dice) {
  Route route;
  for (uint64_t i = dice.below(3); i > 0; --i) {
    route.Points.push_back({.X = dice.real(), .Z = dice.real()});
  }
  for (uint64_t i = dice.below(3); i > 0; --i) {
    route.Moods.push_back(dice.coin() ? Mood::Excited : Mood::Calm);
  }
  for (uint64_t i = dice.below(3); i > 0; --i) {
    route.Stops.push_back(dice.key());
  }
  route.Low = static_cast<int64_t>(dice.bits());
  route.High = dice.bits();
  return route;
}

// A resolved world two ticks into its probes' filling. Key 1 always holds a route, so that its
// derived entity exists to hold a probe, key 2 never holds anything saved, and the last counter
// key is destroyed, so that the counter runs ahead of the live keys. Key 1's components also hold
// the boundaries of each field.
World buildRandomWorld(std::shared_ptr<const WorldSchema> schema, uint64_t seed) {
  Dice dice(seed);
  World world(std::move(schema), dice.bits());
  world.Tick = dice.below(1'000'000);
  constexpr uint64_t COUNT = 8;
  for (uint64_t i = 0; i < COUNT; ++i) {
    world.createEntity();
  }
  for (uint64_t i = 3; i <= COUNT; ++i) {
    const EntityKey key{i};
    if (dice.below(3) == 0) {
      world.emplace<Route>(key, randomRoute(dice));
    }
    if (dice.coin()) {
      world.emplace<Probe>(key, randomProbe(dice));
    }
    if (dice.coin()) {
      world.emplace<Tag>(key);
    }
  }

  Probe edge = randomProbe(dice);
  edge.Flag = true;
  edge.Count = std::numeric_limits<int32_t>::min();
  edge.Small = std::numeric_limits<uint16_t>::max();
  edge.Samples = {std::numeric_limits<double>::denorm_min()};
  edge.Place = {.X = -0.0, .Z = -std::numeric_limits<double>::max()};
  Route edgeRoute;
  edgeRoute.Low = std::numeric_limits<int64_t>::min();
  edgeRoute.High = std::numeric_limits<uint64_t>::max();
  world.emplace<Probe>(FIRST, edge);
  world.emplace<Route>(FIRST, edgeRoute);

  world.destroyEntity(EntityKey{COUNT});
  resolveWorld(world);
  Probe slot = randomProbe(dice);
  slot.Samples.clear();
  world.emplace<Probe>(deriveKey(FIRST, SLOT_PURPOSE, 0), slot);
  stepWorld(world);
  stepWorld(world);
  return world;
}

std::string lines(const std::vector<std::string> &textLines) {
  std::string text;
  for (const std::string &line : textLines) {
    text += line;
    text += '\n';
  }
  return text;
}

TEST_CASE("loading the save of a resolved world and resolving gives the world saved, whose save "
          "is the same text") {
  const auto schema = makeSaveSchema();
  for (const uint64_t seed : {1U, 2U, 3U}) {
    CAPTURE(seed);
    const World original = buildRandomWorld(schema, seed);
    const std::string text = saveWorld(original);

    World loaded = loadWorld(schema, text);
    resolveWorld(loaded);

    requireSameValue(loaded, original);
    REQUIRE(saveWorld(loaded) == text);
  }
}

TEST_CASE("equal worlds give identical saves however they were built") {
  const auto schema = makeSaveSchema();
  const World plain = buildFormWorld(schema);
  const World reordered = buildFormWorldReordered(schema);
  requireSameValue(plain, reordered);
  REQUIRE(saveWorld(reordered) == saveWorld(plain));
}

TEST_CASE("a loaded world is pending resolution") {
  const auto schema = makeSaveSchema();
  World resolved = buildFormWorld(schema);
  resolveWorld(resolved);
  REQUIRE(loadWorld(schema, saveWorld(resolved)).isResolvePending());
}

TEST_CASE("a save is independent of derived data") {
  const auto schema = makeSaveSchema();
  World world = buildFormWorld(schema);
  resolveWorld(world);
  const std::string text = saveWorld(world);

  componentOf<Cached>(world, SLOT).Total += 1;

  REQUIRE(saveWorld(world) == text);
}

std::vector<std::string> splitLines(const std::string &text) {
  std::vector<std::string> result;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    result.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return result;
}

TEST_CASE("a failed load names the first line it cannot read") {
  const auto schema = makeSaveSchema();
  World resolved = buildFormWorld(schema);
  resolveWorld(resolved);
  std::vector<std::string> broken = splitLines(saveWorld(resolved));

  // No header, section, key, or field begins with %, so both lines are unreadable wherever they
  // fall, and only reading in order makes the earlier one the line named.
  const size_t first = broken.size() / 2;
  const size_t later = broken.size() - 1;
  REQUIRE(first < later);
  broken[first] = "%";
  broken[later] = "%";

  std::optional<size_t> named;
  std::string message;
  try {
    static_cast<void>(loadWorld(schema, lines(broken)));
  } catch (const LoadError &error) {
    named = error.line();
    message = error.what();
  }
  // Lines are numbered from 1.
  REQUIRE(named == first + 1);
  CHECK_THAT(message, ContainsSubstring("line " + std::to_string(first + 1)));
}

TEST_CASE("a load ignores carriage returns and blank lines") {
  const auto schema = makeSaveSchema();
  World resolved = buildFormWorld(schema);
  resolveWorld(resolved);
  const std::string text = saveWorld(resolved);

  // A carriage return ends every line, and a blank line, itself ending in a carriage return, comes
  // before the first line and after every line.
  std::string variant = "\r\n";
  for (const char ch : text) {
    if (ch == '\n') {
      variant += "\r\n\r\n";
    } else {
      variant += ch;
    }
  }

  REQUIRE(worldsEqual(loadWorld(schema, variant), loadWorld(schema, text)));
}

const std::vector<std::string> HEADER = {"tpj-park 1", "seed 5", "tick 0", "next-key 4"};
const std::string PROBE_LINE = "1 flag=true count=-7 small=9 level=1.5 feeling=excited target=2 "
                               "samples=[0.25 -3] place={x=4.5 z=-2}";

std::vector<std::string> withHeader(const std::vector<std::string> &body) {
  std::vector<std::string> all = HEADER;
  all.insert(all.end(), body.begin(), body.end());
  return all;
}

TEST_CASE("a number std::from_chars reads in full loads as its value, and saves in its canonical "
          "spelling") {
  const auto schema = makeSaveSchema();
  const std::string canonical = lines(withHeader({"", "[probe]", PROBE_LINE}));
  const std::string handWritten =
      lines(withHeader({"", "[probe]",
                        "01 flag=true count=-007 small=09 level=1.50 feeling=excited target=2 "
                        "samples=[25e-2 -3.0] place={x=4.5000 z=-2E0}"}));

  const World loaded = loadWorld(schema, handWritten);
  requireSameValue(loaded, loadWorld(schema, canonical));
  REQUIRE(saveWorld(loaded) == canonical);
}

TEST_CASE("saveWorld refuses an enum value its type gives no name, naming the field") {
  World world = buildFormWorld(makeSaveSchema());
  // A value outside the enumerators is the case under test.
  componentOf<Probe>(world, SECOND).Feeling = static_cast<Mood>(2);
  std::optional<std::string> message;
  try {
    static_cast<void>(saveWorld(world));
  } catch (const std::invalid_argument &error) {
    message = error.what();
  }
  REQUIRE(message.has_value());
  CHECK_THAT(message.value_or(""), ContainsSubstring("feeling"));
}

} // namespace
} // namespace tpj
