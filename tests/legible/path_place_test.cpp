#include "legible/path_place.h"

#include "legible/food.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// Two guest paths meeting at a corner: one straight from the entrance, the other turning off its
// far end. A shop faces the first from 3.5 m and backs onto a backstage path served by a depot, so
// it is supplied and places on the guest paths have food to explain.
constexpr std::string_view PLACES_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 7\n"
                                         "\n[entrance]\n"
                                         "1 x=0 z=3.5 facing-x=0 facing-z=-1\n"
                                         "\n[path]\n"
                                         "2 kind=guest points=[{x=0 z=0} {x=0 z=-40}]\n"
                                         "3 kind=backstage points=[{x=12 z=-2} {x=12 z=-40}]\n"
                                         "4 kind=guest points=[{x=0 z=-40} {x=-20 z=-40}]\n"
                                         "\n[box]\n"
                                         "5 kind=shop x=6.5 z=-10 facing-x=-1 facing-z=0\n"
                                         "6 kind=depot x=12 z=-46 facing-x=0 facing-z=1\n";

constexpr std::string_view BACKSTAGE_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 2\n"
                                            "\n[path]\n"
                                            "1 kind=backstage points=[{x=0 z=0} {x=0 z=-20}]\n";

constexpr EntityKey STRAIGHT_PATH{2};
constexpr EntityKey CORNER_PATH{4};
constexpr EntityKey SHOP{5};

// Beside the straight path, halfway along it, and well within a band's width of it.
constexpr GroundPoint BESIDE_STRAIGHT{3.0, -20.0};

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITE = std::numeric_limits<double>::infinity();

World openText(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

const Network &guestNetwork(const World &world) { return parkNetwork(world, PathKind::Guest); }

// The keys of the guest paths of parkPaths that are carriers of the guest network.
std::vector<EntityKey> guestPathCarriers(const World &world) {
  const std::vector<Carrier> &carriers = guestNetwork(world).carriers();
  std::vector<EntityKey> keys;
  for (const ParkPath &path : parkPaths(world)) {
    const bool carried = std::ranges::any_of(
        carriers, [&path](const Carrier &carrier) { return carrier.Key == path.Key; });
    if (path.Kind == PathKind::Guest && carried) {
      keys.push_back(path.Key);
    }
  }
  return keys;
}

double straightDistance(GroundPoint from, GroundPoint to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  return std::sqrt(dx * dx + dz * dz);
}

// The straight distance from the point to the ground point of the place nearestPlaceOn gives on
// the carrier.
double distanceOn(const World &world, EntityKey carrier, GroundPoint point) {
  const std::optional<Place> place = guestNetwork(world).nearestPlaceOn(carrier, point);
  REQUIRE(place.has_value());
  const std::optional<GroundPoint> ground =
      guestNetwork(world).groundPoint(place.value_or(Place{}));
  REQUIRE(ground.has_value());
  return straightDistance(point, ground.value_or(GroundPoint{}));
}

// The ground point of the door where the entity's connector starts on the guest network.
GroundPoint doorOf(const World &world, EntityKey entity) {
  const std::vector<uint32_t> anchors = guestNetwork(world).anchoredNodes(entity);
  REQUIRE_FALSE(anchors.empty());
  const std::optional<GroundPoint> door =
      guestNetwork(world).groundPoint(guestNetwork(world).nodePlace(anchors.front()));
  REQUIRE(door.has_value());
  return door.value_or(GroundPoint{});
}

TEST_CASE("nearestGuestPathPlace gives the place on a guest path nearest the point, at its "
          "straight distance, with no guest path's place nearer") {
  const World world = openText(PLACES_PARK);
  const std::vector<EntityKey> carriers = guestPathCarriers(world);
  REQUIRE(carriers == std::vector<EntityKey>{STRAIGHT_PATH, CORNER_PATH});
  struct Case {
    std::string_view Name;
    GroundPoint Point;
  };
  const std::vector<Case> cases = {
      {"beside the straight path", BESIDE_STRAIGHT},
      // The higher key's path wins when it is strictly nearer.
      {"nearer the corner path than the straight one", GroundPoint{-10.0, -36.0}},
      // The shop's connector starts here, so the connector is nearer than any guest path.
      {"at the shop's guest door", doorOf(world, SHOP)},
      // The nearest place is the path's end, not a projection onto its extended line.
      {"beyond the straight path's start", GroundPoint{4.0, 6.0}},
  };
  for (const Case &test : cases) {
    INFO(test.Name);
    const std::optional<PathPlace> found = nearestGuestPathPlace(world, test.Point);
    REQUIRE(found.has_value());
    const PathPlace nearest = found.value_or(PathPlace{});
    CHECK(std::ranges::find(carriers, nearest.At.Carrier) != carriers.end());
    CHECK(guestNetwork(world).nearestPlaceOn(nearest.At.Carrier, test.Point) ==
          std::optional<Place>{nearest.At});
    const std::optional<GroundPoint> ground = guestNetwork(world).groundPoint(nearest.At);
    REQUIRE(ground.has_value());
    CHECK(nearest.Distance == straightDistance(test.Point, ground.value_or(GroundPoint{})));
    for (const EntityKey carrier : carriers) {
      INFO("guest path " << static_cast<uint64_t>(carrier));
      CHECK_FALSE(distanceOn(world, carrier, test.Point) < nearest.Distance);
    }
  }
}

TEST_CASE("nearestGuestPathPlace gives none for a point that is not finite") {
  const World world = openText(PLACES_PARK);
  CHECK_FALSE(nearestGuestPathPlace(world, GroundPoint{NOT_A_NUMBER, -20.0}).has_value());
  CHECK_FALSE(nearestGuestPathPlace(world, GroundPoint{3.0, INFINITE}).has_value());
}

TEST_CASE("nearestGuestPathPlace gives none in a world whose guest network has no guest path") {
  struct Case {
    std::string_view Name;
    World Park;
  };
  std::vector<Case> cases;
  cases.push_back({"a park with a backstage path alone", openText(BACKSTAGE_PARK)});
  // Its intent holds guest paths, but no resolution has derived the network they carry.
  cases.push_back({"a park not yet resolved", loadWorld(makeParkSchema(), PLACES_PARK)});
  for (const Case &test : cases) {
    INFO(test.Name);
    CHECK_FALSE(nearestGuestPathPlace(test.Park, BESIDE_STRAIGHT).has_value());
  }
}

TEST_CASE("foodNear gives the food availability at the nearest guest path place when the point "
          "lies within the reach, the reach included") {
  const World world = openText(PLACES_PARK);
  struct Case {
    std::string_view Name;
    GroundPoint Point;
  };
  const std::vector<Case> cases = {
      {"beside the straight path", BESIDE_STRAIGHT},
      // The connector is nearer here, and availability on it differs from the path's.
      {"at the shop's guest door", doorOf(world, SHOP)},
  };
  for (const Case &test : cases) {
    INFO(test.Name);
    const std::optional<PathPlace> found = nearestGuestPathPlace(world, test.Point);
    REQUIRE(found.has_value());
    const PathPlace nearest = found.value_or(PathPlace{});
    const FoodAvailability expected = foodAvailability(world, nearest.At);
    REQUIRE_FALSE(expected.Contributions.empty());
    CHECK(foodNear(world, test.Point, nearest.Distance) ==
          std::optional<FoodAvailability>{expected});
    CHECK(foodNear(world, test.Point, nearest.Distance + 1.0) ==
          std::optional<FoodAvailability>{expected});
  }
}

TEST_CASE("foodNear gives none beyond the reach, or where there is no nearest guest path place") {
  const World world = openText(PLACES_PARK);
  const std::optional<PathPlace> nearest = nearestGuestPathPlace(world, BESIDE_STRAIGHT);
  REQUIRE(nearest.has_value());
  const double distance = nearest.value_or(PathPlace{}).Distance;
  CHECK_FALSE(foodNear(world, BESIDE_STRAIGHT, std::nextafter(distance, 0.0)).has_value());
  CHECK_FALSE(foodNear(world, GroundPoint{NOT_A_NUMBER, -20.0}, INFINITE).has_value());
  CHECK_FALSE(foodNear(openText(BACKSTAGE_PARK), BESIDE_STRAIGHT, INFINITE).has_value());
}

} // namespace
} // namespace tpj
