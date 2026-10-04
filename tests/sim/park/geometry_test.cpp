#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using Catch::Matchers::WithinAbs;

constexpr double NOT_A_NUMBER = std::numeric_limits<double>::quiet_NaN();
constexpr double INFINITE = std::numeric_limits<double>::infinity();
constexpr double LARGEST = std::numeric_limits<double>::max();
constexpr double SMALLEST = std::numeric_limits<double>::denorm_min();

bool sameBits(double left, double right) {
  return std::bit_cast<uint64_t>(left) == std::bit_cast<uint64_t>(right);
}

bool sameLine(const std::vector<CarrierPoint> &left, const std::vector<CarrierPoint> &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (!sameBits(left[index].X, right[index].X) || !sameBits(left[index].Z, right[index].Z) ||
        !sameBits(left[index].Distance, right[index].Distance)) {
      return false;
    }
  }
  return true;
}

bool isAt(const CarrierPoint &point, ParkPoint at) {
  return sameBits(point.X, at.X) && sameBits(point.Z, at.Z);
}

bool footprintsWithin(const Footprint &left, const Footprint &right, double tolerance) {
  const auto near = [tolerance](ParkPoint a, ParkPoint b) {
    return std::abs(a.X - b.X) <= tolerance && std::abs(a.Z - b.Z) <= tolerance;
  };
  bool corners = true;
  for (std::size_t index = 0; index < left.Corners.size(); ++index) {
    corners = corners && near(left.Corners[index], right.Corners[index]);
  }
  return corners && near(left.Forward, right.Forward) && near(left.Right, right.Right);
}

// The pose's footprint, failing the test when there is none.
Footprint requireFootprint(const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  REQUIRE(footprint.has_value());
  return footprint.value_or(Footprint{});
}

// Whether the line of the points is empty, failing the test when groundLine throws.
bool isEmptyLine(const std::vector<ParkPoint> &points) {
  std::vector<CarrierPoint> line;
  CHECK_NOTHROW(line = groundLine(points));
  return line.empty();
}

TEST_CASE("groundLine is empty exactly when a point is not finite or outside the park, or fewer "
          "than two are kept, and never throws") {
  constexpr double EDGE = PARK_SIZE / 2.0;
  const double beyond = std::nextafter(EDGE, INFINITE);
  constexpr double NEAR = MIN_POINT_SPACING / 2.0;

  // The square includes its edges.
  CHECK_FALSE(isEmptyLine({{-EDGE, -EDGE}, {EDGE, EDGE}}));
  CHECK(isEmptyLine({{0.0, 0.0}, {beyond, 0.0}}));
  CHECK(isEmptyLine({{0.0, 0.0}, {10.0, 0.0}, {NOT_A_NUMBER, 0.0}}));
  CHECK(isEmptyLine({{0.0, 0.0}, {10.0, INFINITE}}));
  // Every later point is closer than MIN_POINT_SPACING to the first, so only the first is kept.
  CHECK(isEmptyLine({{0.0, 0.0}, {NEAR, 0.0}, {NEAR, NEAR}}));
}

TEST_CASE("A ground line passes through every kept point bit for bit, in order, from distance 0 to "
          "its last point") {
  const std::vector<ParkPoint> kept{{0.1, 0.2}, {3.7, -1.3}, {-2.9, 5.55}};
  const auto line = groundLine({kept[0], {0.103, 0.2}, kept[1], kept[2]});
  REQUIRE_FALSE(line.empty());

  CHECK(isAt(line.front(), kept[0]));
  CHECK(sameBits(line.front().Distance, 0.0));
  CHECK(isAt(line.back(), kept[2]));

  std::size_t next = 0;
  for (const ParkPoint &point : kept) {
    while (next < line.size() && !isAt(line[next], point)) {
      ++next;
    }
    CHECK(next < line.size());
  }
}

TEST_CASE("groundLine depends only on keptPoints, which keeps each point at least "
          "MIN_POINT_SPACING from the last kept one") {
  constexpr double SPACING = MIN_POINT_SPACING;
  // The third point is closer than MIN_POINT_SPACING to the first, and is dropped. The fourth is
  // closer than that to the third, but not to the first, which is the last kept, so it is kept.
  // The fifth lies exactly MIN_POINT_SPACING from the fourth.
  const ParkPoint first{0.0, 0.0};
  const ParkPoint drifting{1.2 * SPACING, 0.0};
  const ParkPoint spaced{drifting.X, SPACING};
  const ParkPoint last{5.0, 5.0};
  const std::vector<ParkPoint> points{first, first, {0.6 * SPACING, 0.0}, drifting, spaced, last};

  const std::vector<ParkPoint> kept = keptPoints(points);

  CHECK(kept == std::vector<ParkPoint>{first, drifting, spaced, last});
  for (std::size_t index = 1; index < kept.size(); ++index) {
    const double dx = kept[index].X - kept[index - 1].X;
    const double dz = kept[index].Z - kept[index - 1].Z;
    CHECK((dx * dx) + (dz * dz) >= SPACING * SPACING);
  }
  CHECK(sameLine(groundLine(points), groundLine(kept)));
}

TEST_CASE("A ground line's distances start at 0 and strictly increase") {
  // A bend, a hairpin that doubles back, and the shortest segment the line keeps.
  const std::vector<std::vector<ParkPoint>> paths{{{0.0, 0.0}, {1.0, 0.0}, {1.0, 4.0}},
                                                  {{0.0, 0.0}, {10.0, 0.0}, {0.0, 1.0}},
                                                  {{0.0, 0.0}, {MIN_POINT_SPACING, 0.0}}};
  for (const auto &points : paths) {
    const auto line = groundLine(points);
    REQUIRE(line.size() >= 2);
    CHECK(sameBits(line.front().Distance, 0.0));
    for (std::size_t index = 1; index < line.size(); ++index) {
      CHECK(line[index].Distance > line[index - 1].Distance);
    }
  }
}

TEST_CASE("footprintOf gives none exactly when the pose is not finite or its facing has zero "
          "length, and never throws") {
  CHECK(footprintOf(Pose{0.0, 0.0, SMALLEST, 0.0}, ENTRANCE_SIZE).has_value());
  CHECK(footprintOf(Pose{0.0, 0.0, LARGEST, -LARGEST}, ENTRANCE_SIZE).has_value());
  CHECK(footprintOf(Pose{300.0, -1e300, 0.0, -1.0}, ENTRANCE_SIZE).has_value());

  CHECK_FALSE(footprintOf(Pose{NOT_A_NUMBER, 0.0, 0.0, -1.0}, ENTRANCE_SIZE).has_value());
  CHECK_FALSE(footprintOf(Pose{0.0, INFINITE, 0.0, -1.0}, ENTRANCE_SIZE).has_value());
  CHECK_FALSE(footprintOf(Pose{0.0, 0.0, -INFINITE, 1.0}, ENTRANCE_SIZE).has_value());
  CHECK_FALSE(footprintOf(Pose{0.0, 0.0, 1.0, NOT_A_NUMBER}, ENTRANCE_SIZE).has_value());
  CHECK_FALSE(footprintOf(Pose{0.0, 0.0, 0.0, 0.0}, ENTRANCE_SIZE).has_value());
  CHECK_FALSE(footprintOf(Pose{0.0, 0.0, -0.0, -0.0}, ENTRANCE_SIZE).has_value());
}

TEST_CASE("footprintOf's forward is the unit vector in the facing's direction") {
  const double half = std::sqrt(0.5);
  // A typical facing, one along an axis, and facings whose squares underflow or overflow.
  const std::vector<std::pair<Pose, ParkPoint>> cases{
      {Pose{0.0, 0.0, 3.0, 4.0}, {0.6, 0.8}},
      {Pose{0.0, 0.0, 0.0, -2.0}, {0.0, -1.0}},
      {Pose{0.0, 0.0, SMALLEST, 0.0}, {1.0, 0.0}},
      {Pose{0.0, 0.0, -1e-310, 1e-310}, {-half, half}},
      {Pose{0.0, 0.0, LARGEST, -LARGEST}, {half, -half}}};
  for (const auto &[pose, forward] : cases) {
    const Footprint footprint = requireFootprint(pose, boxSize(BoxKind::Shop));
    CHECK_THAT(footprint.Forward.X, WithinAbs(forward.X, 1e-12));
    CHECK_THAT(footprint.Forward.Z, WithinAbs(forward.Z, 1e-12));
  }
}

TEST_CASE("footprintOf's corners are front left, front right, back right, and back left") {
  // Each corner is the position plus or minus half the depth along Forward, front plus, and plus
  // or minus half the width along Right, right plus, with Right (-Forward.Z, Forward.X).
  const auto checkCorners = [](const Pose &pose, FootprintSize size) {
    INFO("pose at " << pose.X << ", " << pose.Z << " facing " << pose.FacingX << ", "
                    << pose.FacingZ);
    const Footprint footprint = requireFootprint(pose, size);
    const double length = std::sqrt((pose.FacingX * pose.FacingX) + (pose.FacingZ * pose.FacingZ));
    const ParkPoint forward{pose.FacingX / length, pose.FacingZ / length};
    const ParkPoint right{-forward.Z, forward.X};
    CHECK_THAT(footprint.Right.X, WithinAbs(right.X, 1e-12));
    CHECK_THAT(footprint.Right.Z, WithinAbs(right.Z, 1e-12));
    const auto corner = [&](double along, double across) {
      return ParkPoint{
          pose.X + (along * size.Depth / 2.0 * forward.X) + (across * size.Width / 2.0 * right.X),
          pose.Z + (along * size.Depth / 2.0 * forward.Z) + (across * size.Width / 2.0 * right.Z)};
    };
    const std::vector<ParkPoint> expected{corner(1.0, -1.0), corner(1.0, 1.0), corner(-1.0, 1.0),
                                          corner(-1.0, -1.0)};
    for (std::size_t index = 0; index < expected.size(); ++index) {
      INFO("corner " << index);
      CHECK_THAT(footprint.Corners.at(index).X, WithinAbs(expected[index].X, 1e-9));
      CHECK_THAT(footprint.Corners.at(index).Z, WithinAbs(expected[index].Z, 1e-9));
    }
  };

  // The default pose faces -z.
  checkCorners(Pose{}, ENTRANCE_SIZE);
  checkCorners(Pose{10.0, 20.0, 3.0, 4.0}, boxSize(BoxKind::Shop));
}

TEST_CASE("Scaling a pose's facing by a positive factor leaves its footprint unchanged") {
  const FootprintSize size = boxSize(BoxKind::Depot);
  const Footprint base = requireFootprint(Pose{10.0, 20.0, 3.0, 4.0}, size);
  const Footprint tiny = requireFootprint(Pose{10.0, 20.0, 3e-300, 4e-300}, size);
  const Footprint huge = requireFootprint(Pose{10.0, 20.0, 3e300, 4e300}, size);
  CHECK(footprintsWithin(tiny, base, 1e-12));
  CHECK(footprintsWithin(huge, base, 1e-12));

  // A zero component stays zero under scaling, down to a subnormal facing.
  const Footprint axis = requireFootprint(Pose{-40.0, 7.5, 0.0, -1.0}, size);
  const Footprint subnormal = requireFootprint(Pose{-40.0, 7.5, 0.0, -1e-310}, size);
  CHECK(footprintsWithin(subnormal, axis, 1e-12));
}

} // namespace
} // namespace tpj
