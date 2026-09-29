#include "render/park_mesh.h"

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/geometry.h"
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
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <set>
#include <sstream>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
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

// A path's or walkway's ribbon along n points: its first 2n vertices and its first two triangles
// per pair of consecutive points, its round joints following both.
ParkMesh ribbonOf(const ParkMesh &mesh, size_t points) {
  REQUIRE(mesh.Vertices.size() >= 2 * points);
  REQUIRE(mesh.Indices.size() >= 6 * (points - 1));
  return {
      {mesh.Vertices.begin(), mesh.Vertices.begin() + static_cast<std::ptrdiff_t>(2 * points)},
      {mesh.Indices.begin(), mesh.Indices.begin() + static_cast<std::ptrdiff_t>(6 * (points - 1))}};
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

// Resolved, as every world the app holds is, so its entrance's front door has a walkway to the
// guest path.
World mixedPark() {
  World world = loadWorld(makeParkSchema(), MIXED_PARK);
  resolveWorld(world);
  return world;
}

TEST_CASE(
    "A guest path lies 4 cm above the ground and a backstage path 2 cm, so a guest ribbon lies "
    "above a backstage one by at least as much as a backstage one lies above the terrain") {
  CHECK(pathLift(PathKind::Guest) == 0.04f);
  CHECK(pathLift(PathKind::Backstage) == 0.02f);
  CHECK(pathLift(PathKind::Guest) - pathLift(PathKind::Backstage) >= pathLift(PathKind::Backstage));
}

TEST_CASE("appendPath begins with two vertices per ground line point, left edge then right, "
          "pathWidth apart across the tangent and centered on the point at pathLift(kind)") {
  for (const PathCase &path : {GUEST_CURVE, BACKSTAGE_LINE, GUEST_REVERSAL}) {
    INFO(path.Name);
    const std::vector<CarrierPoint> line = requireLine(path);
    if (path.Name == GUEST_REVERSAL.Name) {
      REQUIRE(hasReversal(line));
    }
    ParkMesh whole;
    appendPath(whole, path.Kind, path.Points);
    const size_t n = line.size();
    const ParkMesh mesh = ribbonOf(whole, n);

    const double width = pathWidth(path.Kind);
    const double lift = pathLift(path.Kind);
    for (size_t i = 0; i < n; ++i) {
      INFO("point " << i);
      const Triple left = positionOf(mesh.Vertices[2 * i]);
      const Triple right = positionOf(mesh.Vertices[2 * i + 1]);
      const Triple middle{(left.X + right.X) / 2.0, (left.Y + right.Y) / 2.0,
                          (left.Z + right.Z) / 2.0};
      CHECK(near(middle, {line[i].X, lift, line[i].Z}, POSITION_TOLERANCE));
      const ParkPoint tangent = tangentAt(line, i);
      const Triple across{-tangent.Z * width, 0.0, tangent.X * width};
      CHECK(near(right - left, across, POSITION_TOLERANCE));
    }
  }
}

TEST_CASE("appendPath's vertices, ribbon and joints, have upward normals and the kind's path "
          "color") {
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

// A ribbon's triangles grouped by the pair of consecutive points whose quad they lie on, checking
// that each faces up and names only that pair's four vertices. Point k's vertices are 2k, its left
// edge, and 2k + 1, its right edge.
std::vector<std::vector<std::array<uint32_t, 3>>> trianglesByPair(const ParkMesh &mesh,
                                                                  size_t pairs) {
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
  return byPair;
}

// Checks that a ribbon of pairs + 1 points covers the quad between each pair of consecutive
// points' four vertices with two triangles facing up.
void checkRibbonQuads(const ParkMesh &mesh, size_t pairs) {
  REQUIRE(mesh.Vertices.size() == 2 * (pairs + 1));
  REQUIRE(mesh.Indices.size() == 6 * pairs);
  const auto byPair = trianglesByPair(mesh, pairs);
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

TEST_CASE("appendPath's first triangles cover the quad between each pair of consecutive points "
          "with two triangles wound to face up") {
  const std::vector<CarrierPoint> line = requireLine(GUEST_CURVE);
  ParkMesh mesh;
  appendPath(mesh, GUEST_CURVE.Kind, GUEST_CURVE.Points);
  checkRibbonQuads(ribbonOf(mesh, line.size()), line.size() - 1);
}

// A round joint's rim has one more vertex than its segments, since a half circle is open.
constexpr size_t JOINT_RIM = JOINT_SEGMENTS + 1;
constexpr size_t JOINT_VERTICES = 1 + JOINT_RIM;

// Where a joint lies in a mesh: the index of its center vertex, followed by its rim, and of its
// first triangle, followed by the rest of its triangles.
struct JointAt {
  size_t Center = 0;
  size_t FirstTriangle = 0;
};

// A joint's triangles, one per segment.
constexpr size_t JOINT_TRIANGLES = JOINT_SEGMENTS;

// Checks the vertices of the round joint closing a ribbon of a width at a lift at the end point p,
// with t the unit direction pointing away from the ribbon there: its center at p, then its k-th rim
// vertex at p + h * (cos(πk/16) * r + sin(πk/16) * t), with h half the width and r = (-t.z, t.x).
void checkJointVertices(const ParkMesh &mesh, size_t center, const CarrierPoint &p, ParkPoint t,
                        double width, double lift) {
  REQUIRE(JOINT_SEGMENTS == 16);
  REQUIRE(mesh.Vertices.size() >= center + JOINT_VERTICES);
  CHECK(near(positionOf(mesh.Vertices[center]), {p.X, lift, p.Z}, POSITION_TOLERANCE));
  const double half = width / 2.0;
  const ParkPoint r{-t.Z, t.X};
  for (size_t k = 0; k < JOINT_RIM; ++k) {
    INFO("rim vertex " << k);
    const double angle = std::numbers::pi * static_cast<double>(k) / JOINT_SEGMENTS;
    const double across = half * std::cos(angle);
    const double beyond = half * std::sin(angle);
    const Triple expected{p.X + across * r.X + beyond * t.X, lift,
                          p.Z + across * r.Z + beyond * t.Z};
    CHECK(near(positionOf(mesh.Vertices[center + 1 + k]), expected, POSITION_TOLERANCE));
  }
}

// Checks that a joint's triangles are one facing up from its center to each rim vertex and the
// next. A half circle is open, so none joins its last rim vertex back to its first.
void checkJointTriangles(const ParkMesh &mesh, JointAt at) {
  REQUIRE(mesh.Indices.size() >= 3 * (at.FirstTriangle + JOINT_TRIANGLES));
  const auto center = static_cast<uint32_t>(at.Center);
  std::set<std::set<uint32_t>> expected;
  for (uint32_t k = 0; k < JOINT_SEGMENTS; ++k) {
    expected.insert({center, center + 1 + k, center + 2 + k});
  }
  std::set<std::set<uint32_t>> found;
  for (size_t triangle = at.FirstTriangle; triangle < at.FirstTriangle + JOINT_TRIANGLES;
       ++triangle) {
    INFO("triangle " << triangle);
    const std::array<uint32_t, 3> corners = triangleAt(mesh, triangle);
    CHECK(windingOf(mesh, triangle).Y > 0.0);
    found.emplace(corners.begin(), corners.end());
  }
  CHECK(found == expected);
}

// Checks the round joint a mesh holds at a place, closing a ribbon of a width at a lift at the end
// point p, with t the unit direction pointing away from the ribbon there.
void checkJoint(const ParkMesh &mesh, JointAt at, const CarrierPoint &p, ParkPoint t, double width,
                double lift) {
  checkJointVertices(mesh, at.Center, p, t, width, lift);
  checkJointTriangles(mesh, at);
}

ParkPoint reversed(ParkPoint direction) { return {-direction.X, -direction.Z}; }

// The two cases differ in kind, so in width and lift, and in direction, and each ground line's
// first and last segments run different ways, so each joint must be turned to its own end.
TEST_CASE("appendPath follows its ribbon with a round joint beyond its first point and then one "
          "beyond its last, each of JOINT_SEGMENTS + 1, 17, rim vertices at pathLift(kind)") {
  for (const PathCase &path : {GUEST_CURVE, BACKSTAGE_LINE}) {
    INFO(path.Name);
    const std::vector<CarrierPoint> line = requireLine(path);
    const size_t n = line.size();
    ParkMesh mesh;
    appendPath(mesh, path.Kind, path.Points);
    REQUIRE(mesh.Vertices.size() == 2 * n + 2 * JOINT_VERTICES);
    REQUIRE(mesh.Indices.size() == 3 * (2 * (n - 1) + 2 * JOINT_TRIANGLES));

    const JointAt first{2 * n, 2 * (n - 1)};
    const JointAt last{first.Center + JOINT_VERTICES, first.FirstTriangle + JOINT_TRIANGLES};
    {
      INFO("the joint beyond the first point");
      checkJoint(mesh, first, line.front(), reversed(unitStep(line[0], line[1])),
                 pathWidth(path.Kind), pathLift(path.Kind));
    }
    {
      INFO("the joint beyond the last point");
      checkJoint(mesh, last, line.back(), unitStep(line[n - 2], line[n - 1]), pathWidth(path.Kind),
                 pathLift(path.Kind));
    }
  }
}

// Whether a ground point lies inside, or on an edge of, a triangle of the mesh wound to face up.
bool coveredFromAbove(const ParkMesh &mesh, double x, double z) {
  // Points on the edge two triangles share must count for one of them despite rounding.
  constexpr double EDGE_TOLERANCE = 1e-4;
  for (size_t triangle = 0; triangle < mesh.Indices.size() / 3; ++triangle) {
    if (windingOf(mesh, triangle).Y <= 0.0) {
      continue;
    }
    const auto [a, b, c] = triangleAt(mesh, triangle);
    const std::array<Triple, 3> corners = {positionOf(mesh.Vertices.at(a)),
                                           positionOf(mesh.Vertices.at(b)),
                                           positionOf(mesh.Vertices.at(c))};
    bool inside = true;
    for (size_t edge = 0; edge < 3; ++edge) {
      const Triple &from = corners.at(edge);
      const Triple &to = corners.at((edge + 1) % 3);
      // The y of cross(to - from, point - from), positive when the point lies on the side an
      // upward-facing triangle's interior does.
      const double side = (to.Z - from.Z) * (x - from.X) - (to.X - from.X) * (z - from.Z);
      inside = inside && side >= -EDGE_TOLERANCE;
    }
    if (inside) {
      return true;
    }
  }
  return false;
}

struct MeetingCase {
  std::string_view Name;
  PathKind Kind;
  // The angle between the two paths at the end point they share.
  double Angle;
};

// A right angle, where square ends leave the plainest notch, a sharp angle, where the outer corner
// is widest, and a shallow one, where the notch is a thin wedge; the last in the other kind, so the
// width differs.
const std::array<MeetingCase, 3> MEETINGS = {
    {{"guest paths at a right angle", PathKind::Guest, std::numbers::pi / 2.0},
     {"guest paths at 30 degrees", PathKind::Guest, std::numbers::pi / 6.0},
     {"backstage paths at 150 degrees", PathKind::Backstage, 5.0 * std::numbers::pi / 6.0}}};

TEST_CASE("Two straight paths of one kind whose ends meet at a point, at any angle, cover every "
          "ground point within h * cos(pi/32) of it with an upward-facing triangle") {
  // The paths meet at a point off the axes, one ending there and the other starting there, so both
  // joints serve, and each runs 10 m straight into it, more than half its width.
  const ParkPoint end{10.0, 20.0};
  const ParkPoint along{0.6, 0.8};
  constexpr double LENGTH = 10.0;
  for (const MeetingCase &meeting : MEETINGS) {
    INFO(meeting.Name);
    const double cosine = std::cos(meeting.Angle);
    const double sine = std::sin(meeting.Angle);
    // Unit directions from the end point back along each path.
    const ParkPoint towardFirst = along;
    const ParkPoint towardSecond{cosine * along.X - sine * along.Z,
                                 sine * along.X + cosine * along.Z};
    const std::vector<ParkPoint> first{
        {end.X + LENGTH * towardFirst.X, end.Z + LENGTH * towardFirst.Z}, end};
    const std::vector<ParkPoint> second{
        end, {end.X + LENGTH * towardSecond.X, end.Z + LENGTH * towardSecond.Z}};
    const std::vector<CarrierPoint> firstLine = groundLine(first);
    const std::vector<CarrierPoint> secondLine = groundLine(second);
    REQUIRE(firstLine.size() >= 2);
    REQUIRE(secondLine.size() >= 2);
    REQUIRE(near({firstLine.back().X, 0.0, firstLine.back().Z}, {end.X, 0.0, end.Z},
                 POSITION_TOLERANCE));
    REQUIRE(near({secondLine.front().X, 0.0, secondLine.front().Z}, {end.X, 0.0, end.Z},
                 POSITION_TOLERANCE));

    ParkMesh mesh;
    appendPath(mesh, meeting.Kind, first);
    appendPath(mesh, meeting.Kind, second);

    // Just inside the distance from a joint's center to its nearest rim edge.
    const double reach =
        pathWidth(meeting.Kind) / 2.0 * std::cos(std::numbers::pi / 32.0) - POSITION_TOLERANCE;
    const double sumX = towardFirst.X + towardSecond.X;
    const double sumZ = towardFirst.Z + towardSecond.Z;
    const double sumLength = std::sqrt(sumX * sumX + sumZ * sumZ);
    // The outer bisector, away from both paths, is where square ends leave their notch; straight
    // beyond each path's end, and across it at its corners, where its ribbon and joint meet.
    std::vector<std::pair<std::string_view, ParkPoint>> directions = {
        {"the outer bisector", {-sumX / sumLength, -sumZ / sumLength}}};
    for (const ParkPoint &back : {towardFirst, towardSecond}) {
      directions.emplace_back("beyond a path's end", reversed(back));
      directions.emplace_back("a path's left corner", ParkPoint{-back.Z, back.X});
      directions.emplace_back("a path's right corner", ParkPoint{back.Z, -back.X});
    }
    CHECK(coveredFromAbove(mesh, end.X, end.Z));
    for (const auto &[name, direction] : directions) {
      INFO(name << " (" << direction.X << ", " << direction.Z << ")");
      CHECK(coveredFromAbove(mesh, end.X + reach * direction.X, end.Z + reach * direction.Z));
    }
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

struct WalkwayCase {
  std::string_view Name;
  PathKind Kind;
  std::vector<CarrierPoint> Points;
};

// A connector's two points, on a diagonal so the across direction is along no axis.
const WalkwayCase BACKSTAGE_CONNECTOR{"a backstage connector on a diagonal",
                                      PathKind::Backstage,
                                      {{10.0, -20.0, 0.0}, {13.0, -16.0, 5.0}}};
// A connector's reach need only exceed a millimeter, so its points can lie closer than
// MIN_POINT_SPACING, which a ground line would merge.
const WalkwayCase SHORT_CONNECTOR{
    "a guest connector 5 mm long", PathKind::Guest, {{0.0, 0.0, 0.0}, {0.003, 0.004, 0.005}}};
// A line that bends, so a point between has a tangent from the segments on both sides.
const WalkwayCase GUEST_BEND{"a guest line that bends",
                             PathKind::Guest,
                             {{0.0, 0.0, 0.0}, {30.0, 0.0, 30.0}, {50.0, 20.0, 58.2842712474619}}};

// A color no kind or tint uses, translucent so its alpha must be kept as given.
constexpr Rgba WALKWAY_MARK{0.3f, 0.6f, 0.9f, 0.4f};

ParkMesh walkwayMesh(const WalkwayCase &walkway) {
  ParkMesh mesh;
  appendWalkway(mesh, walkway.Kind, walkway.Points, WALKWAY_MARK);
  return mesh;
}

std::string routesText() {
  std::ifstream file(TPJ_PARKS_DIR "/routes.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// A park whose doors connect to both networks, two to each, beside carriers that are paths.
World routesPark() {
  World world = loadWorld(makeParkSchema(), routesText());
  resolveWorld(world);
  return world;
}

bool isConnector(const Network &network, const Carrier &carrier) {
  REQUIRE_FALSE(carrier.Stops.empty());
  return network.nodeAnchor(carrier.Stops.front().Node) != NULL_KEY;
}

TEST_CASE("appendWalkway begins with two vertices per point, left edge then right, pathWidth "
          "apart across the tangent and centered on the point at pathLift(kind)") {
  for (const WalkwayCase &walkway : {BACKSTAGE_CONNECTOR, SHORT_CONNECTOR, GUEST_BEND}) {
    INFO(walkway.Name);
    const std::vector<CarrierPoint> &line = walkway.Points;
    const size_t n = line.size();
    const ParkMesh mesh = ribbonOf(walkwayMesh(walkway), n);

    const double width = pathWidth(walkway.Kind);
    for (size_t i = 0; i < n; ++i) {
      INFO("point " << i);
      const Triple left = positionOf(mesh.Vertices[2 * i]);
      const Triple right = positionOf(mesh.Vertices[2 * i + 1]);
      const Triple middle{(left.X + right.X) / 2.0, (left.Y + right.Y) / 2.0,
                          (left.Z + right.Z) / 2.0};
      CHECK(near(middle, {line[i].X, pathLift(walkway.Kind), line[i].Z}, POSITION_TOLERANCE));
      const ParkPoint tangent = tangentAt(line, i);
      const Triple across{-tangent.Z * width, 0.0, tangent.X * width};
      CHECK(near(right - left, across, POSITION_TOLERANCE));
    }
  }
}

TEST_CASE("appendWalkway's vertices, ribbon and joint, have upward normals and the color given") {
  for (const WalkwayCase &walkway : {BACKSTAGE_CONNECTOR, GUEST_BEND}) {
    INFO(walkway.Name);
    const ParkMesh mesh = walkwayMesh(walkway);
    REQUIRE_FALSE(mesh.Vertices.empty());
    for (const ParkVertex &vertex : mesh.Vertices) {
      CHECK(near(normalOf(vertex), UP, UNIT_TOLERANCE));
      CHECK(vertex.Color == WALKWAY_MARK);
    }
  }
}

TEST_CASE("appendWalkway's first triangles cover the quad between each pair of consecutive points "
          "with two triangles wound to face up") {
  for (const WalkwayCase &walkway : {BACKSTAGE_CONNECTOR, GUEST_BEND}) {
    INFO(walkway.Name);
    const size_t n = walkway.Points.size();
    checkRibbonQuads(ribbonOf(walkwayMesh(walkway), n), n - 1);
  }
}

// The two cases end at different points in different widths and lifts along different
// directions, and the bend's last point is not its second, so the joint must be placed at the last
// point and turned to the last segment.
TEST_CASE("appendWalkway follows its ribbon with a round joint beyond its last point, of "
          "JOINT_SEGMENTS + 1, 17, rim vertices at pathLift(kind)") {
  for (const WalkwayCase &walkway : {BACKSTAGE_CONNECTOR, GUEST_BEND}) {
    INFO(walkway.Name);
    const ParkMesh mesh = walkwayMesh(walkway);
    const std::vector<CarrierPoint> &line = walkway.Points;
    const size_t n = line.size();
    REQUIRE(mesh.Vertices.size() == 2 * n + JOINT_VERTICES);
    REQUIRE(mesh.Indices.size() == 3 * (2 * (n - 1) + JOINT_TRIANGLES));
    checkJoint(mesh, JointAt{2 * n, 2 * (n - 1)}, line.back(), unitStep(line[n - 2], line[n - 1]),
               pathWidth(walkway.Kind), pathLift(walkway.Kind));
  }
}

// A face's unit normal oblique to each walkway's first segment, each giving a move along it shorter
// than the segment: 0.75 m of the backstage connector's 5 m, and 2 m of the bend's 30 m.
const std::array<std::pair<WalkwayCase, ParkPoint>, 2> OBLIQUE_FACES = {
    {{BACKSTAGE_CONNECTOR, {0.0, 1.0}}, {GUEST_BEND, {0.6, 0.8}}}};

TEST_CASE("appendWalkway given a face's normal moves its first left and right vertices along the "
          "first segment, opposite ways, onto the face's line through the first point") {
  for (const auto &[walkway, normal] : OBLIQUE_FACES) {
    INFO(walkway.Name);
    ParkMesh mesh;
    appendWalkway(mesh, walkway.Kind, walkway.Points, normal, WALKWAY_MARK);
    REQUIRE(mesh.Vertices.size() >= 2);

    const CarrierPoint &first = walkway.Points[0];
    const ParkPoint t = unitStep(walkway.Points[0], walkway.Points[1]);
    const ParkPoint r{-t.Z, t.X};
    const double half = pathWidth(walkway.Kind) / 2.0;
    const double move =
        half * (r.X * normal.X + r.Z * normal.Z) / (t.X * normal.X + t.Z * normal.Z);
    const Triple left{first.X - half * r.X + move * t.X, pathLift(walkway.Kind),
                      first.Z - half * r.Z + move * t.Z};
    const Triple right{first.X + half * r.X - move * t.X, pathLift(walkway.Kind),
                       first.Z + half * r.Z - move * t.Z};
    CHECK(near(positionOf(mesh.Vertices[0]), left, POSITION_TOLERANCE));
    CHECK(near(positionOf(mesh.Vertices[1]), right, POSITION_TOLERANCE));
  }
}

TEST_CASE("appendWalkway given a face's normal keeps every vertex but its first two positions, and "
          "every index, as it draws them without one") {
  for (const auto &[walkway, normal] : OBLIQUE_FACES) {
    INFO(walkway.Name);
    // Appended to a mesh already holding vertices, so the indices must match past them too.
    ParkMesh square = heldMesh();
    appendWalkway(square, walkway.Kind, walkway.Points, WALKWAY_MARK);
    ParkMesh flush = heldMesh();
    appendWalkway(flush, walkway.Kind, walkway.Points, normal, WALKWAY_MARK);
    REQUIRE(flush.Vertices.size() == square.Vertices.size());
    CHECK(flush.Indices == square.Indices);

    const size_t firstLeft = heldMesh().Vertices.size();
    for (size_t index = 0; index < flush.Vertices.size(); ++index) {
      INFO("vertex " << index);
      if (index == firstLeft || index == firstLeft + 1) {
        CHECK(std::ranges::equal(flush.Vertices[index].Normal, square.Vertices[index].Normal));
        CHECK(flush.Vertices[index].Color == square.Vertices[index].Color);
      } else {
        CHECK(sameVertex(flush.Vertices[index], square.Vertices[index]));
      }
    }
  }
}

// Moving the start along a first segment the face's line runs along, or past the segment's end,
// would fold the ribbon.
TEST_CASE("appendWalkway given a face's normal keeps its square start when the first segment runs "
          "along the face or the move is longer than the first segment") {
  const std::array<std::pair<WalkwayCase, ParkPoint>, 2> folding = {
      {// The bend's first segment runs along +x, so dot(t, n) is exactly 0.
       {GUEST_BEND, {0.0, -1.0}},
       // A move of 1.125 m along a first segment 5 mm long.
       {SHORT_CONNECTOR, {0.0, 1.0}}}};
  for (const auto &[walkway, normal] : folding) {
    INFO(walkway.Name);
    ParkMesh flush;
    appendWalkway(flush, walkway.Kind, walkway.Points, normal, WALKWAY_MARK);
    CHECK(sameMesh(flush, walkwayMesh(walkway)));
  }
}

TEST_CASE("appendWalkway adds nothing for fewer than two points") {
  const std::vector<std::vector<CarrierPoint>> fewer = {{}, {{5.0, 5.0, 0.0}}};
  for (const std::vector<CarrierPoint> &points : fewer) {
    INFO(points.size() << " points");
    ParkMesh mesh = heldMesh();
    appendWalkway(mesh, PathKind::Guest, points, WALKWAY_MARK);
    CHECK(sameMesh(mesh, heldMesh()));
  }
}

// Doors whose connectors leave them at an angle to their faces, each reaching the end of a path:
// the entrance's front door, at (0, 125), and the shop's, at (-7, 105), reach guest paths, and the
// shop's back door, at (-13, 105), a backstage path. Every path stays more than its half width from
// every footprint.
constexpr std::string_view ANGLED_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 6\n"
                                         "\n[entrance]\n"
                                         "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
                                         "\n[path]\n"
                                         "2 kind=guest points=[{x=2 z=122} {x=2 z=100}]\n"
                                         "3 kind=guest points=[{x=-5 z=103} {x=-5 z=90}]\n"
                                         "4 kind=backstage points=[{x=-15 z=107} {x=-30 z=107}]\n"
                                         "\n[box]\n"
                                         "5 kind=shop x=-10 z=105 facing-x=1 facing-z=0\n";

World angledPark() {
  World world = loadWorld(makeParkSchema(), ANGLED_PARK);
  resolveWorld(world);
  return world;
}

// Forward of the footprint of the entrance or box the key names, or none when it has no footprint.
std::optional<ParkPoint> forwardOf(const World &world, EntityKey entity) {
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    if (entrance.Key == entity) {
      const std::optional<Footprint> footprint = footprintOf(entrance.At, ENTRANCE_SIZE);
      return footprint ? std::optional<ParkPoint>(footprint->Forward) : std::nullopt;
    }
  }
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == entity) {
      const std::optional<Footprint> footprint = footprintOf(box.At, boxSize(box.Kind));
      return footprint ? std::optional<ParkPoint>(footprint->Forward) : std::nullopt;
    }
  }
  FAIL("a connector is anchored to neither an entrance nor a box");
  return std::nullopt;
}

TEST_CASE("appendWalkways adds a walkway for each connector, guest network then backstage, in "
          "carrier key order, starting flush with its door's face, in the kind's path color with "
          "the alpha given") {
  const World world = angledPark();
  // An alpha neither opaque nor GHOST_ALPHA, so it must come from the argument.
  constexpr float ALPHA = 0.25f;

  ParkMesh expected;
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    const Network &network = parkNetwork(world, kind);
    const auto connectors = std::ranges::count_if(
        network.carriers(), [&](const Carrier &carrier) { return isConnector(network, carrier); });
    // The guest network holds two connectors, so their order shows, and each network holds
    // carriers that are paths, which draw no walkway.
    REQUIRE(connectors == (kind == PathKind::Guest ? 2 : 1));
    REQUIRE(std::cmp_greater(network.carriers().size(), connectors));
    Rgba color = pathColor(kind);
    color.A = ALPHA;
    for (const Carrier &carrier : network.carriers()) {
      if (isConnector(network, carrier)) {
        const std::optional<ParkPoint> normal =
            forwardOf(world, network.nodeAnchor(carrier.Stops.front().Node));
        REQUIRE(normal.has_value());
        // Each connector leaves its door at an angle, so its face moves its start.
        ParkMesh flush;
        appendWalkway(flush, kind, carrier.Points, normal, color);
        ParkMesh square;
        appendWalkway(square, kind, carrier.Points, color);
        REQUIRE_FALSE(sameMesh(flush, square));
        appendWalkway(expected, kind, carrier.Points, normal, color);
      }
    }
  }
  ParkMesh mesh;
  appendWalkways(mesh, world, ALPHA);
  CHECK(sameMesh(mesh, expected));
}

TEST_CASE("appendWalkways adds nothing for a world with no networks") {
  // A loaded world holds no networks until its first resolution.
  const World world = loadWorld(makeParkSchema(), routesText());
  REQUIRE(world.isResolvePending());
  REQUIRE(parkNetwork(world, PathKind::Guest).carriers().empty());
  REQUIRE(parkNetwork(world, PathKind::Backstage).carriers().empty());
  ParkMesh mesh = heldMesh();
  appendWalkways(mesh, world, 1.0f);
  CHECK(sameMesh(mesh, heldMesh()));
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
  SECTION("appendWalkway") {
    const ParkMesh before = mesh;
    appendWalkway(mesh, GUEST_BEND.Kind, GUEST_BEND.Points, WALKWAY_MARK);
    checkAppend(before);
  }
  SECTION("appendWalkways") {
    const ParkMesh before = mesh;
    appendWalkways(mesh, routesPark(), 1.0f);
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

TEST_CASE("buildParkMesh appends each entrance, then each path, then the walkways at alpha 1, then "
          "each box, in key order") {
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
  const size_t beforeWalkways = expected.Vertices.size();
  appendWalkways(expected, world, 1.0f);
  REQUIRE(expected.Vertices.size() > beforeWalkways);
  for (const ParkBox &box : parkBoxes(world)) {
    appendBox(expected, box.At, boxSize(box.Kind), boxHeight(box.Kind), boxColor(box.Kind));
  }
  REQUIRE_FALSE(expected.Vertices.empty());
  CHECK(sameMesh(buildParkMesh(world), expected));
}

TEST_CASE("buildParkMesh lays every vertex in a path kind's color at that kind's lift, paths and "
          "walkways alike") {
  // Guest and backstage paths cross there, and doors connect to both networks.
  const World world = routesPark();
  ParkMesh walkways;
  appendWalkways(walkways, world, 1.0f);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    REQUIRE(std::ranges::any_of(parkPaths(world),
                                [kind](const ParkPath &path) { return path.Kind == kind; }));
    REQUIRE(std::ranges::any_of(walkways.Vertices, [kind](const ParkVertex &vertex) {
      return vertex.Color == pathColor(kind);
    }));
  }

  const ParkMesh mesh = buildParkMesh(world);
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    INFO("kind " << static_cast<int>(kind));
    size_t count = 0;
    for (const ParkVertex &vertex : mesh.Vertices) {
      if (vertex.Color == pathColor(kind)) {
        ++count;
        CHECK(std::abs(vertex.Position[1] - pathLift(kind)) <= POSITION_TOLERANCE);
      }
    }
    CHECK(count > 0);
  }
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
TEST_CASE("Building a world's park mesh or its walkways leaves its save and hash unchanged") {
  const World world = mixedPark();
  const std::string save = saveWorld(world);
  const uint64_t hash = hashWorld(world);
  const ParkMesh mesh = buildParkMesh(world);
  REQUIRE_FALSE(mesh.Vertices.empty());
  ParkMesh walkways;
  appendWalkways(walkways, world, GHOST_ALPHA);
  REQUIRE_FALSE(walkways.Vertices.empty());
  CHECK(saveWorld(world) == save);
  CHECK(hashWorld(world) == hash);
}

} // namespace
} // namespace tpj
