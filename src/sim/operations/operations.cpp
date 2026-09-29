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

// Which of a field's layers a sample reads: both, by the layer rule, or the resolved alone, as a
// resolver must.
enum class Layers : uint8_t { Both, Resolved };

// Each backstage route distance entry sampled from the layers at the nodes anchored to the entity,
// as its source and distance, in node order and then source order.
std::vector<std::pair<EntityKey, double>> routeLengthsAt(const World &world, EntityKey at,
                                                         Layers layers) {
  using Field = RouteDistance<PathKind::Backstage>;
  const Network &network = parkNetwork(world, PathKind::Backstage);
  std::vector<std::pair<EntityKey, double>> lengths;
  for (const uint32_t node : network.anchoredNodes(at)) {
    const Place place = network.nodePlace(node);
    for (const SampledEntry<RouteEntry> &entry :
         layers == Layers::Both ? sampleField<Field>(world, network, place)
                                : sampleResolvedField<Field>(world, network, place)) {
      lengths.emplace_back(entry.Source, entry.Value.Distance);
    }
  }
  return lengths;
}

// The least route length from the nodes anchored to at to the source, sampled from the layers.
std::optional<double> routeLengthBy(const World &world, EntityKey at, EntityKey source,
                                    Layers layers) {
  std::optional<double> least;
  for (const auto &[from, distance] : routeLengthsAt(world, at, layers)) {
    if (from == source && (!least || distance < *least)) {
      least = distance;
    }
  }
  return least;
}

// The shop's nearest depot, sampled from the layers.
std::optional<DepotRoute> depotRouteBy(const World &world, EntityKey shop, Layers layers) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  std::optional<DepotRoute> nearest;
  for (const auto &[source, distance] : routeLengthsAt(world, shop, layers)) {
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
// what a starved shop cannot cover, and serves the guest at the front when it can, giving its
// service after.
ShopService serveGuests(World &world, EntityKey shop, bool supplied) {
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
    world.Registry.emplace_or_replace<ShopService>(entity, service);
  }
  return service;
}

// The ticks a shipment from the route's depot takes to the shop: by the depot's own supply route
// length, the one it ships by, or by the shop's when the depot has none.
uint64_t deliveryDelay(const World &world, const DepotRoute &route, EntityKey shop, Layers layers) {
  return shipmentDelay(routeLengthBy(world, route.Depot, shop, layers).value_or(route.Distance));
}

// The shop's entries: the offer at each node anchored to it on the guest network.
std::vector<PlacedEntry<OfferEntry>> offerEntries(const World &world, EntityKey shop,
                                                  const OfferEntry &offer) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::vector<PlacedEntry<OfferEntry>> entries;
  for (const uint32_t node : network.anchoredNodes(shop)) {
    entries.push_back({network.nodePlace(node), offer});
  }
  return entries;
}

// Publishes each shop box's offer as for a shop that has not stepped: with nothing queued, held,
// or shipped, its first guest waits for the order it places in its first cycle.
void resolveFoodOffers(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    OfferEntry offer;
    if (const std::optional<DepotRoute> route = depotRouteBy(world, shop, Layers::Resolved)) {
      offer = OfferEntry{.Relief = MEAL_RELIEF,
                         .Wait = ORDER_DELAY + deliveryDelay(world, *route, shop, Layers::Resolved),
                         .Supplied = true};
    }
    publishResolved<FoodOffer>(world, shop, offerEntries(world, shop, offer));
  }
}

// A shipment's units and the tick they arrive.
struct Arriving {
  uint64_t Tick = 0;
  int64_t Units = 0;
};

// The ticks the next visit waits, from the cycle stepping from, in which the shop first holds it,
// to the one that takes its guest, behind the queued guests. Supply units come from the stock at
// from, then from the shipments in the order given, then from an order arriving at ordered, and
// each guest is taken at the later of its unit's tick and the previous guest's take plus
// SERVICE_INTERVAL, the first no earlier than from or freeAt.
uint64_t nextWait(uint64_t from, uint64_t freeAt, size_t queued, int64_t stock,
                  const std::vector<Arriving> &shipments, uint64_t ordered) {
  auto shipment = shipments.begin();
  int64_t left = stock;
  uint64_t unitAt = from;
  uint64_t ready = std::max(from, freeAt);
  uint64_t taken = ready;
  for (size_t guest = 0; guest <= queued; ++guest) {
    while (left == 0 && shipment != shipments.end()) {
      unitAt = shipment->Tick;
      left = shipment->Units;
      ++shipment;
    }
    if (left > 0) {
      --left;
    } else {
      unitAt = ordered;
    }
    taken = std::max(ready, unitAt);
    ready = taken + SERVICE_INTERVAL;
  }
  return taken - from;
}

// Publishes the shop's offer for the next visit to arrive, from its service, stock, and shipments
// after serving, and the order it would place in this cycle.
void publishOffer(World &world, EntityKey shop, const std::optional<DepotRoute> &route,
                  const ShopService &service) {
  OfferEntry offer;
  if (route) {
    std::vector<Arriving> shipments;
    for (const FlowPacket &packet : addressedTo<Supplies>(world, shop).Packets) {
      if (packet.To == shop) {
        shipments.push_back({packet.Arrival, packet.Units});
      }
    }
    const uint64_t ordered =
        world.Tick + ORDER_DELAY + deliveryDelay(world, *route, shop, Layers::Both);
    offer = OfferEntry{.Relief = MEAL_RELIEF,
                       .Wait = nextWait(world.Tick + 1, service.FreeAt, service.Queue.size(),
                                        unitsHeld<Supplies>(world, shop, shop), shipments, ordered),
                       .Supplied = true};
  }
  publishStepped<FoodOffer>(world, shop, offerEntries(world, shop, offer));
}

// Each shop, in ascending key order, orders supplies, serves its guests, and publishes its offer.
void stepShops(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    const std::optional<DepotRoute> route = nearestDepot(world, shop);
    orderSupplies(world, shop, route);
    const ShopService service = serveGuests(world, shop, route.has_value());
    publishOffer(world, shop, route, service);
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
  return routeLengthBy(world, at, source, Layers::Both);
}

std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop) {
  return depotRouteBy(world, shop, Layers::Both);
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

std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop) {
  const std::vector<EntityKey> shops = boxKeys(world, BoxKind::Shop);
  if (std::ranges::find(shops, shop) == shops.end()) {
    return std::nullopt;
  }
  ShopRecord record;
  record.Stock = unitsHeld<Supplies>(world, shop, shop);
  for (const FlowHolding &held : stockOf<GuestVisits>(world, shop)) {
    if (isLive(world, held.Handle)) {
      record.Queue += held.Units;
    }
  }
  record.OnOrder = inventoryPosition(world, shop) - record.Stock;
  record.Starved = !nearestDepot(world, shop).has_value();
  if (record.Starved) {
    record.Limit = LimitingFactor::NoSupplyRoute;
  } else if (record.Queue == 0) {
    record.Limit = LimitingFactor::Demand;
  } else if (record.Stock < record.Queue) {
    record.Limit = LimitingFactor::Supply;
  } else {
    record.Limit = LimitingFactor::ServiceRate;
  }
  return record;
}

std::string_view limitingFactorName(LimitingFactor factor) {
  switch (factor) {
  case LimitingFactor::Demand:
    return "demand";
  case LimitingFactor::Supply:
    return "supply";
  case LimitingFactor::ServiceRate:
    return "service rate";
  case LimitingFactor::NoSupplyRoute:
    return "no supply route";
  }
  return "";
}

void addOperations(WorldSchema &schema) {
  addFlow<SupplyOrders>(schema);
  addFlow<Supplies>(schema);
  addFlow<GuestVisits>(schema);
  addFlow<Meals>(schema);
  schema.addComponent<ShopService>("shop-service", DataKind::State);
  addField<FoodOffer>(schema);
  schema.addResolver("food-offer", &resolveFoodOffers,
                     {"path-networks", "route-distance", "food-offer-field"});
  schema.addSystem(&stepShops);
  schema.addSystem(&stepDepots);
}

} // namespace tpj
