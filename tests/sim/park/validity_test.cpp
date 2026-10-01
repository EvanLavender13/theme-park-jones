#include "support/park_worlds.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::ParkIntent;
using test::worldOf;

// A conflict at least this deep is never contact.
constexpr double INSET = 2.0 * CONTACT_TOLERANCE;
constexpr double EDGE = PARK_SIZE / 2.0;

constexpr Pose facingSouth(double x, double z) { return Pose{x, z, 0.0, -1.0}; }

Footprint requireFootprint(const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  REQUIRE(footprint.has_value());
  return footprint.value_or(Footprint{});
}

// The corner of a footprint at the origin with the least x, which a footprint elsewhere with the
// same facing holds offset by its position.
ParkPoint leastXCorner(const Footprint &footprint) {
  return *std::ranges::min_element(footprint.Corners, {}, &ParkPoint::X);
}

bool boxesValid(const ParkBox &first, const ParkBox &second) {
  return isPhysicallyValid(worldOf(ParkIntent{{}, {}, {first, second}}));
}

bool pathAndBoxValid(const ParkPath &path, const ParkBox &box) {
  return isPhysicallyValid(worldOf(ParkIntent{{}, {path}, {box}}));
}

TEST_CASE("makeNewPark's world is physically valid") { CHECK(isPhysicallyValid(makeNewPark(1))); }

TEST_CASE("Two footprints overlap when one reaches 2 mm into the other, and never when their "
          "interiors are disjoint, at any facing") {
  SECTION("side by side, facing along an axis") {
    const ParkBox left{EntityKey{1}, BoxKind::Shop, facingSouth(0.0, 0.0)};
    CHECK(boxesValid(left, {EntityKey{2}, BoxKind::Shop, facingSouth(8.0, 0.0)}));
    CHECK_FALSE(boxesValid(left, {EntityKey{2}, BoxKind::Shop, facingSouth(8.0 - INSET, 0.0)}));
  }

  // A diagonal facing normalizes with rounding, so flush neighbors meet only to within rounding.
  SECTION("side by side, facing diagonally") {
    const Pose diagonal{0.0, 0.0, 1.0, 1.0};
    const ParkPoint right = requireFootprint(diagonal, boxSize(BoxKind::Shop)).Right;
    const auto beside = [&](double offset) {
      return ParkBox{EntityKey{2}, BoxKind::Shop,
                     Pose{offset * right.X, offset * right.Z, 1.0, 1.0}};
    };
    const ParkBox first{EntityKey{1}, BoxKind::Shop, diagonal};
    CHECK(boxesValid(first, beside(8.0)));
    CHECK_FALSE(boxesValid(first, beside(8.0 - INSET)));
  }

  // A shop turned 45 degrees touches the depot's side, x = 6, with one corner only.
  SECTION("a corner against a side, at different facings") {
    const ParkBox depot{EntityKey{1}, BoxKind::Depot, facingSouth(0.0, 0.0)};
    const ParkPoint corner =
        leastXCorner(requireFootprint(Pose{0.0, 0.0, 1.0, 1.0}, boxSize(BoxKind::Shop)));
    const auto shopReaching = [&](double depth) {
      return ParkBox{EntityKey{2}, BoxKind::Shop,
                     Pose{6.0 - depth - corner.X, -corner.Z, 1.0, 1.0}};
    };
    CHECK(boxesValid(depot, shopReaching(0.0)));
    CHECK_FALSE(boxesValid(depot, shopReaching(INSET)));
  }

  SECTION("a box against the entrance") {
    const ParkIntent touching{{{EntityKey{1}, facingSouth(0.0, 0.0)}},
                              {},
                              {{EntityKey{2}, BoxKind::Shop, facingSouth(9.0, 0.0)}}};
    const ParkIntent reaching{{{EntityKey{1}, facingSouth(0.0, 0.0)}},
                              {},
                              {{EntityKey{2}, BoxKind::Shop, facingSouth(9.0 - INSET, 0.0)}}};
    CHECK(isPhysicallyValid(worldOf(touching)));
    CHECK_FALSE(isPhysicallyValid(worldOf(reaching)));
  }
}

TEST_CASE("A ground line meets a footprint when a point of its segments comes 2 mm inside its half "
          "width, and never when every point is its half width away") {
  // The shop covers x from -4 to 4, and a backstage path's half width is 1 m.
  SECTION("beside a side, facing along an axis") {
    const ParkBox shop{EntityKey{1}, BoxKind::Shop, facingSouth(0.0, 0.0)};
    const auto pathAt = [](double x) {
      return ParkPath{EntityKey{2}, PathKind::Backstage, {{x, -20.0}, {x, 20.0}}};
    };
    CHECK(pathAndBoxValid(pathAt(5.0), shop));
    CHECK_FALSE(pathAndBoxValid(pathAt(5.0 - INSET), shop));
  }

  // The line's own points lie at least a quarter meter to either side of the corner, so only the
  // segment between them comes within the half width: the check must measure segments, not points.
  SECTION("a corner below the middle of a segment, at a diagonal facing") {
    const ParkPath path{EntityKey{2}, PathKind::Guest, {{-20.0, 0.0}, {20.0, 0.0}}};
    const std::vector<CarrierPoint> line = groundLine(path.Points);
    const auto after = std::ranges::find_if(line, [](const CarrierPoint &p) { return p.X > 0.0; });
    REQUIRE(after != line.begin());
    REQUIRE(after != line.end());
    const CarrierPoint &next = *after;
    const CarrierPoint &previous = *std::prev(after);
    REQUIRE(next.X - previous.X >= 0.5);
    const double middle = (previous.X + next.X) / 2.0;

    const Footprint atOrigin = requireFootprint(Pose{0.0, 0.0, 1.0, 1.0}, boxSize(BoxKind::Shop));
    const ParkPoint top = *std::ranges::max_element(atOrigin.Corners, {}, &ParkPoint::Z);
    const double halfWidth = pathWidth(PathKind::Guest) / 2.0;
    const auto shopBelow = [&](double distance) {
      return ParkBox{EntityKey{1}, BoxKind::Shop,
                     Pose{middle - top.X, -distance - top.Z, 1.0, 1.0}};
    };
    CHECK(pathAndBoxValid(path, shopBelow(halfWidth)));
    CHECK_FALSE(pathAndBoxValid(path, shopBelow(halfWidth - INSET)));
  }
}

TEST_CASE("A footprint leaves the park exactly when a corner passes the edge by more than the "
          "contact tolerance") {
  const auto valid = [](const ParkBox &box) {
    return isPhysicallyValid(worldOf(ParkIntent{{}, {}, {box}}));
  };

  SECTION("facing along an axis, at +x") {
    // The shop's right side lies 4 m from its position.
    const auto shopPassing = [](double beyond) {
      return ParkBox{EntityKey{1}, BoxKind::Shop, facingSouth(EDGE - 4.0 + beyond, 0.0)};
    };
    CHECK(valid(shopPassing(0.5 * CONTACT_TOLERANCE)));
    CHECK_FALSE(valid(shopPassing(1.5 * CONTACT_TOLERANCE)));
  }

  SECTION("facing diagonally, at -z") {
    const Footprint atOrigin = requireFootprint(Pose{0.0, 0.0, 3.0, 4.0}, boxSize(BoxKind::Depot));
    const ParkPoint lowest = *std::ranges::min_element(atOrigin.Corners, {}, &ParkPoint::Z);
    const auto depotPassing = [&](double beyond) {
      return ParkBox{EntityKey{1}, BoxKind::Depot, Pose{0.0, -EDGE - beyond - lowest.Z, 3.0, 4.0}};
    };
    CHECK(valid(depotPassing(0.5 * CONTACT_TOLERANCE)));
    CHECK_FALSE(valid(depotPassing(1.5 * CONTACT_TOLERANCE)));
  }
}

TEST_CASE("A ground line leaves the park exactly when a point of it passes the edge, less its half "
          "width, by more than the contact tolerance") {
  const auto valid = [](const ParkPath &path) {
    return isPhysicallyValid(worldOf(ParkIntent{{}, {path}, {}}));
  };

  SECTION("a straight backstage path at -z") {
    const double limit = EDGE - pathWidth(PathKind::Backstage) / 2.0;
    const auto pathPassing = [&](double beyond) {
      const double z = -(limit + beyond);
      return ParkPath{EntityKey{1}, PathKind::Backstage, {{-20.0, z}, {20.0, z}}};
    };
    CHECK(valid(pathPassing(0.5 * CONTACT_TOLERANCE)));
    CHECK_FALSE(valid(pathPassing(1.5 * CONTACT_TOLERANCE)));
  }

  // The curve swings past its clicked points before turning, so only its ground line reaches the
  // edge: the check must judge the line, not the points.
  SECTION("a curved guest path at +x") {
    const double limit = EDGE - pathWidth(PathKind::Guest) / 2.0;
    const std::vector<ParkPoint> shape{{100.0, 0.0}, {120.0, 0.0}, {120.0, 1.0}};
    const auto furthestX = [](const std::vector<CarrierPoint> &line) {
      return std::ranges::max_element(line, {}, &CarrierPoint::X)->X;
    };
    const double swing = furthestX(groundLine(shape));
    REQUIRE(swing > 120.0 + 0.1);

    const auto pathPassing = [&](double beyond) {
      const double shift = limit + beyond - swing;
      std::vector<ParkPoint> points;
      points.reserve(shape.size());
      for (const ParkPoint &point : shape) {
        points.push_back({point.X + shift, point.Z});
      }
      REQUIRE(std::abs(furthestX(groundLine(points)) - (limit + beyond)) < 1e-9);
      return ParkPath{EntityKey{1}, PathKind::Guest, points};
    };
    CHECK(valid(pathPassing(0.5 * CONTACT_TOLERANCE)));
    CHECK_FALSE(valid(pathPassing(1.5 * CONTACT_TOLERANCE)));
  }
}

// Where paths cross, the network derives a junction, so crossing is never a conflict.
TEST_CASE("Paths never conflict with each other, crossing or lying along one another") {
  const ParkIntent paths{{},
                         {{EntityKey{1}, PathKind::Guest, {{-20.0, 0.0}, {20.0, 0.0}}},
                          {EntityKey{2}, PathKind::Backstage, {{0.0, -20.0}, {0.0, 20.0}}},
                          {EntityKey{3}, PathKind::Guest, {{-10.0, 0.0}, {10.0, 0.0}}}},
                         {}};
  CHECK(isPhysicallyValid(worldOf(paths)));
}

TEST_CASE("A world is not physically valid when an entrance or box has no footprint or a path has "
          "an empty ground line") {
  const ParkBox shop{EntityKey{1}, BoxKind::Shop, facingSouth(0.0, 0.0)};
  REQUIRE(isPhysicallyValid(worldOf(ParkIntent{{}, {}, {shop}})));

  const Pose unfacing{50.0, 50.0, 0.0, 0.0};
  CHECK_FALSE(isPhysicallyValid(worldOf(ParkIntent{{{EntityKey{2}, unfacing}}, {}, {shop}})));
  CHECK_FALSE(isPhysicallyValid(
      worldOf(ParkIntent{{}, {}, {shop, {EntityKey{2}, BoxKind::Depot, unfacing}}})));

  const auto withPath = [&](std::vector<ParkPoint> points) {
    return worldOf(ParkIntent{{}, {{EntityKey{2}, PathKind::Guest, std::move(points)}}, {shop}});
  };
  CHECK_FALSE(isPhysicallyValid(withPath({})));
  CHECK_FALSE(isPhysicallyValid(withPath({{50.0, 50.0}})));
  CHECK_FALSE(isPhysicallyValid(withPath({{50.0, 50.0}, {50.0, 50.0}, {50.005, 50.0}})));
  CHECK_FALSE(isPhysicallyValid(withPath({{50.0, 50.0}, {EDGE + 1.0, 50.0}})));
}

} // namespace
} // namespace tpj
