#include "render/food_overlay.h"

#include "render/park_mesh.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// Positions are floats a hundred meters or so from the origin, so they are checked to a tenth of a
// millimeter; the tent's heights, a few millimeters apart, to a hundredth of one.
constexpr double POSITION_TOLERANCE = 1e-4;
constexpr double HEIGHT_TOLERANCE = 1e-5;
// Ramp colors are float interpolations of the stops.
constexpr double COLOR_TOLERANCE = 1e-6;

struct Triple {
  double X = 0.0;
  double Y = 0.0;
  double Z = 0.0;
};

Triple operator-(Triple a, Triple b) { return {a.X - b.X, a.Y - b.Y, a.Z - b.Z}; }

Triple cross(Triple a, Triple b) {
  return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X};
}

bool near(Triple a, Triple b, double tolerance) {
  return std::abs(a.X - b.X) <= tolerance && std::abs(a.Y - b.Y) <= tolerance &&
         std::abs(a.Z - b.Z) <= tolerance;
}

Triple positionOf(const ParkVertex &vertex) {
  return {vertex.Position[0], vertex.Position[1], vertex.Position[2]};
}

Triple normalOf(const ParkVertex &vertex) {
  return {vertex.Normal[0], vertex.Normal[1], vertex.Normal[2]};
}

bool nearColor(Rgba a, Rgba b) {
  return std::abs(a.R - b.R) <= COLOR_TOLERANCE && std::abs(a.G - b.G) <= COLOR_TOLERANCE &&
         std::abs(a.B - b.B) <= COLOR_TOLERANCE && a.A == b.A;
}

bool sameShade(Rgba a, Rgba b) { return a.R == b.R && a.G == b.G && a.B == b.B; }

std::array<uint32_t, 3> triangleAt(const ParkMesh &mesh, size_t triangle) {
  return {mesh.Indices.at(3 * triangle), mesh.Indices.at(3 * triangle + 1),
          mesh.Indices.at(3 * triangle + 2)};
}

// The side a triangle's winding faces, by the right-hand rule: counter-clockwise seen from there.
Triple windingOf(const ParkMesh &mesh, size_t triangle) {
  const auto [a, b, c] = triangleAt(mesh, triangle);
  const Triple first = positionOf(mesh.Vertices.at(a));
  return cross(positionOf(mesh.Vertices.at(b)) - first, positionOf(mesh.Vertices.at(c)) - first);
}

World openText(std::string_view text) {
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

// fed.park has guest paths meeting at junctions and one with a bend, connectors from an entrance
// and a shop, and a backstage path.
World fedPark() {
  std::ifstream file(std::filesystem::path(TPJ_PARKS_DIR) / "fed.park", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  return openText(text);
}

// A value that differs along every line, and stays below OVERLAY_FULL on fed.park's, so each
// sample's color tells its place apart from its neighbors'.
double distanceValue(const Place &place) { return place.Distance / 100.0; }

struct Direction {
  double X = 0.0;
  double Z = 0.0;
};

Direction unitStep(const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double size = std::sqrt(dx * dx + dz * dz);
  return {dx / size, dz / size};
}

bool atInnerPoint(const Carrier &carrier, double distance) {
  const std::vector<CarrierPoint> &points = carrier.Points;
  return std::any_of(points.begin() + 1, points.end() - 1,
                     [distance](const CarrierPoint &point) { return point.Distance == distance; });
}

// A sample's direction: its segment's at a line's ends and between its points, and at a point
// strictly between the ends the normalized sum of the directions of the segments either side, or
// the one before when that sum has zero length.
Direction directionAt(const Carrier &carrier, double distance) {
  const std::vector<CarrierPoint> &points = carrier.Points;
  const size_t last = points.size() - 1;
  if (distance == points.front().Distance) {
    return unitStep(points[0], points[1]);
  }
  if (distance == points.back().Distance) {
    return unitStep(points[last - 1], points[last]);
  }
  for (size_t index = 1; index < last; ++index) {
    if (points[index].Distance == distance) {
      const Direction before = unitStep(points[index - 1], points[index]);
      const Direction after = unitStep(points[index], points[index + 1]);
      const double sumX = before.X + after.X;
      const double sumZ = before.Z + after.Z;
      const double size = std::sqrt(sumX * sumX + sumZ * sumZ);
      return size == 0.0 ? before : Direction{sumX / size, sumZ / size};
    }
  }
  for (size_t index = 0; index < last; ++index) {
    if (points[index].Distance < distance && distance < points[index + 1].Distance) {
      return unitStep(points[index], points[index + 1]);
    }
  }
  FAIL("distance " << distance << " is not on the carrier");
  return {};
}

// A band line's samples and where its pieces sit in the overlay: its rows' vertices, three per
// sample, and then each end cone's center and rim.
struct BandLine {
  const Carrier *Line = nullptr;
  std::vector<double> Samples;
  size_t RowStart = 0;
  size_t FirstConeStart = 0;
  size_t LastConeStart = 0;
  size_t End = 0;
};

constexpr size_t CONE_VERTICES = 1 + JOINT_SEGMENTS + 1;
constexpr size_t CONE_TRIANGLES = JOINT_SEGMENTS;

// The distances of the carrier's points, of its stops, and of each positive multiple of the
// spacing below its length, ascending and each once.
std::vector<double> sampleDistances(const Carrier &carrier) {
  std::set<double> distances;
  for (const CarrierPoint &point : carrier.Points) {
    distances.insert(point.Distance);
  }
  for (const CarrierStop &stop : carrier.Stops) {
    distances.insert(stop.Distance);
  }
  const double length = carrier.Points.back().Distance;
  for (int multiple = 1; multiple * OVERLAY_SPACING < length; ++multiple) {
    distances.insert(multiple * OVERLAY_SPACING);
  }
  return {distances.begin(), distances.end()};
}

// The carriers of the guest network keyed by a guest path, in key order, laid out one after
// another as the overlay holds them.
std::vector<BandLine> bandLines(const World &world) {
  std::set<EntityKey> guestPaths;
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Guest) {
      guestPaths.insert(path.Key);
    }
  }
  std::vector<BandLine> lines;
  size_t next = 0;
  for (const Carrier &carrier : parkNetwork(world, PathKind::Guest).carriers()) {
    if (!guestPaths.contains(carrier.Key)) {
      continue;
    }
    BandLine line{&carrier, sampleDistances(carrier)};
    line.RowStart = next;
    line.FirstConeStart = line.RowStart + 3 * line.Samples.size();
    line.LastConeStart = line.FirstConeStart + CONE_VERTICES;
    line.End = line.LastConeStart + CONE_VERTICES;
    next = line.End;
    lines.push_back(line);
  }
  return lines;
}

size_t triangleCount(const std::vector<BandLine> &lines) {
  size_t count = 0;
  for (const BandLine &line : lines) {
    count += 4 * (line.Samples.size() - 1) + 2 * CONE_TRIANGLES;
  }
  return count;
}

// The overlay of fed.park, required to hold exactly the vertices and triangles its band lines
// give, so the other tests can find each piece.
struct FedOverlay {
  World Park;
  std::vector<BandLine> Lines;
  ParkMesh Mesh;
};

FedOverlay fedOverlay() {
  FedOverlay overlay{fedPark(), {}, {}};
  overlay.Lines = bandLines(overlay.Park);
  REQUIRE_FALSE(overlay.Lines.empty());
  overlay.Mesh = buildFoodOverlay(overlay.Park, distanceValue);
  REQUIRE(overlay.Mesh.Vertices.size() == overlay.Lines.back().End);
  REQUIRE(overlay.Mesh.Indices.size() == 3 * triangleCount(overlay.Lines));
  return overlay;
}

GroundPoint requireGround(const World &world, const Place &place) {
  const std::optional<GroundPoint> point = parkNetwork(world, PathKind::Guest).groundPoint(place);
  REQUIRE(point.has_value());
  return point.value_or(GroundPoint{});
}

Triple at(GroundPoint point, double height) { return {point.X, height, point.Z}; }

TEST_CASE("foodColor is the zero gray for a value not above 0, including a NaN") {
  constexpr double INFINITE = std::numeric_limits<double>::infinity();
  for (const double value :
       {0.0, -0.0, -0.25, -INFINITE, std::numeric_limits<double>::quiet_NaN()}) {
    INFO("value " << value);
    CHECK(foodColor(value) == OVERLAY_ZERO_COLOR);
  }
}

TEST_CASE("foodColor of a positive value interpolates the ramp's stops, and is the last stop at "
          "and above OVERLAY_FULL") {
  // At each inner stop's quarter of the range the color is that stop.
  for (size_t stop = 1; stop < OVERLAY_RAMP.size() - 1; ++stop) {
    INFO("stop " << stop);
    CHECK(foodColor(OVERLAY_FULL * static_cast<double>(stop) / 4.0) == OVERLAY_RAMP.at(stop));
  }
  // Halfway between the first two stops and the last two, the color is halfway between them.
  for (const size_t stop : {size_t{0}, size_t{3}}) {
    INFO("halfway from stop " << stop);
    const Rgba a = OVERLAY_RAMP.at(stop);
    const Rgba b = OVERLAY_RAMP.at(stop + 1);
    const Rgba halfway{(a.R + b.R) / 2.0f, (a.G + b.G) / 2.0f, (a.B + b.B) / 2.0f, 1.0f};
    CHECK(nearColor(foodColor(OVERLAY_FULL * (2.0 * static_cast<double>(stop) + 1.0) / 8.0),
                    halfway));
  }
  // More than a meal's relief with no walk and no wait saturates.
  constexpr double INFINITE = std::numeric_limits<double>::infinity();
  for (const double value : {OVERLAY_FULL, 2.0 * OVERLAY_FULL, INFINITE}) {
    INFO("value " << value);
    CHECK(nearColor(foodColor(value), OVERLAY_RAMP.back()));
  }
}

TEST_CASE("No positive value's color is the zero gray") {
  // The least positive double, each stop, and where the ramp's red and its green each pass the
  // gray's 0.55, the only places one channel could match it.
  const auto crossing = [](size_t stop, float from, float to) {
    return OVERLAY_FULL * (static_cast<double>(stop) + (0.55 - from) / (to - from)) / 4.0;
  };
  const std::vector<double> values = {std::numeric_limits<double>::denorm_min(),
                                      OVERLAY_FULL / 4.0,
                                      OVERLAY_FULL / 2.0,
                                      3.0 * OVERLAY_FULL / 4.0,
                                      OVERLAY_FULL,
                                      crossing(3, OVERLAY_RAMP[3].R, OVERLAY_RAMP[4].R),
                                      crossing(1, OVERLAY_RAMP[1].G, OVERLAY_RAMP[2].G)};
  for (const double value : values) {
    INFO("value " << value);
    CHECK_FALSE(sameShade(foodColor(value), OVERLAY_ZERO_COLOR));
  }
}

TEST_CASE("The overlay holds, for each guest path's line in key order, a row per sample and a cone "
          "at each end, and nothing for connectors or backstage paths") {
  SECTION("fed.park, with guest paths, connectors, and a backstage path") {
    const World world = fedPark();
    const std::vector<BandLine> lines = bandLines(world);
    // Its four guest paths, and none of its connectors or its backstage path.
    CHECK(lines.size() == 4);
    const ParkMesh mesh = buildFoodOverlay(world, distanceValue);
    CHECK(mesh.Vertices.size() == lines.back().End);
    CHECK(mesh.Indices.size() == 3 * triangleCount(lines));
  }
  SECTION("a park with a backstage path alone, so no guest network") {
    const World world = openText("tpj-park 1\nseed 1\ntick 0\nnext-key 2\n"
                                 "\n[path]\n"
                                 "1 kind=backstage points=[{x=0 z=0} {x=0 z=-20}]\n");
    const ParkMesh mesh = buildFoodOverlay(world, distanceValue);
    CHECK(mesh.Vertices.empty());
    CHECK(mesh.Indices.empty());
  }
}

TEST_CASE("Each row's center lies on top of the tent at its sample's ground point, and its edges "
          "OVERLAY_BAND either side across the line at the tent's edge") {
  const FedOverlay overlay = fedOverlay();
  for (const BandLine &line : overlay.Lines) {
    for (size_t row = 0; row < line.Samples.size(); ++row) {
      const double distance = line.Samples[row];
      INFO("carrier " << static_cast<uint64_t>(line.Line->Key) << " at " << distance);
      const GroundPoint p = requireGround(overlay.Park, Place{line.Line->Key, distance});
      const Direction t = directionAt(*line.Line, distance);
      const GroundPoint left{p.X + t.Z * OVERLAY_BAND, p.Z - t.X * OVERLAY_BAND};
      const GroundPoint right{p.X - t.Z * OVERLAY_BAND, p.Z + t.X * OVERLAY_BAND};
      const size_t first = line.RowStart + 3 * row;
      CHECK(near(positionOf(overlay.Mesh.Vertices.at(first)), at(left, OVERLAY_EDGE_LIFT),
                 POSITION_TOLERANCE));
      CHECK(near(positionOf(overlay.Mesh.Vertices.at(first + 1)), at(p, OVERLAY_TOP_LIFT),
                 POSITION_TOLERANCE));
      CHECK(near(positionOf(overlay.Mesh.Vertices.at(first + 2)), at(right, OVERLAY_EDGE_LIFT),
                 POSITION_TOLERANCE));
    }
  }
}

TEST_CASE("Each end cone fans OVERLAY_BAND around its end's ground point, away from the line") {
  const FedOverlay overlay = fedOverlay();
  for (const BandLine &line : overlay.Lines) {
    const std::vector<CarrierPoint> &points = line.Line->Points;
    const Direction firstStep = unitStep(points[0], points[1]);
    const Direction lastStep = unitStep(points[points.size() - 2], points.back());
    struct End {
      std::string_view Name;
      size_t Start;
      double Distance;
      Direction Away;
    };
    const std::array<End, 2> ends = {
        {{"first", line.FirstConeStart, points.front().Distance, {-firstStep.X, -firstStep.Z}},
         {"last", line.LastConeStart, points.back().Distance, lastStep}}};
    for (const End &end : ends) {
      INFO("carrier " << static_cast<uint64_t>(line.Line->Key) << ", " << end.Name << " end");
      const GroundPoint p = requireGround(overlay.Park, Place{line.Line->Key, end.Distance});
      CHECK(near(positionOf(overlay.Mesh.Vertices.at(end.Start)), at(p, OVERLAY_TOP_LIFT),
                 POSITION_TOLERANCE));
      const Direction t = end.Away;
      const Direction r{-t.Z, t.X};
      for (uint32_t k = 0; k <= JOINT_SEGMENTS; ++k) {
        INFO("rim vertex " << k);
        const double angle = std::numbers::pi * static_cast<double>(k) / 16.0;
        const GroundPoint rim{p.X + OVERLAY_BAND * (std::cos(angle) * r.X + std::sin(angle) * t.X),
                              p.Z + OVERLAY_BAND * (std::cos(angle) * r.Z + std::sin(angle) * t.Z)};
        CHECK(near(positionOf(overlay.Mesh.Vertices.at(end.Start + 1 + k)),
                   at(rim, OVERLAY_EDGE_LIFT), POSITION_TOLERANCE));
      }
    }
  }
}

TEST_CASE("Every overlay vertex points up and has the color of the value at its sample's place") {
  const FedOverlay overlay = fedOverlay();
  const auto checkVertex = [&overlay](size_t index, const Place &place) {
    const ParkVertex &vertex = overlay.Mesh.Vertices.at(index);
    CHECK(near(normalOf(vertex), Triple{0.0, 1.0, 0.0}, 0.0));
    CHECK(vertex.Color == foodColor(distanceValue(place)));
  };
  for (const BandLine &line : overlay.Lines) {
    const EntityKey key = line.Line->Key;
    for (size_t row = 0; row < line.Samples.size(); ++row) {
      INFO("carrier " << static_cast<uint64_t>(key) << " row at " << line.Samples[row]);
      for (size_t vertex = 0; vertex < 3; ++vertex) {
        checkVertex(line.RowStart + 3 * row + vertex, Place{key, line.Samples[row]});
      }
    }
    for (size_t vertex = 0; vertex < CONE_VERTICES; ++vertex) {
      INFO("carrier " << static_cast<uint64_t>(key) << " cone vertex " << vertex);
      checkVertex(line.FirstConeStart + vertex, Place{key, line.Samples.front()});
      checkVertex(line.LastConeStart + vertex, Place{key, line.Samples.back()});
    }
  }
}

TEST_CASE("Every cone triangle faces up, and so does every triangle joining consecutive rows "
          "neither of which is at a point strictly between the line's ends") {
  const FedOverlay overlay = fedOverlay();
  // fed.park's path 5 bends, so the rows at its bend are exempt, but not the rest of that line's.
  bool checkedBentLine = false;
  for (size_t triangle = 0; triangle < overlay.Mesh.Indices.size() / 3; ++triangle) {
    const std::array<uint32_t, 3> corners = triangleAt(overlay.Mesh, triangle);
    const uint32_t low = std::ranges::min(corners);
    const uint32_t high = std::ranges::max(corners);
    const auto owner =
        std::ranges::find_if(overlay.Lines, [low](const BandLine &line) { return low < line.End; });
    REQUIRE(owner != overlay.Lines.end());
    const BandLine &line = *owner;
    INFO("triangle " << triangle << " of carrier " << static_cast<uint64_t>(line.Line->Key));
    const auto inCone = [high, low](size_t start) {
      return low >= start && high < start + CONE_VERTICES;
    };
    if (inCone(line.FirstConeStart) || inCone(line.LastConeStart)) {
      CHECK(windingOf(overlay.Mesh, triangle).Y > 0.0);
      continue;
    }
    REQUIRE(low >= line.RowStart);
    REQUIRE(high < line.FirstConeStart);
    const size_t firstRow = (low - line.RowStart) / 3;
    const size_t lastRow = (high - line.RowStart) / 3;
    // Consecutive rows are joined, and nothing else.
    REQUIRE(lastRow == firstRow + 1);
    if (atInnerPoint(*line.Line, line.Samples[firstRow]) ||
        atInnerPoint(*line.Line, line.Samples[lastRow])) {
      continue;
    }
    checkedBentLine = checkedBentLine || line.Line->Points.size() > 2;
    CHECK(windingOf(overlay.Mesh, triangle).Y > 0.0);
  }
  CHECK(checkedBentLine);
}

// Two straight guest paths 8 m apart along the direction (0.8, 0.6), the second offset along its
// right direction (-0.6, 0.8), so their bands overlap across the 4 m between their edges, with no
// axis-aligned shortcut.
constexpr std::string_view PARALLEL_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 4\n"
                                           "\n[path]\n"
                                           "2 kind=guest points=[{x=0 z=0} {x=16 z=12}]\n"
                                           "3 kind=guest points=[{x=-4.8 z=6.4} {x=11.2 z=18.4}]\n";

// The height of the line's surface over a ground point, from the row triangle whose ground
// projection holds it, or none when none does.
std::optional<double> surfaceHeight(const ParkMesh &mesh, const BandLine &line, GroundPoint point) {
  constexpr double INSIDE = -1e-9;
  for (size_t triangle = 0; triangle < mesh.Indices.size() / 3; ++triangle) {
    const std::array<uint32_t, 3> corners = triangleAt(mesh, triangle);
    if (!std::ranges::all_of(corners, [&line](uint32_t index) {
          return index >= line.RowStart && index < line.FirstConeStart;
        })) {
      continue;
    }
    const Triple a = positionOf(mesh.Vertices.at(corners[0]));
    const Triple b = positionOf(mesh.Vertices.at(corners[1]));
    const Triple c = positionOf(mesh.Vertices.at(corners[2]));
    const double area = (b.X - a.X) * (c.Z - a.Z) - (c.X - a.X) * (b.Z - a.Z);
    if (area == 0.0) {
      continue;
    }
    const double wb = ((point.X - a.X) * (c.Z - a.Z) - (c.X - a.X) * (point.Z - a.Z)) / area;
    const double wc = ((b.X - a.X) * (point.Z - a.Z) - (point.X - a.X) * (b.Z - a.Z)) / area;
    const double wa = 1.0 - wb - wc;
    if (wa >= INSIDE && wb >= INSIDE && wc >= INSIDE) {
      return wa * a.Y + wb * b.Y + wc * c.Y;
    }
  }
  return std::nullopt;
}

TEST_CASE("Between rows of a straight segment the surface falls linearly from the line to the "
          "band's edge, so where bands overlap the nearer line's is higher") {
  const World world = openText(PARALLEL_PARK);
  const std::vector<BandLine> lines = bandLines(world);
  REQUIRE(lines.size() == 2);
  const ParkMesh mesh = buildFoodOverlay(world, distanceValue);
  REQUIRE(mesh.Vertices.size() == lines.back().End);

  const auto tent = [](double d) {
    return OVERLAY_TOP_LIFT - (OVERLAY_TOP_LIFT - OVERLAY_EDGE_LIFT) * d / OVERLAY_BAND;
  };
  // The ground point s along the first line and o across it, toward the second line for o > 0.
  const auto ground = [](double s, double o) {
    return GroundPoint{0.8 * s - 0.6 * o, 0.6 * s + 0.8 * o};
  };
  struct Probe {
    std::string_view Name;
    GroundPoint Point;
    double FromFirst;
    std::optional<double> FromSecond;
  };
  const std::array<Probe, 4> probes = {
      {{"on the first line, between two samples", ground(7.5, 0.0), 0.0, std::nullopt},
       {"beside the first line, away from the second, between its end row and the next",
        ground(0.4, -2.0), 2.0, std::nullopt},
       {"between the lines, nearer the first", ground(7.5, 3.0), 3.0, 5.0},
       {"between the lines, nearer the second", ground(12.25, 5.5), 5.5, 2.5}}};
  for (const Probe &probe : probes) {
    INFO(probe.Name);
    const std::optional<double> first = surfaceHeight(mesh, lines[0], probe.Point);
    REQUIRE(first.has_value());
    CHECK(std::abs(first.value_or(0.0) - tent(probe.FromFirst)) <= HEIGHT_TOLERANCE);
    if (probe.FromSecond) {
      const std::optional<double> second = surfaceHeight(mesh, lines[1], probe.Point);
      REQUIRE(second.has_value());
      CHECK(std::abs(second.value_or(0.0) - tent(*probe.FromSecond)) <= HEIGHT_TOLERANCE);
      if (probe.FromFirst < *probe.FromSecond) {
        CHECK(first.value_or(0.0) > second.value_or(0.0));
      } else {
        CHECK(second.value_or(0.0) > first.value_or(0.0));
      }
    }
  }
}

} // namespace
} // namespace tpj
