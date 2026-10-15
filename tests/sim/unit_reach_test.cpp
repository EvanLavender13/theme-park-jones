#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park/surface.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/save.h"
#include "sim/schema.h"
#include "sim/units.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tpj {
namespace {

// The unit kinds the path ground and the paved surface derive in. Their indexes derive no units.
constexpr std::array<std::string_view, 10> KINDS{
    "straight-lines", "point-arms", "corners",      "ground-lines",   "group-labels",
    "point-asks",     "point-fits", "point-paving", "segment-paving", "hidden-edges"};

// The unit kinds the doors' connections and walkways and the walked surface derive in.
constexpr std::array<std::string_view, 6> CONNECTION_KINDS{
    "park-boxes",    "park-entrances",     "door-connections",
    "door-walkways", "walked-point-edges", "walked-segment-edges"};

// The path ground's and the paved surface's kinds, then the connections' and walkways' kinds.
std::vector<std::string_view> allKinds() {
  std::vector<std::string_view> kinds(KINDS.begin(), KINDS.end());
  kinds.insert(kinds.end(), CONNECTION_KINDS.begin(), CONNECTION_KINDS.end());
  return kinds;
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The park file as the app opens it: loaded with the park's schema and resolved.
World openPark(std::string_view name) {
  const std::string text = readText(std::filesystem::path(TPJ_PARKS_DIR) / name);
  REQUIRE_FALSE(text.empty());
  World world = loadWorld(makeParkSchema(), text);
  resolveWorld(world);
  return world;
}

std::vector<EntityKey> derivedOf(const World &world, std::string_view kind) {
  std::vector<EntityKey> keys;
  derivedUnits(world, kind, keys);
  return keys;
}

void commit(World &world, const ParkEdit &edit) {
  REQUIRE(isAccepted(world, edit));
  CommandQueue commands;
  queueEdit(commands, edit);
  commitCommands(world, commands);
}

// The pose moved the distance along its face, facing as it did.
Pose movedAlongFace(const Pose &pose, double distance) {
  const double length = std::hypot(pose.FacingX, pose.FacingZ);
  REQUIRE(length > 0.0);
  const double alongX = -pose.FacingZ / length;
  const double alongZ = pose.FacingX / length;
  return {pose.X + (distance * alongX), pose.Z + (distance * alongZ), pose.FacingX, pose.FacingZ};
}

std::vector<ParkSegment> segmentsAt(const std::vector<ParkSegment> &segments, EntityKey point) {
  std::vector<ParkSegment> at;
  for (const ParkSegment &segment : segments) {
    if (segment.From == point || segment.To == point) {
      at.push_back(segment);
    }
  }
  return at;
}

std::optional<ParkPoint> positionOf(const World &world, EntityKey point) {
  return stopPosition(world, OnPoint{point});
}

TEST_CASE("Resolving full.park again with nothing changed derives no unit of any kind the "
          "schema registers") {
  World world = openPark("stress/full.park");
  // If opening derived nothing either, an empty list after it would show nothing.
  for (const std::string_view kind : KINDS) {
    INFO(kind);
    REQUIRE_FALSE(derivedOf(world, kind).empty());
  }

  resolveWorld(world);

  // The schema's unit tables also hold its indexes, which derive no units, so they pass as well.
  REQUIRE_FALSE(world.schema().unitTables().empty());
  for (const UnitTableType &table : world.schema().unitTables()) {
    INFO(table.Name);
    CHECK(derivedOf(world, table.Name).empty());
  }
}

// The first shop the world accepts moving a few meters along its face, trying each shop in key
// order and each way along its face.
MoveBox aShopMove(const World &world) {
  constexpr double DISTANCE = 3.0;
  std::optional<MoveBox> move;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind != BoxKind::Shop) {
      continue;
    }
    for (const double side : {1.0, -1.0}) {
      const MoveBox candidate{box.Key, movedAlongFace(box.At, side * DISTANCE)};
      if (!move && isAccepted(world, ParkEdit{candidate})) {
        move = candidate;
      }
    }
  }
  REQUIRE(move.has_value());
  return move.value_or(MoveBox{});
}

TEST_CASE("Moving a shop on full.park a few meters along its paving derives no unit of the path "
          "ground or the paved surface") {
  World world = openPark("stress/full.park");
  const MoveBox moved = aShopMove(world);

  commit(world, moved);

  const std::optional<ParkBox> box = parkBox(world, moved.Box);
  REQUIRE(box.has_value());
  REQUIRE(box.value_or(ParkBox{}).At == moved.At);
  for (const std::string_view kind : KINDS) {
    INFO(kind);
    CHECK(derivedOf(world, kind).empty());
  }
}

// The units of a kind a resolution derived, split by whether their keys existed before the edit
// or the edit made them.
struct DerivedByAge {
  std::vector<EntityKey> Existing;
  size_t Made = 0;
};

DerivedByAge derivedByAge(const World &world, std::string_view kind, uint64_t firstMade) {
  DerivedByAge split;
  for (const EntityKey key : derivedOf(world, kind)) {
    const auto value = static_cast<uint64_t>(key);
    if (value >= firstMade && value < world.nextKey()) {
      ++split.Made;
    } else {
      split.Existing.push_back(key);
    }
  }
  return split;
}

// A guest path from the end of a guest path in the world to a new point a few meters on, the
// first that the world accepts, trying each end point in key order and three ways from it.
AddPath pathFromAnEnd(const World &world) {
  constexpr double REACH = 4.0;
  const std::vector<ParkSegment> segments = parkSegments(world);
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    const std::vector<ParkSegment> at = segmentsAt(segments, point.Key);
    if (at.size() != 1 || at.front().Kind != PathKind::Guest || at.front().From == at.front().To) {
      continue;
    }
    const EntityKey far = at.front().From == point.Key ? at.front().To : at.front().From;
    const std::optional<ParkPoint> farAt = positionOf(world, far);
    REQUIRE(farAt.has_value());
    const ParkPoint farPoint = farAt.value_or(ParkPoint{});
    const double outX = point.At.X - farPoint.X;
    const double outZ = point.At.Z - farPoint.Z;
    const double length = std::hypot(outX, outZ);
    if (!(length > 0.0)) {
      continue;
    }
    const std::array<ParkPoint, 3> ways{ParkPoint{outX / length, outZ / length},
                                        ParkPoint{-outZ / length, outX / length},
                                        ParkPoint{outZ / length, -outX / length}};
    for (const ParkPoint &way : ways) {
      const ParkPoint end{point.At.X + (REACH * way.X), point.At.Z + (REACH * way.Z)};
      const AddPath path{PathKind::Guest, {OnPoint{point.Key}, NewPoint{end}}};
      if (isAccepted(world, ParkEdit{path})) {
        return path;
      }
    }
  }
  FAIL("no end of a guest path accepts a path drawn a few meters on from it");
  return {};
}

// The path points that a chain of segments joins to the start, the start included.
std::vector<EntityKey> joinedPoints(const std::vector<ParkSegment> &segments, EntityKey start) {
  std::vector<EntityKey> joined{start};
  for (size_t next = 0; next < joined.size(); ++next) {
    const EntityKey point = joined[next];
    for (const ParkSegment &segment : segmentsAt(segments, point)) {
      const EntityKey far = segment.From == point ? segment.To : segment.From;
      if (std::ranges::find(joined, far) == joined.end()) {
        joined.push_back(far);
      }
    }
  }
  return joined;
}

double distanceBetween(const ParkPoint &first, const ParkPoint &second) {
  return std::hypot(first.X - second.X, first.Z - second.Z);
}

// The points other than the start that a chain of segments joins to it, farthest from the place
// first.
std::vector<ParkPathPoint> joinedFarthestFirst(const World &world, EntityKey start,
                                               const ParkPoint &from) {
  const std::vector<EntityKey> joined = joinedPoints(parkSegments(world), start);
  std::vector<ParkPathPoint> starts;
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    if (point.Key != start && std::ranges::find(joined, point.Key) != joined.end()) {
      starts.push_back(point);
    }
  }
  std::ranges::stable_sort(starts, [&](const ParkPathPoint &first, const ParkPathPoint &second) {
    return distanceBetween(first.At, from) > distanceBetween(second.At, from);
  });
  return starts;
}

// A straight guest path from the point along the unit direction, with the given number of new
// points spread evenly over the length.
AddPath straightBranch(const ParkPathPoint &start, const ParkPoint &way, double length,
                       size_t newPoints) {
  AddPath branch{PathKind::Guest, {OnPoint{start.Key}}};
  for (size_t made = 1; made <= newPoints; ++made) {
    const double along = length * static_cast<double>(made) / static_cast<double>(newPoints);
    branch.Stops.emplace_back(
        NewPoint{ParkPoint{start.At.X + (along * way.X), start.At.Z + (along * way.Z)}});
  }
  return branch;
}

// A straight guest path of a few stops from a point joined to the edit's start, that ends at least
// 100 m from every stop of the edit and crosses no path. It tries the joined points farthest from
// the edit first, and from each heads away from the edit before it tries either side.
AddPath branchAwayFrom(const World &world, const AddPath &edit) {
  constexpr double BRANCH_LENGTH = 40.0;
  constexpr size_t BRANCH_NEW_POINTS = 4;
  constexpr double FAR = 100.0;
  const std::optional<std::vector<ParkPoint>> editPositions = stopPositions(world, edit);
  REQUIRE(editPositions.has_value());
  const std::vector<ParkPoint> editAt = editPositions.value_or(std::vector<ParkPoint>{});
  REQUIRE_FALSE(editAt.empty());
  const OnPoint *editStart = std::get_if<OnPoint>(&edit.Stops.front());
  REQUIRE(editStart != nullptr);
  const ParkPoint from = editAt.front();

  for (const ParkPathPoint &start : joinedFarthestFirst(world, editStart->Point, from)) {
    const double apart = distanceBetween(start.At, from);
    if (!(apart > 0.0)) {
      continue;
    }
    const ParkPoint away{(start.At.X - from.X) / apart, (start.At.Z - from.Z) / apart};
    const std::array<ParkPoint, 3> ways{away, ParkPoint{-away.Z, away.X},
                                        ParkPoint{away.Z, -away.X}};
    for (const ParkPoint &way : ways) {
      const AddPath branch = straightBranch(start, way, BRANCH_LENGTH, BRANCH_NEW_POINTS);
      const ParkPoint end{start.At.X + (BRANCH_LENGTH * way.X),
                          start.At.Z + (BRANCH_LENGTH * way.Z)};
      const bool farFromEdit = std::ranges::all_of(
          editAt, [&](const ParkPoint &at) { return distanceBetween(at, end) >= FAR; });
      if (farFromEdit && isAccepted(world, ParkEdit{branch}) &&
          pathCrossings(world, branch).empty()) {
        return branch;
      }
    }
  }
  FAIL("no point joined to the edit's start accepts a long straight branch away from the edit");
  return {};
}

TEST_CASE("Drawing a path from an existing point derives the same units whether or not its "
          "network also runs out along a long branch far from the edit") {
  const World small = openPark("sketch.park");
  World smallEdited = copyWorld(small);
  const AddPath edit = pathFromAnEnd(smallEdited);

  World large = copyWorld(small);
  const AddPath branch = branchAwayFrom(large, edit);
  const size_t pointsBefore = parkPathPoints(large).size();
  commit(large, branch);
  REQUIRE(parkPathPoints(large).size() == pointsBefore + branch.Stops.size() - 1);

  const uint64_t smallFirstMade = smallEdited.nextKey();
  const uint64_t largeFirstMade = large.nextKey();
  commit(smallEdited, edit);
  commit(large, edit);

  for (const std::string_view kind : allKinds()) {
    INFO(kind);
    const DerivedByAge inSmall = derivedByAge(smallEdited, kind, smallFirstMade);
    const DerivedByAge inLarge = derivedByAge(large, kind, largeFirstMade);
    CHECK(inSmall.Existing == inLarge.Existing);
    CHECK(inSmall.Made == inLarge.Made);
  }
}

TEST_CASE("Setting a taper length where two segments of the same width meet derives units only "
          "for that point, its segments, and their far points, and no group label") {
  World world = openPark("sketch.park");
  const std::vector<ParkSegment> segments = parkSegments(world);
  EntityKey pointKey = NULL_KEY;
  std::vector<ParkSegment> atPoint;
  for (const ParkPathPoint &point : parkPathPoints(world)) {
    const std::vector<ParkSegment> at = segmentsAt(segments, point.Key);
    if (at.size() == 2 && at[0].Width == at[1].Width) {
      pointKey = point.Key;
      atPoint = at;
      break;
    }
  }
  REQUIRE(pointKey != NULL_KEY);
  std::vector<EntityKey> reached{pointKey};
  for (const ParkSegment &segment : atPoint) {
    reached.push_back(segment.Key);
    reached.push_back(segment.From == pointKey ? segment.To : segment.From);
  }

  constexpr double LENGTH = 4.0;
  commit(world, SetTaperLength{pointKey, LENGTH});

  const std::vector<ParkPathPoint> points = parkPathPoints(world);
  const auto set = std::ranges::find_if(
      points, [&](const ParkPathPoint &point) { return point.Key == pointKey; });
  REQUIRE(set != points.end());
  REQUIRE(set->Taper == LENGTH);
  for (const std::string_view kind : allKinds()) {
    INFO(kind);
    for (const EntityKey key : derivedOf(world, kind)) {
      INFO("key " << static_cast<uint64_t>(key));
      CHECK(std::ranges::find(reached, key) != reached.end());
    }
  }
  CHECK(derivedOf(world, "group-labels").empty());
}

// Whether the carrier is a connector: its first stop's node is anchored to an entity.
bool isConnector(const Network &network, const Carrier &carrier) {
  return !carrier.Stops.empty() && network.nodeAnchor(carrier.Stops.front().Node) != NULL_KEY;
}

bool isConnectorKey(const Network &network, EntityKey key) {
  return std::ranges::any_of(network.carriers(), [&](const Carrier &carrier) {
    return carrier.Key == key && isConnector(network, carrier);
  });
}

// The segments' carriers held at the node of a connector's last stop.
std::vector<EntityKey> segmentCarriersAtEnd(const Network &network, const Carrier &connector) {
  std::vector<EntityKey> held;
  for (const Place &place : network.stopPlaces(connector.Stops.back().Node)) {
    if (!isConnectorKey(network, place.Carrier)) {
      held.push_back(place.Carrier);
    }
  }
  return held;
}

// Appends the entities with a connector whose last stop is at a node holding a segment's carrier
// that the last stop of one of the shop's connectors also holds.
void appendSharingAPath(const Network &network, EntityKey shop, std::vector<EntityKey> &sharing) {
  std::vector<EntityKey> shopSegments;
  for (const Carrier &carrier : network.carriers()) {
    if (isConnector(network, carrier) && network.nodeAnchor(carrier.Stops.front().Node) == shop) {
      const std::vector<EntityKey> held = segmentCarriersAtEnd(network, carrier);
      shopSegments.insert(shopSegments.end(), held.begin(), held.end());
    }
  }
  for (const Carrier &carrier : network.carriers()) {
    if (!isConnector(network, carrier)) {
      continue;
    }
    const std::vector<EntityKey> held = segmentCarriersAtEnd(network, carrier);
    const bool shares = std::ranges::any_of(held, [&](EntityKey segment) {
      return std::ranges::find(shopSegments, segment) != shopSegments.end();
    });
    if (shares) {
      sharing.push_back(network.nodeAnchor(carrier.Stops.front().Node));
    }
  }
}

bool isNear(const ParkPoint &at, std::span<const ParkPoint> places, double reach) {
  return std::ranges::any_of(
      places, [&](const ParkPoint &place) { return distanceBetween(at, place) <= reach; });
}

// Appends the owners of the world's walked surface with a piece or an edge-line position within
// the reach of any of the places.
void appendWalkedOwnersNear(const World &world, std::span<const ParkPoint> places, double reach,
                            std::vector<EntityKey> &owners) {
  for (const auto &[owner, paving] : walkedSurface(world).Owners) {
    const bool pieceNear = std::ranges::any_of(paving.Pieces, [&](const SurfacePiece &piece) {
      return std::ranges::any_of(
          places, [&](const ParkPoint &place) { return pieceDistance(piece, place) <= reach; });
    });
    const bool edgeNear = std::ranges::any_of(
        paving.EdgePoints, [&](const ParkPoint &at) { return isNear(at, places, reach); });
    if (pieceNear || edgeNear) {
      owners.push_back(owner);
    }
  }
}

// Appends the segments that the world's walkways of the entities join.
void appendJoinedByWalkways(const World &world, std::span<const EntityKey> entities,
                            std::vector<EntityKey> &segments) {
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    for (const KeyedWalkway &walkway : keyedWalkways(world, kind)) {
      const bool ofEntity = std::ranges::any_of(entities, [&](EntityKey entity) {
        return walkway.Connector == connectorKey(entity, Face::Front) ||
               walkway.Connector == connectorKey(entity, Face::Back);
      });
      if (ofEntity) {
        segments.push_back(walkway.Way.Joins);
      }
    }
  }
}

void checkDerivedWithin(const World &world, std::string_view kind,
                        const std::vector<EntityKey> &allowed) {
  INFO(kind);
  for (const EntityKey key : derivedOf(world, kind)) {
    INFO("key " << static_cast<uint64_t>(key));
    CHECK(std::ranges::find(allowed, key) != allowed.end());
  }
}

TEST_CASE("Moving a shop on full.park derives connections only for that shop, walkways only for "
          "the doors that share its paths, and walked edges only near it") {
  World world = openPark("stress/full.park");
  const MoveBox moved = aShopMove(world);
  const World before = copyWorld(world);
  const std::optional<ParkBox> boxBefore = parkBox(before, moved.Box);
  REQUIRE(boxBefore.has_value());

  commit(world, moved);

  const std::optional<ParkBox> boxAfter = parkBox(world, moved.Box);
  REQUIRE(boxAfter.has_value());
  REQUIRE(boxAfter.value_or(ParkBox{}).At == moved.At);

  CHECK(derivedOf(world, "park-boxes") == std::vector<EntityKey>{moved.Box});
  CHECK(derivedOf(world, "door-connections") == std::vector<EntityKey>{moved.Box});
  CHECK(derivedOf(world, "park-entrances").empty());

  std::vector<EntityKey> sharing{moved.Box};
  for (const World *state : {&before, static_cast<const World *>(&world)}) {
    for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
      appendSharingAPath(parkNetwork(*state, kind), moved.Box, sharing);
    }
  }
  checkDerivedWithin(world, "door-walkways", sharing);

  constexpr double REACH = 40.0;
  const Pose atBefore = boxBefore.value_or(ParkBox{}).At;
  const std::array<ParkPoint, 2> shopAt{ParkPoint{atBefore.X, atBefore.Z},
                                        ParkPoint{moved.At.X, moved.At.Z}};
  const std::vector<EntityKey> withWalkways = derivedOf(world, "door-walkways");
  std::vector<EntityKey> walkedNear;
  for (const World *state : {&before, static_cast<const World *>(&world)}) {
    appendWalkedOwnersNear(*state, shopAt, REACH, walkedNear);
    appendJoinedByWalkways(*state, withWalkways, walkedNear);
  }
  checkDerivedWithin(world, "walked-point-edges", walkedNear);
  checkDerivedWithin(world, "walked-segment-edges", walkedNear);
}

} // namespace
} // namespace tpj
