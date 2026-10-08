// Undo on a checked-in park: deleting a segment of tests/parks/main-street.park and undoing the
// deletion through the tools' history gives back the same networks, although the restored segment
// takes a fresh key. The test compares the networks by their lines, widths, and route distances,
// not by keys.

#include "support/park_files.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"
#include "tools/history.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <vector>

namespace tpj {
namespace {

using test::openPark;
using test::parkFile;

constexpr std::array<PathKind, 2> KINDS{PathKind::Guest, PathKind::Backstage};

// A carrier as the test compares it: its width and the x, z, and distance of each point of its
// ground line.
using CarrierShape = std::tuple<double, std::vector<std::tuple<double, double, double>>>;

std::vector<CarrierShape> carrierShapes(const Network &network) {
  std::vector<CarrierShape> shapes;
  for (const Carrier &carrier : network.carriers()) {
    std::vector<std::tuple<double, double, double>> points;
    points.reserve(carrier.Points.size());
    for (const CarrierPoint &point : carrier.Points) {
      points.emplace_back(point.X, point.Z, point.Distance);
    }
    shapes.emplace_back(carrier.Width, points);
  }
  std::ranges::sort(shapes);
  return shapes;
}

// A node as the test compares it: where it lies, and its route distance to each shop, in key order
// of the shops, none where no route reaches the shop.
using NodeDistances = std::tuple<double, double, std::vector<std::optional<double>>>;

std::vector<NodeDistances> nodeDistances(const Network &network,
                                         const std::vector<EntityKey> &shops) {
  std::vector<std::vector<std::optional<RouteEntry>>> entries;
  entries.reserve(shops.size());
  for (const EntityKey shop : shops) {
    entries.push_back(routeEntries(network, shop));
  }
  std::vector<NodeDistances> nodes;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const std::optional<GroundPoint> at = network.groundPoint(network.nodePlace(node));
    REQUIRE(at.has_value());
    std::vector<std::optional<double>> distances;
    for (const std::vector<std::optional<RouteEntry>> &shopEntries : entries) {
      const std::optional<RouteEntry> &entry = shopEntries.at(node);
      distances.push_back(entry.has_value() ? std::optional{entry->Distance} : std::nullopt);
    }
    const GroundPoint point = at.value_or(GroundPoint{});
    nodes.emplace_back(point.X, point.Z, distances);
  }
  std::ranges::sort(nodes);
  return nodes;
}

std::vector<EntityKey> shopKeys(const World &world) {
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  return shops;
}

// The lowest-keyed guest segment whose two points each make a corner, so deleting it also changes
// both corners and the segments beside it, which the undo has to put back.
std::optional<ParkSegment> segmentBetweenCorners(const World &world) {
  std::set<EntityKey> cornerPoints;
  for (const ParkCorner &corner : parkCorners(world)) {
    cornerPoints.insert(corner.Point);
  }
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Guest && cornerPoints.contains(segment.From) &&
        cornerPoints.contains(segment.To)) {
      return segment;
    }
  }
  return std::nullopt;
}

// Catches an undo that puts the segment back without the corners, widths, or joins it had, and
// networks or route distances that depend on a segment's key rather than on its intent.
TEST_CASE("Undoing a segment's deletion gives back the same networks and route distances") {
  World world = openPark(parkFile("main-street.park"));
  const std::vector<EntityKey> shops = shopKeys(world);
  REQUIRE_FALSE(shops.empty());
  std::vector<std::vector<CarrierShape>> shapesBefore;
  std::vector<std::vector<NodeDistances>> distancesBefore;
  for (const PathKind kind : KINDS) {
    shapesBefore.push_back(carrierShapes(parkNetwork(world, kind)));
    distancesBefore.push_back(nodeDistances(parkNetwork(world, kind), shops));
  }

  EditHistory history;
  clearHistory(history, world);
  const std::optional<ParkSegment> deleted = segmentBetweenCorners(world);
  REQUIRE(deleted.has_value());
  const DeleteSegment deletion{deleted.value_or(ParkSegment{}).Key};
  REQUIRE(isAccepted(world, deletion));
  CommandQueue deleting;
  deleting.push(deletion);
  stepWorld(world, deleting);
  noteEdits(history, world);
  REQUIRE(carrierShapes(parkNetwork(world, PathKind::Guest)) != shapesBefore.front());

  const std::optional<RestoreIntent> undo = undoEdit(history);
  REQUIRE(undo.has_value());
  CommandQueue undoing;
  undoing.push(undo.value_or(RestoreIntent{}));
  stepWorld(world, undoing);
  noteEdits(history, world);

  for (size_t i = 0; i < std::size(KINDS); ++i) {
    const Network &network = parkNetwork(world, KINDS[i]);
    CHECK(carrierShapes(network) == shapesBefore.at(i));
    CHECK(nodeDistances(network, shops) == distancesBefore.at(i));
  }
}

} // namespace
} // namespace tpj
