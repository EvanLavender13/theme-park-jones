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
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

using test::ParkIntent;
using test::worldOf;

// Deeper than CONTACT_TOLERANCE, so an object moved this far into another conflicts with it.
constexpr double INSET = 2.0 * CONTACT_TOLERANCE;
constexpr double EDGE = PARK_SIZE / 2.0;

constexpr Pose facingSouth(double x, double z) { return Pose{x, z, 0.0, -1.0}; }

Footprint requireFootprint(const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  REQUIRE(footprint.has_value());
  return footprint.value_or(Footprint{});
}

bool isValid(const ParkIntent &intent) { return isPhysicallyValid(worldOf(intent)); }

// A copy of the points moved along x.
std::vector<ParkPoint> shifted(const std::vector<ParkPoint> &points, double dx) {
  std::vector<ParkPoint> moved;
  moved.reserve(points.size());
  for (const ParkPoint &point : points) {
    moved.push_back({point.X + dx, point.Z});
  }
  return moved;
}

double furthestX(const std::vector<CarrierPoint> &line) {
  return std::ranges::max_element(line, {}, &CarrierPoint::X)->X;
}

TEST_CASE("makeNewPark's world is physically valid") { CHECK(isPhysicallyValid(makeNewPark(1))); }

// Each pair of worlds holds the same objects, one placed to touch and one moved INSET deeper.
TEST_CASE("Objects that only touch never conflict, and objects that interpenetrate by more than "
          "CONTACT_TOLERANCE always do") {
  const FootprintSize shopSize = boxSize(BoxKind::Shop);
  const FootprintSize depotSize = boxSize(BoxKind::Depot);
  const double guestHalf = pathWidth(PathKind::Guest) / 2.0;
  const double backstageHalf = pathWidth(PathKind::Backstage) / 2.0;
  // A diagonal facing normalizes with rounding, so flush objects meet only to within rounding.
  const Pose diagonal{0.0, 0.0, 1.0, 1.0};
  const Footprint diagonalShop = requireFootprint(diagonal, shopSize);
  const auto diagonalShopAt = [](uint64_t key, double x, double z) {
    return ParkBox{EntityKey{key}, BoxKind::Shop, Pose{x, z, 1.0, 1.0}};
  };

  // Solid against solid. A shop beside the entrance, both facing along an axis.
  const auto shopBesideEntrance = [&](double depth) {
    return ParkIntent{{{EntityKey{1}, facingSouth(0.0, 0.0)}},
                      {},
                      {{EntityKey{2}, BoxKind::Shop,
                        facingSouth(((ENTRANCE_SIZE.Width + shopSize.Width) / 2.0) - depth, 0.0)}}};
  };
  CHECK(isValid(shopBesideEntrance(0.0)));
  CHECK_FALSE(isValid(shopBesideEntrance(INSET)));

  // Two diagonal shops side by side along their Right.
  const auto diagonalShops = [&](double depth) {
    const double offset = shopSize.Width - depth;
    return ParkIntent{{},
                      {},
                      {diagonalShopAt(1, 0.0, 0.0), diagonalShopAt(2, offset * diagonalShop.Right.X,
                                                                   offset * diagonalShop.Right.Z)}};
  };
  CHECK(isValid(diagonalShops(0.0)));
  CHECK_FALSE(isValid(diagonalShops(INSET)));

  // A diagonal shop's corner against a depot's side, so the solids touch at one point.
  const ParkPoint leastX = *std::ranges::min_element(diagonalShop.Corners, {}, &ParkPoint::X);
  const auto cornerAgainstSide = [&](double depth) {
    return ParkIntent{{},
                      {},
                      {{EntityKey{1}, BoxKind::Depot, facingSouth(0.0, 0.0)},
                       diagonalShopAt(2, (depotSize.Width / 2.0) - depth - leastX.X, -leastX.Z)}};
  };
  CHECK(isValid(cornerAgainstSide(0.0)));
  CHECK_FALSE(isValid(cornerAgainstSide(INSET)));

  // Line against solid. A backstage path beside a shop's side.
  const auto pathBesideShop = [&](double depth) {
    const double x = (shopSize.Width / 2.0) + backstageHalf - depth;
    return ParkIntent{{},
                      {{EntityKey{2}, PathKind::Backstage, {{x, -20.0}, {x, 20.0}}}},
                      {{EntityKey{1}, BoxKind::Shop, facingSouth(0.0, 0.0)}}};
  };
  CHECK(isValid(pathBesideShop(0.0)));
  CHECK_FALSE(isValid(pathBesideShop(INSET)));

  // A diagonal shop's corner below the middle of a segment of a straight line, whose own points lie
  // farther from the corner than the half width less CONTACT_TOLERANCE even when the corner
  // reaches INSET inside it, so only the segment between them meets the shop: a line is judged by
  // its segments, not its points.
  const ParkPath straight{EntityKey{2}, PathKind::Guest, {{-20.0, 0.0}, {20.0, 0.0}}};
  const std::vector<CarrierPoint> line = groundLine(straight.Points);
  const auto after = std::ranges::find_if(line, [](const CarrierPoint &p) { return p.X > 0.0; });
  REQUIRE(after != line.begin());
  REQUIRE(after != line.end());
  const CarrierPoint &next = *after;
  const CarrierPoint &previous = *std::prev(after);
  // The segment's points lie half the segment along the line and guestHalf - INSET across it from
  // the corner. The shop's sides fall away from the corner at 45 degrees, so while the half segment
  // is shorter than guestHalf - INSET the corner is the shop's nearest point to them, and when it
  // is longer they lie beyond the half width.
  const double halfSegment = (next.X - previous.X) / 2.0;
  const double across = guestHalf - INSET;
  REQUIRE((halfSegment * halfSegment) + (across * across) >
          (guestHalf - CONTACT_TOLERANCE) * (guestHalf - CONTACT_TOLERANCE));
  const double middle = (previous.X + next.X) / 2.0;
  const ParkPoint top = *std::ranges::max_element(diagonalShop.Corners, {}, &ParkPoint::Z);
  const auto cornerBelowSegment = [&](double depth) {
    return ParkIntent{
        {}, {straight}, {diagonalShopAt(1, middle - top.X, -(guestHalf - depth) - top.Z)}};
  };
  CHECK(isValid(cornerBelowSegment(0.0)));
  CHECK_FALSE(isValid(cornerBelowSegment(INSET)));

  // Solid against the edge. A shop facing along an axis at +x.
  const auto shopAtEdge = [&](double beyond) {
    return ParkIntent{
        {},
        {},
        {{EntityKey{1}, BoxKind::Shop, facingSouth(EDGE - (shopSize.Width / 2.0) + beyond, 0.0)}}};
  };
  CHECK(isValid(shopAtEdge(0.0)));
  CHECK_FALSE(isValid(shopAtEdge(INSET)));

  // A depot facing off the axes at -z.
  const Pose turned{0.0, 0.0, 3.0, 4.0};
  const Footprint turnedDepot = requireFootprint(turned, depotSize);
  const ParkPoint lowest = *std::ranges::min_element(turnedDepot.Corners, {}, &ParkPoint::Z);
  const auto depotAtEdge = [&](double beyond) {
    return ParkIntent{{},
                      {},
                      {{EntityKey{1}, BoxKind::Depot,
                        Pose{0.0, -EDGE - beyond - lowest.Z, turned.FacingX, turned.FacingZ}}}};
  };
  CHECK(isValid(depotAtEdge(0.0)));
  CHECK_FALSE(isValid(depotAtEdge(INSET)));

  // Line against the edge. A straight backstage path at -z.
  const auto pathAtEdge = [&](double beyond) {
    const double z = -(EDGE - backstageHalf + beyond);
    return ParkIntent{{}, {{EntityKey{1}, PathKind::Backstage, {{-20.0, z}, {20.0, z}}}}, {}};
  };
  CHECK(isValid(pathAtEdge(0.0)));
  CHECK_FALSE(isValid(pathAtEdge(INSET)));

  // A curved guest path at +x, whose line swings past its clicked points before turning, so only
  // its line reaches the edge: the edge judges the line, not the points.
  const std::vector<ParkPoint> shape{{100.0, 0.0}, {120.0, 0.0}, {120.0, 1.0}};
  const double swing = furthestX(groundLine(shape)) - 120.0;
  REQUIRE(swing > INSET);
  const auto curveAtEdge = [&](double beyond) {
    const double reach = EDGE - guestHalf + beyond;
    const std::vector<ParkPoint> points = shifted(shape, reach - (120.0 + swing));
    REQUIRE(std::abs(furthestX(groundLine(points)) - reach) < 1e-9);
    return ParkIntent{{}, {{EntityKey{1}, PathKind::Guest, points}}, {}};
  };
  CHECK(isValid(curveAtEdge(0.0)));
  CHECK_FALSE(isValid(curveAtEdge(INSET)));
}

TEST_CASE("Paths never conflict with each other, crossing or lying along one another") {
  const ParkIntent paths{{},
                         {{EntityKey{1}, PathKind::Guest, {{-20.0, 0.0}, {20.0, 0.0}}},
                          {EntityKey{2}, PathKind::Backstage, {{0.0, -20.0}, {0.0, 20.0}}},
                          {EntityKey{3}, PathKind::Guest, {{-10.0, 0.0}, {10.0, 0.0}}}},
                         {}};
  CHECK(isPhysicallyValid(worldOf(paths)));
}

TEST_CASE("A world is not physically valid when it holds intent with no footprint or no ground "
          "line") {
  const ParkBox shop{EntityKey{1}, BoxKind::Shop, facingSouth(0.0, 0.0)};
  REQUIRE(isValid(ParkIntent{{}, {}, {shop}}));

  const Pose unfacing{50.0, 50.0, 0.0, 0.0};
  CHECK_FALSE(isValid(ParkIntent{{{EntityKey{2}, unfacing}}, {}, {shop}}));
  CHECK_FALSE(isValid(ParkIntent{{}, {}, {shop, {EntityKey{2}, BoxKind::Depot, unfacing}}}));
  CHECK_FALSE(isValid(ParkIntent{{}, {{EntityKey{2}, PathKind::Guest, {{50.0, 50.0}}}}, {shop}}));
}

} // namespace
} // namespace tpj
