#include "support/synthetic_fields.h"

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

using test::addSource;
using test::CARRIER_A;
using test::CARRIER_B;
using test::CARRIER_C;
using test::Emits;
using test::entryAt;
using test::Footfall;
using test::LAYOUT_KEY;
using test::makeFieldSchema;
using test::makeFieldWorld;
using test::networkOf;
using test::PublishOrder;
using test::PublishOrderScope;
using test::Reach;
using test::ReachCall;
using test::reachCalls;

using Sampled = std::vector<std::pair<EntityKey, double>>;

constexpr Place place(EntityKey carrier, double distance) {
  return {.Carrier = carrier, .Distance = distance};
}

template <typename F>
Sampled sampledOn(const World &world, const Network &network, const Place &at) {
  Sampled result;
  for (const SampledEntry<double> &entry : sampleField<F>(world, network, at)) {
    result.emplace_back(entry.Source, entry.Value);
  }
  return result;
}

// Sampled on the world's own network.
template <typename F> Sampled sampled(const World &world, const Place &at) {
  return sampledOn<F>(world, networkOf(world), at);
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

// What recording resolvers and systems saw. They take only the world, so the record lives outside
// it.
std::vector<Thrown> &outcomes() {
  static std::vector<Thrown> recorded;
  return recorded;
}

std::vector<std::string> &journal() {
  static std::vector<std::string> recorded;
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

bool hasResolver(const WorldSchema &schema, std::string_view name) {
  return std::ranges::any_of(schema.resolvers(),
                             [name](const ResolverType &type) { return type.Name == name; });
}

EdgePosition edgePositionOf(const Network &network, const Place &at) {
  const std::optional<NetworkPosition> position = network.resolve(at);
  const auto *edge = position.has_value() ? std::get_if<EdgePosition>(&*position) : nullptr;
  REQUIRE(edge != nullptr);
  return *edge;
}

void requireSameEdge(const NetworkEdge &actual, const NetworkEdge &expected) {
  REQUIRE(actual.Carrier == expected.Carrier);
  REQUIRE(actual.From == expected.From);
  REQUIRE(actual.To == expected.To);
  REQUIRE(actual.FromDistance == expected.FromDistance);
  REQUIRE(actual.ToDistance == expected.ToDistance);
}

// An entry inside an edge as the rule should receive it: its offsets as resolve gives them.
struct ExpectedAlong {
  Place At;
  double Value = 0.0;
};

void requireAlong(const Network &network, const std::vector<EdgeEntry<double>> &actual,
                  const std::vector<ExpectedAlong> &expected) {
  REQUIRE(actual.size() == expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    CAPTURE(i);
    const EdgePosition position = edgePositionOf(network, expected[i].At);
    REQUIRE(std::bit_cast<uint64_t>(actual[i].FromOffset) ==
            std::bit_cast<uint64_t>(position.FromOffset));
    REQUIRE(std::bit_cast<uint64_t>(actual[i].ToOffset) ==
            std::bit_cast<uint64_t>(position.ToOffset));
    REQUIRE(actual[i].Value == expected[i].Value);
  }
}

TEST_CASE("addField registers the field's derived component as <name>-resolved and its resolver "
          "as <name>-field") {
  const auto schema = makeFieldSchema();

  for (const std::string_view name : {"footfall", "reach"}) {
    CAPTURE(name);
    const ComponentType *resolved = componentNamed(*schema, std::string(name) + "-resolved");
    REQUIRE(resolved != nullptr);
    REQUIRE(resolved->Kind == DataKind::Derived);
    REQUIRE(hasResolver(*schema, std::string(name) + "-field"));
  }
}

TEST_CASE("isResolving is true while a resolver runs and false otherwise") {
  auto schema = std::make_shared<WorldSchema>();
  schema->addSystem([](World &world) {
    journal().push_back(std::string("system ") + (world.isResolving() ? "true" : "false"));
  });
  schema->addResolver("watch", [](World &world) {
    journal().push_back(std::string("resolver ") + (world.isResolving() ? "true" : "false"));
  });
  World world(schema, 0);
  REQUIRE_FALSE(world.isResolving());

  journal().clear();
  stepWorld(world);

  REQUIRE(journal() == std::vector<std::string>{"resolver true", "system false"});
  REQUIRE_FALSE(world.isResolving());
}

TEST_CASE("entries a resolver publishes are sampled at once, by later resolvers and after the "
          "resolution") {
  auto schema = makeFieldSchema();
  schema->addResolver(
      "observer",
      [](World &world) { observed().push_back(sampled<Footfall>(world, place(CARRIER_A, 4))); },
      {"footfall-sources"});
  World world = makeFieldWorld(schema);
  const EntityKey source =
      addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.5), entryAt(CARRIER_B, 0, 2.5)});
  const Sampled expected = {{source, 1.5}, {source, 2.5}};

  observed().clear();
  resolveWorld(world);

  REQUIRE(observed() == std::vector<Sampled>{expected});
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 4)) == expected);
}

// Publishes into footfall for the layout's key twice, the first time with the given entries.
template <size_t FirstCount> void publishTwice(World &world) {
  std::vector<PlacedEntry<double>> first;
  if constexpr (FirstCount > 0) {
    first.push_back(entryAt(CARRIER_A, 4, 1.0));
  }
  outcomes().push_back(
      thrownBy([&] { publishResolved<Footfall>(world, LAYOUT_KEY, std::move(first)); }));
  outcomes().push_back(thrownBy(
      [&] { publishResolved<Footfall>(world, LAYOUT_KEY, {entryAt(CARRIER_A, 4, 2.0)}); }));
}

// The refusal is per resolution: the next resolution accepts the source's first publication again.
TEST_CASE("publishResolved throws std::invalid_argument for a source that has already published "
          "into the field in the same resolution, even with no entries") {
  auto schema = std::make_shared<WorldSchema>();
  addField<Footfall>(*schema);
  SECTION("a first publication with entries") {
    schema->addResolver("twice-publisher", publishTwice<1>, {"footfall-field"});
  }
  SECTION("a first publication with no entries") {
    schema->addResolver("twice-publisher", publishTwice<0>, {"footfall-field"});
  }
  World world(schema, 0);
  world.createEntity();

  outcomes().clear();
  resolveWorld(world);
  resolveWorld(world);

  REQUIRE(outcomes() == std::vector<Thrown>{Thrown::Nothing, Thrown::InvalidArgument,
                                            Thrown::Nothing, Thrown::InvalidArgument});
}

TEST_CASE("a publishResolved that throws leaves the field's entries unchanged") {
  SECTION("a repeated source keeps its first entries, and a null source adds none") {
    auto schema = std::make_shared<WorldSchema>();
    test::addSyntheticNetwork(*schema);
    addField<Footfall>(*schema);
    schema->addResolver("refused-publisher",
                        [](World &world) {
                          publishTwice<1>(world);
                          outcomes().push_back(thrownBy([&] {
                            publishResolved<Footfall>(world, NULL_KEY,
                                                      {entryAt(CARRIER_A, 4, 3.0)});
                          }));
                        },
                        {"footfall-field"});
    World world = makeFieldWorld(schema);

    outcomes().clear();
    resolveWorld(world);

    REQUIRE(outcomes() ==
            std::vector<Thrown>{Thrown::Nothing, Thrown::InvalidArgument, Thrown::InvalidArgument});
    REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 4)) == Sampled{{LAYOUT_KEY, 1.0}});
  }
  SECTION("outside resolution, the world is unchanged") {
    World world = makeFieldWorld(makeFieldSchema());
    const EntityKey source = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
    resolveWorld(world);
    const World before = copyWorld(world);

    REQUIRE(thrownBy([&] {
              publishResolved<Footfall>(world, source, {entryAt(CARRIER_A, 4, 2.0)});
            }) == Thrown::OtherLogicError);
    requireSameValue(world, before);
  }
}

TEST_CASE("every resolution replaces every source's resolved entries in full") {
  World world = makeFieldWorld(makeFieldSchema());
  const EntityKey kept =
      addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0), entryAt(CARRIER_A, 4, 2.0)});
  const EntityKey dropped = addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 5.0)});
  resolveWorld(world);
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 4)) ==
          Sampled{{kept, 1.0}, {kept, 2.0}, {dropped, 5.0}});

  // One source publishes different entries, and the other stops publishing.
  world.Registry.get<Emits<Footfall>>(world.findEntity(kept)).Entries = {
      entryAt(CARRIER_A, 4, 3.0)};
  world.Registry.remove<Emits<Footfall>>(world.findEntity(dropped));
  resolveWorld(world);

  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 4)) == Sampled{{kept, 3.0}});
}

// Three sources, each with entries whose values are in neither ascending nor descending order, so
// only publication order explains the order within a source.
World threeSourceWorld() {
  World world = makeFieldWorld(makeFieldSchema());
  addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 7.0)});
  addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 5.0), entryAt(CARRIER_B, 0, 3.0)});
  addSource<Footfall>(world, {entryAt(CARRIER_B, 0, 1.0), entryAt(CARRIER_A, 4, 9.0)});
  return world;
}

const std::vector<PublishOrder> PUBLISH_ORDERS = {PublishOrder::Ascending, PublishOrder::Descending,
                                                  PublishOrder::Rotated};

TEST_CASE("publishing the same sources in different orders gives the same world and the same "
          "hash") {
  std::vector<World> worlds;
  for (const PublishOrder order : PUBLISH_ORDERS) {
    const PublishOrderScope scope(order);
    worlds.push_back(threeSourceWorld());
    resolveWorld(worlds.back());
  }
  REQUIRE_FALSE(sampled<Footfall>(worlds.front(), place(CARRIER_A, 4)).empty());

  requireSameValue(worlds[0], worlds[1]);
  requireSameValue(worlds[0], worlds[2]);
}

TEST_CASE("at a place that resolves to a node, a source's sampled entries are those whose places "
          "resolve to the same node, whatever carrier they name") {
  World world = makeFieldWorld(makeFieldSchema());
  // Node 1 is A at 4 and B at 0; node 3 is B at 6 and C at both 0 and 8.
  const EntityKey source = addSource<Footfall>(world, {
                                                          entryAt(CARRIER_A, 4, 1.0),
                                                          entryAt(CARRIER_A, 2, 2.0),
                                                          entryAt(CARRIER_B, 0, 3.0),
                                                          entryAt(CARRIER_A, 0, 4.0),
                                                          entryAt(CARRIER_B, 6, 5.0),
                                                          entryAt(CARRIER_C, 0, 6.0),
                                                      });
  resolveWorld(world);

  const Sampled atJunction = {{source, 1.0}, {source, 3.0}};
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 4)) == atJunction);
  REQUIRE(sampled<Footfall>(world, place(CARRIER_B, 0)) == atJunction);
  // A place no entry names, at the end of a carrier that starts at the same node.
  REQUIRE(sampled<Footfall>(world, place(CARRIER_C, 8)) == Sampled{{source, 5.0}, {source, 6.0}});
}

TEST_CASE("at a place that resolves to a node, a field with sampleEdge gives the entries at the "
          "same node, and never calls sampleEdge") {
  World world = makeFieldWorld(makeFieldSchema());
  // Entries at node 1 through both carriers that meet there, and inside both of A's edges.
  const EntityKey source = addSource<Reach>(world, {
                                                       entryAt(CARRIER_A, 4, 1.0),
                                                       entryAt(CARRIER_A, 2, 2.0),
                                                       entryAt(CARRIER_B, 0, 3.0),
                                                       entryAt(CARRIER_A, 4.3, 4.0),
                                                   });
  resolveWorld(world);

  for (const Place at : {place(CARRIER_A, 4), place(CARRIER_B, 0)}) {
    CAPTURE(at.Carrier);
    reachCalls().clear();
    REQUIRE(sampled<Reach>(world, at) == Sampled{{source, 1.0}, {source, 3.0}});
    REQUIRE(reachCalls().empty());
  }
}

TEST_CASE("at a place strictly inside an edge, a field without sampleEdge gives the entries whose "
          "places equal it") {
  World world = makeFieldWorld(makeFieldSchema());
  const EntityKey source = addSource<Footfall>(world, {
                                                          entryAt(CARRIER_A, 2, 1.0),
                                                          entryAt(CARRIER_A, 2.5, 2.0),
                                                          entryAt(CARRIER_A, 0, 3.0),
                                                          entryAt(CARRIER_A, 4, 4.0),
                                                          entryAt(CARRIER_A, 2, 5.0),
                                                          entryAt(CARRIER_B, 2, 6.0),
                                                      });
  resolveWorld(world);

  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 2)) == Sampled{{source, 1.0}, {source, 5.0}});
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 2.5)) == Sampled{{source, 2.0}});
  // The same distance on another carrier is another place.
  REQUIRE(sampled<Footfall>(world, place(CARRIER_B, 2)) == Sampled{{source, 6.0}});
}

TEST_CASE("a place that does not resolve on the network gives no entries") {
  World world = makeFieldWorld(makeFieldSchema());
  // Entries at the unresolvable places themselves, beside one that does resolve.
  addSource<Footfall>(world, {entryAt(CARRIER_A, 11, 1.0), entryAt(EntityKey{99}, 1, 2.0),
                              entryAt(CARRIER_A, 4, 3.0)});
  resolveWorld(world);

  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 11)).empty());
  REQUIRE(sampled<Footfall>(world, place(EntityKey{99}, 1)).empty());
  // A network lacking the carrier.
  REQUIRE(sampledOn<Footfall>(world, Network{}, place(CARRIER_A, 4)).empty());
}

TEST_CASE("an entry whose place does not resolve on the network is never sampled") {
  World world = makeFieldWorld(makeFieldSchema());
  // Just beyond each end of A, and on a carrier the network lacks.
  addSource<Footfall>(world, {entryAt(CARRIER_A, -1, 1.0), entryAt(CARRIER_A, 11, 2.0),
                              entryAt(EntityKey{99}, 0, 3.0)});
  addSource<Reach>(world, {entryAt(CARRIER_A, -1, 4.0), entryAt(EntityKey{99}, 0, 5.0),
                           entryAt(CARRIER_A, 3, 7.0)});
  resolveWorld(world);
  const Network &network = networkOf(world);

  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 0)).empty());
  REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 10)).empty());

  reachCalls().clear();
  static_cast<void>(sampleField<Reach>(world, network, place(CARRIER_A, 2)));
  REQUIRE(reachCalls().size() == 1);
  const EdgeSample<double> &sample = reachCalls().front().Sample;
  REQUIRE(sample.AtFrom.empty());
  REQUIRE(sample.AtTo.empty());
  requireAlong(network, sample.Along, {{.At = place(CARRIER_A, 3), .Value = 7.0}});
}

TEST_CASE("a world holding no entries for a field samples as empty, with value 0.0") {
  World resolved = makeFieldWorld(makeFieldSchema());
  addSource<Footfall>(resolved, {entryAt(CARRIER_A, 4, 1.0)});
  resolveWorld(resolved);
  // A load holds the sources' intent, but no resolved entries until it is resolved.
  const World loaded = loadWorld(makeFieldSchema(), saveWorld(resolved));
  const Network &network = networkOf(resolved);

  REQUIRE(sampledOn<Footfall>(loaded, network, place(CARRIER_A, 4)).empty());
  REQUIRE(std::bit_cast<uint64_t>(fieldValue<Footfall>(loaded, network, place(CARRIER_A, 4))) ==
          std::bit_cast<uint64_t>(0.0));
}

// Around the place A at 4.3, inside A's edge from node 1 at 4 to node 2 at 10. The first source
// has entries at both nodes, one named through B, inside the edge, and elsewhere. The second has
// entries only elsewhere. The third has one entry at the place itself. They are published in
// descending key order.
struct EdgeRuleCase {
  World Resolved;
  EntityKey First = NULL_KEY;
  EntityKey Third = NULL_KEY;
};

EdgeRuleCase edgeRuleCase() {
  EdgeRuleCase result{.Resolved = makeFieldWorld(makeFieldSchema())};
  World &world = result.Resolved;
  result.First = addSource<Reach>(world, {
                                             entryAt(CARRIER_B, 0, 10.0),
                                             entryAt(CARRIER_A, 10, 11.0),
                                             entryAt(CARRIER_A, 7.1, 12.0),
                                             entryAt(CARRIER_A, 4, 13.0),
                                             entryAt(CARRIER_A, 2, 14.0),
                                             entryAt(CARRIER_A, 5.5, 15.0),
                                             entryAt(CARRIER_B, 6, 16.0),
                                         });
  addSource<Reach>(world, {entryAt(CARRIER_A, 2, 20.0), entryAt(CARRIER_B, 3, 21.0),
                           entryAt(CARRIER_A, 0, 22.0)});
  result.Third = addSource<Reach>(world, {entryAt(CARRIER_A, 4.3, 30.0)});
  const PublishOrderScope scope(PublishOrder::Descending);
  resolveWorld(world);
  return result;
}

constexpr Place EDGE_RULE_PLACE = place(CARRIER_A, 4.3);

TEST_CASE("a field's sampleEdge is called once for each source with an entry at either of the "
          "edge's nodes or inside the edge, and for no other source") {
  const EdgeRuleCase edgeCase = edgeRuleCase();

  reachCalls().clear();
  static_cast<void>(
      sampleField<Reach>(edgeCase.Resolved, networkOf(edgeCase.Resolved), EDGE_RULE_PLACE));

  // The rule is not told its source, so the calls are recognized by what they were given: the
  // first source's by its entry at node 1, the third's by its entry at the place.
  REQUIRE(reachCalls().size() == 2);
  REQUIRE(reachCalls()[0].Sample.AtFrom == std::vector<double>{10.0, 13.0});
  REQUIRE(reachCalls()[1].Sample.AtFrom.empty());
  REQUIRE(reachCalls()[1].Sample.Along.size() == 1);
  REQUIRE(reachCalls()[1].Sample.Along.front().Value == 30.0);
}

TEST_CASE("sampleEdge is given the edge, the place's offsets as resolve gives them, and the "
          "source's entries at the edge's From node, at its To node, and inside it, each in the "
          "source's order") {
  const EdgeRuleCase edgeCase = edgeRuleCase();
  const Network &network = networkOf(edgeCase.Resolved);
  const EdgePosition position = edgePositionOf(network, EDGE_RULE_PLACE);

  reachCalls().clear();
  static_cast<void>(sampleField<Reach>(edgeCase.Resolved, network, EDGE_RULE_PLACE));
  REQUIRE(reachCalls().size() == 2);

  for (const ReachCall &call : reachCalls()) {
    requireSameEdge(call.Sample.Edge, network.edges().at(position.Edge));
    REQUIRE(std::bit_cast<uint64_t>(call.Sample.FromOffset) ==
            std::bit_cast<uint64_t>(position.FromOffset));
    REQUIRE(std::bit_cast<uint64_t>(call.Sample.ToOffset) ==
            std::bit_cast<uint64_t>(position.ToOffset));
  }
  // The edge is A's from node 1 to node 2.
  REQUIRE(reachCalls()[0].Sample.Edge.Carrier == CARRIER_A);
  REQUIRE(reachCalls()[0].Sample.Edge.From == 1U);
  REQUIRE(reachCalls()[0].Sample.Edge.To == 2U);

  const EdgeSample<double> &first = reachCalls()[0].Sample;
  REQUIRE(first.AtFrom == std::vector<double>{10.0, 13.0});
  REQUIRE(first.AtTo == std::vector<double>{11.0});
  requireAlong(
      network, first.Along,
      {{.At = place(CARRIER_A, 7.1), .Value = 12.0}, {.At = place(CARRIER_A, 5.5), .Value = 15.0}});

  const EdgeSample<double> &third = reachCalls()[1].Sample;
  REQUIRE(third.AtFrom.empty());
  REQUIRE(third.AtTo.empty());
  requireAlong(network, third.Along, {{.At = EDGE_RULE_PLACE, .Value = 30.0}});
}

TEST_CASE("the entries sampleEdge returns are the source's sampled entries") {
  const EdgeRuleCase edgeCase = edgeRuleCase();

  reachCalls().clear();
  const Sampled result = sampled<Reach>(edgeCase.Resolved, EDGE_RULE_PLACE);
  REQUIRE(reachCalls().size() == 2);

  Sampled expected;
  for (const double value : reachCalls()[0].Returned) {
    expected.emplace_back(edgeCase.First, value);
  }
  for (const double value : reachCalls()[1].Returned) {
    expected.emplace_back(edgeCase.Third, value);
  }
  REQUIRE(result == expected);
}

TEST_CASE("on an edge whose two ends are one node, sampleEdge is given that node's entries as "
          "both its From and its To entries") {
  World world = makeFieldWorld(makeFieldSchema());
  // C's one edge runs from node 3 back to node 3, which B also reaches at 6.
  addSource<Reach>(world, {entryAt(CARRIER_C, 0, 1.0), entryAt(CARRIER_B, 6, 2.0),
                           entryAt(CARRIER_C, 8, 3.0), entryAt(CARRIER_C, 4, 4.0)});
  resolveWorld(world);
  const Network &network = networkOf(world);

  reachCalls().clear();
  static_cast<void>(sampleField<Reach>(world, network, place(CARRIER_C, 2)));

  REQUIRE(reachCalls().size() == 1);
  const EdgeSample<double> &sample = reachCalls().front().Sample;
  REQUIRE(sample.AtFrom == std::vector<double>{1.0, 2.0, 3.0});
  REQUIRE(sample.AtTo == sample.AtFrom);
  requireAlong(network, sample.Along, {{.At = place(CARRIER_C, 4), .Value = 4.0}});
}

double orderedSum(const Sampled &entries) {
  double sum = 0.0;
  for (const auto &entry : entries) {
    sum += entry.second;
  }
  return sum;
}

TEST_CASE("fieldValue of a scalar field is 0.0 with each entry sampleField gives added in turn, "
          "bit for bit") {
  World world = makeFieldWorld(makeFieldSchema());

  SECTION("an order-sensitive sum") {
    // 1 + 1e16 rounds back to 1e16, so the sum's order decides whether the 1 survives.
    addSource<Footfall>(world, {entryAt(CARRIER_A, 4, 1.0)});
    addSource<Footfall>(world, {entryAt(CARRIER_B, 0, 1e16)});
    addSource<Footfall>(world, {entryAt(CARRIER_A, 4, -1e16)});
    resolveWorld(world);
    const Sampled entries = sampled<Footfall>(world, place(CARRIER_A, 4));
    REQUIRE(entries.size() == 3);
    Sampled reversed = entries;
    std::ranges::reverse(reversed);
    REQUIRE(orderedSum(entries) != orderedSum(reversed));

    const double value = fieldValue<Footfall>(world, networkOf(world), place(CARRIER_A, 4));
    REQUIRE(std::bit_cast<uint64_t>(value) == std::bit_cast<uint64_t>(orderedSum(entries)));
  }
  SECTION("a sum that starts from positive zero") {
    // -0.0 alone keeps its sign, but 0.0 + -0.0 is 0.0.
    addSource<Footfall>(world, {entryAt(CARRIER_A, 4, -0.0)});
    resolveWorld(world);
    REQUIRE(sampled<Footfall>(world, place(CARRIER_A, 4)).size() == 1);

    const double value = fieldValue<Footfall>(world, networkOf(world), place(CARRIER_A, 4));
    REQUIRE(std::bit_cast<uint64_t>(value) == std::bit_cast<uint64_t>(0.0));
  }
}

} // namespace
} // namespace tpj
