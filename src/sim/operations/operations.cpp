#include "sim/operations/operations.h"

#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/facilities.h"
#include "sim/operations/internal/offer_entries.h"
#include "sim/operations/internal/shop_service.h"
#include "sim/operations/seats.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <stddef.h>
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

// A source and its route length.
using RouteLength = std::pair<EntityKey, double>;

// Each backstage route distance entry sampled from the layers at the nodes anchored to the entity,
// as its source and distance, in node order and then source order.
std::vector<RouteLength> routeLengthsAt(const World &world, EntityKey at, Layers layers) {
  using Field = RouteDistance<PathKind::Backstage>;
  const Network &network = parkNetwork(world, PathKind::Backstage);
  std::vector<RouteLength> lengths;
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

// The lists that reading route lengths at the tick reuses, and the lengths it gives.
struct RouteLengths {
  std::vector<uint32_t> Nodes;
  FieldSample<RouteEntry> Sample;
  std::vector<RouteLength> Lengths;
};

// Fills lengths.Lengths with what routeLengthsAt gives from both layers. Allocates nothing once the
// lists have grown.
void routeLengthsAt(const World &world, EntityKey at, RouteLengths &lengths) {
  using Field = RouteDistance<PathKind::Backstage>;
  const Network &network = parkNetwork(world, PathKind::Backstage);
  lengths.Lengths.clear();
  network.anchoredNodes(at, lengths.Nodes);
  for (const uint32_t node : lengths.Nodes) {
    sampleField<Field>(world, network, network.nodePlace(node), lengths.Sample);
    for (const SampledEntry<RouteEntry> &entry : lengths.Sample.Entries) {
      lengths.Lengths.emplace_back(entry.Source, entry.Value.Distance);
    }
  }
}

// The least of the lengths to the source.
std::optional<double> leastLengthTo(std::span<const RouteLength> lengths, EntityKey source) {
  std::optional<double> least;
  for (const auto &[from, distance] : lengths) {
    if (from == source && (!least || distance < *least)) {
      least = distance;
    }
  }
  return least;
}

// The least route length from the nodes anchored to at to the source, sampled from the layers.
std::optional<double> routeLengthBy(const World &world, EntityKey at, EntityKey source,
                                    Layers layers) {
  return leastLengthTo(routeLengthsAt(world, at, layers), source);
}

// The same from both layers, read through the kept lists.
std::optional<double> routeLengthBy(const World &world, EntityKey at, EntityKey source,
                                    RouteLengths &lengths) {
  routeLengthsAt(world, at, lengths);
  return leastLengthTo(lengths.Lengths, source);
}

// The nearest of the depots, whose keys are ascending, among the lengths.
std::optional<DepotRoute> nearestDepotIn(std::span<const RouteLength> lengths,
                                         std::span<const EntityKey> depots) {
  std::optional<DepotRoute> nearest;
  for (const auto &[source, distance] : lengths) {
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

// The shop's nearest depot, sampled from the layers.
std::optional<DepotRoute> depotRouteBy(const World &world, EntityKey shop, Layers layers) {
  return nearestDepotIn(routeLengthsAt(world, shop, layers), boxKeys(world, BoxKind::Depot));
}

// The same from both layers, among the depots, whose keys are ascending, read through the kept
// lists.
std::optional<DepotRoute> depotRouteBy(const World &world, EntityKey shop,
                                       std::span<const EntityKey> depots, RouteLengths &lengths) {
  routeLengthsAt(world, shop, lengths);
  return nearestDepotIn(lengths.Lengths, depots);
}

constexpr size_t goodIndex(Good good) { return static_cast<size_t>(good); }

static_assert(goodIndex(GOODS[0]) == 0 && goodIndex(GOODS[1]) == 1,
              "GOODS lists the goods in their enum order");

// The kinds and relief of each good.
template <Good G> struct GoodKinds;

template <> struct GoodKinds<Good::Food> {
  using Orders = SupplyOrders;
  using Stock = Supplies;
  using Visits = GuestVisits;
  using Servings = Meals;
  using Offer = FoodOffer;
  static constexpr double Relief = MEAL_RELIEF;
};

template <> struct GoodKinds<Good::Drink> {
  using Orders = DrinkOrders;
  using Stock = DrinkSupplies;
  using Visits = DrinkVisits;
  using Servings = Drinks;
  using Offer = DrinkOffer;
  static constexpr double Relief = DRINK_RELIEF;
};

template <typename Action, size_t... At>
void forEachGoodAt(Action &action, std::index_sequence<At...> /*at*/) {
  (action.template operator()<GOODS[At]>(), ...);
}

// Calls the action's operator()<G>() for each good G of GOODS, in order.
template <typename Action> void forEachGood(Action &&action) {
  forEachGoodAt(action, std::make_index_sequence<GOODS.size()>{});
}

// A value for each good, indexed by the good.
template <typename Value> using PerGood = std::array<Value, GOODS.size()>;

// A shipment's units and the tick they arrive.
struct Arriving {
  uint64_t Tick = 0;
  int64_t Units = 0;
};

// What a shop can serve a good from: its stock, its shipments in arrival order, and the tick an
// order placed now would arrive, or none when the shop doesn't order the good.
struct GoodSupply {
  int64_t Stock = 0;
  std::vector<Arriving> Shipments;
  std::optional<uint64_t> Ordered;
};

// A count for each key, keys ascending.
using KeyCounts = std::vector<std::pair<EntityKey, int64_t>>;

// The lists stepShops reuses for every shop, so its loop allocates only while they grow.
struct ShopBuffers {
  // Every box, ascending, read once a tick.
  std::vector<ParkBox> Boxes;
  // The depot boxes' keys, ascending, the same for every shop in a tick.
  std::vector<EntityKey> Depots;
  // A copy of the stock entries being consumed from.
  std::vector<FlowStock> Held;
  // The nodes an offer's entries are placed at.
  std::vector<uint32_t> Nodes;
  // The shop's queue as it is being served.
  std::vector<EntityKey> Queue;
  // The good of each entry of Queue.
  std::vector<Good> Goods;
  KeyCounts Counts;
  std::vector<EntityKey> Guests;
  std::vector<EntityKey> Kept;
  PerGood<KeyCounts> Returned;
  PerGood<GoodSupply> Supply;
};

// The key's count, added at 0 when the counts have none.
int64_t &countOf(KeyCounts &counts, EntityKey key) {
  auto at = std::ranges::lower_bound(counts, key, {}, &std::pair<EntityKey, int64_t>::first);
  if (at == counts.end() || at->first != key) {
    at = counts.insert(at, {key, 0});
  }
  return at->second;
}

// The endpoint's stock entries of the kind, handles ascending, read in place.
template <typename K> std::span<const FlowStock> stocksAt(const World &world, EntityKey endpoint) {
  const Ledger *ledger = ledgerOf<K>(world);
  if (ledger == nullptr) {
    return {};
  }
  const auto [first, last] =
      std::ranges::equal_range(ledger->Stocks, endpoint, {}, &FlowStock::Endpoint);
  return {first, last};
}

// The units of the kind in packets to the shop under its own handle.
template <typename K> int64_t unitsComing(const World &world, EntityKey shop) {
  const Ledger *ledger = ledgerOf<K>(world);
  int64_t units = 0;
  if (ledger != nullptr) {
    for (const FlowPacket &packet : ledger->Packets) {
      if (packet.Handle == shop && packet.To == shop) {
        units += packet.Units;
      }
    }
  }
  return units;
}

// The supplies of the good the shop holds or has coming, and its orders for the good on their way
// to or held by one of the depots, whose keys are ascending.
template <Good G>
int64_t positionOf(const World &world, EntityKey shop, std::span<const EntityKey> depots) {
  using Kinds = GoodKinds<G>;
  const auto isDepot = [depots](EntityKey key) { return std::ranges::binary_search(depots, key); };
  int64_t position = unitsHeld<typename Kinds::Stock>(world, shop, shop) +
                     unitsComing<typename Kinds::Stock>(world, shop);
  if (const Ledger *orders = ledgerOf<typename Kinds::Orders>(world)) {
    for (const FlowPacket &packet : orders->Packets) {
      if (packet.Handle == shop && isDepot(packet.To)) {
        position += packet.Units;
      }
    }
    for (const FlowStock &stock : orders->Stocks) {
      if (stock.Handle == shop && isDepot(stock.Endpoint)) {
        position += stock.Units;
      }
    }
  }
  return position;
}

// The shop takes back its orders for the good returned to it, and when it sells the good, has a
// nearest depot, and its position for the good is at the reorder point or below, orders up to
// ORDER_UP_TO from that depot.
template <Good G>
void orderSupplies(World &world, EntityKey shop, const std::optional<DepotRoute> &route,
                   bool selling, std::span<const EntityKey> depots) {
  using Orders = typename GoodKinds<G>::Orders;
  const int64_t returned = unitsHeld<Orders>(world, shop, shop);
  if (returned > 0) {
    consumeUnits<Orders>(world, shop, shop, returned, UNFILLED_CAUSE);
  }
  if (!selling || !route) {
    return;
  }
  const int64_t position = positionOf<G>(world, shop, depots);
  if (position > REORDER_POINT) {
    return;
  }
  const int64_t wanted = ORDER_UP_TO - position;
  createUnits<Orders>(world, shop, shop, wanted);
  sendUnits<Orders>(world, shop, route->Depot, shop, wanted, ORDER_DELAY);
}

bool isLive(const World &world, EntityKey key) { return key != NULL_KEY && world.isLive(key); }

// Consumes, as abandoned, every serving the shop holds, which only a gone guest's returned serving
// gives it, and every visit it holds for a guest that is gone. Copies each stock into held before
// consuming from it.
void abandonGone(World &world, EntityKey shop, std::vector<FlowStock> &held) {
  forEachGood([&]<Good G>() {
    using Kinds = GoodKinds<G>;
    const std::span<const FlowStock> servings = stocksAt<typename Kinds::Servings>(world, shop);
    copyGrowing(held, servings);
    for (const FlowStock &serving : held) {
      consumeUnits<typename Kinds::Servings>(world, shop, serving.Handle, serving.Units,
                                             ABANDONED_CAUSE);
    }
    const std::span<const FlowStock> visits = stocksAt<typename Kinds::Visits>(world, shop);
    if (std::ranges::all_of(
            visits, [&world](const FlowStock &visit) { return isLive(world, visit.Handle); })) {
      return;
    }
    copyGrowing(held, visits);
    for (const FlowStock &visit : held) {
      if (!isLive(world, visit.Handle)) {
        consumeUnits<typename Kinds::Visits>(world, shop, visit.Handle, visit.Units,
                                             ABANDONED_CAUSE);
      }
    }
  });
}

// The visits of the good the shop holds for the guest.
int64_t visitsHeld(const World &world, EntityKey shop, EntityKey guest, Good good) {
  int64_t units = 0;
  forEachGood([&]<Good G>() {
    if (G == good) {
      units = unitsHeld<typename GoodKinds<G>::Visits>(world, shop, guest);
    }
  });
  return units;
}

// The visits of every good the shop holds for the guest.
int64_t allVisitsHeld(const World &world, EntityKey shop, EntityKey guest) {
  int64_t units = 0;
  forEachGood(
      [&]<Good G>() { units += unitsHeld<typename GoodKinds<G>::Visits>(world, shop, guest); });
  return units;
}

// Makes the buffers' queue the queue brought up to the visits the shop holds, of every good: each
// guest's earliest entries, up to its held units, then one entry for each further unit, guests in
// ascending key order.
void reconcileQueue(const World &world, EntityKey shop, const std::vector<EntityKey> &queue,
                    ShopBuffers &buffers) {
  buffers.Queue.clear();
  buffers.Counts.clear();
  for (const EntityKey guest : queue) {
    int64_t &kept = countOf(buffers.Counts, guest);
    if (kept < allVisitsHeld(world, shop, guest)) {
      ++kept;
      buffers.Queue.push_back(guest);
    }
  }
  buffers.Guests.clear();
  forEachGood([&]<Good G>() {
    for (const FlowStock &held : stocksAt<typename GoodKinds<G>::Visits>(world, shop)) {
      buffers.Guests.push_back(held.Handle);
    }
  });
  std::ranges::sort(buffers.Guests);
  const auto repeated = std::ranges::unique(buffers.Guests);
  buffers.Guests.erase(repeated.begin(), repeated.end());
  for (const EntityKey guest : buffers.Guests) {
    const int64_t units = allVisitsHeld(world, shop, guest);
    for (int64_t unit = countOf(buffers.Counts, guest); unit < units; ++unit) {
      buffers.Queue.push_back(guest);
    }
  }
}

// Makes the buffers' goods the good each entry of their queue came for: a guest's entries take its
// visits in the order of the goods.
void fillQueueGoods(const World &world, EntityKey shop, ShopBuffers &buffers) {
  buffers.Counts.clear();
  buffers.Goods.clear();
  for (const EntityKey guest : buffers.Queue) {
    const int64_t entry = countOf(buffers.Counts, guest)++;
    Good good = GOODS.back();
    int64_t before = 0;
    for (const Good candidate : GOODS) {
      before += visitsHeld(world, shop, guest, candidate);
      if (entry < before) {
        good = candidate;
        break;
      }
    }
    buffers.Goods.push_back(good);
  }
}

// The supplies of the good the shop holds under its own handle and those on their way to it.
template <Good G> int64_t supplyCover(const World &world, EntityKey shop) {
  using Stock = typename GoodKinds<G>::Stock;
  return unitsHeld<Stock>(world, shop, shop) + unitsComing<Stock>(world, shop);
}

// For each good to return that the shop holds visits for, keeps the queue's entries for it up to
// the shop's cover of it and sends the rest back unserved, one packet per guest and good.
void returnUncovered(World &world, EntityKey shop, const PerGood<bool> &returning,
                     ShopBuffers &buffers) {
  PerGood<bool> checked{};
  PerGood<int64_t> cover{};
  forEachGood([&]<Good G>() {
    const size_t good = goodIndex(G);
    if (returning[good] && !stocksAt<typename GoodKinds<G>::Visits>(world, shop).empty()) {
      checked[good] = true;
      cover[good] = supplyCover<G>(world, shop);
    }
  });
  if (std::ranges::none_of(checked, [](bool good) { return good; })) {
    return;
  }
  fillQueueGoods(world, shop, buffers);
  PerGood<int64_t> counted{};
  for (KeyCounts &returned : buffers.Returned) {
    returned.clear();
  }
  buffers.Kept.clear();
  for (size_t entry = 0; entry < buffers.Queue.size(); ++entry) {
    const size_t good = goodIndex(buffers.Goods[entry]);
    if (checked[good] && ++counted[good] > cover[good]) {
      ++countOf(buffers.Returned[good], buffers.Queue[entry]);
    } else {
      buffers.Kept.push_back(buffers.Queue[entry]);
    }
  }
  if (buffers.Kept.size() == buffers.Queue.size()) {
    return;
  }
  buffers.Queue.swap(buffers.Kept);
  forEachGood([&]<Good G>() {
    for (const auto &[guest, units] : buffers.Returned[goodIndex(G)]) {
      sendUnits<typename GoodKinds<G>::Visits>(world, shop, guest, guest, units, RETURN_DELAY);
    }
  });
}

// When the shop is free, it takes the guest at the front of the queue for the first good the shop
// holds a visit of it for. If it holds a supply of that good, it converts one into a serving for
// the guest, and sends the serving and the visit to the guest after the time service takes.
void serveFront(World &world, EntityKey shop, std::vector<EntityKey> &queue, uint64_t &freeAt) {
  if (queue.empty() || world.Tick < freeAt) {
    return;
  }
  const EntityKey guest = queue.front();
  bool decided = false;
  forEachGood([&]<Good G>() {
    using Kinds = GoodKinds<G>;
    if (decided || unitsHeld<typename Kinds::Visits>(world, shop, guest) < 1) {
      return;
    }
    decided = true;
    if (unitsHeld<typename Kinds::Stock>(world, shop, shop) < 1) {
      return;
    }
    queue.erase(queue.begin());
    consumeUnits<typename Kinds::Stock>(world, shop, shop, 1, SERVED_CAUSE);
    createUnits<typename Kinds::Servings>(world, shop, guest, 1);
    sendUnits<typename Kinds::Servings>(world, shop, guest, guest, 1, SERVICE_INTERVAL);
    sendUnits<typename Kinds::Visits>(world, shop, guest, guest, 1, SERVICE_INTERVAL);
    freeAt = world.Tick + SERVICE_INTERVAL;
  });
}

// The shop abandons what gone guests left, queues the visits it holds in arrival order, returns
// what it cannot cover of each good when it is starved or no longer sells the good, and serves the
// guest at the front when it can. Leaves the queue after serving in the buffers, and gives the
// first tick at which the shop may serve again.
uint64_t serveGuests(World &world, EntityKey shop, ShopStock stock, bool supplied,
                     ShopBuffers &buffers) {
  abandonGone(world, shop, buffers.Held);
  ShopService *stored = world.tryGet<ShopService>(shop);
  static const std::vector<EntityKey> NONE;
  reconcileQueue(world, shop, stored == nullptr ? NONE : stored->Queue, buffers);
  PerGood<bool> returning{};
  for (const Good good : GOODS) {
    returning[goodIndex(good)] = !supplied || !sells(stock, good);
  }
  returnUncovered(world, shop, returning, buffers);
  uint64_t freeAt = stored == nullptr ? 0 : stored->FreeAt;
  serveFront(world, shop, buffers.Queue, freeAt);
  if (stored != nullptr) {
    copyGrowing(stored->Queue, std::span<const EntityKey>(buffers.Queue));
    stored->FreeAt = freeAt;
  } else if (!buffers.Queue.empty() || freeAt != 0) {
    world.emplace<ShopService>(shop, ShopService{buffers.Queue, freeAt});
  }
  return freeAt;
}

// The ticks a shipment from the route's depot takes to the shop: by the depot's own supply route
// length, the one it ships by, or by the shop's when the depot has none.
uint64_t deliveryDelay(const World &world, const DepotRoute &route, EntityKey shop, Layers layers) {
  return shipmentDelay(routeLengthBy(world, route.Depot, shop, layers).value_or(route.Distance));
}

// The same from both layers, read through the kept lists.
uint64_t deliveryDelay(const World &world, const DepotRoute &route, EntityKey shop,
                       RouteLengths &lengths) {
  return shipmentDelay(routeLengthBy(world, route.Depot, shop, lengths).value_or(route.Distance));
}

// Publishes the offer of the good of each shop box that sells it, as for a shop that has not
// stepped: with nothing queued, held, or shipped, its first guest waits for the order it places in
// its first cycle.
template <Good G> void resolveOffers(World &world) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind != BoxKind::Shop || !sells(box.Stock, G)) {
      continue;
    }
    OfferEntry offer;
    if (const std::optional<DepotRoute> route = depotRouteBy(world, box.Key, Layers::Resolved)) {
      offer =
          OfferEntry{.Relief = GoodKinds<G>::Relief,
                     .Wait = ORDER_DELAY + deliveryDelay(world, *route, box.Key, Layers::Resolved),
                     .Supplied = true};
    }
    publishResolved<typename GoodKinds<G>::Offer>(world, box.Key,
                                                  offerEntries(world, box.Key, offer));
  }
}

// Whether the source is a shop box that no longer sells the good but holds or awaits some.
template <Good G> bool keepsOffer(const World &world, EntityKey source) {
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Key == source) {
      return box.Kind == BoxKind::Shop && !sells(box.Stock, G) &&
             positionOf<G>(world, source, boxKeys(world, BoxKind::Depot)) > 0;
    }
  }
  return false;
}

// The ticks the next visit, for the arriving good, waits, from the cycle stepping from, in which
// the shop first holds it, to the one that takes its guest, behind the queued guests, whose goods
// are given. Each guest's unit comes from its good's stock at from, then the good's shipments in
// the order given, then the good's order, and each guest is taken at the later of its unit's tick
// and the previous guest's take plus SERVICE_INTERVAL, the first no earlier than from or freeAt.
// None when a guest's good has no unit left and no order.
std::optional<uint64_t> nextWait(uint64_t from, uint64_t freeAt, const std::vector<Good> &queued,
                                 const PerGood<GoodSupply> &supply, Good arriving) {
  struct Cursor {
    int64_t Left = 0;
    uint64_t UnitAt = 0;
    size_t Next = 0;
  };
  PerGood<Cursor> cursors;
  for (size_t good = 0; good < GOODS.size(); ++good) {
    cursors[good] = Cursor{.Left = supply[good].Stock, .UnitAt = from, .Next = 0};
  }
  uint64_t ready = std::max(from, freeAt);
  uint64_t taken = ready;
  for (size_t guest = 0; guest <= queued.size(); ++guest) {
    const size_t good = goodIndex(guest < queued.size() ? queued[guest] : arriving);
    const GoodSupply &source = supply[good];
    Cursor &cursor = cursors[good];
    while (cursor.Left == 0 && cursor.Next < source.Shipments.size()) {
      cursor.UnitAt = source.Shipments[cursor.Next].Tick;
      cursor.Left = source.Shipments[cursor.Next].Units;
      ++cursor.Next;
    }
    if (cursor.Left > 0) {
      --cursor.Left;
    } else if (source.Ordered) {
      cursor.UnitAt = *source.Ordered;
    } else {
      return std::nullopt;
    }
    taken = std::max(ready, cursor.UnitAt);
    ready = taken + SERVICE_INTERVAL;
  }
  return taken - from;
}

// Fills the buffers' supply with what the shop can serve each good from after serving, ordering
// each good it sells by the route's depot.
void fillSupply(const World &world, EntityKey shop, ShopStock stock, const DepotRoute &route,
                RouteLengths &lengths, ShopBuffers &buffers) {
  const uint64_t ordered = world.Tick + ORDER_DELAY + deliveryDelay(world, route, shop, lengths);
  forEachGood([&]<Good G>() {
    using Stock = typename GoodKinds<G>::Stock;
    GoodSupply &good = buffers.Supply[goodIndex(G)];
    good.Stock = unitsHeld<Stock>(world, shop, shop);
    good.Shipments.clear();
    if (const Ledger *ledger = ledgerOf<Stock>(world)) {
      for (const FlowPacket &packet : ledger->Packets) {
        if (packet.Handle == shop && packet.To == shop) {
          good.Shipments.push_back({packet.Arrival, packet.Units});
        }
      }
    }
    good.Ordered.reset();
    if (sells(stock, G)) {
      good.Ordered = ordered;
    }
  });
}

// Publishes the shop's offer of the good for the next visit for it to arrive, from its queue's
// goods after serving, the tick it may serve again, and, when supplied, what it can serve each good
// from. A shop that doesn't sell the good and neither holds nor awaits any publishes nothing.
template <Good G>
void publishOffer(World &world, EntityKey shop, bool selling, uint64_t freeAt, bool supplied,
                  ShopBuffers &buffers) {
  if (!selling && positionOf<G>(world, shop, buffers.Depots) == 0) {
    return;
  }
  OfferEntry offer;
  if (supplied) {
    if (const std::optional<uint64_t> wait =
            nextWait(world.Tick + 1, freeAt, buffers.Goods, buffers.Supply, G)) {
      offer = OfferEntry{.Relief = GoodKinds<G>::Relief, .Wait = *wait, .Supplied = true};
    }
  }
  fillOfferEntries(world, shop, offer, buffers.Nodes,
                   publishStepped<typename GoodKinds<G>::Offer>(world, shop));
}

// Each shop, in ascending key order, orders each good, serves its guests, and publishes its offer
// of each good.
void stepShops(World &world, TickBuffers &tick) {
  ShopBuffers &buffers = tick.get<ShopBuffers>();
  // Shared with stepDepots, which samples the same field, so that its lists have grown whenever a
  // depot ships.
  RouteLengths &lengths = tick.get<RouteLengths>();
  parkBoxes(world, buffers.Boxes);
  buffers.Depots.clear();
  for (const ParkBox &box : buffers.Boxes) {
    if (box.Kind == BoxKind::Depot) {
      buffers.Depots.push_back(box.Key);
    }
  }
  for (const ParkBox &box : buffers.Boxes) {
    if (box.Kind != BoxKind::Shop) {
      continue;
    }
    const EntityKey shop = box.Key;
    const std::optional<DepotRoute> route = depotRouteBy(world, shop, buffers.Depots, lengths);
    forEachGood([&]<Good G>() {
      orderSupplies<G>(world, shop, route, sells(box.Stock, G), buffers.Depots);
    });
    const uint64_t freeAt = serveGuests(world, shop, box.Stock, route.has_value(), buffers);
    fillQueueGoods(world, shop, buffers);
    if (route) {
      fillSupply(world, shop, box.Stock, *route, lengths, buffers);
    }
    forEachGood([&]<Good G>() {
      publishOffer<G>(world, shop, sells(box.Stock, G), freeAt, route.has_value(), buffers);
    });
  }
}

// The depot takes back the supplies of the good returned to it, and then, for each shop it holds
// orders for the good for, cancels them when the shop is gone, sends them back when it cannot
// reach the shop, and otherwise ships as many supplies over the backstage route.
// The lists stepDepots reuses from tick to tick.
struct DepotBuffers {
  std::vector<ParkBox> Boxes;
  // The shop and depot boxes' keys, ascending.
  std::vector<EntityKey> Shops;
  std::vector<EntityKey> Depots;
  // A copy of the stock entries being consumed from.
  std::vector<FlowStock> Held;
};

template <Good G>
void shipGood(World &world, EntityKey depot, DepotBuffers &buffers, RouteLengths &lengths) {
  using Orders = typename GoodKinds<G>::Orders;
  using Stock = typename GoodKinds<G>::Stock;
  const std::span<const FlowStock> returned = stocksAt<Stock>(world, depot);
  copyGrowing(buffers.Held, returned);
  for (const FlowStock &held : buffers.Held) {
    consumeUnits<Stock>(world, depot, held.Handle, held.Units, RETURNED_CAUSE);
  }
  const std::span<const FlowStock> orders = stocksAt<Orders>(world, depot);
  copyGrowing(buffers.Held, orders);
  for (const FlowStock &held : buffers.Held) {
    const EntityKey shop = held.Handle;
    if (!std::ranges::binary_search(buffers.Shops, shop)) {
      consumeUnits<Orders>(world, depot, shop, held.Units, CANCELLED_CAUSE);
      continue;
    }
    const std::optional<double> length = routeLengthBy(world, depot, shop, lengths);
    if (!length) {
      sendUnits<Orders>(world, depot, shop, shop, held.Units, ORDER_DELAY);
      continue;
    }
    consumeUnits<Orders>(world, depot, shop, held.Units, FULFILLED_CAUSE);
    createUnits<Stock>(world, depot, shop, held.Units);
    sendUnits<Stock>(world, depot, shop, shop, held.Units, shipmentDelay(*length));
  }
}

// Each depot, in ascending key order, handles each good's returned supplies and orders.
void stepDepots(World &world, TickBuffers &tick) {
  DepotBuffers &buffers = tick.get<DepotBuffers>();
  RouteLengths &lengths = tick.get<RouteLengths>();
  parkBoxes(world, buffers.Boxes);
  buffers.Shops.clear();
  buffers.Depots.clear();
  for (const ParkBox &box : buffers.Boxes) {
    if (box.Kind == BoxKind::Shop) {
      buffers.Shops.push_back(box.Key);
    } else if (box.Kind == BoxKind::Depot) {
      buffers.Depots.push_back(box.Key);
    }
  }
  for (const EntityKey depot : buffers.Depots) {
    forEachGood([&]<Good G>() { shipGood<G>(world, depot, buffers, lengths); });
  }
}

} // namespace

std::vector<PlacedEntry<OfferEntry>> offerEntries(const World &world, EntityKey source,
                                                  const OfferEntry &offer) {
  std::vector<uint32_t> nodes;
  std::vector<PlacedEntry<OfferEntry>> entries;
  fillOfferEntries(world, source, offer, nodes, entries);
  return entries;
}

void fillOfferEntries(const World &world, EntityKey source, const OfferEntry &offer,
                      std::vector<uint32_t> &nodes, std::vector<PlacedEntry<OfferEntry>> &entries) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  network.anchoredNodes(source, nodes);
  for (const uint32_t node : nodes) {
    entries.push_back({network.nodePlace(node), offer});
  }
}

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

bool sells(ShopStock stock, Good good) {
  switch (stock) {
  case ShopStock::Food:
    return good == Good::Food;
  case ShopStock::Drink:
    return good == Good::Drink;
  case ShopStock::Both:
    return true;
  }
  return false;
}

bool FoodOffer::keepStepped(const World &world, EntityKey source) {
  return keepsOffer<Good::Food>(world, source);
}

bool DrinkOffer::keepStepped(const World &world, EntityKey source) {
  return keepsOffer<Good::Drink>(world, source);
}

int64_t inventoryPosition(const World &world, EntityKey shop, Good good) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  int64_t position = 0;
  forEachGood([&]<Good G>() {
    if (G == good) {
      position = positionOf<G>(world, shop, depots);
    }
  });
  return position;
}

// The lists shopRecord keeps in the buffers from call to call.
struct ShopRecordLists {
  std::vector<ParkBox> Boxes;
  // The depot boxes' keys, ascending.
  std::vector<EntityKey> Depots;
  RouteLengths Lengths;
};

std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop) {
  TickBuffers buffers;
  return shopRecord(world, shop, buffers);
}

std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop, TickBuffers &buffers) {
  const std::optional<ParkBox> box = parkBox(world, shop);
  if (!box || box->Kind != BoxKind::Shop) {
    return std::nullopt;
  }
  ShopRecordLists &lists = buffers.get<ShopRecordLists>();
  parkBoxes(world, lists.Boxes);
  lists.Depots.clear();
  for (const ParkBox &depot : lists.Boxes) {
    if (depot.Kind == BoxKind::Depot) {
      lists.Depots.push_back(depot.Key);
    }
  }
  ShopRecord record;
  bool shortOfAGood = false;
  forEachGood([&]<Good G>() {
    using Kinds = GoodKinds<G>;
    GoodRecord &good = record.Goods[goodIndex(G)];
    good.Stock = unitsHeld<typename Kinds::Stock>(world, shop, shop);
    good.OnOrder = positionOf<G>(world, shop, lists.Depots) - good.Stock;
    int64_t queued = 0;
    if (const Ledger *visits = ledgerOf<typename Kinds::Visits>(world)) {
      const auto [first, last] =
          std::ranges::equal_range(visits->Stocks, shop, {}, &FlowStock::Endpoint);
      for (const FlowStock &held : std::span<const FlowStock>(first, last)) {
        if (isLive(world, held.Handle)) {
          queued += held.Units;
        }
      }
    }
    record.Queue += queued;
    shortOfAGood = shortOfAGood || good.Stock < queued;
  });
  record.Starved = !depotRouteBy(world, shop, lists.Depots, lists.Lengths).has_value();
  if (record.Starved) {
    record.Limit = LimitingFactor::NoSupplyRoute;
  } else if (record.Queue == 0) {
    record.Limit = LimitingFactor::Demand;
  } else if (shortOfAGood) {
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
  addFlow<DrinkOrders>(schema);
  addFlow<DrinkSupplies>(schema);
  addFlow<DrinkVisits>(schema);
  addFlow<Drinks>(schema);
  schema.addComponent<ShopService>("shop-service", DataKind::State);
  addField<FoodOffer>(schema);
  addField<DrinkOffer>(schema);
  schema.addResolver("food-offer", &resolveOffers<Good::Food>,
                     {"path-networks", "route-distance", "food-offer-field"});
  schema.addResolver("drink-offer", &resolveOffers<Good::Drink>,
                     {"path-networks", "route-distance", "drink-offer-field"});
  schema.addSystem(&stepShops);
  schema.addSystem(&stepDepots);
  addFacilities(schema);
  addSeats(schema);
}

} // namespace tpj
