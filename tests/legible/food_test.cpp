#include "legible/food.h"

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// A straight guest path from an entrance, with two shops served from a depot along a backstage
// path, and between them, across the guest path, a shop with no backstage connector. So a place on
// the path reaches the entrance, which offers nothing, two supplied shops, and one starved shop.
//
// The entrance's door is 2 m from the path's start. Shops 4 and 5 face the guest path from 3.5 m
// and back onto the backstage path from 2.5 m; shop 6 faces the guest path from 3.5 m, and its back
// door is 21.5 m from the backstage path. The depot's door is 2 m from the backstage path's end.
constexpr std::string_view SHOPS_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 8\n"
                                        "\n[entrance]\n"
                                        "1 x=0 z=3.5 facing-x=0 facing-z=-1\n"
                                        "\n[path]\n"
                                        "2 kind=guest points=[{x=0 z=0} {x=0 z=-40}]\n"
                                        "3 kind=backstage points=[{x=12 z=-2} {x=12 z=-40}]\n"
                                        "\n[box]\n"
                                        "4 kind=shop x=6.5 z=-10 facing-x=-1 facing-z=0\n"
                                        "5 kind=shop x=6.5 z=-25 facing-x=-1 facing-z=0\n"
                                        "6 kind=shop x=-6.5 z=-18 facing-x=1 facing-z=0\n"
                                        "7 kind=depot x=12 z=-46 facing-x=0 facing-z=1\n";

constexpr EntityKey GUEST_PATH{2};
constexpr EntityKey FIRST_SHOP{4};
constexpr EntityKey SECOND_SHOP{5};
constexpr EntityKey STARVED_SHOP{6};

World openText(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

World openPark(std::string_view name) {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / name, std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  return openText(text);
}

const Network &guestNetwork(const World &world) { return parkNetwork(world, PathKind::Guest); }

// The first entry the source has in the guest route distance at the place.
std::optional<RouteEntry> firstRouteEntry(const World &world, const Place &place,
                                          EntityKey source) {
  for (const SampledEntry<RouteEntry> &entry :
       sampleField<RouteDistance<PathKind::Guest>>(world, guestNetwork(world), place)) {
    if (entry.Source == source) {
      return entry.Value;
    }
  }
  return std::nullopt;
}

// The source's offer, as guests find it: its first own entry in the food offer at the nodePlace of
// its lowest anchored node on the guest network.
std::optional<OfferEntry> offerOf(const World &world, EntityKey source) {
  const Network &network = guestNetwork(world);
  const std::vector<uint32_t> anchors = network.anchoredNodes(source);
  if (anchors.empty()) {
    return std::nullopt;
  }
  for (const SampledEntry<OfferEntry> &entry :
       sampleField<FoodOffer>(world, network, network.nodePlace(anchors.front()))) {
    if (entry.Source == source) {
      return entry.Value;
    }
  }
  return std::nullopt;
}

Place anchorPlace(const World &world, EntityKey entity) {
  const std::vector<uint32_t> anchors = guestNetwork(world).anchoredNodes(entity);
  REQUIRE_FALSE(anchors.empty());
  return guestNetwork(world).nodePlace(anchors.front());
}

std::vector<EntityKey> shopsOf(const FoodAvailability &availability) {
  std::vector<EntityKey> shops;
  shops.reserve(availability.Contributions.size());
  for (const FoodContribution &contribution : availability.Contributions) {
    shops.push_back(contribution.Shop);
  }
  return shops;
}

uint64_t bitsOf(double value) { return std::bit_cast<uint64_t>(value); }

TEST_CASE("Exactly the reachable shops whose offers say meals are supplied contribute, in "
          "ascending key order") {
  const World world = openText(SHOPS_PARK);
  // Places both on the guest path and on a connector: nearer the first shop than the second, and
  // at the starved shop's own door, where it is nearest of all yet offers no meals.
  const std::vector<Place> places = {Place{GUEST_PATH, 5.0}, anchorPlace(world, STARVED_SHOP)};
  for (const Place &place : places) {
    INFO("place on carrier " << static_cast<uint64_t>(place.Carrier) << " at " << place.Distance);
    CHECK(shopsOf(foodAvailability(world, place)) ==
          std::vector<EntityKey>{FIRST_SHOP, SECOND_SHOP});
  }
}

TEST_CASE("Each contribution holds its shop's route distance and offer, its wait in seconds, its "
          "effective time, and its relief discounted by that time") {
  struct Case {
    std::string_view Name;
    World Park;
    Place At;
    bool Queued = false;
  };
  // Two supplied shops at different distances, and a park whose shop has guests queued, so its
  // wait is more than zero.
  std::vector<Case> cases;
  cases.push_back({"two supplied shops, between the entrance and the first", openText(SHOPS_PARK),
                   Place{GUEST_PATH, 5.0}, false});
  cases.push_back({"warm.park, between its entrance and its shop", openPark("warm.park"),
                   Place{EntityKey{2}, 5.0}, true});
  for (const Case &test : cases) {
    INFO(test.Name);
    const FoodAvailability availability = foodAvailability(test.Park, test.At);
    REQUIRE_FALSE(availability.Contributions.empty());
    for (const FoodContribution &contribution : availability.Contributions) {
      INFO("shop " << static_cast<uint64_t>(contribution.Shop));
      const std::optional<RouteEntry> foundRoute =
          firstRouteEntry(test.Park, test.At, contribution.Shop);
      const std::optional<OfferEntry> foundOffer = offerOf(test.Park, contribution.Shop);
      REQUIRE(foundRoute.has_value());
      REQUIRE(foundOffer.has_value());
      const RouteEntry route = foundRoute.value_or(RouteEntry{});
      const OfferEntry offer = foundOffer.value_or(OfferEntry{});
      CHECK(offer.Supplied);
      CHECK(contribution.Relief == offer.Relief);
      CHECK(contribution.Distance == route.Distance);
      CHECK(contribution.Wait == static_cast<double>(offer.Wait) * SIM_TICK_SECONDS);
      CHECK(contribution.Time == contribution.Distance / REFERENCE_SPEED + contribution.Wait);
      CHECK(contribution.Term == contribution.Relief * foodDiscount(contribution.Time));
    }
    if (test.Queued) {
      CHECK(availability.Contributions.front().Wait > 0.0);
    }
  }
}

TEST_CASE("The value is 0.0 with each contribution's term added in order, bit for bit") {
  const World world = openText(SHOPS_PARK);
  // Two contributions, so the order of the additions shows.
  const FoodAvailability availability = foodAvailability(world, Place{GUEST_PATH, 5.0});
  REQUIRE(availability.Contributions.size() == 2);
  double sum = 0.0;
  for (const FoodContribution &contribution : availability.Contributions) {
    sum += contribution.Term;
  }
  CHECK(bitsOf(availability.Value) == bitsOf(sum));
  CHECK(availability.Value > 0.0);
}

TEST_CASE("A place with no contributions, including one that does not resolve and any place in a "
          "world with no guest network, has the value 0.0") {
  struct Case {
    std::string_view Name;
    World Park;
    Place At;
  };
  std::vector<Case> cases;
  cases.push_back({"beyond the guest path's end", openText(SHOPS_PARK), Place{GUEST_PATH, 41.0}});
  cases.push_back({"at a NaN distance", openText(SHOPS_PARK),
                   Place{GUEST_PATH, std::numeric_limits<double>::quiet_NaN()}});
  // The backstage path's key names no carrier of the guest network.
  cases.push_back(
      {"on a carrier the guest network lacks", openText(SHOPS_PARK), Place{EntityKey{3}, 5.0}});
  cases.push_back({"in a park with a backstage path alone",
                   openText("tpj-park 1\nseed 1\ntick 0\nnext-key 2\n"
                            "\n[path]\n"
                            "1 kind=backstage points=[{x=0 z=0} {x=0 z=-20}]\n"),
                   Place{EntityKey{1}, 5.0}});
  for (const Case &test : cases) {
    INFO(test.Name);
    const FoodAvailability availability = foodAvailability(test.Park, test.At);
    CHECK(availability.Contributions.empty());
    CHECK(bitsOf(availability.Value) == bitsOf(0.0));
  }
}

TEST_CASE("foodDiscount is the curve's Y at each of its points") {
  for (const CurvePoint &point : FOOD_DISCOUNT_CURVE) {
    INFO("time " << point.X);
    CHECK(foodDiscount(point.X) == point.Y);
  }
}

TEST_CASE("foodDiscount interpolates linearly strictly between consecutive points") {
  for (size_t index = 0; index + 1 < FOOD_DISCOUNT_CURVE.size(); ++index) {
    const CurvePoint a = FOOD_DISCOUNT_CURVE.at(index);
    const CurvePoint b = FOOD_DISCOUNT_CURVE.at(index + 1);
    // A quarter of the way along, where interpolating from the wrong end would differ.
    const double t = a.X + (b.X - a.X) / 4.0;
    INFO("time " << t);
    CHECK(foodDiscount(t) == a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X));
  }
}

TEST_CASE("foodDiscount is 1 before the curve's first point, 0 beyond its last, and 0 for a NaN") {
  const double first = FOOD_DISCOUNT_CURVE.front().X;
  const double last = FOOD_DISCOUNT_CURVE.back().X;
  constexpr double INFINITE = std::numeric_limits<double>::infinity();
  CHECK(foodDiscount(first - 1.0) == 1.0);
  CHECK(foodDiscount(-INFINITE) == 1.0);
  CHECK(foodDiscount(last + 1.0) == 0.0);
  CHECK(foodDiscount(INFINITE) == 0.0);
  CHECK(foodDiscount(std::numeric_limits<double>::quiet_NaN()) == 0.0);
}

} // namespace
} // namespace tpj
