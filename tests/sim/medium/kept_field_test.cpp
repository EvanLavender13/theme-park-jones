#include "support/synthetic_fields.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/kept_field.h"
#include "sim/medium/network.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::addSyntheticNetwork;
using test::CARRIER_A;
using test::CARRIER_B;
using test::CARRIER_C;
using test::Layout;
using test::LAYOUT_KEY;
using test::makeFieldWorld;
using test::networkOf;
using test::standardLayout;

using Sampled = std::vector<std::pair<EntityKey, double>>;
using SampledBits = std::vector<std::pair<EntityKey, uint64_t>>;

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();

constexpr Place place(EntityKey carrier, double distance) {
  return {.Carrier = carrier, .Distance = distance};
}

// The value halved once for each tick, the ticks clamped so the exponent fits an int.
double halved(double value, uint64_t ticks) {
  return std::ldexp(value, -static_cast<int>(std::min<uint64_t>(ticks, 2000)));
}

// A kept field read by the default rules. Its owner's rule halves a value for each tick since it
// last changed, so a read shows how many ticks the rule was given.
struct Litter {
  using Entry = double;
  static constexpr std::string_view Name = "litter";
  static constexpr FieldKind Kind = FieldKind::Scalar;

  static double readKept(double value, uint64_t ticks) { return halved(value, ticks); }
};

// What Crowding's rules were given, in call order. The rules take only their samples, so the
// record lives outside them.
std::vector<NodeSample> &nodeCalls() {
  static std::vector<NodeSample> calls;
  return calls;
}

std::vector<EdgeSample<double>> &edgeCalls() {
  static std::vector<EdgeSample<double>> calls;
  return calls;
}

// A kept field with its owner's rules at nodes and inside edges, read as Litter is. Each rule
// returns the total of the values it was given, so each result is traceable to its sample.
struct Crowding {
  using Entry = double;
  static constexpr std::string_view Name = "crowding";
  static constexpr FieldKind Kind = FieldKind::Scalar;

  static double readKept(double value, uint64_t ticks) { return halved(value, ticks); }

  static std::vector<double> sampleNode(const NodeSample &sample) {
    nodeCalls().push_back(sample);
    double total = 0.0;
    for (const double value : sample.AtNode) {
      total += value;
    }
    for (const NodeEnd &end : sample.Ends) {
      for (const EdgeEntry<double> &entry : end.Along) {
        total += entry.Value;
      }
    }
    return {total};
  }

  static std::vector<double> sampleEdge(const EdgeSample<double> &sample) {
    edgeCalls().push_back(sample);
    double total = 0.0;
    for (const double value : sample.AtFrom) {
      total += value;
    }
    for (const double value : sample.AtTo) {
      total += value;
    }
    for (const EdgeEntry<double> &entry : sample.Along) {
      total += entry.Value;
    }
    return {total};
  }
};

// A change for a system to keep while stepping the given tick.
struct Keep {
  uint64_t Tick = 0;
  EntityKey Source = NULL_KEY;
  Place At;
  double Value = 0.0;
};

template <typename F> std::vector<Keep> &plannedKeeps() {
  static std::vector<Keep> keeps;
  return keeps;
}

// Keeps the changes planned for the tick being stepped, in the order planned.
template <typename F> void keepPlanned(World &world) {
  for (const Keep &keep : plannedKeeps<F>()) {
    if (keep.Tick == world.Tick) {
      keepEntry<F>(world, keep.Source, keep.At, keep.Value);
    }
  }
}

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

// Inside A's second edge.
constexpr Place WATCHED = place(CARRIER_A, 7);
// Node 1, where A, its second edge, and B meet.
constexpr Place JUNCTION = place(CARRIER_A, 4);

// What a system registered after the keepers sampled of litter at WATCHED, tick by tick.
std::vector<Sampled> &observed() {
  static std::vector<Sampled> recorded;
  return recorded;
}

void observeWatched(World &world) { observed().push_back(sampled<Litter>(world, WATCHED)); }

using Replacement = std::pair<EntityKey, std::vector<KeptEntry>>;

std::vector<Replacement> &plannedReplacements() {
  static std::vector<Replacement> replacements;
  return replacements;
}

// The owner's finisher: makes the planned replacements of litter's entries, then forgets them.
void replacePlanned(World &world) {
  for (const auto &[source, entries] : plannedReplacements()) {
    replaceKeptEntries<Litter>(world, source, entries);
  }
  plannedReplacements().clear();
}

// A command that changes nothing, so that a cycle applying it resolves.
struct Nudge {};

[[maybe_unused]] void applyCommand(World & /*world*/, const Nudge & /*nudge*/) {}

std::shared_ptr<WorldSchema> makeKeptSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addSyntheticNetwork(*schema);
  addKeptField<Litter>(*schema);
  addKeptField<Crowding>(*schema);
  schema->addSystem(keepPlanned<Litter>);
  schema->addSystem(keepPlanned<Crowding>);
  // After the keepers, so that it would see a change made in its own tick were one readable.
  schema->addSystem(observeWatched);
  schema->addFinisher(replacePlanned);
  schema->addCommand<Nudge>();
  return schema;
}

// The record of refused calls: whether each threw std::invalid_argument, and whether it left the
// world as it was.
struct Refusal {
  bool Threw = false;
  bool Unchanged = false;
  bool operator==(const Refusal &) const = default;
};

std::vector<Refusal> &refusals() {
  static std::vector<Refusal> recorded;
  return recorded;
}

// A resolved world holding the standard layout, with every plan and record emptied.
World makeKeptWorld(std::shared_ptr<const WorldSchema> schema) {
  plannedKeeps<Litter>().clear();
  plannedKeeps<Crowding>().clear();
  plannedReplacements().clear();
  observed().clear();
  nodeCalls().clear();
  edgeCalls().clear();
  refusals().clear();
  World world = makeFieldWorld(std::move(schema));
  resolveWorld(world);
  return world;
}

KeptEntry kept(EntityKey carrier, double distance, double value, uint64_t tick) {
  return {.At = place(carrier, distance), .Value = value, .Tick = tick};
}

template <typename F> std::vector<KeptEntry> held(const World &world, EntityKey source) {
  const std::span<const KeptEntry> entries = keptEntries<F>(world, source);
  return {entries.begin(), entries.end()};
}

void requireSameEntries(const std::vector<KeptEntry> &actual,
                        const std::vector<KeptEntry> &expected) {
  REQUIRE(actual.size() == expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);
    REQUIRE(actual[i].At == expected[i].At);
    REQUIRE(std::bit_cast<uint64_t>(actual[i].Value) == std::bit_cast<uint64_t>(expected[i].Value));
    REQUIRE(actual[i].Tick == expected[i].Tick);
  }
}

void requireSameValue(const World &one, const World &other) {
  REQUIRE(worldsEqual(one, other));
  REQUIRE(worldsEqual(other, one));
  REQUIRE(hashWorld(one) == hashWorld(other));
}

template <typename Call> bool throwsInvalidArgument(Call call) {
  try {
    call();
  } catch (const std::invalid_argument &) {
    return true;
  } catch (...) {
    return false;
  }
  return false;
}

TEST_CASE("resolving never changes the kept entries a world holds") {
  World world = makeKeptWorld(makeKeptSchema());
  const EntityKey first = world.createEntity();
  const EntityKey second = world.createEntity();
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = first, .At = place(CARRIER_A, 7), .Value = 8.0},
      {.Tick = 1, .Source = second, .At = place(CARRIER_B, 3), .Value = 4.0},
      {.Tick = 1, .Source = first, .At = JUNCTION, .Value = 2.0},
  };
  stepWorld(world);
  stepWorld(world);
  const std::vector<KeptEntry> firstBefore = held<Litter>(world, first);
  const std::vector<KeptEntry> secondBefore = held<Litter>(world, second);
  REQUIRE(firstBefore.size() == 2);
  REQUIRE(secondBefore.size() == 1);

  // A layout of carrier A alone, so the network changes and B's places no longer resolve.
  world.Registry.get<Layout>(world.findEntity(LAYOUT_KEY)) =
      Layout{.Carriers = {standardLayout().Carriers.front()}, .NodeCount = 3};
  resolveWorld(world);

  requireSameEntries(held<Litter>(world, first), firstBefore);
  requireSameEntries(held<Litter>(world, second), secondBefore);
}

TEST_CASE("a kept change is read by nothing before the swap that ends its tick, and from that "
          "swap on is held unchanged until the next change at its place") {
  World world = makeKeptWorld(makeKeptSchema());
  const EntityKey source = world.createEntity();
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = source, .At = WATCHED, .Value = 8.0},
      {.Tick = 2, .Source = source, .At = place(CARRIER_A, 2), .Value = 1.0},
      {.Tick = 3, .Source = source, .At = WATCHED, .Value = 16.0},
  };

  stepWorld(world);
  REQUIRE(observed() == std::vector<Sampled>{Sampled{}});
  requireSameEntries(held<Litter>(world, source), {kept(CARRIER_A, 7, 8.0, 1)});

  // A change at another place leaves it as it was.
  stepWorld(world);
  stepWorld(world);
  requireSameEntries(held<Litter>(world, source),
                     {kept(CARRIER_A, 7, 8.0, 1), kept(CARRIER_A, 2, 1.0, 3)});

  // Tick 3 still reads the value held since tick 1, decayed by its rule.
  stepWorld(world);
  REQUIRE(observed() ==
          std::vector<Sampled>{Sampled{}, {{source, 8.0}}, {{source, 4.0}}, {{source, 2.0}}});
  requireSameEntries(held<Litter>(world, source),
                     {kept(CARRIER_A, 7, 16.0, 4), kept(CARRIER_A, 2, 1.0, 3)});
}

TEST_CASE("a source holds one entry at each place it has changed, in the order first kept, with "
          "the last value given there and the tick of the swap that applied it") {
  World world = makeKeptWorld(makeKeptSchema());
  const EntityKey source = world.createEntity();
  // Places compare as numbers, so -0.0 is the place 0.0.
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 7), .Value = 1.0},
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 2), .Value = 2.0},
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 7), .Value = 3.0},
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 0.0), .Value = 4.0},
      {.Tick = 1, .Source = source, .At = place(CARRIER_A, 2), .Value = 6.0},
      {.Tick = 1, .Source = source, .At = place(CARRIER_A, -0.0), .Value = 5.0},
  };

  stepWorld(world);
  stepWorld(world);

  requireSameEntries(
      held<Litter>(world, source),
      {kept(CARRIER_A, 7, 3.0, 1), kept(CARRIER_A, 2, 6.0, 2), kept(CARRIER_A, 0.0, 5.0, 2)});
}

TEST_CASE("a kept entry reads as its owner's rule of its held value and the ticks since its tick, "
          "or 0 ticks when its tick is later than the reading world's") {
  const auto schema = makeKeptSchema();
  World world = makeKeptWorld(schema);
  const EntityKey source = world.createEntity();
  plannedKeeps<Litter>() = {{.Tick = 0, .Source = source, .At = WATCHED, .Value = 8.0}};

  // The entry's tick is 1, the world's tick after the swap that applied it.
  stepWorld(world);
  REQUIRE(keptValue<Litter>(world, source, WATCHED) == std::optional<double>{8.0});
  stepWorld(world);
  stepWorld(world);
  REQUIRE(keptValue<Litter>(world, source, WATCHED) == std::optional<double>{2.0});
  REQUIRE_FALSE(keptValue<Litter>(world, source, place(CARRIER_A, 6)).has_value());

  // The same entries in a world whose tick is 0, before the entry's tick.
  std::string text = saveWorld(world);
  const std::string tickLine = "\ntick 3\n";
  const size_t at = text.find(tickLine);
  REQUIRE(at != std::string::npos);
  text.replace(at, tickLine.size(), "\ntick 0\n");
  const World earlier = loadWorld(schema, text);
  REQUIRE(earlier.Tick == 0);
  REQUIRE(keptValue<Litter>(earlier, source, WATCHED) == std::optional<double>{8.0});
}

TEST_CASE("sampleField and fieldValue of a kept field without node or edge rules give the read "
          "values of each source's entries at the place, sources in ascending key order") {
  World world = makeKeptWorld(makeKeptSchema());
  const EntityKey lower = world.createEntity();
  const EntityKey higher = world.createEntity();
  // The higher key keeps first. A's 4 and B's 0 are both stop places of node 1.
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = higher, .At = JUNCTION, .Value = 8.0},
      {.Tick = 0, .Source = higher, .At = place(CARRIER_B, 0), .Value = 4.0},
      {.Tick = 0, .Source = higher, .At = WATCHED, .Value = 64.0},
      {.Tick = 0, .Source = lower, .At = place(CARRIER_B, 0), .Value = 16.0},
      {.Tick = 0, .Source = lower, .At = WATCHED, .Value = 32.0},
  };
  // Read one tick after the entries' tick, so each read value is half the held one.
  stepWorld(world);
  stepWorld(world);

  const Sampled atNode = {{lower, 8.0}, {higher, 4.0}, {higher, 2.0}};
  REQUIRE(sampled<Litter>(world, JUNCTION) == atNode);
  REQUIRE(sampled<Litter>(world, place(CARRIER_B, 0)) == atNode);
  REQUIRE(sampled<Litter>(world, WATCHED) == Sampled{{lower, 16.0}, {higher, 32.0}});
  REQUIRE(sampled<Litter>(world, place(CARRIER_A, 6)).empty());
  REQUIRE(fieldValue<Litter>(world, networkOf(world), WATCHED) == 48.0);
}

TEST_CASE("a kept field's edge rule is given the read values of the source's entries") {
  World world = makeKeptWorld(makeKeptSchema());
  const EntityKey source = world.createEntity();
  // At node 1 and node 2, the ends of A's second edge, and inside it.
  plannedKeeps<Crowding>() = {
      {.Tick = 0, .Source = source, .At = JUNCTION, .Value = 8.0},
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 10), .Value = 4.0},
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 6), .Value = 2.0},
  };
  stepWorld(world);
  stepWorld(world);
  edgeCalls().clear();

  REQUIRE(sampled<Crowding>(world, WATCHED) == Sampled{{source, 7.0}});

  REQUIRE(edgeCalls().size() == 1);
  const EdgeSample<double> &sample = edgeCalls().front();
  REQUIRE(sample.AtFrom == std::vector<double>{4.0});
  REQUIRE(sample.AtTo == std::vector<double>{2.0});
  REQUIRE(sample.Along.size() == 1);
  REQUIRE(sample.Along.front().FromOffset == 2.0);
  REQUIRE(sample.Along.front().ToOffset == 4.0);
  REQUIRE(sample.Along.front().Value == 1.0);
}

struct ExpectedEnd {
  uint32_t Edge = 0;
  bool AtFrom = true;
  std::vector<EdgeEntry<double>> Along;
};

void requireAlong(const std::vector<EdgeEntry<double>> &along,
                  const std::vector<EdgeEntry<double>> &expected) {
  REQUIRE(along.size() == expected.size());
  for (size_t j = 0; j < expected.size(); ++j) {
    CAPTURE(j);
    REQUIRE(along[j].FromOffset == expected[j].FromOffset);
    REQUIRE(along[j].ToOffset == expected[j].ToOffset);
    REQUIRE(along[j].Value == expected[j].Value);
  }
}

void requireNodeEnd(const NodeEnd &end, const NetworkEdge &edge, const ExpectedEnd &expected) {
  REQUIRE(end.Edge.Carrier == edge.Carrier);
  REQUIRE(end.Edge.From == edge.From);
  REQUIRE(end.Edge.To == edge.To);
  REQUIRE(end.Edge.FromDistance == edge.FromDistance);
  REQUIRE(end.Edge.ToDistance == edge.ToDistance);
  REQUIRE(end.AtFrom == expected.AtFrom);
  requireAlong(end.Along, expected.Along);
}

void requireNodeSample(const NodeSample &sample, const Network &network, uint32_t node,
                       const std::vector<double> &atNode, const std::vector<ExpectedEnd> &ends) {
  REQUIRE(sample.Node == node);
  REQUIRE(sample.AtNode == atNode);
  REQUIRE(sample.Ends.size() == ends.size());
  for (size_t i = 0; i < ends.size(); ++i) {
    CAPTURE(i);
    requireNodeEnd(sample.Ends[i], network.edges()[ends[i].Edge], ends[i]);
  }
}

// The standard layout's edges are 0, A from node 0 to 1; 1, A from node 1 to 2; 2, B from node 1
// to 3; and 3, C from node 3 back to 3.
TEST_CASE("at a node, a kept field's node rule is given every end of every edge meeting the node, "
          "each with the source's entries strictly inside that edge, and what it returns is the "
          "source's sample there") {
  World world = makeKeptWorld(makeKeptSchema());
  // Inside the loop C and at node 0, so it meets node 3 and not node 1.
  const EntityKey loopOnly = world.createEntity();
  const EntityKey everywhere = world.createEntity();
  // Only inside A's second edge, so it has nothing at node 1 itself.
  const EntityKey edgeOnly = world.createEntity();
  plannedKeeps<Crowding>() = {
      {.Tick = 0, .Source = loopOnly, .At = place(CARRIER_C, 4), .Value = 512.0},
      {.Tick = 0, .Source = loopOnly, .At = place(CARRIER_A, 0), .Value = 1024.0},
      {.Tick = 0, .Source = everywhere, .At = JUNCTION, .Value = 2.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_A, 2), .Value = 4.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_B, 0), .Value = 8.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_A, 8), .Value = 16.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_B, 3), .Value = 32.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_A, 6), .Value = 64.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_A, 10), .Value = 128.0},
      {.Tick = 0, .Source = everywhere, .At = place(CARRIER_C, 4), .Value = 256.0},
      {.Tick = 0, .Source = edgeOnly, .At = place(CARRIER_A, 7), .Value = 2048.0},
  };
  // Read one tick after the entries' tick, so each read value is half the held one.
  stepWorld(world);
  stepWorld(world);
  const Network &network = networkOf(world);

  nodeCalls().clear();
  REQUIRE(sampled<Crowding>(world, JUNCTION) == Sampled{{everywhere, 63.0}, {edgeOnly, 1024.0}});
  REQUIRE(nodeCalls().size() == 2);
  requireNodeSample(nodeCalls()[0], network, 1, {1.0, 4.0},
                    {{.Edge = 0, .AtFrom = false, .Along = {{2.0, 2.0, 2.0}}},
                     {.Edge = 1, .AtFrom = true, .Along = {{4.0, 2.0, 8.0}, {2.0, 4.0, 32.0}}},
                     {.Edge = 2, .AtFrom = true, .Along = {{3.0, 3.0, 16.0}}}});
  requireNodeSample(nodeCalls()[1], network, 1, {},
                    {{.Edge = 0, .AtFrom = false, .Along = {}},
                     {.Edge = 1, .AtFrom = true, .Along = {{3.0, 3.0, 1024.0}}},
                     {.Edge = 2, .AtFrom = true, .Along = {}}});

  // Node 3 meets B's To end and both ends of the loop C.
  nodeCalls().clear();
  REQUIRE(sampled<Crowding>(world, place(CARRIER_B, 6)) ==
          Sampled{{loopOnly, 512.0}, {everywhere, 272.0}});
  REQUIRE(nodeCalls().size() == 2);
  requireNodeSample(nodeCalls()[0], network, 3, {},
                    {{.Edge = 2, .AtFrom = false, .Along = {}},
                     {.Edge = 3, .AtFrom = true, .Along = {{4.0, 4.0, 256.0}}},
                     {.Edge = 3, .AtFrom = false, .Along = {{4.0, 4.0, 256.0}}}});
  requireNodeSample(nodeCalls()[1], network, 3, {},
                    {{.Edge = 2, .AtFrom = false, .Along = {{3.0, 3.0, 16.0}}},
                     {.Edge = 3, .AtFrom = true, .Along = {{4.0, 4.0, 128.0}}},
                     {.Edge = 3, .AtFrom = false, .Along = {{4.0, 4.0, 128.0}}}});
}

// The first key the counter gives after the layout's.
constexpr EntityKey FIRST_SOURCE{2};

// Registered after the keepers, so the tick already holds a change when the refused call is made.
void refuseNaNKeep(World &world) {
  const World before = copyWorld(world);
  const bool threw = throwsInvalidArgument(
      [&] { keepEntry<Litter>(world, FIRST_SOURCE, place(CARRIER_A, NOT_A_NUMBER), 1.0); });
  refusals().push_back({.Threw = threw, .Unchanged = worldsEqual(world, before)});
}

TEST_CASE("a kept field call that throws changes nothing") {
  auto schema = makeKeptSchema();
  schema->addSystem(refuseNaNKeep);
  World world = makeKeptWorld(schema);
  const EntityKey source = world.createEntity();
  REQUIRE(source == FIRST_SOURCE);
  plannedKeeps<Litter>() = {{.Tick = 0, .Source = FIRST_SOURCE, .At = WATCHED, .Value = 8.0}};

  stepWorld(world);

  REQUIRE(refusals() == std::vector<Refusal>{{.Threw = true, .Unchanged = true}});
}

TEST_CASE("replaceKeptEntries, called by a finisher, makes the source's held entries exactly the "
          "ones given, and an empty list removes the source") {
  World world = makeKeptWorld(makeKeptSchema());
  const EntityKey replaced = world.createEntity();
  const EntityKey removed = world.createEntity();
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = replaced, .At = WATCHED, .Value = 8.0},
      {.Tick = 0, .Source = removed, .At = place(CARRIER_A, 2), .Value = 4.0},
  };
  stepWorld(world);

  // The finisher runs at tick 2, after the commanded cycle's swap. Not in place order.
  const std::vector<KeptEntry> replacement = {
      kept(CARRIER_B, 3, 1.0, 2), kept(CARRIER_A, 2, 2.0, 0), kept(CARRIER_A, 7, 3.0, 1)};
  plannedReplacements() = {{replaced, replacement}, {removed, {}}};
  CommandQueue queue;
  queue.push(Nudge{});
  stepWorld(world, queue);
  REQUIRE(plannedReplacements().empty());

  requireSameEntries(held<Litter>(world, replaced), replacement);
  REQUIRE(held<Litter>(world, removed).empty());
  REQUIRE(sampled<Litter>(world, place(CARRIER_A, 2)) == Sampled{{replaced, 0.5}});
}

// A load leaves behind the order by place kept beside a source's entries, so a change in the
// loaded world must find the place it already holds again.
TEST_CASE("in a world loaded from a save, a change at a held place leaves one entry there, with "
          "the last value given and the tick of the swap that applied it") {
  const auto schema = makeKeptSchema();
  World world = makeKeptWorld(schema);
  const EntityKey source = world.createEntity();
  // Kept out of place order, so the order by place differs from the order first kept.
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 7), .Value = 1.0},
      {.Tick = 0, .Source = source, .At = place(CARRIER_A, 2), .Value = 2.0},
      {.Tick = 1, .Source = source, .At = place(CARRIER_A, 2), .Value = 3.0},
  };
  stepWorld(world);
  World loaded = loadWorld(schema, saveWorld(world));

  stepWorld(loaded);

  requireSameEntries(held<Litter>(loaded, source),
                     {kept(CARRIER_A, 7, 1.0, 1), kept(CARRIER_A, 2, 3.0, 2)});
}

TEST_CASE("a saved world holding kept entries, loaded and resolved, reads exactly as the world "
          "saved, and reading it never changes its save") {
  const auto schema = makeKeptSchema();
  World world = makeKeptWorld(schema);
  const EntityKey first = world.createEntity();
  const EntityKey second = world.createEntity();
  plannedKeeps<Litter>() = {
      {.Tick = 0, .Source = second, .At = JUNCTION, .Value = 8.0},
      {.Tick = 0, .Source = first, .At = WATCHED, .Value = 16.0},
      {.Tick = 1, .Source = first, .At = place(CARRIER_B, 0), .Value = 4.0},
      {.Tick = 1, .Source = second, .At = WATCHED, .Value = 2.0},
  };
  plannedKeeps<Crowding>() = {
      {.Tick = 0, .Source = first, .At = place(CARRIER_A, 6), .Value = 32.0},
      {.Tick = 0, .Source = first, .At = place(CARRIER_B, 3), .Value = 64.0},
  };
  stepWorld(world);
  stepWorld(world);
  stepWorld(world);

  const std::string text = saveWorld(world);
  REQUIRE(text.find("[litter-kept]") != std::string::npos);
  REQUIRE(text.find("[crowding-kept]") != std::string::npos);
  World loaded = loadWorld(schema, text);
  resolveWorld(loaded);
  requireSameValue(loaded, world);

  REQUIRE(sampledBits<Litter>(loaded, JUNCTION) == sampledBits<Litter>(world, JUNCTION));
  REQUIRE(sampledBits<Litter>(loaded, WATCHED) == sampledBits<Litter>(world, WATCHED));
  REQUIRE(sampledBits<Crowding>(loaded, JUNCTION) == sampledBits<Crowding>(world, JUNCTION));
  REQUIRE(sampledBits<Crowding>(loaded, WATCHED) == sampledBits<Crowding>(world, WATCHED));
  REQUIRE(keptValue<Litter>(loaded, first, WATCHED) == keptValue<Litter>(world, first, WATCHED));
  REQUIRE_FALSE(sampled<Litter>(loaded, WATCHED).empty());
  REQUIRE_FALSE(sampled<Crowding>(loaded, JUNCTION).empty());

  REQUIRE(saveWorld(loaded) == text);
  requireSameValue(loaded, world);
}

} // namespace
} // namespace tpj
