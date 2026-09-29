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
constexpr EntityKey GUEST_PATH{2};
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

// Resolved, as every world the app holds is, so the entrance's front door has a walkway to the
// guest path, and a ghost that wrongly drew the world's walkways would show one.
World ghostPark() {
  World world = loadWorld(makeParkSchema(), GHOST_PARK);
  resolveWorld(world);
  return world;
}

// A shop facing -x whose front door, at (3, 110), lies 3 m from the guest path along x = 0.
constexpr Pose SHOP_IN_REACH{6.0, 110.0, -1.0, 0.0};
// A pose more than CONNECTION_REACH from every path, for a shop or a depot.
constexpr Pose OUT_OF_REACH{60.0, -60.0, 3.0, 4.0};

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

// Whether the ghost's first vertices and indices are the own ghost's, as an accepted edit's are,
// its walkways following.
bool startsWith(const ParkMesh &ghost, const ParkMesh &own) {
  return ghost.Vertices.size() >= own.Vertices.size() &&
         ghost.Indices.size() >= own.Indices.size() &&
         std::equal(own.Vertices.begin(), own.Vertices.end(), ghost.Vertices.begin(), sameVertex) &&
         std::equal(own.Indices.begin(), own.Indices.end(), ghost.Indices.begin());
}

World candidateOf(const World &world, const ParkEdit &edit) {
  CommandQueue queue;
  queueEdit(queue, edit);
  return makeCandidate(world, queue);
}

ParkMesh withWalkways(ParkMesh own, const World &candidate) {
  appendWalkways(own, candidate, GHOST_ALPHA);
  return own;
}

// A box's mesh has five faces of four vertices each.
constexpr std::size_t BOX_VERTICES = 20;

// What buildParkMesh draws before its walkways: each entrance, then each path.
ParkMesh entrancesAndPaths(const World &world) {
  ParkMesh mesh;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    appendBox(mesh, entrance.At, ENTRANCE_SIZE, ENTRANCE_HEIGHT, ENTRANCE_COLOR);
  }
  for (const ParkPath &path : parkPaths(world)) {
    appendPath(mesh, path.Kind, path.Points);
  }
  return mesh;
}

// The vertices buildParkMesh draws for the world's walkways: after its entrances and paths, and
// before its boxes.
std::vector<ParkVertex> walkwaysDrawn(const World &world) {
  const ParkMesh drawn = buildParkMesh(world);
  const std::size_t before = entrancesAndPaths(world).Vertices.size();
  const std::size_t after = BOX_VERTICES * parkBoxes(world).size();
  REQUIRE(drawn.Vertices.size() >= before + after);
  return {std::next(drawn.Vertices.begin(), static_cast<std::ptrdiff_t>(before)),
          std::prev(drawn.Vertices.end(), static_cast<std::ptrdiff_t>(after))};
}

// The ghost's vertices after its own ghost's.
std::vector<ParkVertex> ghostWalkways(const ParkMesh &ghost, std::size_t own) {
  REQUIRE(ghost.Vertices.size() >= own);
  return {std::next(ghost.Vertices.begin(), static_cast<std::ptrdiff_t>(own)),
          ghost.Vertices.end()};
}

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

TEST_CASE("appendPath given a color draws, vertex for vertex, the ribbon and joints appendPath "
          "draws, in that color") {
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

TEST_CASE("An AddBox's own ghost is its box in its kind's color at GHOST_ALPHA when accepted, and "
          "its whole ghost is its box in INVALID_TINT when refused") {
  const World world = ghostPark();
  const AddBox accepted{BoxKind::Depot, Pose{60.0, -60.0, 3.0, 4.0}};
  // Over the shop.
  const AddBox refused{BoxKind::Shop, Pose{42.0, 0.0, 0.0, -1.0}};
  REQUIRE(isAccepted(world, accepted));
  REQUIRE_FALSE(isAccepted(world, refused));

  CHECK(startsWith(buildGhostMesh(world, accepted),
                   boxOf(BoxKind::Depot, accepted.At, ghostOf(boxColor(BoxKind::Depot)))));
  CHECK(sameMesh(buildGhostMesh(world, refused), boxOf(BoxKind::Shop, refused.At, INVALID_TINT)));
}

TEST_CASE(
    "A MoveBox's own ghost is the box its key holds at the new pose, in the box's color at "
    "GHOST_ALPHA when accepted, its whole ghost is that box in INVALID_TINT when refused, and "
    "it has none when the key holds no box") {
  const World world = ghostPark();
  const MoveBox accepted{SHOP, Pose{60.0, 60.0, -1.0, 2.0}};
  // The depot onto the shop, so the ghost's size and height must be the depot's.
  const MoveBox refused{DEPOT, Pose{40.0, 0.0, 1.0, 0.0}};
  REQUIRE(isAccepted(world, accepted));
  REQUIRE_FALSE(isAccepted(world, refused));

  CHECK(startsWith(buildGhostMesh(world, accepted),
                   boxOf(BoxKind::Shop, accepted.At, ghostOf(boxColor(BoxKind::Shop)))));
  CHECK(sameMesh(buildGhostMesh(world, refused), boxOf(BoxKind::Depot, refused.At, INVALID_TINT)));
  CHECK(isEmpty(buildGhostMesh(world, MoveBox{BACKSTAGE, Pose{60.0, 60.0, 0.0, -1.0}})));
  CHECK(isEmpty(buildGhostMesh(world, MoveBox{MISSING, Pose{60.0, 60.0, 0.0, -1.0}})));
}

TEST_CASE("An AddPath's own ghost is its ribbon and joints in its kind's color at GHOST_ALPHA when "
          "accepted, and its whole ghost is them in INVALID_TINT when refused") {
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

  CHECK(startsWith(buildGhostMesh(world, accepted), acceptedRibbon));
  CHECK(sameMesh(buildGhostMesh(world, refused), refusedRibbon));
}

TEST_CASE("A deletion's own ghost is the entity it deletes in DELETE_TINT when accepted, and its "
          "whole ghost is nothing when refused") {
  const World world = ghostPark();
  const ParkMesh deletedBox = entityOf(world, SHOP, DELETE_TINT);
  const ParkMesh deletedPath = entityOf(world, BACKSTAGE, DELETE_TINT);
  REQUIRE_FALSE(deletedBox.Vertices.empty());
  REQUIRE_FALSE(deletedPath.Vertices.empty());

  CHECK(startsWith(buildGhostMesh(world, DeleteBox{SHOP}), deletedBox));
  CHECK(startsWith(buildGhostMesh(world, DeletePath{BACKSTAGE}), deletedPath));
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

// Principle 8: an accepted ghost's own ghost, its first vertices, is the box its commit draws.
void checkGhostIsCommittedBox(const World &resolved, const ParkEdit &edit) {
  REQUIRE(isAccepted(resolved, edit));
  const ParkMesh ghost = buildGhostMesh(resolved, edit);
  REQUIRE(ghost.Vertices.size() >= BOX_VERTICES);
  const std::vector<ParkVertex> committed =
      lastBoxOf(buildParkMesh(candidateOf(resolved, edit)), BOX_VERTICES);

  CHECK(std::ranges::equal(
      ghost.Vertices.begin(),
      std::next(ghost.Vertices.begin(), static_cast<std::ptrdiff_t>(BOX_VERTICES)),
      committed.begin(), committed.end(), sameShape));
}

TEST_CASE("An accepted AddBox or MoveBox's own ghost has, in order, the positions and normals "
          "buildParkMesh draws for its box in the candidate world the edit gives") {
  const World world = ghostPark();
  // An added box takes the next key, the highest; the depot is already the highest-keyed box.
  SECTION("AddBox") {
    checkGhostIsCommittedBox(world, AddBox{BoxKind::Shop, Pose{60.0, -60.0, 3.0, 4.0}});
  }
  SECTION("MoveBox") {
    checkGhostIsCommittedBox(world, MoveBox{DEPOT, Pose{-60.0, 60.0, -1.0, 2.0}});
  }
}

// Principle 8: an accepted path's own ghost, its first vertices, is the ribbon and joints its
// commit draws.
// The added path takes the highest key, so buildParkMesh draws it after every path it drew before
// and ahead of the walkways.
void checkGhostIsCommittedPath(const AddPath &edit) {
  World world = makeNewPark(1);
  resolveWorld(world);
  REQUIRE(isAccepted(world, edit));
  const ParkMesh ghost = buildGhostMesh(world, edit);
  const std::size_t own = pathOf(edit.Kind, edit.Points, MARK).Vertices.size();
  REQUIRE(own > 0);
  REQUIRE(ghost.Vertices.size() >= own);
  const World candidate = candidateOf(world, edit);

  const ParkMesh before = entrancesAndPaths(world);
  const ParkMesh after = buildParkMesh(candidate);
  REQUIRE(after.Vertices.size() >= before.Vertices.size() + own);
  const auto added =
      std::next(after.Vertices.begin(), static_cast<std::ptrdiff_t>(before.Vertices.size()));
  REQUIRE(std::ranges::equal(after.Vertices.begin(), added, before.Vertices.begin(),
                             before.Vertices.end(), sameVertex));
  CHECK(std::ranges::equal(ghost.Vertices.begin(),
                           std::next(ghost.Vertices.begin(), static_cast<std::ptrdiff_t>(own)),
                           added, std::next(added, static_cast<std::ptrdiff_t>(own)), sameShape));
}

TEST_CASE(
    "An accepted AddPath's own ghost, joints included, has, in order, the positions and "
    "normals buildParkMesh draws for the path it adds in the candidate world the edit gives") {
  SECTION("a guest path that bends") {
    checkGhostIsCommittedPath(AddPath{PathKind::Guest, {{20.0, 20.0}, {40.0, 30.0}, {50.0, 60.0}}});
  }
  // The path keeps its points without the repeat and the one within MIN_POINT_SPACING, so its
  // points differ from the edit's while its ground line does not.
  SECTION("a backstage path whose points are not all kept") {
    checkGhostIsCommittedPath(AddPath{
        PathKind::Backstage,
        {{-20.0, -20.0}, {-20.0, -20.0}, {-40.0, -30.0}, {-40.005, -30.0}, {-50.0, -60.0}}});
  }
}

TEST_CASE("An accepted edit's ghost is its own ghost followed by the walkways of its candidate "
          "world at GHOST_ALPHA") {
  const World world = ghostPark();
  ParkMesh own;
  ParkEdit edit;

  SECTION("an AddBox whose shop's front door is in reach of a guest path") {
    const AddBox add{BoxKind::Shop, SHOP_IN_REACH};
    own = boxOf(BoxKind::Shop, add.At, ghostOf(boxColor(BoxKind::Shop)));
    edit = add;
  }
  // The new path passes 3 m from the committed shop's front door, at (40, -3).
  SECTION("an AddPath that brings a committed door within reach") {
    const AddPath add{PathKind::Guest, {{30.0, -6.0}, {50.0, -6.0}}};
    own = pathOf(PathKind::Guest, add.Points, ghostOf(pathColor(PathKind::Guest)));
    edit = add;
  }
  // The entrance's walkway leads to this path, so the candidate has none.
  SECTION("a DeletePath that takes a door's path away") {
    own = entityOf(world, GUEST_PATH, DELETE_TINT);
    edit = DeletePath{GUEST_PATH};
  }

  REQUIRE(isAccepted(world, edit));
  REQUIRE_FALSE(own.Vertices.empty());
  const World candidate = candidateOf(world, edit);
  CHECK(sameMesh(buildGhostMesh(world, edit), withWalkways(own, candidate)));
}

// Principle 8: the walkways a ghost shows are the ones its commit draws.
TEST_CASE("An accepted edit's ghost walkways have, in order, the positions and normals of the "
          "walkways buildParkMesh draws in the candidate world the edit gives") {
  const World world = ghostPark();
  SECTION("an AddBox giving its shop a walkway") {
    const AddBox edit{BoxKind::Shop, SHOP_IN_REACH};
    REQUIRE(isAccepted(world, edit));
    const std::vector<ParkVertex> expected = walkwaysDrawn(candidateOf(world, edit));
    REQUIRE(expected.size() > walkwaysDrawn(world).size());
    CHECK(std::ranges::equal(ghostWalkways(buildGhostMesh(world, edit), BOX_VERTICES), expected,
                             sameShape));
  }
  SECTION("a MoveBox taking a shop's door out of reach") {
    const World committed = candidateOf(world, AddBox{BoxKind::Shop, SHOP_IN_REACH});
    const MoveBox edit{parkBoxes(committed).back().Key, OUT_OF_REACH};
    REQUIRE(isAccepted(committed, edit));
    const std::vector<ParkVertex> expected = walkwaysDrawn(candidateOf(committed, edit));
    CHECK(std::ranges::equal(ghostWalkways(buildGhostMesh(committed, edit), BOX_VERTICES), expected,
                             sameShape));
  }
}

TEST_CASE("An accepted edit that leaves a door out of reach shows no walkway for it") {
  // The shop's walkway is the only one the world with it has beyond the world without it.
  const World without = ghostPark();
  const World with = candidateOf(without, AddBox{BoxKind::Shop, SHOP_IN_REACH});
  const MoveBox edit{parkBoxes(with).back().Key, OUT_OF_REACH};
  REQUIRE(parkBoxes(with).back().Kind == BoxKind::Shop);
  REQUIRE(isAccepted(with, edit));
  REQUIRE(walkwaysDrawn(with).size() > walkwaysDrawn(without).size());

  const ParkMesh own = boxOf(BoxKind::Shop, OUT_OF_REACH, ghostOf(boxColor(BoxKind::Shop)));
  CHECK(sameMesh(buildGhostMesh(with, edit), withWalkways(own, without)));
}

// Principle 1: ghosts are derived, and building them leaves nothing behind in what is saved.
TEST_CASE("Building a ghost, walkways included, or appending an entity leaves the world's save and "
          "hash unchanged") {
  const World world = ghostPark();
  const std::string save = saveWorld(world);
  const uint64_t hash = hashWorld(world);

  static_cast<void>(buildGhostMesh(world, AddBox{BoxKind::Depot, Pose{60.0, -60.0, 3.0, 4.0}}));
  static_cast<void>(buildGhostMesh(world, AddBox{BoxKind::Shop, SHOP_IN_REACH}));
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
