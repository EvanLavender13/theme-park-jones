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

// Whether the world's guests' records showed a Position, and a record without one.
struct Placement {
  bool Placed = false;
  bool Unplaced = false;
};

// appendGuest for each guest whose record has a Position, in key order, noting what it saw.
ParkMesh expectedGuestMesh(const World &world, Placement &seen) {
  ParkMesh expected;
  for (const EntityKey guest : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    REQUIRE(record.has_value());
    const GuestRecord held = record.value_or(GuestRecord{});
    if (const std::optional<GroundPoint> position = held.Position) {
      appendGuest(expected, *position, held.Hunger);
    }
    seen.Placed = seen.Placed || held.Position.has_value();
    seen.Unplaced = seen.Unplaced || !held.Position.has_value();
  }
  return expected;
}

TEST_CASE("buildGuestMesh adds appendGuest for each guest whose record has a Position, in key "
          "order, at that Position with its Hunger, and nothing else") {
  // The new park after two arrivals, so key order matters, with every guest on its path.
  World walking = makeNewPark(1);
  while (walking.Tick < (2 * ARRIVAL_INTERVAL) + 1) {
    stepWorld(walking);
  }
  Placement walkingSeen;
  const ParkMesh walkingExpected = expectedGuestMesh(walking, walkingSeen);
  REQUIRE(walkingSeen.Placed);
  CHECK(sameMesh(buildGuestMesh(walking), walkingExpected));

  // The same park through a cycle deleting every guest path: its guests have no Position until
  // the next cycle, when they leave.
  World stranded = copyWorld(walking);
  CommandQueue cut;
  for (const ParkPath &path : parkPaths(stranded)) {
    if (path.Kind == PathKind::Guest) {
      cut.push(DeletePath{path.Key});
    }
  }
  stepWorld(stranded, cut);
  Placement strandedSeen;
  const ParkMesh strandedExpected = expectedGuestMesh(stranded, strandedSeen);
  REQUIRE(strandedSeen.Unplaced);
  CHECK(sameMesh(buildGuestMesh(stranded), strandedExpected));

  // A park no guest has entered yet draws none.
  World empty = makeNewPark(1);
  resolveWorld(empty);
  const ParkMesh none = buildGuestMesh(empty);
  CHECK(none.Vertices.empty());
  CHECK(none.Indices.empty());
}

} // namespace
} // namespace tpj
