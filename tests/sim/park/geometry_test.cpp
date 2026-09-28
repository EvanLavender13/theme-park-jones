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

bool sameFootprint(const Footprint &left, const Footprint &right) {
  const auto same = [](ParkPoint a, ParkPoint b) {
    return sameBits(a.X, b.X) && sameBits(a.Z, b.Z);
  };
  bool corners = true;
  for (std::size_t index = 0; index < left.Corners.size(); ++index) {
    corners = corners && same(left.Corners[index], right.Corners[index]);
  }
  return corners && same(left.Forward, right.Forward) && same(left.Right, right.Right);
}

// The pose's footprint, failing the test when there is none.
Footprint requireFootprint(const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  REQUIRE(footprint.has_value());
  return footprint.value_or(Footprint{});
}

void checkCorners(const Footprint &footprint, const std::vector<ParkPoint> &expected) {
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CHECK_THAT(footprint.Corners.at(index).X, WithinAbs(expected[index].X, 1e-9));
    CHECK_THAT(footprint.Corners.at(index).Z, WithinAbs(expected[index].Z, 1e-9));
  }
}

TEST_CASE("groundLine is empty exactly when a point is not finite or outside the park, or fewer "
          "than two are kept, and never throws") {
  const double beyond = std::nextafter(PARK_SIZE / 2.0, INFINITE);

  // Points exactly MIN_POINT_SPACING apart are kept, and the square includes its edges.
  CHECK_FALSE(groundLine({{0.0, 0.0}, {0.01, 0.0}}).empty());
  CHECK_FALSE(groundLine({{-128.0, -128.0}, {128.0, 128.0}}).empty());

  CHECK(groundLine({}).empty());
  CHECK(groundLine({{5.0, 5.0}}).empty());
  CHECK(groundLine({{5.0, 5.0}, {5.0, 5.0}}).empty());
  CHECK(groundLine({{0.0, 0.0}, {0.006, 0.0}, {0.007, 0.007}}).empty());
  CHECK(groundLine({{0.0, 0.0}, {10.0, 0.0}, {NOT_A_NUMBER, 0.0}}).empty());
  CHECK(groundLine({{0.0, 0.0}, {10.0, INFINITE}}).empty());
  CHECK(groundLine({{-INFINITE, 0.0}, {10.0, 0.0}}).empty());
  CHECK(groundLine({{0.0, 0.0}, {beyond, 0.0}}).empty());
  CHECK(groundLine({{0.0, -beyond}, {0.0, 0.0}, {10.0, 0.0}}).empty());
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

TEST_CASE("Points within 1 cm of the last kept point are dropped without changing the line") {
  // The third point is within 1 cm of the second but not of the first, which is the last kept.
  const auto withRepeats =
      groundLine({{0.0, 0.0}, {0.006, 0.0}, {0.012, 0.0}, {0.015, 0.001}, {5.0, 0.0}});
  const auto keptOnly = groundLine({{0.0, 0.0}, {0.012, 0.0}, {5.0, 0.0}});

  REQUIRE_FALSE(keptOnly.empty());
  CHECK(sameLine(withRepeats, keptOnly));
}

TEST_CASE(
    "keptPoints gives the points groundLine keeps, in order, each at least 1 cm from the last "
    "kept one") {
  // The third point is within 1 cm of the second but not of the first, which is the last kept, and
  // the fifth lies exactly 1 cm from the fourth.
  const std::vector<ParkPoint> points{{0.0, 0.0},   {0.0, 0.0},    {0.006, 0.0},
                                      {0.012, 0.0}, {0.012, 0.01}, {5.0, 5.0}};
  const std::vector<ParkPoint> kept = keptPoints(points);

  const std::vector<ParkPoint> expected{{0.0, 0.0}, {0.012, 0.0}, {0.012, 0.01}, {5.0, 5.0}};
  CHECK(kept == expected);
  CHECK(sameLine(groundLine(kept), groundLine(points)));
}

TEST_CASE("Each ground line segment holds max(8, ceil(chord / 1 m)) points, counting its start") {
  // Chords of 3, 12, and 9.5 m: below the minimum, a whole number of meters, and a fraction.
  const std::vector<ParkPoint> points{{0.0, -20.0}, {0.0, -17.0}, {0.0, -5.0}, {0.0, 4.5}};
  const auto line = groundLine(points);

  REQUIRE(line.size() == 8 + 12 + 10 + 1);
  CHECK(isAt(line[0], points[0]));
  CHECK(isAt(line[8], points[1]));
  CHECK(isAt(line[20], points[2]));
  CHECK(isAt(line[30], points[3]));
}

TEST_CASE("A ground line's points between kept points lie on the centripetal Catmull-Rom curve") {
  SECTION("a two-point path is its chord, sampled at k / n") {
    // The reflected phantoms make the four points evenly spaced on one line, so the curve is the
    // chord at the parameter's fraction. The chord is 20 m, so n is 20.
    const auto line = groundLine({{-3.0, 5.0}, {9.0, -11.0}});
    REQUIRE(line.size() == 21);
    for (std::size_t k = 1; k < 20; ++k) {
      const double fraction = static_cast<double>(k) / 20.0;
      CHECK_THAT(line[k].X, WithinAbs(-3.0 + 12.0 * fraction, 1e-9));
      CHECK_THAT(line[k].Z, WithinAbs(5.0 - 16.0 * fraction, 1e-9));
    }
  }

  SECTION("a bent path follows centripetal knots through its phantom ends") {
    // Chords of 1 and 4 m make every knot a whole number, so the pyramid works out by hand: knots
    // 0, 1, 2, 4 for the first segment and 0, 1, 3, 5 for the second, each segment of 8 points.
    // Uniform or chordal knots give other points.
    const auto line = groundLine({{0.0, 0.0}, {1.0, 0.0}, {1.0, 4.0}});
    REQUIRE(line.size() == 17);
    CHECK_THAT(line[4].X, WithinAbs(13.0 / 24.0, 1e-9));
    CHECK_THAT(line[4].Z, WithinAbs(-1.0 / 12.0, 1e-9));
    CHECK_THAT(line[12].X, WithinAbs(7.0 / 6.0, 1e-9));
    CHECK_THAT(line[12].Z, WithinAbs(5.0 / 3.0, 1e-9));
  }
}

TEST_CASE("Each ground line distance is the previous plus the step's length, so distances strictly "
          "increase") {
  // A bend, a hairpin that doubles back, and the shortest segment the line keeps.
  const std::vector<std::vector<ParkPoint>> paths{{{0.0, 0.0}, {1.0, 0.0}, {1.0, 4.0}},
                                                  {{0.0, 0.0}, {10.0, 0.0}, {0.0, 1.0}},
                                                  {{0.0, 0.0}, {0.01, 0.0}}};
  for (const auto &points : paths) {
    const auto line = groundLine(points);
    REQUIRE(line.size() >= 2);
    for (std::size_t index = 1; index < line.size(); ++index) {
      const double dx = line[index].X - line[index - 1].X;
      const double dz = line[index].Z - line[index - 1].Z;
      CHECK(
          sameBits(line[index].Distance, line[index - 1].Distance + std::sqrt(dx * dx + dz * dz)));
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
  SECTION("the default pose faces -z") {
    const Footprint footprint = requireFootprint(Pose{}, ENTRANCE_SIZE);
    CHECK_THAT(footprint.Right.X, WithinAbs(1.0, 1e-12));
    CHECK_THAT(footprint.Right.Z, WithinAbs(0.0, 1e-12));
    checkCorners(footprint, {{-5.0, -1.5}, {5.0, -1.5}, {5.0, 1.5}, {-5.0, 1.5}});
  }

  SECTION("a shop turned off the axes") {
    const Footprint footprint =
        requireFootprint(Pose{10.0, 20.0, 3.0, 4.0}, boxSize(BoxKind::Shop));
    CHECK_THAT(footprint.Right.X, WithinAbs(-0.8, 1e-12));
    CHECK_THAT(footprint.Right.Z, WithinAbs(0.6, 1e-12));
    checkCorners(footprint, {{15.0, 20.0}, {8.6, 24.8}, {5.0, 20.0}, {11.4, 15.2}});
  }

  SECTION("a depot at the park's corner, where coordinates are largest") {
    const Footprint footprint =
        requireFootprint(Pose{-128.0, 128.0, -1.0, 0.0}, boxSize(BoxKind::Depot));
    checkCorners(footprint, {{-132.0, 134.0}, {-132.0, 122.0}, {-124.0, 122.0}, {-124.0, 134.0}});
  }
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

TEST_CASE(
    "Ground lines and footprints computed twice from the same input are identical bit for bit") {
  const std::vector<ParkPoint> points{{0.1, 0.2}, {3.7, -1.3}, {-2.9, 5.55}, {40.25, -17.125}};
  CHECK(sameLine(groundLine(points), groundLine(points)));

  const Pose pose{-12.3, 45.6, 0.7, -0.3};
  const Footprint first = requireFootprint(pose, boxSize(BoxKind::Shop));
  const Footprint second = requireFootprint(pose, boxSize(BoxKind::Shop));
  CHECK(sameFootprint(first, second));
}

} // namespace
} // namespace tpj
