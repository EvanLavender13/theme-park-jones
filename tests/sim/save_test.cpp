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
#include <functional>
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
void advanceProbes(World &world) {
  world.Registry.view<Probe>().each([](Probe &probe) {
    probe.Level += 0.1;
    if (probe.Samples.size() < 4) {
      probe.Samples.push_back(probe.Level);
    }
  });
}

// Each route derives an entity holding its number of points.
void deriveSlots(World &world) {
  std::vector<std::pair<EntityKey, int64_t>> routes;
  world.Registry.view<Route>().each([&](entt::entity entity, const Route &route) {
    routes.emplace_back(world.keyOf(entity), static_cast<int64_t>(route.Points.size()));
  });
  for (const auto &[owner, points] : routes) {
    const EntityKey key = world.createDerivedEntity(owner, SLOT_PURPOSE, 0);
    world.Registry.emplace_or_replace<Cached>(world.findEntity(key), Cached{.Total = points});
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

std::string decimal(EntityKey key) { return std::to_string(static_cast<uint64_t>(key)); }

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
  world.Registry.emplace<Probe>(world.findEntity(FIRST), firstProbe());
  world.Registry.emplace<Tag>(world.findEntity(FIRST));
  world.Registry.emplace<Probe>(world.findEntity(SECOND));
  world.Registry.emplace<Tag>(world.findEntity(SECOND));
  world.Registry.emplace<Cached>(world.findEntity(BARE), Cached{.Total = 8});
  world.Registry.emplace<Route>(world.findEntity(ROUTED), fourthRoute());
  world.createDerivedEntity(ROUTED, SLOT_PURPOSE, 0);
  world.Registry.emplace<Probe>(world.findEntity(SLOT), slotProbe());
  return world;
}

// The same world by other calls: EnTT's identifiers churned first, the derived entity made before
// the counter entities, and components added in reverse.
World buildFormWorldReordered(std::shared_ptr<const WorldSchema> schema) {
  World world(std::move(schema), 12345);
  world.Tick = 3000;
  world.destroyEntity(world.createDerivedEntity(EntityKey{9}, SLOT_PURPOSE, 3));
  world.createDerivedEntity(ROUTED, SLOT_PURPOSE, 0);
  world.Registry.emplace<Probe>(world.findEntity(SLOT), slotProbe());
  for (int i = 0; i < 5; ++i) {
    world.createEntity();
  }
  world.Registry.emplace<Route>(world.findEntity(ROUTED), fourthRoute());
  world.Registry.emplace<Cached>(world.findEntity(BARE), Cached{.Total = 8});
  world.Registry.emplace<Tag>(world.findEntity(SECOND));
  world.Registry.emplace<Probe>(world.findEntity(SECOND));
  world.Registry.emplace<Tag>(world.findEntity(FIRST));
  world.Registry.emplace<Probe>(world.findEntity(FIRST), firstProbe());
  world.destroyEntity(EntityKey{5});
  return world;
}

std::string formText() {
  return "tpj-park 1\n"
         "seed 12345\n"
         "tick 3000\n"
         "next-key 6\n"
         "\n"
         "[entities]\n"
         "3\n"
         "\n"
         "[probe]\n"
         "1 flag=true count=-7 small=9 level=1.5 feeling=excited target=2 samples=[0.25 -3] "
         "place={x=4.5 z=-2}\n"
         "2 flag=false count=0 small=0 level=0 feeling=calm target=0 samples=[] "
         "place={x=0 z=0}\n" +
         decimal(SLOT) +
         " flag=false count=0 small=0 level=1e+300 feeling=calm target=4 samples=[inf] "
         "place={x=-0 z=-inf}\n"
         "\n"
         "[tag]\n"
         "1\n"
         "2\n"
         "\n"
         "[route]\n"
         "4 points=[{x=1 z=2} {x=-0 z=0.30000000000000004}] moods=[calm excited] stops=[1 3] "
         "low=-9223372036854775808 high=18446744073709551615\n";
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
    const entt::entity entity = world.findEntity(EntityKey{i});
    if (dice.below(3) == 0) {
      world.Registry.emplace<Route>(entity, randomRoute(dice));
    }
    if (dice.coin()) {
      world.Registry.emplace<Probe>(entity, randomProbe(dice));
    }
    if (dice.coin()) {
      world.Registry.emplace<Tag>(entity);
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
  world.Registry.emplace<Probe>(world.findEntity(FIRST), edge);
  world.Registry.emplace<Route>(world.findEntity(FIRST), edgeRoute);

  world.destroyEntity(EntityKey{COUNT});
  resolveWorld(world);
  Probe slot = randomProbe(dice);
  slot.Samples.clear();
  world.Registry.emplace<Probe>(world.findEntity(deriveKey(FIRST, SLOT_PURPOSE, 0)), slot);
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

// The number of the line a LoadError names, or nothing if the load succeeds.
std::optional<size_t> failingLine(const std::shared_ptr<const WorldSchema> &schema,
                                  const std::string &text) {
  try {
    static_cast<void>(loadWorld(schema, text));
  } catch (const LoadError &error) {
    CHECK_THAT(error.what(), ContainsSubstring("line " + std::to_string(error.line())));
    return error.line();
  }
  return std::nullopt;
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

TEST_CASE("a loaded and resolved world steps in lockstep with the world saved") {
  const auto schema = makeSaveSchema();
  World original = buildRandomWorld(schema, 1);
  World loaded = loadWorld(schema, saveWorld(original));
  resolveWorld(loaded);

  // Enough ticks for every probe's samples to finish filling up.
  for (int tick = 0; tick < 4; ++tick) {
    stepWorld(original);
    stepWorld(loaded);
    CAPTURE(tick);
    requireSameValue(loaded, original);
  }
}

// Whether resolution is pending is not saved, so resolving changes nothing in the text.
TEST_CASE("a save is the text form the spec defines, before and after resolution") {
  World world = buildFormWorld(makeSaveSchema());
  REQUIRE(saveWorld(world) == formText());
  resolveWorld(world);
  REQUIRE(saveWorld(world) == formText());
}

TEST_CASE("equal worlds give identical saves however they were built") {
  const auto schema = makeSaveSchema();
  const World plain = buildFormWorld(schema);
  const World reordered = buildFormWorldReordered(schema);
  requireSameValue(plain, reordered);
  REQUIRE(saveWorld(reordered) == saveWorld(plain));
}

TEST_CASE("a loaded world holds the saved seed, tick, and next key, with resolution pending") {
  const auto schema = makeSaveSchema();

  SECTION("values at the edges of their ranges") {
    // Seed and tick past what 63 and 32 bits hold; next-key at its largest, with the counter key
    // just below it.
    const World loaded = loadWorld(schema, lines({"tpj-park 1", "seed 18446744073709551615",
                                                  "tick 4294967296", "next-key 9223372036854775808",
                                                  "", "[entities]", "2", "9223372036854775807"}));
    REQUIRE(loaded.Seed == std::numeric_limits<uint64_t>::max());
    REQUIRE(loaded.Tick == uint64_t{1} << 32U);
    REQUIRE(loaded.nextKey() == DERIVED_KEY_BIT);
    REQUIRE(loaded.keys() == std::vector<EntityKey>{SECOND, EntityKey{DERIVED_KEY_BIT - 1}});
    REQUIRE(loaded.isResolvePending());
  }
  SECTION("the save of a resolved world") {
    World resolved = buildFormWorld(schema);
    resolveWorld(resolved);
    const World loaded = loadWorld(schema, saveWorld(resolved));
    REQUIRE(loaded.Seed == resolved.Seed);
    REQUIRE(loaded.Tick == resolved.Tick);
    REQUIRE(loaded.nextKey() == resolved.nextKey());
    REQUIRE(loaded.isResolvePending());
  }
}

TEST_CASE("a save holds no derived data") {
  const auto schema = makeSaveSchema();
  World world = buildFormWorld(schema);
  resolveWorld(world);
  const std::string text = saveWorld(world);

  for (const ComponentType &type : schema->components()) {
    if (type.Kind == DataKind::Derived) {
      CAPTURE(type.Name);
      REQUIRE(text.find("[" + type.Name + "]") == std::string::npos);
    }
  }

  SECTION("changing a derived component") { componentOf<Cached>(world, SLOT).Total = 99; }
  SECTION("adding derived components, to entities holding saved data and not") {
    world.Registry.emplace<Cached>(world.findEntity(FIRST), Cached{.Total = 1});
    world.Registry.emplace<Cached>(
        world.findEntity(world.createDerivedEntity(BARE, SLOT_PURPOSE, 0)), Cached{.Total = 2});
  }
  SECTION("removing derived components") {
    world.Registry.remove<Cached>(world.findEntity(SLOT));
    world.Registry.remove<Cached>(world.findEntity(BARE));
  }
  REQUIRE(saveWorld(world) == text);
}

// Each case breaks one rule the spec gives for a save, at a line chosen for it.
struct Malformed {
  std::string Label;
  std::vector<std::string> Lines;
  size_t Line = 0;
};

const std::vector<std::string> HEADER = {"tpj-park 1", "seed 5", "tick 0", "next-key 4"};
const std::string PROBE_LINE = "1 flag=true count=-7 small=9 level=1.5 feeling=excited target=2 "
                               "samples=[0.25 -3] place={x=4.5 z=-2}";
const std::string ROUTE_LINE =
    "2 points=[{x=1 z=2} {x=3 z=4}] moods=[calm excited] stops=[1 3] low=-1 high=1";

std::vector<std::string> withHeader(const std::vector<std::string> &body) {
  std::vector<std::string> all = HEADER;
  all.insert(all.end(), body.begin(), body.end());
  return all;
}

// A probe line with one substring replaced, at line 7.
Malformed probeCase(std::string label, const std::string &from, const std::string &to) {
  std::string line = PROBE_LINE;
  const size_t at = line.find(from);
  REQUIRE(at != std::string::npos);
  line.replace(at, from.size(), to);
  return {std::move(label), withHeader({"", "[probe]", line}), 7};
}

std::vector<Malformed> headerCases() {
  return {
      {"empty text", {}, 1},
      {"text ending inside the header", {"tpj-park 1", "seed 5", "tick 0"}, 4},
      {"wrong label", {"tpj-pork 1", "seed 5", "tick 0", "next-key 4"}, 1},
      {"wrong version", {"tpj-park 2", "seed 5", "tick 0", "next-key 4"}, 1},
      {"a NUL in the label",
       {std::string("tpj-park\0 1", 11), "seed 5", "tick 0", "next-key 4"},
       1},
      {"header line missing", {"tpj-park 1", "tick 0", "next-key 4"}, 2},
      {"header lines out of order", {"tpj-park 1", "tick 0", "seed 5", "next-key 4"}, 2},
      {"seed too large for 64 bits",
       {"tpj-park 1", "seed 18446744073709551616", "tick 0", "next-key 4"},
       2},
      {"text after a header value", {"tpj-park 1", "seed 5", "tick 0 0", "next-key 4"}, 3},
      {"next-key zero", {"tpj-park 1", "seed 5", "tick 0", "next-key 0"}, 4},
      {"next-key above 2^63",
       {"tpj-park 1", "seed 5", "tick 0", "next-key 9223372036854775809"},
       4},
  };
}

std::vector<Malformed> sectionCases() {
  return {
      {"malformed section header", withHeader({"", "[probe", PROBE_LINE}), 6},
      {"unknown section", withHeader({"", "[box]", "1"}), 6},
      {"derived type's section", withHeader({"", "[cached]", "1 total=1"}), 6},
      {"repeated section", withHeader({"", "[tag]", "1", "", "[tag]", "2"}), 9},
      {"sections out of registration order",
       withHeader({"", "[route]", ROUTE_LINE, "", "[tag]", "1"}), 9},
      {"[entities] after a type's section", withHeader({"", "[tag]", "1", "", "[entities]", "3"}),
       9},
      {"key line before any section", withHeader({"", "1"}), 6},
      {"section with no key lines before another", withHeader({"", "[entities]", "", "[tag]", "1"}),
       8},
      {"section with no key lines at the end", withHeader({"", "[tag]"}), 7},
  };
}

std::vector<Malformed> keyCases() {
  return {
      {"key zero", withHeader({"", "[tag]", "0"}), 7},
      {"key not a number", withHeader({"", "[tag]", "one"}), 7},
      {"key repeated in a section", withHeader({"", "[tag]", "1", "1"}), 8},
      {"counter key at next-key", withHeader({"", "[tag]", "4"}), 7},
      {"derived key in [entities]", withHeader({"", "[entities]", "9223372036854775808"}), 7},
      {"[entities] key again in a type's section",
       withHeader({"", "[entities]", "1", "", "[tag]", "1"}), 10},
      {"text after the key of a type without fields", withHeader({"", "[tag]", "1 x=1"}), 7},
  };
}

std::vector<Malformed> fieldCases() {
  return {
      probeCase("field missing", " place={x=4.5 z=-2}", ""),
      probeCase("fields out of order", "flag=true count=-7", "count=-7 flag=true"),
      probeCase("field misnamed", "flag=", "flags="),
      probeCase("nested field misnamed", "z=-2", "y=-2"),
      probeCase("bool not true or false", "flag=true", "flag=1"),
      probeCase("integer not read in full", "count=-7", "count=-7.5"),
      probeCase("integer too large for its field", "small=9", "small=65536"),
      probeCase("NaN", "level=1.5", "level=nan"),
      // An enum is written by name, never by number.
      probeCase("enum by number", "feeling=excited", "feeling=1"),
      probeCase("enum name unknown", "feeling=excited", "feeling=angry"),
      probeCase("vector elements separated by two spaces", "[0.25 -3]", "[0.25  -3]"),
      probeCase("vector left open at the end of the line", " -3] place={x=4.5 z=-2}", ""),
      probeCase("struct left open at the end of the line", "place={x=4.5 z=-2}", "place={x=4.5"),
      probeCase("text after the last field", "z=-2}", "z=-2} extra"),
      {"struct in a vector left open", withHeader({"", "[route]", "2 points=[{x=1 z=2} {x=3"}), 7},
  };
}

TEST_CASE("text that is not a save fails the load naming the first line that cannot be read") {
  const auto schema = makeSaveSchema();
  // The pieces the cases break load when whole, so each failure is the rule its case breaks.
  REQUIRE_NOTHROW(
      loadWorld(schema, lines(withHeader({"", "[entities]", "3", "", "[probe]", PROBE_LINE, "",
                                          "[tag]", "1", "2", "", "[route]", ROUTE_LINE}))));

  std::vector<Malformed> cases = headerCases();
  for (const auto &more : {sectionCases(), keyCases(), fieldCases()}) {
    cases.insert(cases.end(), more.begin(), more.end());
  }
  for (const Malformed &broken : cases) {
    const std::string text = lines(broken.Lines);
    CAPTURE(broken.Label, text);
    CHECK(failingLine(schema, text) == broken.Line);
  }
}

TEST_CASE("blank lines and carriage returns do not change what a save loads as") {
  const auto schema = makeSaveSchema();
  World resolved = buildFormWorld(schema);
  resolveWorld(resolved);
  const std::string text = saveWorld(resolved);
  const World expected = loadWorld(schema, text);

  std::string variant;
  SECTION("carriage return before every line feed") {
    for (const char ch : text) {
      if (ch == '\n') {
        variant += '\r';
      }
      variant += ch;
    }
  }
  SECTION("blank lines removed") {
    for (size_t i = 0; i < text.size(); ++i) {
      const bool repeatsLineFeed = text[i] == '\n' && i > 0 && text[i - 1] == '\n';
      if (!repeatsLineFeed) {
        variant += text[i];
      }
    }
    REQUIRE(variant.find("\n\n") == std::string::npos);
  }
  SECTION("blank lines added before, between, and after every line") {
    variant = "\n";
    for (const char ch : text) {
      variant += ch;
      if (ch == '\n') {
        variant += "\n\n";
      }
    }
  }
  requireSameValue(loadWorld(schema, variant), expected);
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

struct UnsavedGadget {
  int Value = 0;
};

// The WorldInvariantError message the call throws, or nothing if it throws none.
std::optional<std::string> invariantRefusal(const std::function<void()> &call) {
  try {
    call();
  } catch (const WorldInvariantError &error) {
    return std::string(error.what());
  }
  return std::nullopt;
}

TEST_CASE("in debug builds, saveWorld refuses a world the walk cannot cover, as validateWorld "
          "does") {
  if (!WORLD_CHECKS) {
    SKIP("world checks run only in debug builds");
  }
  World world = buildFormWorld(makeSaveSchema());
  std::string named;
  SECTION("a component of an unregistered type") {
    world.Registry.emplace<UnsavedGadget>(world.findEntity(FIRST), UnsavedGadget{.Value = 1});
    named = "UnsavedGadget";
  }
  SECTION("a NaN in registered state") {
    componentOf<Probe>(world, SECOND).Level = NOT_A_NUMBER;
    named = "level";
  }
  const std::optional<std::string> expected = invariantRefusal([&] { validateWorld(world); });
  REQUIRE(expected.has_value());
  const std::optional<std::string> refused =
      invariantRefusal([&] { static_cast<void>(saveWorld(world)); });
  REQUIRE(refused == expected);
  CHECK_THAT(refused.value_or(""), ContainsSubstring(named));
}

TEST_CASE("a schema refuses a component type named entities and is left unchanged") {
  WorldSchema schema;
  schema.addComponent<Tag>("tag", DataKind::Intent);

  REQUIRE_THROWS_AS(schema.addComponent<Probe>("entities", DataKind::State), std::invalid_argument);
  REQUIRE(schema.components().size() == 1);
  REQUIRE(schema.components()[0].Name == "tag");
  // The refused type was not half registered.
  REQUIRE_NOTHROW(schema.addComponent<Probe>("probe", DataKind::State));
}

TEST_CASE("a world with nothing registered saves as its header and loads back equal once "
          "resolved") {
  const auto schema = std::make_shared<WorldSchema>();
  World world(schema, 7);
  world.Tick = 5;
  resolveWorld(world);

  const std::string text = saveWorld(world);
  REQUIRE(text == "tpj-park 1\nseed 7\ntick 5\nnext-key 1\n");
  World loaded = loadWorld(schema, text);
  resolveWorld(loaded);
  requireSameValue(loaded, world);
}

} // namespace
} // namespace tpj
