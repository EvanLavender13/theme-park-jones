#include "render/guest_mesh.h"
#include "render/park_mesh.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

bool sameVertex(const ParkVertex &left, const ParkVertex &right) {
  return std::ranges::equal(left.Position, right.Position) &&
         std::ranges::equal(left.Normal, right.Normal) && left.Color == right.Color;
}

bool sameMesh(const ParkMesh &left, const ParkMesh &right) {
  return std::ranges::equal(left.Vertices, right.Vertices, sameVertex) &&
         left.Indices == right.Indices;
}

float channel(float sated, float hungry, float hunger) {
  return sated + ((hungry - sated) * hunger);
}

TEST_CASE("guestColor moves each of red, green, and blue from the sated color toward the hungry "
          "one by the hunger clamped to [0, 1], with alpha 1") {
  // Fully sated, fully hungry, between, and beyond each end.
  const std::vector<std::pair<double, float>> hungers{
      {0.0, 0.0f}, {1.0, 1.0f}, {0.25, 0.25f}, {-0.5, 0.0f}, {1.5, 1.0f}};
  const Rgba sated = GUEST_SATED_COLOR;
  const Rgba hungry = GUEST_HUNGRY_COLOR;
  for (const auto &[hunger, clamped] : hungers) {
    CAPTURE(hunger);
    const Rgba color = guestColor(hunger);
    CHECK(color.R == channel(sated.R, hungry.R, clamped));
    CHECK(color.G == channel(sated.G, hungry.G, clamped));
    CHECK(color.B == channel(sated.B, hungry.B, clamped));
    CHECK(color.A == 1.0f);
  }
}

TEST_CASE("appendGuest adds exactly what appendBox adds for an upright box of GUEST_SIZE and "
          "GUEST_HEIGHT at the point with the default facing, in guestColor of the hunger") {
  // A mesh already holding a box, so the guest's indices must name the vertices it adds.
  ParkMesh guest;
  appendBox(guest, Pose{10.0, 4.0, 1.0, 0.0}, FootprintSize{2.0, 3.0}, 1.0f, ENTRANCE_COLOR);
  ParkMesh box = guest;
  const GroundPoint point{3.5, -7.25};
  appendGuest(guest, point, 0.6);
  Pose standing;
  standing.X = point.X;
  standing.Z = point.Z;
  appendBox(box, standing, GUEST_SIZE, GUEST_HEIGHT, guestColor(0.6));
  CHECK(sameMesh(guest, box));
}

// The new park with a second guest path running on from its path's far end, stepped until its
// first guest walks the second path, and then through a cycle deleting that path. Guests on the
// second path have no Position until the next cycle, and the guests behind them still have one.
World partlyStrandedWorld() {
  World world = makeNewPark(1);
  CommandQueue extend;
  extend.push(AddPath{PathKind::Guest, {{0.0, 103.0}, {0.0, 63.0}}});
  const EntityKey farPath{world.nextKey()};
  stepWorld(world, extend);
  REQUIRE(parkPaths(world).size() == 2);
  bool onFarPath = false;
  for (int cycle = 0; cycle < 1000 && !onFarPath; ++cycle) {
    stepWorld(world);
    const std::vector<EntityKey> guests = parkGuests(world);
    onFarPath = !guests.empty() &&
                guestRecord(world, guests.front()).value_or(GuestRecord{}).At.Carrier == farPath;
  }
  REQUIRE(onFarPath);
  CommandQueue cut;
  cut.push(DeletePath{farPath});
  stepWorld(world, cut);
  return world;
}

TEST_CASE("buildGuestMesh adds appendGuest for each guest whose record has a Position, in key "
          "order, at that Position with its Hunger, and nothing else") {
  const World world = partlyStrandedWorld();
  ParkMesh expected;
  bool sawPlaced = false;
  bool sawUnplaced = false;
  for (const EntityKey guest : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    REQUIRE(record.has_value());
    const GuestRecord held = record.value_or(GuestRecord{});
    if (const std::optional<GroundPoint> position = held.Position) {
      appendGuest(expected, *position, held.Hunger);
    }
    sawPlaced = sawPlaced || held.Position.has_value();
    sawUnplaced = sawUnplaced || !held.Position.has_value();
  }
  REQUIRE(sawPlaced);
  REQUIRE(sawUnplaced);
  CHECK(sameMesh(buildGuestMesh(world), expected));

  // A park no guest has entered yet draws none.
  World empty = makeNewPark(1);
  resolveWorld(empty);
  const ParkMesh none = buildGuestMesh(empty);
  CHECK(none.Vertices.empty());
  CHECK(none.Indices.empty());
}

} // namespace
} // namespace tpj
