#include "render/park_mesh.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

constexpr EntityKey ENTRANCE{1};
constexpr EntityKey BACKSTAGE{3};
constexpr EntityKey SHOP{4};
// The highest-keyed box, so buildParkMesh draws it last.
constexpr EntityKey DEPOT{5};
constexpr EntityKey MISSING{99};

// The template's entrance and guest path, a backstage path along x = -40 from z = -50 to 50, a shop
// covering x from 36 to 44 and z from -3 to 3, and a depot facing +x at (-80, -80).
constexpr std::string_view GHOST_PARK = "tpj-park 1\nseed 1\ntick 0\nnext-key 6\n"
                                        "\n[entrance]\n"
                                        "1 x=0 z=126.5 facing-x=0 facing-z=-1\n"
                                        "\n[path]\n"
                                        "2 kind=guest points=[{x=0 z=123} {x=0 z=103}]\n"
                                        "3 kind=backstage points=[{x=-40 z=-50} {x=-40 z=50}]\n"
                                        "\n[box]\n"
                                        "4 kind=shop x=40 z=0 facing-x=0 facing-z=-1\n"
                                        "5 kind=depot x=-80 z=-80 facing-x=1 facing-z=0\n";

World ghostPark() { return loadWorld(makeParkSchema(), GHOST_PARK); }

bool sameVertex(const ParkVertex &left, const ParkVertex &right) {
  return std::ranges::equal(left.Position, right.Position) &&
         std::ranges::equal(left.Normal, right.Normal) && left.Color == right.Color;
}

bool sameMesh(const ParkMesh &left, const ParkMesh &right) {
  return std::ranges::equal(left.Vertices, right.Vertices, sameVertex) &&
         left.Indices == right.Indices;
}

bool sameShape(const ParkVertex &left, const ParkVertex &right) {
  return std::ranges::equal(left.Position, right.Position) &&
         std::ranges::equal(left.Normal, right.Normal);
}

Rgba ghostOf(Rgba color) { return Rgba{color.R, color.G, color.B, GHOST_ALPHA}; }

ParkMesh boxOf(BoxKind kind, const Pose &at, Rgba color) {
  ParkMesh mesh;
  appendBox(mesh, at, boxSize(kind), boxHeight(kind), color);
  return mesh;
}

ParkMesh pathOf(PathKind kind, const std::vector<ParkPoint> &points, Rgba color) {
  ParkMesh mesh;
  appendPath(mesh, kind, points, color);
  return mesh;
}

ParkMesh entityOf(const World &world, EntityKey key, Rgba color) {
  ParkMesh mesh;
  appendEntity(mesh, world, key, color);
  return mesh;
}

bool isEmpty(const ParkMesh &mesh) { return mesh.Vertices.empty() && mesh.Indices.empty(); }

// A mesh already holding a triangle, so appending nothing must leave it as it was.
ParkMesh heldMesh() {
  const ParkVertex corner{{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, Rgba{0.1f, 0.2f, 0.3f, 1.0f}};
  ParkMesh mesh;
  mesh.Vertices = {corner, corner, corner};
  mesh.Vertices[1].Position[0] = 2.0f;
  mesh.Vertices[2].Position[2] = 0.0f;
  mesh.Indices = {0, 2, 1};
  return mesh;
}

// A color no park kind or tint uses, translucent so alpha must be kept as given.
constexpr Rgba MARK{0.3f, 0.6f, 0.9f, 0.4f};

TEST_CASE("GHOST_ALPHA is 0.5, and the tints are translucent and distinct in color from each other "
          "and from the kinds' colors") {
  CHECK(GHOST_ALPHA == 0.5f);
  const std::array<Rgba, 3> tints = {INVALID_TINT, DELETE_TINT, HIGHLIGHT_TINT};
  const std::array<Rgba, 4> kinds = {pathColor(PathKind::Guest), pathColor(PathKind::Backstage),
                                     boxColor(BoxKind::Shop), boxColor(BoxKind::Depot)};
  // Colors that differ only in alpha look alike on screen, so red, green, and blue must differ.
  const auto sameHue = [](Rgba left, Rgba right) {
    return left.R == right.R && left.G == right.G && left.B == right.B;
  };
  for (std::size_t i = 0; i < tints.size(); ++i) {
    INFO("tint " << i);
    CHECK(tints[i].A > 0.0f);
    CHECK(tints[i].A < 1.0f);
    for (std::size_t j = i + 1; j < tints.size(); ++j) {
      CHECK_FALSE(sameHue(tints[i], tints[j]));
    }
    for (const Rgba &kind : kinds) {
      CHECK_FALSE(sameHue(tints[i], kind));
    }
  }
}

TEST_CASE("appendPath given a color draws the ribbon appendPath draws, in that color") {
  const std::vector<ParkPoint> points{{0.0, 0.0}, {30.0, 0.0}, {50.0, 20.0}};
  ParkMesh plain;
  appendPath(plain, PathKind::Backstage, points);
  const ParkMesh colored = pathOf(PathKind::Backstage, points, MARK);
  REQUIRE_FALSE(plain.Vertices.empty());
  CHECK(std::ranges::equal(colored.Vertices, plain.Vertices, sameShape));
  CHECK(colored.Indices == plain.Indices);
  CHECK(std::ranges::all_of(colored.Vertices,
                            [](const ParkVertex &vertex) { return vertex.Color == MARK; }));
}

TEST_CASE("appendEntity draws the box or path a key holds as buildParkMesh draws it, in the color "
          "given") {
  const World world = ghostPark();

  SECTION("a box") {
    const ParkMesh expected = boxOf(BoxKind::Depot, Pose{-80.0, -80.0, 1.0, 0.0}, MARK);
    REQUIRE_FALSE(expected.Vertices.empty());
    CHECK(sameMesh(entityOf(world, DEPOT, MARK), expected));
  }
  SECTION("a path") {
    const std::vector<ParkPoint> points{{-40.0, -50.0}, {-40.0, 50.0}};
    ParkMesh drawn;
    appendPath(drawn, PathKind::Backstage, points);
    REQUIRE_FALSE(drawn.Vertices.empty());
    const ParkMesh mesh = entityOf(world, BACKSTAGE, MARK);
    CHECK(std::ranges::equal(mesh.Vertices, drawn.Vertices, sameShape));
    CHECK(mesh.Indices == drawn.Indices);
    CHECK(std::ranges::all_of(mesh.Vertices,
                              [](const ParkVertex &vertex) { return vertex.Color == MARK; }));
  }
}

TEST_CASE("appendEntity adds nothing when the key holds neither a box nor a path") {
  const World world = ghostPark();
  for (const EntityKey key : {ENTRANCE, MISSING, NULL_KEY}) {
    INFO("key " << static_cast<uint64_t>(key));
    ParkMesh mesh = heldMesh();
    appendEntity(mesh, world, key, MARK);
    CHECK(sameMesh(mesh, heldMesh()));
  }
}

TEST_CASE("An AddBox's ghost is its box in its kind's color at GHOST_ALPHA when accepted, and in "
          "INVALID_TINT when refused") {
  const World world = ghostPark();
  const AddBox accepted{BoxKind::Depot, Pose{60.0, -60.0, 3.0, 4.0}};
  // Over the shop.
  const AddBox refused{BoxKind::Shop, Pose{42.0, 0.0, 0.0, -1.0}};
  REQUIRE(isAccepted(world, accepted));
  REQUIRE_FALSE(isAccepted(world, refused));

  CHECK(sameMesh(buildGhostMesh(world, accepted),
                 boxOf(BoxKind::Depot, accepted.At, ghostOf(boxColor(BoxKind::Depot)))));
  CHECK(sameMesh(buildGhostMesh(world, refused), boxOf(BoxKind::Shop, refused.At, INVALID_TINT)));
}

TEST_CASE("A MoveBox's ghost is the box its key holds at the new pose, in the box's color at "
          "GHOST_ALPHA when accepted, in INVALID_TINT when refused, and nothing when the key holds "
          "no box") {
  const World world = ghostPark();
  const MoveBox accepted{SHOP, Pose{60.0, 60.0, -1.0, 2.0}};
  // The depot onto the shop, so the ghost's size and height must be the depot's.
  const MoveBox refused{DEPOT, Pose{40.0, 0.0, 1.0, 0.0}};
  REQUIRE(isAccepted(world, accepted));
  REQUIRE_FALSE(isAccepted(world, refused));

  CHECK(sameMesh(buildGhostMesh(world, accepted),
                 boxOf(BoxKind::Shop, accepted.At, ghostOf(boxColor(BoxKind::Shop)))));
  CHECK(sameMesh(buildGhostMesh(world, refused), boxOf(BoxKind::Depot, refused.At, INVALID_TINT)));
  CHECK(isEmpty(buildGhostMesh(world, MoveBox{BACKSTAGE, Pose{60.0, 60.0, 0.0, -1.0}})));
  CHECK(isEmpty(buildGhostMesh(world, MoveBox{MISSING, Pose{60.0, 60.0, 0.0, -1.0}})));
}

TEST_CASE("An AddPath's ghost is its ribbon in its kind's color at GHOST_ALPHA when accepted, and "
          "in INVALID_TINT when refused") {
  const World world = ghostPark();
  const AddPath accepted{PathKind::Backstage, {{60.0, 60.0}, {60.0, 80.0}, {70.0, 90.0}}};
  // Through the shop.
  const AddPath refused{PathKind::Guest, {{40.0, -20.0}, {40.0, 20.0}}};
  REQUIRE(isAccepted(world, accepted));
  REQUIRE_FALSE(isAccepted(world, refused));

  const ParkMesh acceptedRibbon =
      pathOf(PathKind::Backstage, accepted.Points, ghostOf(pathColor(PathKind::Backstage)));
  const ParkMesh refusedRibbon = pathOf(PathKind::Guest, refused.Points, INVALID_TINT);
  REQUIRE_FALSE(acceptedRibbon.Vertices.empty());
  REQUIRE_FALSE(refusedRibbon.Vertices.empty());

  CHECK(sameMesh(buildGhostMesh(world, accepted), acceptedRibbon));
  CHECK(sameMesh(buildGhostMesh(world, refused), refusedRibbon));
}

TEST_CASE("A deletion's ghost is the entity it deletes in DELETE_TINT when accepted, and nothing "
          "when refused") {
  const World world = ghostPark();
  const ParkMesh deletedBox = entityOf(world, SHOP, DELETE_TINT);
  const ParkMesh deletedPath = entityOf(world, BACKSTAGE, DELETE_TINT);
  REQUIRE_FALSE(deletedBox.Vertices.empty());
  REQUIRE_FALSE(deletedPath.Vertices.empty());

  CHECK(sameMesh(buildGhostMesh(world, DeleteBox{SHOP}), deletedBox));
  CHECK(sameMesh(buildGhostMesh(world, DeletePath{BACKSTAGE}), deletedPath));
  // Each names what the other deletes, or nothing.
  CHECK(isEmpty(buildGhostMesh(world, DeleteBox{BACKSTAGE})));
  CHECK(isEmpty(buildGhostMesh(world, DeletePath{SHOP})));
  CHECK(isEmpty(buildGhostMesh(world, DeleteBox{MISSING})));
}

// The vertices buildParkMesh draws for the highest-keyed box, which it draws last.
std::vector<ParkVertex> lastBoxOf(const ParkMesh &mesh, std::size_t count) {
  REQUIRE(mesh.Vertices.size() >= count);
  return {std::prev(mesh.Vertices.end(), static_cast<std::ptrdiff_t>(count)), mesh.Vertices.end()};
}

// Principle 8: an accepted ghost is the box its commit draws.
void checkGhostIsCommittedBox(const World &resolved, const ParkEdit &edit) {
  REQUIRE(isAccepted(resolved, edit));
  const ParkMesh ghost = buildGhostMesh(resolved, edit);
  REQUIRE_FALSE(ghost.Vertices.empty());
  CommandQueue queue;
  queueEdit(queue, edit);
  const World candidate = makeCandidate(resolved, queue);

  CHECK(std::ranges::equal(ghost.Vertices,
                           lastBoxOf(buildParkMesh(candidate), ghost.Vertices.size()), sameShape));
}

TEST_CASE("An accepted AddBox or MoveBox's ghost has, in order, the positions and normals "
          "buildParkMesh draws for its box in the candidate world the edit gives") {
  World world = ghostPark();
  resolveWorld(world);
  // An added box takes the next key, the highest; the depot is already the highest-keyed box.
  SECTION("AddBox") {
    checkGhostIsCommittedBox(world, AddBox{BoxKind::Shop, Pose{60.0, -60.0, 3.0, 4.0}});
  }
  SECTION("MoveBox") {
    checkGhostIsCommittedBox(world, MoveBox{DEPOT, Pose{-60.0, 60.0, -1.0, 2.0}});
  }
}

// Principle 1: ghosts are derived, and building them leaves nothing behind in what is saved.
TEST_CASE("Building a ghost or appending an entity leaves the world's save and hash unchanged") {
  const World world = ghostPark();
  const std::string save = saveWorld(world);
  const uint64_t hash = hashWorld(world);

  static_cast<void>(buildGhostMesh(world, AddBox{BoxKind::Depot, Pose{60.0, -60.0, 3.0, 4.0}}));
  static_cast<void>(buildGhostMesh(world, MoveBox{SHOP, Pose{60.0, 60.0, -1.0, 2.0}}));
  static_cast<void>(buildGhostMesh(world, AddPath{PathKind::Guest, {{60.0, 60.0}, {60.0, 80.0}}}));
  static_cast<void>(buildGhostMesh(world, DeletePath{BACKSTAGE}));
  static_cast<void>(buildGhostMesh(world, DeleteBox{SHOP}));
  static_cast<void>(entityOf(world, SHOP, MARK));
  static_cast<void>(entityOf(world, BACKSTAGE, MARK));

  CHECK(saveWorld(world) == save);
  CHECK(hashWorld(world) == hash);
}

} // namespace
} // namespace tpj
