#include "sim/operations/operations.h"

#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/internal/shop_service.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
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

// The shop takes back the orders returned to it, and when it has a nearest depot and its
// inventory position is at the reorder point or below, orders up to ORDER_UP_TO from that depot.
void orderSupplies(World &world, EntityKey shop, const std::optional<DepotRoute> &route) {
  const int64_t returned = unitsHeld<SupplyOrders>(world, shop, shop);
  if (returned > 0) {
    consumeUnits<SupplyOrders>(world, shop, shop, returned, UNFILLED_CAUSE);
  }
  const int64_t position = inventoryPosition(world, shop);
  if (!route || position > REORDER_POINT) {
    return;
  }
  const int64_t wanted = ORDER_UP_TO - position;
  createUnits<SupplyOrders>(world, shop, shop, wanted);
  sendUnits<SupplyOrders>(world, shop, route->Depot, shop, wanted, ORDER_DELAY);
}

bool isLive(const World &world, EntityKey key) {
  return key != NULL_KEY && world.findEntity(key) != entt::null;
}

// Consumes, as abandoned, every meal the shop holds, which only a gone guest's returned meal gives
// it, and every visit it holds for a guest that is gone.
void abandonGone(World &world, EntityKey shop) {
  for (const FlowHolding &held : stockOf<Meals>(world, shop)) {
    consumeUnits<Meals>(world, shop, held.Handle, held.Units, ABANDONED_CAUSE);
  }
  for (const FlowHolding &held : stockOf<GuestVisits>(world, shop)) {
    if (!isLive(world, held.Handle)) {
      consumeUnits<GuestVisits>(world, shop, held.Handle, held.Units, ABANDONED_CAUSE);
    }
  }
}

// The queue brought up to the visits the shop holds: each guest's earliest entries, up to its
// held units, then one entry for each further unit, guests in ascending key order.
std::vector<EntityKey> reconciledQueue(const World &world, EntityKey shop,
                                       const std::vector<EntityKey> &queue) {
  const std::vector<FlowHolding> held = stockOf<GuestVisits>(world, shop);
  const auto heldUnder = [&held](EntityKey guest) -> int64_t {
    const auto at = std::ranges::lower_bound(held, guest, {}, &FlowHolding::Handle);
    return at != held.end() && at->Handle == guest ? at->Units : 0;
  };
  std::map<EntityKey, int64_t> kept;
  std::vector<EntityKey> reconciled;
  for (const EntityKey guest : queue) {
    if (kept[guest] < heldUnder(guest)) {
      ++kept[guest];
      reconciled.push_back(guest);
    }
  }
  for (const FlowHolding &holding : held) {
    for (int64_t unit = kept[holding.Handle]; unit < holding.Units; ++unit) {
      reconciled.push_back(holding.Handle);
    }
  }
  return reconciled;
}

// The supplies the shop holds under its own handle and those on their way to it.
int64_t supplyCover(const World &world, EntityKey shop) {
  int64_t cover = unitsHeld<Supplies>(world, shop, shop);
  for (const FlowPacket &packet : addressedTo<Supplies>(world, shop).Packets) {
    if (packet.To == shop) {
      cover += packet.Units;
    }
  }
  return cover;
}

// Keeps the queue's first entries up to the shop's cover and sends the rest back unserved, one
// packet per guest.
void returnUncovered(World &world, EntityKey shop, std::vector<EntityKey> &queue) {
  const int64_t cover = supplyCover(world, shop);
  if (std::cmp_less_equal(queue.size(), cover)) {
    return;
  }
  std::map<EntityKey, int64_t> returned;
  for (auto at = queue.begin() + cover; at != queue.end(); ++at) {
    ++returned[*at];
  }
  queue.resize(static_cast<size_t>(cover));
  for (const auto &[guest, units] : returned) {
    sendUnits<GuestVisits>(world, shop, guest, guest, units, RETURN_DELAY);
  }
}

// When the shop is free and holds a supply, it converts one into a meal for the guest at the
// front, and sends the meal and the visit to the guest after the time service takes.
void serveFront(World &world, EntityKey shop, ShopService &service) {
  if (service.Queue.empty() || world.Tick < service.FreeAt ||
      unitsHeld<Supplies>(world, shop, shop) < 1) {
    return;
  }
  const EntityKey guest = service.Queue.front();
  service.Queue.erase(service.Queue.begin());
  consumeUnits<Supplies>(world, shop, shop, 1, SERVED_CAUSE);
  createUnits<Meals>(world, shop, guest, 1);
  sendUnits<Meals>(world, shop, guest, guest, 1, SERVICE_INTERVAL);
  sendUnits<GuestVisits>(world, shop, guest, guest, 1, SERVICE_INTERVAL);
  service.FreeAt = world.Tick + SERVICE_INTERVAL;
}

// The shop abandons what gone guests left, queues the visits it holds in arrival order, returns
// what a starved shop cannot cover, and serves the guest at the front when it can.
void serveGuests(World &world, EntityKey shop, bool supplied) {
  abandonGone(world, shop);
  const entt::entity entity = world.findEntity(shop);
  const ShopService *stored = world.Registry.try_get<ShopService>(entity);
  ShopService service = stored == nullptr ? ShopService{} : *stored;
  service.Queue = reconciledQueue(world, shop, service.Queue);
  if (!supplied) {
    returnUncovered(world, shop, service.Queue);
  }
  serveFront(world, shop, service);
  if (stored != nullptr || !service.Queue.empty() || service.FreeAt != 0) {
    world.Registry.emplace_or_replace<ShopService>(entity, std::move(service));
  }
}

// Each shop, in ascending key order, orders supplies and then serves its guests.
void stepShops(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    const std::optional<DepotRoute> route = nearestDepot(world, shop);
    orderSupplies(world, shop, route);
    serveGuests(world, shop, route.has_value());
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
  schema.addComponent<ShopService>("shop-service", DataKind::State);
  schema.addSystem(&stepShops);
  schema.addSystem(&stepDepots);
}

} // namespace tpj
