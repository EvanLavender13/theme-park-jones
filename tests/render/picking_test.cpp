#include "render/picking.h"

#include "render/guest_mesh.h"
#include "render/math.h"
#include "render/park_mesh.h"
#include "render/renderer.h"
#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {
namespace {

// The round trip is checked to a thousandth of the screen's half extent.
constexpr double NDC_TOLERANCE = 1e-3;

struct Ndc {
  double X = 0.0;
  double Y = 0.0;
};

// Where drawFrame's projection, perspective after a look from the eye to the target with +Y up,
// puts a point.
Ndc projectPoint(const CameraView &view, float aspect, Vec3 point) {
  const Mat4 viewProjection = multiply(perspective(view.FovY, aspect, view.NearZ, view.FarZ),
                                       lookAt(view.Eye, view.Target, Vec3{0.0f, 1.0f, 0.0f}));
  const auto &m = viewProjection.M;
  // Column-major: element (row, col) is M[col * 4 + row], and the point is (x, y, z, 1).
  const auto row = [&m, point](int r) {
    return m[r] * point.X + m[4 + r] * point.Y + m[8 + r] * point.Z + m[12 + r];
  };
  const double w = row(3);
  return {row(0) / w, row(1) / w};
}

// Where the projection puts a point on the ground.
Ndc project(const CameraView &view, float aspect, const ParkPoint &point) {
  return projectPoint(view, aspect,
                      Vec3{static_cast<float>(point.X), 0.0f, static_cast<float>(point.Z)});
}

struct PickCase {
  std::string_view Name;
  CameraView View;
  float Aspect = 1.0f;
  Ndc Cursor;
};

// An oblique view like the orbit camera's, looking down at a target on the ground.
constexpr CameraView OBLIQUE{Vec3{30.0f, 40.0f, 60.0f}, Vec3{5.0f, 0.0f, -10.0f}};
// A level view, whose rays descend only below the middle of the screen.
constexpr CameraView LEVEL{Vec3{0.0f, 10.0f, 20.0f}, Vec3{0.0f, 10.0f, 0.0f}};

TEST_CASE("groundAtCursor gives a ground point that drawFrame's projection maps back to the "
          "cursor") {
  const PickCase cases[] = {
      {"the middle of a wide oblique view", OBLIQUE, 16.0f / 9.0f, {0.0, 0.0}},
      // Off both axes, so x and y must each take their own share of the field of view.
      {"toward a corner of a wide oblique view", OBLIQUE, 16.0f / 9.0f, {0.8, -0.7}},
      // A tall aspect, which must scale x alone.
      {"a tall oblique view", OBLIQUE, 0.6f, {-0.9, 0.4}},
      {"below the middle of a level view", LEVEL, 1.5f, {0.3, -0.5}},
      // A target above the ground, so the ground point is not the target.
      {"a view of a target in the air",
       CameraView{Vec3{-20.0f, 15.0f, -35.0f}, Vec3{10.0f, 2.0f, 5.0f}},
       4.0f / 3.0f,
       {0.2, 0.1}},
  };
  for (const PickCase &pick : cases) {
    INFO(pick.Name);
    const std::optional<ParkPoint> ground =
        groundAtCursor(pick.View, pick.Aspect, static_cast<float>(pick.Cursor.X),
                       static_cast<float>(pick.Cursor.Y));
    REQUIRE(ground.has_value());
    const Ndc back = project(pick.View, pick.Aspect, ground.value_or(ParkPoint{}));
    CHECK(std::abs(back.X - pick.Cursor.X) <= NDC_TOLERANCE);
    CHECK(std::abs(back.Y - pick.Cursor.Y) <= NDC_TOLERANCE);
  }
}

TEST_CASE("groundAtCursor gives none when the eye is not above the ground") {
  // Looking down, so the rays descend, from below the ground and from exactly on it.
  CHECK_FALSE(groundAtCursor(CameraView{Vec3{0.0f, -5.0f, 10.0f}, Vec3{0.0f, -10.0f, 0.0f}}, 1.5f,
                             0.0f, 0.0f)
                  .has_value());
  CHECK_FALSE(groundAtCursor(CameraView{Vec3{0.0f, 0.0f, 10.0f}, Vec3{0.0f, -10.0f, 0.0f}}, 1.5f,
                             0.0f, 0.0f)
                  .has_value());
}

TEST_CASE("groundAtCursor gives none when the ray through the cursor does not descend") {
  // The middle of a level view runs parallel to the ground, and above it the rays climb.
  CHECK_FALSE(groundAtCursor(LEVEL, 1.5f, 0.0f, 0.0f).has_value());
  CHECK_FALSE(groundAtCursor(LEVEL, 1.5f, -0.3f, 0.5f).has_value());
}

TEST_CASE("cursorRay starts at the eye, and drawFrame's projection maps every point along it to "
          "the cursor") {
  const PickCase cases[] = {
      {"toward a corner of a wide oblique view", OBLIQUE, 16.0f / 9.0f, {0.8, -0.7}},
      {"a tall oblique view", OBLIQUE, 0.6f, {-0.9, 0.4}},
      {"above the middle of a level view", LEVEL, 1.5f, {0.3, 0.5}},
  };
  for (const PickCase &pick : cases) {
    INFO(pick.Name);
    const CursorRay ray = cursorRay(pick.View, pick.Aspect, static_cast<float>(pick.Cursor.X),
                                    static_cast<float>(pick.Cursor.Y));
    CHECK(ray.Origin.X == pick.View.Eye.X);
    CHECK(ray.Origin.Y == pick.View.Eye.Y);
    CHECK(ray.Origin.Z == pick.View.Eye.Z);
    // Near the eye, at middle distance, and far off: a ray, not a point.
    for (const float t : {0.5f, 5.0f, 50.0f}) {
      CAPTURE(t);
      const Ndc back = projectPoint(pick.View, pick.Aspect, ray.Origin + (ray.Direction * t));
      CHECK(std::abs(back.X - pick.Cursor.X) <= NDC_TOLERANCE);
      CHECK(std::abs(back.Y - pick.Cursor.Y) <= NDC_TOLERANCE);
    }
  }
}

// The direction is the unit view direction plus parts along the view's right and up, so its
// component along the view is 1 at every cursor, and it is the view direction at the middle.
TEST_CASE("cursorRay's direction advances one unit along the view direction per unit of t") {
  constexpr float DIRECTION_TOLERANCE = 1e-5f;
  const Vec3 forward = normalize(OBLIQUE.Target - OBLIQUE.Eye);
  const CursorRay middle = cursorRay(OBLIQUE, 16.0f / 9.0f, 0.0f, 0.0f);
  CHECK(std::abs(middle.Direction.X - forward.X) <= DIRECTION_TOLERANCE);
  CHECK(std::abs(middle.Direction.Y - forward.Y) <= DIRECTION_TOLERANCE);
  CHECK(std::abs(middle.Direction.Z - forward.Z) <= DIRECTION_TOLERANCE);
  const CursorRay corner = cursorRay(OBLIQUE, 16.0f / 9.0f, 0.8f, -0.7f);
  CHECK(std::abs(dot(corner.Direction, forward) - 1.0f) <= DIRECTION_TOLERANCE);
}

TEST_CASE("groundAtCursor is where cursorRay meets the ground") {
  // Positions a few hundred meters out, computed in float, agree to a millimeter.
  constexpr double GROUND_TOLERANCE = 1e-3;
  const PickCase cases[] = {
      {"toward a corner of a wide oblique view", OBLIQUE, 16.0f / 9.0f, {0.8, -0.7}},
      {"below the middle of a level view", LEVEL, 1.5f, {0.3, -0.5}},
  };
  for (const PickCase &pick : cases) {
    INFO(pick.Name);
    const auto x = static_cast<float>(pick.Cursor.X);
    const auto y = static_cast<float>(pick.Cursor.Y);
    const CursorRay ray = cursorRay(pick.View, pick.Aspect, x, y);
    const double t = -static_cast<double>(ray.Origin.Y) / static_cast<double>(ray.Direction.Y);
    const std::optional<ParkPoint> ground = groundAtCursor(pick.View, pick.Aspect, x, y);
    REQUIRE(ground.has_value());
    const ParkPoint point = ground.value_or(ParkPoint{});
    CHECK(std::abs(point.X - (ray.Origin.X + (ray.Direction.X * t))) <= GROUND_TOLERANCE);
    CHECK(std::abs(point.Z - (ray.Origin.Z + (ray.Direction.Z * t))) <= GROUND_TOLERANCE);
  }
}

// An upright box 2 m square and 3 m high, over x from 19 to 21 and z from -11 to -9.
constexpr Pose BOX_POSE{20.0, -10.0, 0.0, -1.0};
constexpr FootprintSize BOX_SIZE{2.0, 2.0};
constexpr float BOX_HEIGHT = 3.0f;
constexpr double ENTRY_TOLERANCE = 1e-5;

bool entersAt(const std::optional<double> &entry, double t) {
  return entry.has_value() && std::abs(entry.value_or(0.0) - t) <= ENTRY_TOLERANCE;
}

std::optional<double> boxEntry(Vec3 origin, Vec3 direction) {
  return rayEntry(CursorRay{origin, direction}, BOX_POSE, BOX_SIZE, BOX_HEIGHT);
}

TEST_CASE("rayEntry is the least ray parameter at which a ray from outside is in the box") {
  // Down through the top, level through a side, and descending past the side's plane above the
  // box, so it enters only where it falls through the top.
  CHECK(entersAt(boxEntry({20.0f, 10.0f, -10.0f}, {0.0f, -1.0f, 0.0f}), 7.0));
  CHECK(entersAt(boxEntry({10.0f, 1.0f, -10.0f}, {1.0f, 0.0f, 0.0f}), 9.0));
  CHECK(entersAt(boxEntry({20.0f, 13.0f, -20.0f}, {0.0f, -1.0f, 1.0f}), 10.0));
  // The parameter counts lengths of the direction, so a direction twice as long halves it.
  CHECK(entersAt(boxEntry({20.0f, 10.0f, -10.0f}, {0.0f, -2.0f, 0.0f}), 3.5));
}

TEST_CASE("rayEntry counts the box's edges as inside it") {
  // Along the line where the top meets a side, and along a side's plane.
  CHECK(entersAt(boxEntry({10.0f, 3.0f, -9.0f}, {1.0f, 0.0f, 0.0f}), 9.0));
  CHECK(entersAt(boxEntry({19.0f, 1.0f, -20.0f}, {0.0f, 0.0f, 1.0f}), 9.0));
}

TEST_CASE("rayEntry is 0 for a ray starting in the box") {
  CHECK(entersAt(boxEntry({20.0f, 1.0f, -10.0f}, {1.0f, 0.0f, 0.0f}), 0.0));
  // On its top, heading away.
  CHECK(entersAt(boxEntry({20.0f, 3.0f, -10.0f}, {0.0f, 1.0f, 0.0f}), 0.0));
}

TEST_CASE("rayEntry gives none for a ray that never reaches the box") {
  // Passing over it, beside it, under the ground beneath it, and pointing away from it.
  CHECK_FALSE(boxEntry({10.0f, 4.0f, -10.0f}, {1.0f, 0.0f, 0.0f}).has_value());
  CHECK_FALSE(boxEntry({10.0f, 1.0f, -12.0f}, {1.0f, 0.0f, 0.0f}).has_value());
  CHECK_FALSE(boxEntry({10.0f, -0.5f, -10.0f}, {1.0f, 0.0f, 0.0f}).has_value());
  CHECK_FALSE(boxEntry({10.0f, 1.0f, -10.0f}, {-1.0f, 0.0f, 0.0f}).has_value());
}

TEST_CASE("rayEntry follows the footprint the pose's facing turns") {
  // 4 m wide across the facing and 2 m deep along it. Facing +x, it spans z from -12 to -8; facing
  // -z, only from -11 to -9, so a ray at z = -8.5 meets the first and misses the second.
  constexpr FootprintSize wide{4.0, 2.0};
  const CursorRay ray{{30.0f, 1.0f, -8.5f}, {-1.0f, 0.0f, 0.0f}};
  CHECK(entersAt(rayEntry(ray, Pose{20.0, -10.0, 1.0, 0.0}, wide, BOX_HEIGHT), 9.0));
  CHECK_FALSE(rayEntry(ray, Pose{20.0, -10.0, 0.0, -1.0}, wide, BOX_HEIGHT).has_value());
}

TEST_CASE("rayEntry gives none for a pose with no footprint") {
  // A facing of zero length, with the ray starting where the box would stand.
  CHECK_FALSE(rayEntry(CursorRay{{20.0f, 1.0f, -10.0f}, {1.0f, 0.0f, 0.0f}},
                       Pose{20.0, -10.0, 0.0, 0.0}, BOX_SIZE, BOX_HEIGHT)
                  .has_value());
}

// tests/parks/warm.park: entrance 1 at (0, 126.5) facing -z, guest paths 2 to 5, with path 2 along
// x = 0 from z = 123 to 100 and path 3 along z = 110 from x = 0 to -30, shop 7 at (6.5, 115)
// facing -x, depot 8 at (12, 80), and guests 9 to 40. Guest 10 walks alone on path 5's second
// segment; guests 12 and 21 wait at shop 7's front door, on its front face; guest 40 has just
// arrived at the entrance's door, on its front face.
constexpr EntityKey ENTRANCE{1};
constexpr EntityKey SHOP{7};
constexpr EntityKey DEPOT{8};
constexpr EntityKey LONE_GUEST{10};
constexpr EntityKey WAITING_GUEST{12};
constexpr EntityKey OTHER_WAITING_GUEST{21};
constexpr EntityKey ARRIVED_GUEST{40};

World openWarm() {
  std::ifstream file(TPJ_PARKS_DIR "/warm.park", std::ios::binary);
  const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

Pose boxPose(const World &world, EntityKey key) {
  const std::vector<ParkBox> boxes = parkBoxes(world);
  const auto box = std::ranges::find(boxes, key, &ParkBox::Key);
  REQUIRE(box != boxes.end());
  return box == boxes.end() ? Pose{} : box->At;
}

Pose entrancePose(const World &world, EntityKey key) {
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  const auto entrance = std::ranges::find(entrances, key, &ParkEntrance::Key);
  REQUIRE(entrance != entrances.end());
  return entrance == entrances.end() ? Pose{} : entrance->At;
}

GroundPoint positionOf(const World &world, EntityKey guest) {
  const std::optional<GuestRecord> record = guestRecord(world, guest);
  REQUIRE(record.has_value());
  const std::optional<GroundPoint> position = record.value_or(GuestRecord{}).Position;
  REQUIRE(position.has_value());
  return position.value_or(GroundPoint{});
}

Vec3 point(double x, float height, double z) {
  return {static_cast<float>(x), height, static_cast<float>(z)};
}

// The pick at the middle of a view from the eye to the target, whose ray runs straight at it.
std::optional<EntityKey> pickToward(const World &world, Vec3 eye, Vec3 target) {
  return entityAtCursor(world, CameraView{eye, target}, 1.0f, 0.0f, 0.0f);
}

// The pick looking down at a ground point's column from 40 m up and 5 m toward +z.
std::optional<EntityKey> pickDownAt(const World &world, double x, double z, float height) {
  return pickToward(world, point(x, 40.0f, z + 5.0), point(x, height, z));
}

TEST_CASE("entityAtCursor gives the entrance, box, or guest its cursor's ray meets") {
  const World world = openWarm();
  const Pose entrance = entrancePose(world, ENTRANCE);
  CHECK(pickDownAt(world, entrance.X, entrance.Z, ENTRANCE_HEIGHT / 2.0f) == ENTRANCE);
  const Pose shop = boxPose(world, SHOP);
  CHECK(pickDownAt(world, shop.X, shop.Z, boxHeight(BoxKind::Shop) / 2.0f) == SHOP);
  const Pose depot = boxPose(world, DEPOT);
  CHECK(pickDownAt(world, depot.X, depot.Z, boxHeight(BoxKind::Depot) / 2.0f) == DEPOT);
  const GroundPoint guest = positionOf(world, LONE_GUEST);
  CHECK(pickDownAt(world, guest.X, guest.Z, GUEST_HEIGHT / 2.0f) == LONE_GUEST);
}

TEST_CASE("entityAtCursor gives the solid its ray meets first") {
  // A level line 1 m up through the lone guest and the depot's center, with open ground 20 m
  // beyond each: looking along it from either end, the nearer one takes the click.
  const World world = openWarm();
  const GroundPoint guest = positionOf(world, LONE_GUEST);
  const Pose depot = boxPose(world, DEPOT);
  const double dx = depot.X - guest.X;
  const double dz = depot.Z - guest.Z;
  const double length = std::sqrt((dx * dx) + (dz * dz));
  const double ux = dx / length;
  const double uz = dz / length;
  const Vec3 guestAt = point(guest.X, 1.0f, guest.Z);
  const Vec3 depotAt = point(depot.X, 1.0f, depot.Z);
  CHECK(pickToward(world, point(depot.X + (ux * 20.0), 1.0f, depot.Z + (uz * 20.0)), depotAt) ==
        DEPOT);
  CHECK(pickToward(world, point(guest.X - (ux * 20.0), 1.0f, guest.Z - (uz * 20.0)), guestAt) ==
        LONE_GUEST);
}

TEST_CASE("Of equal entries, an entrance or a box takes the click before a guest") {
  const World world = openWarm();
  // An eye a meter up, 10 cm inside the front face where the guest stands at the door, looking
  // inward: it starts inside both the guest's box and the entrance's or shop's, so both enter at 0.
  const Pose entrance = entrancePose(world, ENTRANCE);
  const GroundPoint arrived = positionOf(world, ARRIVED_GUEST);
  const Vec3 intoEntrance =
      point(arrived.X - (entrance.FacingX * 0.1), 1.0f, arrived.Z - (entrance.FacingZ * 0.1));
  CHECK(pickToward(world, intoEntrance,
                   intoEntrance + Vec3{-static_cast<float>(entrance.FacingX), 0.0f,
                                       -static_cast<float>(entrance.FacingZ)}) == ENTRANCE);

  const Pose shop = boxPose(world, SHOP);
  const GroundPoint waiting = positionOf(world, WAITING_GUEST);
  const Vec3 intoShop =
      point(waiting.X - (shop.FacingX * 0.1), 1.0f, waiting.Z - (shop.FacingZ * 0.1));
  CHECK(pickToward(world, intoShop,
                   intoShop + Vec3{-static_cast<float>(shop.FacingX), 0.0f,
                                   -static_cast<float>(shop.FacingZ)}) == SHOP);
}

TEST_CASE("Of guests met at equal entries, the one parkGuests gives first takes the click") {
  // The waiting guests stand at one point, so a ray meets their boxes at the same entry. It runs
  // level along the shop's front, 10 cm in front of it, toward the door from 5 m away.
  const World world = openWarm();
  const GroundPoint waiting = positionOf(world, WAITING_GUEST);
  REQUIRE(positionOf(world, OTHER_WAITING_GUEST) == waiting);
  const Pose shop = boxPose(world, SHOP);
  const double x = waiting.X + (shop.FacingX * 0.1);
  const double z = waiting.Z + (shop.FacingZ * 0.1);
  CHECK(pickToward(world, point(x, 1.0f, z + 5.0), point(x, 1.0f, z)) == WAITING_GUEST);
}

TEST_CASE("entityAtCursor gives none when its ray meets no solid, passing through paths") {
  // Down onto path 3 where no guest walks, 25 m along it.
  const World world = openWarm();
  CHECK_FALSE(pickDownAt(world, -25.0, 110.0, 0.0f).has_value());
}

TEST_CASE("A guest whose place does not resolve cannot be picked") {
  // A level ray 1 m up along guest path 2, from just past the entrance's guests.
  const Vec3 eye{0.0f, 1.0f, 122.0f};
  const Vec3 along{0.0f, 1.0f, 100.0f};
  const World world = openWarm();
  const std::vector<EntityKey> guests = parkGuests(world);
  const std::optional<EntityKey> walking = pickToward(world, eye, along);
  CHECK(walking.has_value());
  CHECK(std::ranges::find(guests, walking.value_or(NULL_KEY)) != guests.end());

  // After a cycle deleting every guest path, the guests remain with no place on the ground.
  World cut = openWarm();
  CommandQueue edits;
  for (const ParkPath &path : parkPaths(cut)) {
    if (path.Kind == PathKind::Guest) {
      queueEdit(edits, DeletePath{path.Key});
    }
  }
  stepWorld(cut, edits);
  REQUIRE_FALSE(parkGuests(cut).empty());
  for (const EntityKey guest : parkGuests(cut)) {
    REQUIRE_FALSE(guestRecord(cut, guest).value_or(GuestRecord{}).Position.has_value());
  }
  CHECK_FALSE(pickToward(cut, eye, along).has_value());
}

TEST_CASE("entityAtCursor changes nothing in the world") {
  const World world = openWarm();
  const uint64_t before = hashWorld(world);
  const Pose shop = boxPose(world, SHOP);
  (void)pickDownAt(world, shop.X, shop.Z, 2.0f);
  (void)pickDownAt(world, -25.0, 110.0, 0.0f);
  CHECK(hashWorld(world) == before);
}

} // namespace
} // namespace tpj
