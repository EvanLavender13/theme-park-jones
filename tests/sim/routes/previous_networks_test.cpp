#include "support/same_networks.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace tpj {
namespace {

using test::sameNetwork;

TEST_CASE("A world made with makeParkSchema that has stepped a cycle, and every candidate made "
          "from it, hold no previous network of either kind") {
  World world = makeNewPark(1);
  // An edit, so the cycle's second resolution re-derives networks the first one derived.
  CommandQueue extend;
  extend.push(AddPath{PathKind::Guest, {{0.0, 103.0}, {20.0, 103.0}}});
  stepWorld(world, extend);
  REQUIRE(parkPaths(world).size() == 2);
  CHECK(previousNetwork(world, PathKind::Guest) == nullptr);
  CHECK(previousNetwork(world, PathKind::Backstage) == nullptr);

  SECTION("a candidate with an edit") {
    CommandQueue cut;
    cut.push(DeletePath{parkPaths(world).back().Key});
    const World candidate = makeCandidate(world, cut);
    CHECK(previousNetwork(candidate, PathKind::Guest) == nullptr);
    CHECK(previousNetwork(candidate, PathKind::Backstage) == nullptr);
  }
  SECTION("a candidate with no commands") {
    const CommandQueue none;
    const World candidate = makeCandidate(world, none);
    CHECK(previousNetwork(candidate, PathKind::Guest) == nullptr);
    CHECK(previousNetwork(candidate, PathKind::Backstage) == nullptr);
  }
}

// What a finisher read through previousNetwork in one resolution, for each kind.
struct PreviousRead {
  std::optional<Network> Guest;
  std::optional<Network> Backstage;
};

std::vector<PreviousRead> &reads() {
  static std::vector<PreviousRead> entries;
  return entries;
}

std::optional<Network> readPrevious(const World &world, PathKind kind) {
  const Network *previous = previousNetwork(world, kind);
  return previous == nullptr ? std::nullopt : std::optional<Network>(*previous);
}

// The networks' registrations, then a finisher that records what previousNetwork gives, then the
// finisher that drops the previous networks, so the recording finisher runs between them.
std::shared_ptr<const WorldSchema> readingSchema() {
  auto schema = std::make_shared<WorldSchema>();
  addNetworkComponent(*schema);
  addParkIntent(*schema);
  addParkEdits(*schema);
  addRoutes(*schema);
  schema->addFinisher([](World &world) {
    reads().push_back(PreviousRead{.Guest = readPrevious(world, PathKind::Guest),
                                   .Backstage = readPrevious(world, PathKind::Backstage)});
  });
  addDropPreviousNetworks(*schema);
  return schema;
}

// A guest path, a backstage path, and a shop between them whose doors reach both, so both networks
// have carriers, junctions, and anchors.
void queueNetworks(CommandQueue &queue) {
  queue.push(AddPath{PathKind::Guest, {{0.0, 100.0}, {0.0, 60.0}}});
  queue.push(AddPath{PathKind::Backstage, {{12.0, 90.0}, {12.0, 70.0}}});
  queue.push(AddBox{BoxKind::Shop, Pose{6.5, 80.0, -1.0, 0.0}});
}

TEST_CASE("A finisher registered between addRoutes and addDropPreviousNetworks reads no previous "
          "network in a world's first resolution") {
  const auto schema = readingSchema();

  SECTION("a new world") {
    World world(schema, 1);
    reads().clear();
    resolveWorld(world);
    REQUIRE(reads().size() == 1);
    CHECK_FALSE(reads().front().Guest.has_value());
    CHECK_FALSE(reads().front().Backstage.has_value());
  }
  SECTION("a world loaded from the save of a world whose networks have carriers") {
    World world(schema, 1);
    CommandQueue build;
    queueNetworks(build);
    stepWorld(world, build);
    REQUIRE_FALSE(parkNetwork(world, PathKind::Guest).carriers().empty());
    REQUIRE_FALSE(parkNetwork(world, PathKind::Backstage).carriers().empty());

    World loaded = loadWorld(schema, saveWorld(world));
    reads().clear();
    resolveWorld(loaded);
    REQUIRE(reads().size() == 1);
    CHECK_FALSE(reads().front().Guest.has_value());
    CHECK_FALSE(reads().front().Backstage.has_value());
  }
}

// The networks of both kinds, copied, as a world holds them now.
PreviousRead networksOf(const World &world) {
  return PreviousRead{.Guest = parkNetwork(world, PathKind::Guest),
                      .Backstage = parkNetwork(world, PathKind::Backstage)};
}

// Checks that the one read recorded since reads() was cleared gives each kind's network as it was
// before the resolution.
void checkReadIs(const PreviousRead &before) {
  REQUIRE(reads().size() == 1);
  const PreviousRead &read = reads().front();
  REQUIRE(read.Guest.has_value());
  REQUIRE(read.Backstage.has_value());
  CHECK(sameNetwork(read.Guest.value_or(Network{}), before.Guest.value_or(Network{})));
  CHECK(sameNetwork(read.Backstage.value_or(Network{}), before.Backstage.value_or(Network{})));
}

TEST_CASE("A finisher registered between addRoutes and addDropPreviousNetworks reads, in each "
          "resolution after a world's first, each kind's network as parkNetwork gave it before "
          "that resolution") {
  const auto schema = readingSchema();
  World world(schema, 1);
  resolveWorld(world);

  // From the empty networks of the first resolution to networks with carriers and anchors.
  PreviousRead before = networksOf(world);
  CommandQueue build;
  queueNetworks(build);
  reads().clear();
  stepWorld(world, build);
  checkReadIs(before);
  REQUIRE_FALSE(parkNetwork(world, PathKind::Guest).anchoredNodes(EntityKey{3}).empty());

  // A path crossing the guest path, which splits its edge, from networks with anchors.
  before = networksOf(world);
  CommandQueue cross;
  cross.push(AddPath{PathKind::Guest, {{-10.0, 70.0}, {8.0, 70.0}}});
  reads().clear();
  stepWorld(world, cross);
  checkReadIs(before);

  // A candidate's resolution follows the world's.
  before = networksOf(world);
  CommandQueue cut;
  cut.push(DeletePath{EntityKey{2}});
  reads().clear();
  static_cast<void>(makeCandidate(world, cut));
  checkReadIs(before);
}

} // namespace
} // namespace tpj
