// Drawing a guest path across a street splits it without moving the guests on it.

#include "support/park_files.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/routes/networks.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <stddef.h>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;
using test::recordOf;

// The guests whose place lies on the carrier, in key order.
std::vector<EntityKey> guestsOn(const World &world, EntityKey carrier) {
  std::vector<EntityKey> on;
  for (const EntityKey guest : parkGuests(world)) {
    if (recordOf(world, guest).At.Carrier == carrier) {
      on.push_back(guest);
    }
  }
  return on;
}

// The main street: of the widest guest segments, the one most guests stand on.
ParkSegment mainStreet(const World &world) {
  double widest = 0.0;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Guest) {
      widest = std::max(widest, segment.Width);
    }
  }
  std::optional<ParkSegment> street;
  size_t most = 0;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind != PathKind::Guest || segment.Width != widest) {
      continue;
    }
    const size_t count = guestsOn(world, segment.Key).size();
    if (!street.has_value() || count > most) {
      street = segment;
      most = count;
    }
  }
  REQUIRE(street.has_value());
  return street.value_or(ParkSegment{});
}

bool onGuestPaving(const PavedSurface &surface, GroundPoint point) {
  return std::ranges::any_of(surface.Pieces, [&](const SurfacePiece &piece) {
    return piece.Kind == PathKind::Guest &&
           pieceDistance(piece, ParkPoint{point.X, point.Z}) == 0.0;
  });
}

// Catches carry-over that moves a guest on the split-off half, whose ground now lies under a new
// segment key, to the split point or anywhere else.
TEST_CASE("A guest path drawn across the main street leaves every guest on it where it stood") {
  World unchanged = openPark(parkFile("warm-main-street.park"));
  const ParkSegment street = mainStreet(unchanged);
  const Network &network = parkNetwork(unchanged, PathKind::Guest);

  // The crossing goes in the middle of the widest gap between guests along the street, so guests
  // stand on both halves the split makes.
  std::vector<double> along;
  for (const EntityKey guest : guestsOn(unchanged, street.Key)) {
    along.push_back(recordOf(unchanged, guest).At.Distance);
  }
  std::ranges::sort(along);
  REQUIRE(along.size() >= 2);
  size_t widestGap = 0;
  for (size_t i = 1; i + 1 < along.size(); ++i) {
    if (along[i + 1] - along[i] > along[widestGap + 1] - along[widestGap]) {
      widestGap = i;
    }
  }
  const double crossingDistance = (along[widestGap] + along[widestGap + 1]) / 2.0;
  const Place crossingPlace{street.Key, crossingDistance};
  const std::optional<GroundPoint> centre = network.groundPoint(crossingPlace);
  const std::optional<GroundDirection> direction = network.directionAt(crossingPlace);
  REQUIRE(centre.has_value());
  REQUIRE(direction.has_value());
  const GroundPoint at = centre.value_or(GroundPoint{});
  const GroundDirection way = direction.value_or(GroundDirection{});

  // A short path square across the street whose ends lie inside the street's paving, so it meets
  // nothing but the street.
  const double reach = street.Width / 4.0;
  const AddPath across{PathKind::Guest,
                       {NewPoint{{at.X - (way.Z * reach), at.Z + (way.X * reach)}},
                        NewPoint{{at.X + (way.Z * reach), at.Z - (way.X * reach)}}}};
  REQUIRE(isAccepted(unchanged, across));
  REQUIRE(pathCrossings(unchanged, across).size() == 1);

  World changed = copyWorld(unchanged);
  CommandQueue crossing;
  crossing.push(across);
  stepWorld(unchanged);
  stepWorld(changed, crossing);
  REQUIRE(changed.Tick == unchanged.Tick);

  // The street keeps its key up to the crossing, and a new segment runs on from there.
  const std::vector<ParkSegment> segmentsNow = parkSegments(changed);
  const auto kept = std::ranges::find_if(
      segmentsNow, [&](const ParkSegment &segment) { return segment.Key == street.Key; });
  REQUIRE(kept != segmentsNow.end());
  REQUIRE(kept->To != street.To);
  REQUIRE(std::ranges::any_of(segmentsNow, [&](const ParkSegment &segment) {
    return segment.Key != street.Key && segment.From == kept->To && segment.To == street.To;
  }));

  const std::vector<EntityKey> onStreet = guestsOn(unchanged, street.Key);
  REQUIRE(std::ranges::any_of(onStreet, [&](EntityKey guest) {
    return recordOf(unchanged, guest).At.Distance > crossingDistance;
  }));
  REQUIRE(std::ranges::any_of(onStreet, [&](EntityKey guest) {
    return recordOf(unchanged, guest).At.Distance < crossingDistance;
  }));
  const PavedSurface paving = parkSurface(changed, connectorWalkways(changed, PathKind::Guest));
  for (const EntityKey guest : onStreet) {
    const std::optional<GroundPoint> stood = recordOf(unchanged, guest).Position;
    const std::optional<GroundPoint> stands = recordOf(changed, guest).Position;
    REQUIRE(stood.has_value());
    REQUIRE(stands.has_value());
    CHECK(stands == stood);
    CHECK(onGuestPaving(paving, stands.value_or(GroundPoint{})));
  }
}

} // namespace
} // namespace tpj
