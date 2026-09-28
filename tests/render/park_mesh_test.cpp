#include "render/park_mesh.h"

#include "sim/medium/network.h"
#include "sim/park/geometry.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// Positions are checked to the millimeter, normals and colors more finely, since they are
// computed from unit values.
constexpr double POSITION_TOLERANCE = 1e-3;
constexpr double UNIT_TOLERANCE = 1e-5;

struct Triple {
  double X = 0.0;
  double Y = 0.0;
  double Z = 0.0;
};

Triple operator-(Triple a, Triple b) { return {a.X - b.X, a.Y - b.Y, a.Z - b.Z}; }

double dot(Triple a, Triple b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }

Triple cross(Triple a, Triple b) {
  return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X};
}

double length(Triple a) { return std::sqrt(dot(a, a)); }

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

constexpr Triple UP{0.0, 1.0, 0.0};

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

bool sameVertex(const ParkVertex &left, const ParkVertex &right) {
  return std::ranges::equal(left.Position, right.Position) &&
         std::ranges::equal(left.Normal, right.Normal) && left.Color == right.Color;
}

bool sameMesh(const ParkMesh &left, const ParkMesh &right) {
  return std::ranges::equal(left.Vertices, right.Vertices, sameVertex) &&
         left.Indices == right.Indices;
}

ParkPoint unitStep(const CarrierPoint &from, const CarrierPoint &to) {
  const double dx = to.X - from.X;
  const double dz = to.Z - from.Z;
  const double size = std::sqrt(dx * dx + dz * dz);
  return {dx / size, dz / size};
}

// The tangent the render spec gives at point i of a ground line: the segment after it at the
// first point, the one before at the last, and between them the normalized sum of both, or the
// one before when the sum has zero length.
ParkPoint tangentAt(const std::vector<CarrierPoint> &line, size_t i) {
  if (i == 0) {
    return unitStep(line[0], line[1]);
  }
  const ParkPoint before = unitStep(line[i - 1], line[i]);
  if (i + 1 == line.size()) {
    return before;
  }
  const ParkPoint after = unitStep(line[i], line[i + 1]);
  const double sumX = before.X + after.X;
  const double sumZ = before.Z + after.Z;
  const double size = std::sqrt(sumX * sumX + sumZ * sumZ);
  if (size == 0.0) {
    return before;
  }
  return {sumX / size, sumZ / size};
}

bool hasReversal(const std::vector<CarrierPoint> &line) {
  for (size_t i = 1; i + 1 < line.size(); ++i) {
    const ParkPoint before = unitStep(line[i - 1], line[i]);
    const ParkPoint after = unitStep(line[i], line[i + 1]);
    if (before.X + after.X == 0.0 && before.Z + after.Z == 0.0) {
      return true;
    }
  }
  return false;
}

struct PathCase {
  std::string_view Name;
  PathKind Kind;
  std::vector<ParkPoint> Points;
};

// A gentle curve, whose ribbon never folds over itself.
const PathCase GUEST_CURVE{
    "a guest curve", PathKind::Guest, {{0.0, 0.0}, {30.0, 0.0}, {50.0, 20.0}}};
// The other kind, straight and on a diagonal.
const PathCase BACKSTAGE_LINE{
    "a straight backstage diagonal", PathKind::Backstage, {{-5.0, -5.0}, {5.0, 5.0}}};
// A path that turns straight back on itself, where the sum of the directions before and after
// the turning point has zero length.
const PathCase GUEST_REVERSAL{
    "a guest path turning back on itself", PathKind::Guest, {{0.0, 0.0}, {10.0, 0.0}, {0.0, 0.0}}};

std::vector<CarrierPoint> requireLine(const PathCase &path) {
  std::vector<CarrierPoint> line = groundLine(path.Points);
  REQUIRE(line.size() >= 2);
  return line;
}

Footprint requireFootprint(const Pose &pose, FootprintSize size) {
  const std::optional<Footprint> footprint = footprintOf(pose, size);
  REQUIRE(footprint.has_value());
  return footprint.value_or(Footprint{});
}

// A face of a box: its outward normal and the ground positions its four vertices stand over,
// at the ground and at the height, or all at the height for the top.
struct Face {
  std::string_view Name;
  Triple Normal;
  std::array<Triple, 4> Positions;
};

Triple at(const ParkPoint &point, double height) { return {point.X, height, point.Z}; }

std::array<Face, 5> facesOf(const Footprint &footprint, double height) {
  const auto &c = footprint.Corners;
  const Triple forward{footprint.Forward.X, 0.0, footprint.Forward.Z};
  const Triple right{footprint.Right.X, 0.0, footprint.Right.Z};
  const auto side = [height](std::string_view name, Triple normal, const ParkPoint &first,
                             const ParkPoint &second) {
    return Face{
        name, normal, {at(first, 0.0), at(first, height), at(second, 0.0), at(second, height)}};
  };
  return {Face{"top", UP, {at(c[0], height), at(c[1], height), at(c[2], height), at(c[3], height)}},
          side("front", forward, c[0], c[1]),
          side("back", {-forward.X, 0.0, -forward.Z}, c[2], c[3]), side("right", right, c[1], c[2]),
          side("left", {-right.X, 0.0, -right.Z}, c[3], c[0])};
}

// The vertices whose normal is the face's.
std::vector<uint32_t> verticesFacing(const ParkMesh &mesh, const Face &face) {
  std::vector<uint32_t> found;
  for (size_t index = 0; index < mesh.Vertices.size(); ++index) {
    if (near(normalOf(mesh.Vertices[index]), face.Normal, UNIT_TOLERANCE)) {
      found.push_back(static_cast<uint32_t>(index));
    }
  }
  return found;
}

struct BoxCase {
  std::string_view Name;
  Pose At;
  FootprintSize Size;
  float Height;
  Rgba Color;
};

// A facing not of unit length and along no axis, so the normals must be normalized and rotated.
// The other box has a color with channels at neither end and an alpha below 1, so the front's
// lightening shows in every channel.
const BoxCase SHOP_TURNED{"a shop facing (3, 4)", Pose{10.0, -20.0, 3.0, 4.0},
                          boxSize(BoxKind::Shop), 4.0f, boxColor(BoxKind::Shop)};
const BoxCase DEPOT_WEST{"a depot facing -x", Pose{-30.0, 12.5, -1.0, 0.0}, boxSize(BoxKind::Depot),
                         6.0f, Rgba{0.2f, 0.5f, 0.9f, 0.75f}};

// A mesh already holding a triangle, so appending must keep it and index past it.
ParkMesh heldMesh() {
  const ParkVertex corner{{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, Rgba{0.1f, 0.2f, 0.3f, 1.0f}};
  ParkMesh mesh;
  mesh.Vertices = {corner, corner, corner};
  mesh.Vertices[1].Position[0] = 2.0f;
  mesh.Vertices[2].Position[2] = 0.0f;
  mesh.Indices = {0, 2, 1};
  return mesh;
}

ParkMesh boxMesh(const BoxCase &box) {
  ParkMesh mesh;
  appendBox(mesh, box.At, box.Size, box.Height, box.Color);
  return mesh;
}

// Intent of every kind, with keys interleaved across kinds and two entrances, which only a save
// can hold.
constexpr std::string_view MIXED_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 9\n"
                                        "\n[entrance]\n"
                                        "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
                                        "6 x=-120 z=0 facing-x=1 facing-z=0\n"
                                        "\n[path]\n"
                                        "2 kind=guest points=[{x=0 z=123} {x=0 z=103}]\n"
                                        "5 kind=backstage points=[{x=-90 z=0} {x=-70 z=10} "
                                        "{x=-50 z=0}]\n"
                                        "\n[box]\n"
                                        "3 kind=depot x=40 z=40 facing-x=0 facing-z=1\n"
                                        "4 kind=shop x=20 z=100 facing-x=-1 facing-z=0\n"
                                        "8 kind=shop x=-20 z=-20 facing-x=3 facing-z=4\n";

World mixedPark() { return loadWorld(makeParkSchema(), MIXED_PARK); }

TEST_CASE("appendPath lays two vertices per ground line point, pathWidth apart across the "
          "tangent and centered on the point at PATH_LIFT") {
  for (const PathCase &path : {GUEST_CURVE, BACKSTAGE_LINE, GUEST_REVERSAL}) {
    INFO(path.Name);
    const std::vector<CarrierPoint> line = requireLine(path);
    if (path.Name == GUEST_REVERSAL.Name) {
      REQUIRE(hasReversal(line));
    }
    ParkMesh mesh;
    appendPath(mesh, path.Kind, path.Points);
    const size_t n = line.size();
    REQUIRE(mesh.Vertices.size() == 2 * n);
    CHECK(mesh.Indices.size() == 6 * (n - 1));

    const double width = pathWidth(path.Kind);
    for (size_t i = 0; i < n; ++i) {
      INFO("point " << i);
      const Triple left = positionOf(mesh.Vertices[2 * i]);
      const Triple right = positionOf(mesh.Vertices[2 * i + 1]);
      const Triple middle{(left.X + right.X) / 2.0, (left.Y + right.Y) / 2.0,
                          (left.Z + right.Z) / 2.0};
      CHECK(near(middle, {line[i].X, PATH_LIFT, line[i].Z}, POSITION_TOLERANCE));
      const ParkPoint tangent = tangentAt(line, i);
      const Triple across{-tangent.Z * width, 0.0, tangent.X * width};
      CHECK(near(right - left, across, POSITION_TOLERANCE));
    }
  }
}

TEST_CASE("appendPath's vertices have upward normals and the kind's path color") {
  for (const PathCase &path : {GUEST_CURVE, BACKSTAGE_LINE}) {
    INFO(path.Name);
    ParkMesh mesh;
    appendPath(mesh, path.Kind, path.Points);
    REQUIRE_FALSE(mesh.Vertices.empty());
    for (const ParkVertex &vertex : mesh.Vertices) {
      CHECK(near(normalOf(vertex), UP, UNIT_TOLERANCE));
      CHECK(vertex.Color == pathColor(path.Kind));
    }
  }
}

TEST_CASE("appendPath covers the quad between each pair of consecutive points with two "
          "triangles wound to face up") {
  const std::vector<CarrierPoint> line = requireLine(GUEST_CURVE);
  ParkMesh mesh;
  appendPath(mesh, GUEST_CURVE.Kind, GUEST_CURVE.Points);
  const size_t pairs = line.size() - 1;
  REQUIRE(mesh.Vertices.size() == 2 * line.size());
  REQUIRE(mesh.Indices.size() == 6 * pairs);

  // Point k's vertices are 2k, its left edge, and 2k + 1, its right edge.
  std::vector<std::vector<std::array<uint32_t, 3>>> byPair(pairs);
  for (size_t triangle = 0; triangle < mesh.Indices.size() / 3; ++triangle) {
    INFO("triangle " << triangle);
    const std::array<uint32_t, 3> corners = triangleAt(mesh, triangle);
    CHECK(windingOf(mesh, triangle).Y > 0.0);
    const uint32_t least = *std::ranges::min_element(corners);
    const uint32_t most = *std::ranges::max_element(corners);
    const size_t pair = least / 2;
    REQUIRE(pair < pairs);
    CHECK(most <= 2 * pair + 3);
    byPair[pair].push_back(corners);
  }
  for (size_t pair = 0; pair < pairs; ++pair) {
    INFO("pair " << pair);
    REQUIRE(byPair[pair].size() == 2);
    const std::set<uint32_t> first(byPair[pair][0].begin(), byPair[pair][0].end());
    const std::set<uint32_t> second(byPair[pair][1].begin(), byPair[pair][1].end());
    std::set<uint32_t> both = first;
    both.insert(second.begin(), second.end());
    CHECK(both.size() == 4);
    // Two triangles cover the quad only when the edge they share is one of its diagonals.
    std::set<uint32_t> shared;
    std::ranges::set_intersection(first, second, std::inserter(shared, shared.begin()));
    const auto k = static_cast<uint32_t>(2 * pair);
    CHECK((shared == std::set<uint32_t>{k, k + 3} || shared == std::set<uint32_t>{k + 1, k + 2}));
  }
}

TEST_CASE("appendPath adds nothing for a path whose ground line is empty") {
  const std::vector<PathCase> empties = {
      {"no points", PathKind::Guest, {}},
      {"one point", PathKind::Guest, {{0.0, 0.0}}},
      {"a second point within a centimeter of the first",
       PathKind::Backstage,
       {{0.0, 0.0}, {0.005, 0.005}}},
      {"a point outside the park", PathKind::Guest, {{0.0, 0.0}, {200.0, 0.0}}}};
  for (const PathCase &path : empties) {
    INFO(path.Name);
    REQUIRE(groundLine(path.Points).empty());
    ParkMesh mesh = heldMesh();
    const ParkMesh before = mesh;
    appendPath(mesh, path.Kind, path.Points);
    CHECK(sameMesh(mesh, before));
  }
}

TEST_CASE("appendBox adds a top and four sides over the footprint's corners, each with its "
          "outward unit normal") {
  for (const BoxCase &box : {SHOP_TURNED, DEPOT_WEST}) {
    INFO(box.Name);
    const Footprint footprint = requireFootprint(box.At, box.Size);
    const ParkMesh mesh = boxMesh(box);
    REQUIRE(mesh.Vertices.size() == 20);
    CHECK(mesh.Indices.size() == 30);

    for (const Face &face : facesOf(footprint, box.Height)) {
      INFO(face.Name << " face");
      const std::vector<uint32_t> vertices = verticesFacing(mesh, face);
      REQUIRE(vertices.size() == 4);
      for (const Triple &expected : face.Positions) {
        CHECK(std::ranges::any_of(vertices, [&](uint32_t index) {
          return near(positionOf(mesh.Vertices[index]), expected, POSITION_TOLERANCE);
        }));
      }
      for (const uint32_t index : vertices) {
        CHECK(std::ranges::any_of(face.Positions, [&](const Triple &expected) {
          return near(positionOf(mesh.Vertices[index]), expected, POSITION_TOLERANCE);
        }));
      }
    }
  }
}

TEST_CASE("appendBox covers each face with two triangles wound toward its outward normal") {
  const Footprint footprint = requireFootprint(SHOP_TURNED.At, SHOP_TURNED.Size);
  const ParkMesh mesh = boxMesh(SHOP_TURNED);
  REQUIRE(mesh.Vertices.size() == 20);
  REQUIRE(mesh.Indices.size() == 30);

  for (const Face &face : facesOf(footprint, SHOP_TURNED.Height)) {
    INFO(face.Name << " face");
    const std::vector<uint32_t> vertices = verticesFacing(mesh, face);
    REQUIRE(vertices.size() == 4);
    const std::set<uint32_t> faceVertices(vertices.begin(), vertices.end());

    std::vector<std::set<uint32_t>> triangles;
    for (size_t triangle = 0; triangle < mesh.Indices.size() / 3; ++triangle) {
      const std::array<uint32_t, 3> corners = triangleAt(mesh, triangle);
      if (std::ranges::all_of(corners,
                              [&](uint32_t index) { return faceVertices.contains(index); })) {
        CHECK(dot(windingOf(mesh, triangle), face.Normal) > 0.0);
        triangles.emplace_back(corners.begin(), corners.end());
      }
    }
    REQUIRE(triangles.size() == 2);
    std::set<uint32_t> both = triangles[0];
    both.insert(triangles[1].begin(), triangles[1].end());
    CHECK(both == faceVertices);

    // Two triangles cover the face only when the edge they share is one of its diagonals, the
    // longest distance between two of its corners.
    std::vector<uint32_t> shared;
    std::ranges::set_intersection(triangles[0], triangles[1], std::back_inserter(shared));
    REQUIRE(shared.size() == 2);
    double diagonal = 0.0;
    for (const uint32_t a : vertices) {
      for (const uint32_t b : vertices) {
        diagonal =
            std::max(diagonal, length(positionOf(mesh.Vertices[a]) - positionOf(mesh.Vertices[b])));
      }
    }
    const double sharedEdge =
        length(positionOf(mesh.Vertices[shared[0]]) - positionOf(mesh.Vertices[shared[1]]));
    CHECK(std::abs(sharedEdge - diagonal) <= POSITION_TOLERANCE);
  }
}

TEST_CASE("appendBox gives the front face the color lightened and every other face the color") {
  for (const BoxCase &box : {SHOP_TURNED, DEPOT_WEST}) {
    INFO(box.Name);
    const Footprint footprint = requireFootprint(box.At, box.Size);
    const ParkMesh mesh = boxMesh(box);
    REQUIRE(mesh.Vertices.size() == 20);
    for (const Face &face : facesOf(footprint, box.Height)) {
      INFO(face.Name << " face");
      const Rgba expected = face.Name == "front" ? lightened(box.Color) : box.Color;
      const std::vector<uint32_t> vertices = verticesFacing(mesh, face);
      REQUIRE(vertices.size() == 4);
      for (const uint32_t index : vertices) {
        CHECK(mesh.Vertices[index].Color == expected);
      }
    }
  }
}

TEST_CASE("lightened moves red, green, and blue 0.4 of the way to 1 and keeps alpha") {
  // Channels at both ends of the range and between, and an alpha that is not 1.
  const Rgba color = lightened(Rgba{0.0f, 0.5f, 1.0f, 0.25f});
  CHECK(std::abs(color.R - 0.4f) <= UNIT_TOLERANCE);
  CHECK(std::abs(color.G - 0.7f) <= UNIT_TOLERANCE);
  CHECK(std::abs(color.B - 1.0f) <= UNIT_TOLERANCE);
  CHECK(color.A == 0.25f);
}

TEST_CASE("appendBox adds nothing for a pose with no footprint") {
  const std::vector<Pose> noFootprint = {
      Pose{5.0, 5.0, 0.0, 0.0}, Pose{std::numeric_limits<double>::infinity(), 0.0, 0.0, -1.0}};
  for (const Pose &pose : noFootprint) {
    INFO("pose at x " << pose.X << " facing (" << pose.FacingX << ", " << pose.FacingZ << ")");
    REQUIRE_FALSE(footprintOf(pose, boxSize(BoxKind::Shop)).has_value());
    ParkMesh mesh = heldMesh();
    const ParkMesh before = mesh;
    appendBox(mesh, pose, boxSize(BoxKind::Shop), 4.0f, boxColor(BoxKind::Shop));
    CHECK(sameMesh(mesh, before));
  }
}

TEST_CASE("Appending keeps what the mesh held and adds only indices naming the vertices it adds") {
  ParkMesh mesh = heldMesh();
  const auto checkAppend = [&mesh](const ParkMesh &before) {
    REQUIRE(mesh.Vertices.size() > before.Vertices.size());
    REQUIRE(mesh.Indices.size() > before.Indices.size());
    CHECK(std::ranges::equal(
        before.Vertices,
        std::vector<ParkVertex>(mesh.Vertices.begin(),
                                mesh.Vertices.begin() +
                                    static_cast<std::ptrdiff_t>(before.Vertices.size())),
        sameVertex));
    CHECK(std::ranges::equal(
        before.Indices, std::vector<uint32_t>(mesh.Indices.begin(),
                                              mesh.Indices.begin() + static_cast<std::ptrdiff_t>(
                                                                         before.Indices.size()))));
    for (size_t at = before.Indices.size(); at < mesh.Indices.size(); ++at) {
      CHECK(mesh.Indices[at] >= before.Vertices.size());
      CHECK(mesh.Indices[at] < mesh.Vertices.size());
    }
  };

  SECTION("appendPath") {
    const ParkMesh before = mesh;
    appendPath(mesh, GUEST_CURVE.Kind, GUEST_CURVE.Points);
    checkAppend(before);
  }
  SECTION("appendBox") {
    const ParkMesh before = mesh;
    appendBox(mesh, DEPOT_WEST.At, DEPOT_WEST.Size, DEPOT_WEST.Height, DEPOT_WEST.Color);
    checkAppend(before);
  }
}

TEST_CASE("meshBounds gives the least ground rectangle holding every vertex's x and z") {
  // The extremes of x and z at different vertices, and a height that must not count.
  ParkMesh mesh;
  mesh.Vertices = {ParkVertex{{-3.0f, 5.0f, 2.0f}, {0.0f, 1.0f, 0.0f}, {}},
                   ParkVertex{{4.0f, 0.0f, -7.0f}, {0.0f, 1.0f, 0.0f}, {}},
                   ParkVertex{{1.0f, 900.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {}}};
  const std::optional<GroundBounds> bounds = meshBounds(mesh);
  REQUIRE(bounds.has_value());
  const GroundBounds rectangle = bounds.value_or(GroundBounds{});
  CHECK(rectangle.MinX == -3.0f);
  CHECK(rectangle.MaxX == 4.0f);
  CHECK(rectangle.MinZ == -7.0f);
  CHECK(rectangle.MaxZ == 2.0f);

  SECTION("a single vertex bounds a rectangle of no extent") {
    const ParkMesh single{{ParkVertex{{6.0f, 1.0f, -2.5f}, {0.0f, 1.0f, 0.0f}, {}}}, {}};
    const GroundBounds point = meshBounds(single).value_or(GroundBounds{});
    REQUIRE(meshBounds(single).has_value());
    CHECK(point.MinX == 6.0f);
    CHECK(point.MaxX == 6.0f);
    CHECK(point.MinZ == -2.5f);
    CHECK(point.MaxZ == -2.5f);
  }
}

TEST_CASE("meshBounds gives none for a mesh with no vertices") {
  CHECK_FALSE(meshBounds(ParkMesh{}).has_value());
}

TEST_CASE("buildParkMesh appends each entrance, then each path, then each box, in key order") {
  const World world = mixedPark();
  REQUIRE(parkEntrances(world).size() == 2);
  REQUIRE(parkPaths(world).size() == 2);
  REQUIRE(parkBoxes(world).size() == 3);

  ParkMesh expected;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    appendBox(expected, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT, ENTRANCE_COLOR);
  }
  for (const ParkPath &path : parkPaths(world)) {
    appendPath(expected, path.Kind, path.Points);
  }
  for (const ParkBox &box : parkBoxes(world)) {
    appendBox(expected, box.At, boxSize(box.Kind), boxHeight(box.Kind), boxColor(box.Kind));
  }
  REQUIRE_FALSE(expected.Vertices.empty());
  CHECK(sameMesh(buildParkMesh(world), expected));
}

TEST_CASE("An entrance is 5 m tall, a shop 4 m, and a depot 6 m") {
  CHECK(ENTRANCE_HEIGHT == 5.0f);
  CHECK(boxHeight(BoxKind::Shop) == 4.0f);
  CHECK(boxHeight(BoxKind::Depot) == 6.0f);
}

TEST_CASE("Guest paths, backstage paths, shops, depots, and the entrance have distinct, opaque "
          "colors") {
  const std::array<Rgba, 5> colors = {pathColor(PathKind::Guest), pathColor(PathKind::Backstage),
                                      boxColor(BoxKind::Shop), boxColor(BoxKind::Depot),
                                      ENTRANCE_COLOR};
  for (size_t i = 0; i < colors.size(); ++i) {
    INFO("color " << i);
    CHECK(colors[i].A == 1.0f);
    for (size_t j = i + 1; j < colors.size(); ++j) {
      CHECK_FALSE(colors[i] == colors[j]);
    }
  }
}

// The mesh is derived, so building it must leave nothing behind in what is saved or hashed.
TEST_CASE("Building a world's park mesh leaves its save and hash unchanged") {
  const World world = mixedPark();
  const std::string save = saveWorld(world);
  const uint64_t hash = hashWorld(world);
  const ParkMesh mesh = buildParkMesh(world);
  REQUIRE_FALSE(mesh.Vertices.empty());
  CHECK(saveWorld(world) == save);
  CHECK(hashWorld(world) == hash);
}

} // namespace
} // namespace tpj
