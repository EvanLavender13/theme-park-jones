#include "sim/operations/operations.h"

#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// The keys of the boxes of the kind, ascending.
std::vector<EntityKey> boxKeys(const World &world, BoxKind kind) {
  std::vector<EntityKey> keys;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == kind) {
      keys.push_back(box.Key);
    }
  }
  return keys;
}

// Each backstage route distance entry sampled at the nodes anchored to the entity, as its source
// and distance, in node order and then source order.
std::vector<std::pair<EntityKey, double>> routeLengthsAt(const World &world, EntityKey at) {
  const Network &network = parkNetwork(world, PathKind::Backstage);
  std::vector<std::pair<EntityKey, double>> lengths;
  for (const uint32_t node : network.anchoredNodes(at)) {
    for (const SampledEntry<RouteEntry> &entry :
         sampleField<RouteDistance<PathKind::Backstage>>(world, network, network.nodePlace(node))) {
      lengths.emplace_back(entry.Source, entry.Value.Distance);
    }
  }
  return lengths;
}

// Each shop, in ascending key order, takes back the orders returned to it, and when it has a
// nearest depot and its inventory position is at the reorder point or below, orders up to
// ORDER_UP_TO from that depot.
void stepShops(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    const int64_t returned = unitsHeld<SupplyOrders>(world, shop, shop);
    if (returned > 0) {
      consumeUnits<SupplyOrders>(world, shop, shop, returned, UNFILLED_CAUSE);
    }
    const std::optional<DepotRoute> route = nearestDepot(world, shop);
    const int64_t position = inventoryPosition(world, shop);
    if (!route || position > REORDER_POINT) {
      continue;
    }
    const int64_t wanted = ORDER_UP_TO - position;
    createUnits<SupplyOrders>(world, shop, shop, wanted);
    sendUnits<SupplyOrders>(world, shop, route->Depot, shop, wanted, ORDER_DELAY);
  }
}

// Each depot, in ascending key order, takes back the supplies returned to it, and then, for each
// shop it holds orders for, cancels them when the shop is gone, sends them back when it cannot
// reach the shop, and otherwise ships as many supplies over the backstage route.
void stepDepots(World &world) {
  const std::vector<EntityKey> shops = boxKeys(world, BoxKind::Shop);
  for (const EntityKey depot : boxKeys(world, BoxKind::Depot)) {
    for (const FlowHolding &held : stockOf<Supplies>(world, depot)) {
      consumeUnits<Supplies>(world, depot, held.Handle, held.Units, RETURNED_CAUSE);
    }
    for (const FlowHolding &held : stockOf<SupplyOrders>(world, depot)) {
      const EntityKey shop = held.Handle;
      if (!std::ranges::binary_search(shops, shop)) {
        consumeUnits<SupplyOrders>(world, depot, shop, held.Units, CANCELLED_CAUSE);
        continue;
      }
      const std::optional<double> length = supplyRouteLength(world, depot, shop);
      if (!length) {
        sendUnits<SupplyOrders>(world, depot, shop, shop, held.Units, ORDER_DELAY);
        continue;
      }
      consumeUnits<SupplyOrders>(world, depot, shop, held.Units, FULFILLED_CAUSE);
      createUnits<Supplies>(world, depot, shop, held.Units);
      sendUnits<Supplies>(world, depot, shop, shop, held.Units, shipmentDelay(*length));
    }
  }
}

} // namespace

std::optional<double> supplyRouteLength(const World &world, EntityKey at, EntityKey source) {
  std::optional<double> least;
  for (const auto &[from, distance] : routeLengthsAt(world, at)) {
    if (from == source && (!least || distance < *least)) {
      least = distance;
    }
  }
  return least;
}

std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  std::optional<DepotRoute> nearest;
  for (const auto &[source, distance] : routeLengthsAt(world, shop)) {
    if (!std::ranges::binary_search(depots, source)) {
      continue;
    }
    if (!nearest || distance < nearest->Distance ||
        (distance == nearest->Distance && source < nearest->Depot)) {
      nearest = DepotRoute{source, distance};
    }
  }
  return nearest;
}

uint32_t shipmentDelay(double distance) {
  const double ticks = std::ceil(distance / (SUPPLY_SPEED * SIM_TICK_SECONDS));
  // The negated test also takes a NaN to 1.
  if (!(ticks >= 1.0)) {
    return 1;
  }
  constexpr double LONGEST = static_cast<double>(std::numeric_limits<uint32_t>::max());
  return ticks >= LONGEST ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(ticks);
}

int64_t inventoryPosition(const World &world, EntityKey shop) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  const auto isDepot = [&depots](EntityKey key) { return std::ranges::binary_search(depots, key); };
  int64_t position = 0;
  const FlowAddressed supplies = addressedTo<Supplies>(world, shop);
  for (const FlowStock &stock : supplies.Stocks) {
    if (stock.Endpoint == shop) {
      position += stock.Units;
    }
  }
  for (const FlowPacket &packet : supplies.Packets) {
    if (packet.To == shop) {
      position += packet.Units;
    }
  }
  const FlowAddressed orders = addressedTo<SupplyOrders>(world, shop);
  for (const FlowPacket &packet : orders.Packets) {
    if (isDepot(packet.To)) {
      position += packet.Units;
    }
  }
  for (const FlowStock &stock : orders.Stocks) {
    if (isDepot(stock.Endpoint)) {
      position += stock.Units;
    }
  }
  return position;
}

void addOperations(WorldSchema &schema) {
  addFlow<SupplyOrders>(schema);
  addFlow<Supplies>(schema);
  addFlow<GuestVisits>(schema);
  addFlow<Meals>(schema);
  schema.addSystem(&stepShops);
  schema.addSystem(&stepDepots);
}

} // namespace tpj
