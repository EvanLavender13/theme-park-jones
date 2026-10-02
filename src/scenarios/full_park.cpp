#include "scenarios/full_park.h"

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"

#include <array>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace tpj {
namespace {

// The template's guest path, which the full park replaces with its own.
constexpr EntityKey TEMPLATE_PATH{2};
// The backstage rows' z, each serving the shops in the cells beside it.
constexpr std::array<double, 5> ROWS{-80.0, -40.0, 0.0, 40.0, 80.0};
// The paths of each grid direction, 20 m apart from -90 m.
constexpr int GRID_LINES = 10;
// A row's cells, 20 m apart from -80 m; those whose index mod 3 is 2 stay empty.
constexpr int ROW_CELLS = 9;

// Applies the command, or throws naming it when the world refuses it.
template <typename Command>
void applyAccepted(World &world, const Command &command, const std::string &what) {
  if (!isAccepted(world, command)) {
    throw std::logic_error("full park: the world refuses " + what);
  }
  applyCommand(world, command);
}

void addPath(World &world, PathKind kind, ParkPoint from, ParkPoint to) {
  applyAccepted(world, AddPath{kind, {from, to}},
                std::format("the {} path from ({}, {}) to ({}, {})",
                            kind == PathKind::Guest ? "guest" : "backstage", from.X, from.Z, to.X,
                            to.Z));
}

void addBox(World &world, BoxKind kind, Pose at) {
  applyAccepted(
      world, AddBox{kind, at},
      std::format("the {} at ({}, {})", kind == BoxKind::Shop ? "shop" : "depot", at.X, at.Z));
}

// The coordinate of a grid line or cell: the first plus 20 m per step.
double gridAt(double first, int step) { return first + 20.0 * static_cast<double>(step); }

// The entrance's path, the guest grid, the backstage spine and rows, the shops, and the depots.
void layOut(World &world) {
  applyAccepted(world, DeletePath{TEMPLATE_PATH}, "deleting the template's path");
  addPath(world, PathKind::Guest, {0.0, 123.0}, {0.0, 90.0});
  for (int i = 0; i < GRID_LINES; ++i) {
    const double x = gridAt(-90.0, i);
    addPath(world, PathKind::Guest, {x, -100.0}, {x, 100.0});
  }
  for (int j = 0; j < GRID_LINES; ++j) {
    const double z = gridAt(-90.0, j);
    addPath(world, PathKind::Guest, {-100.0, z}, {100.0, z});
  }
  addPath(world, PathKind::Backstage, {-106.0, -100.0}, {-106.0, 100.0});
  for (const double row : ROWS) {
    addPath(world, PathKind::Backstage, {-106.0, row}, {100.0, row});
  }
  for (const double row : ROWS) {
    for (int k = 0; k < ROW_CELLS; ++k) {
      if (k % 3 != 2) {
        addBox(world, BoxKind::Shop,
               Pose{.X = gridAt(-80.0, k), .Z = row - 4.5, .FacingX = 0.0, .FacingZ = -1.0});
      }
    }
  }
  for (const double z : {-60.0, 0.0, 60.0}) {
    addBox(world, BoxKind::Depot, Pose{.X = -111.5, .Z = z, .FacingX = 1.0, .FacingZ = 0.0});
  }
}

// Throws naming the first shop, in key order, with no depot to supply it.
void refuseStarvedShops(const World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop && !nearestDepot(world, box.Key)) {
      throw std::logic_error(
          std::format("full park: shop {} is starved", static_cast<uint64_t>(box.Key)));
    }
  }
}

// Adds the guests, each at a keyed draw along the guest network, edges weighed by their length,
// staying until the tick given.
void placeGuests(World &world, uint64_t stayUntil) {
  const std::vector<NetworkEdge> edges = parkNetwork(world, PathKind::Guest).edges();
  std::vector<double> lengths;
  lengths.reserve(edges.size());
  for (const NetworkEdge &edge : edges) {
    lengths.push_back(edge.ToDistance - edge.FromDistance);
  }
  for (uint64_t i = 0; i < FULL_PARK_GUESTS; ++i) {
    const NetworkEdge &edge =
        edges[drawPick(drawKey(world, NULL_KEY, hashName("full-park-edge"), i), lengths)];
    const double along = drawUniform(drawKey(world, NULL_KEY, hashName("full-park-along"), i));
    addGuest(world,
             Place{edge.Carrier, edge.FromDistance + along * (edge.ToDistance - edge.FromDistance)},
             stayUntil);
  }
}

} // namespace

World makeFullPark(uint64_t warmTicks) {
  World world = makeNewPark(FULL_PARK_SEED);
  layOut(world);
  resolveWorld(world);
  refuseStarvedShops(world);
  placeGuests(world, warmTicks + FULL_PARK_STAY);
  for (uint64_t tick = 0; tick < warmTicks; ++tick) {
    stepWorld(world);
  }
  return world;
}

} // namespace tpj
