#include "support/ledger_writes.h"
#include "support/line_park.h"
#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

using test::carry;
using test::DEPOT;
using test::depotAt;
using test::GONE;
using test::hold;
using test::intentOf;
using test::LINE;
using test::linePath;
using test::LOOSE_SHOP;
using test::looseDepot;
using test::looseShop;
using test::packetsFrom;
using test::ParkIntent;
using test::SHOP;
using test::shopAt;
using test::worldOf;

// The line park with a guest path along z = -12 as well, 3 m from the front door of every shop on
// the line, and reaching x = 90, beyond the backstage line's end at x = 64.
constexpr EntityKey GUEST_LINE{9};
// A shop at x = 90, on the guest path, whose back door is far from the backstage line, so it has a
// guest anchor and no backstage connector.
constexpr EntityKey CUT_OFF_SHOP{10};

ParkPath guestLine() {
  return ParkPath{
      .Key = GUEST_LINE, .Kind = PathKind::Guest, .Points = {{-100.0, -12.0}, {100.0, -12.0}}};
}

// The line park with its guest path and the boxes, resolved, at tick 0 with empty ledgers.
World offerWorld(std::vector<ParkBox> boxes) {
  World world = worldOf(
      ParkIntent{.Entrances = {}, .Paths = {linePath(), guestLine()}, .Boxes = std::move(boxes)});
  resolveWorld(world);
  return world;
}

// The nodePlace of the shop's guest anchor, or none when it has no guest connector.
std::optional<Place> offerPlace(const World &world, EntityKey shop) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::vector<uint32_t> nodes = network.anchoredNodes(shop);
  REQUIRE(nodes.size() <= 1);
  if (nodes.empty()) {
    return std::nullopt;
  }
  return network.nodePlace(nodes.front());
}

std::vector<EntityKey> shopBoxes(const World &world) {
  std::vector<EntityKey> shops;
  for (const ParkBox &box : parkBoxes(world)) {
    if (box.Kind == BoxKind::Shop) {
      shops.push_back(box.Key);
    }
  }
  std::ranges::sort(shops);
  return shops;
}

entt::entity offerHolder(const World &world) {
  const entt::entity holder = world.findEntity(fieldKey(FoodOffer::Name));
  REQUIRE(holder != entt::null);
  return holder;
}

const std::vector<FieldSlot<OfferEntry>> &resolvedSlots(const World &world) {
  return world.Registry.get<ResolvedEntries<FoodOffer>>(offerHolder(world)).Slots;
}

// The stepped entries the last swap made readable.
const std::vector<FieldSlot<OfferEntry>> &steppedSlots(const World &world) {
  return world.Registry.get<SteppedEntries<FoodOffer>>(offerHolder(world)).Readable;
}

std::vector<EntityKey> sourcesOf(const std::vector<FieldSlot<OfferEntry>> &slots) {
  std::vector<EntityKey> sources;
  sources.reserve(slots.size());
  for (const FieldSlot<OfferEntry> &slot : slots) {
    sources.push_back(slot.Source);
  }
  return sources;
}

using Offers = std::vector<std::pair<Place, OfferEntry>>;

// The source's entries among the slots, or none when it has no slot.
std::optional<Offers> offersOf(const std::vector<FieldSlot<OfferEntry>> &slots, EntityKey source) {
  const auto slot = std::ranges::find(slots, source, &FieldSlot<OfferEntry>::Source);
  if (slot == slots.end()) {
    return std::nullopt;
  }
  Offers offers;
  for (const PlacedEntry<OfferEntry> &entry : slot->Entries) {
    offers.emplace_back(entry.At, entry.Value);
  }
  return offers;
}

// The places of the source's entries among the slots.
std::vector<Place> placesOf(const std::vector<FieldSlot<OfferEntry>> &slots, EntityKey source) {
  std::vector<Place> places;
  for (const auto &[at, offer] : offersOf(slots, source).value_or(Offers{})) {
    places.push_back(at);
  }
  return places;
}

// The shipment delay of the shop's nearest depot: shipmentDelay of the depot's supply route length
// to the shop, or of the nearest depot's Distance when that is none.
uint64_t shipDelay(const World &world, EntityKey shop) {
  const std::optional<DepotRoute> route = nearestDepot(world, shop);
  REQUIRE(route.has_value());
  const DepotRoute depot = route.value_or(DepotRoute{});
  return shipmentDelay(supplyRouteLength(world, depot.Depot, shop).value_or(depot.Distance));
}

// The offer a shop's resolved entry holds, in a world whose route distance has no stepped entries.
OfferEntry resolvedOffer(const World &world, EntityKey shop) {
  if (!nearestDepot(world, shop).has_value()) {
    return OfferEntry{};
  }
  return OfferEntry{
      .Relief = MEAL_RELIEF, .Wait = ORDER_DELAY + shipDelay(world, shop), .Supplied = true};
}

// The shop's entries: one at its offer place holding the offer, or none without a guest anchor.
Offers offeredAt(const World &world, EntityKey shop, const OfferEntry &offer) {
  const std::optional<Place> at = offerPlace(world, shop);
  return at.has_value() ? Offers{{at.value_or(Place{}), offer}} : Offers{};
}

std::vector<EntityKey> addGuests(World &world, std::size_t count) {
  std::vector<EntityKey> guests;
  for (std::size_t index = 0; index < count; ++index) {
    guests.push_back(world.createEntity());
  }
  return guests;
}

// Puts one visit of each of count new guests in the shop's stock.
void queueGuests(World &world, EntityKey shop, std::size_t count) {
  for (const EntityKey guest : addGuests(world, count)) {
    hold<GuestVisits>(world, shop, guest, 1);
  }
}

// A shipment of the units from the depot to the shop, arriving at the tick given.
FlowPacket shipment(int64_t units, uint64_t arrival) {
  return FlowPacket{.Arrival = arrival,
                    .From = DEPOT,
                    .To = SHOP,
                    .Handle = SHOP,
                    .Units = units,
                    .Delay = static_cast<uint32_t>(arrival),
                    .Returning = false};
}

void stepUntil(World &world, uint64_t tick) {
  while (world.Tick < tick) {
    stepWorld(world);
  }
}

// The walk's words for the world's resolved offers, so they compare as worlds do.
std::vector<uint64_t> resolvedWords(const World &world) {
  std::vector<FieldSlot<OfferEntry>> slots = resolvedSlots(world);
  WordCollector collector;
  emitField(collector, "slots", slots);
  return collector.words();
}

// Resolved offers.

TEST_CASE("After a resolution, food-offer's resolved entries hold one source for each shop box, "
          "with one entry at its offer place when it has a guest anchor and none otherwise, "
          "offering {MEAL_RELIEF, ORDER_DELAY + D, true} with a nearest depot and OfferEntry{} "
          "without") {
  CHECK(MEAL_RELIEF == 0.5);
  World world = offerWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), looseShop(), looseDepot(),
                            shopAt(CUT_OFF_SHOP, 90.0)});
  SECTION("the world's first resolution") {
    // A supplied shop with a guest anchor, a starved one with a guest anchor, and one with neither
    // connector.
    REQUIRE(nearestDepot(world, SHOP).has_value());
    REQUIRE(offerPlace(world, SHOP).has_value());
    REQUIRE_FALSE(nearestDepot(world, CUT_OFF_SHOP).has_value());
    REQUIRE(offerPlace(world, CUT_OFF_SHOP).has_value());
    REQUIRE_FALSE(offerPlace(world, LOOSE_SHOP).has_value());
  }
  SECTION("a resolution after a command that cuts the supply route") {
    CommandQueue queue;
    queueEdit(queue, DeletePath{LINE});
    stepWorld(world, queue);
    REQUIRE_FALSE(nearestDepot(world, SHOP).has_value());
  }

  const std::vector<EntityKey> shops = shopBoxes(world);
  CHECK(sourcesOf(resolvedSlots(world)) == shops);
  for (const EntityKey shop : shops) {
    CAPTURE(shop);
    CHECK(offersOf(resolvedSlots(world), shop) ==
          std::optional<Offers>(offeredAt(world, shop, resolvedOffer(world, shop))));
  }
}

using Backstage = RouteDistance<PathKind::Backstage>;

// Writes the source's stepped entries into backstage route distance, as a world's state may hold.
void writeSteppedRoutes(World &world, EntityKey source,
                        std::vector<PlacedEntry<RouteEntry>> entries) {
  const entt::entity holder = world.findEntity(fieldKey(Backstage::Name));
  REQUIRE(holder != entt::null);
  auto &readable = world.Registry.get<SteppedEntries<Backstage>>(holder).Readable;
  readable.insert(std::ranges::lower_bound(readable, source, {}, &FieldSlot<RouteEntry>::Source),
                  FieldSlot<RouteEntry>{.Source = source, .Entries = std::move(entries)});
}

Place backstagePlace(const World &world, EntityKey box) {
  const Network &network = parkNetwork(world, PathKind::Backstage);
  const std::vector<uint32_t> nodes = network.anchoredNodes(box);
  REQUIRE(nodes.size() == 1);
  return network.nodePlace(nodes.front());
}

TEST_CASE("A world's resolved food-offer entries after a resolution equal those of a new world "
          "with the same intent after its first resolution, whatever the world's state") {
  World world = offerWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), shopAt(CUT_OFF_SHOP, 90.0)});
  hold<Supplies>(world, SHOP, SHOP, 3);
  queueGuests(world, SHOP, 4);
  hold<Supplies>(world, CUT_OFF_SHOP, CUT_OFF_SHOP, 2);
  queueGuests(world, CUT_OFF_SHOP, 1);
  stepUntil(world, 5);

  SECTION("a resolution of a world holding stock, queues, orders, and stepped offers") {
    resolveWorld(world);
  }
  SECTION("a resolution after a command") {
    CommandQueue queue;
    queueEdit(queue, DeleteBox{CUT_OFF_SHOP});
    stepWorld(world, queue);
  }
  SECTION("stepped route distance entries that would leave the shop no depot") {
    writeSteppedRoutes(world, DEPOT, {});
    REQUIRE_FALSE(nearestDepot(world, SHOP).has_value());
    resolveWorld(world);
  }
  SECTION("stepped route distance entries that would shorten the depot's route to the shop") {
    writeSteppedRoutes(world, SHOP,
                       {PlacedEntry<RouteEntry>{.At = backstagePlace(world, DEPOT),
                                                .Value = RouteEntry{.Distance = 1.0, .Next = {}}}});
    REQUIRE(supplyRouteLength(world, DEPOT, SHOP) == std::optional<double>(1.0));
    resolveWorld(world);
  }

  World fresh = worldOf(intentOf(world));
  resolveWorld(fresh);
  CHECK(resolvedWords(world) == resolvedWords(fresh));
}

// Stepped offers.

TEST_CASE("In a cycle, each shop box publishes stepped entries at the places of its resolved ones, "
          "and a shop with no nearest depot offers OfferEntry{}") {
  World world = offerWorld(
      {shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0), looseShop(), shopAt(CUT_OFF_SHOP, 90.0)});
  // The starved shop holds stock and a queue, and still offers no meals.
  hold<Supplies>(world, CUT_OFF_SHOP, CUT_OFF_SHOP, 3);
  queueGuests(world, CUT_OFF_SHOP, 2);
  REQUIRE_FALSE(nearestDepot(world, CUT_OFF_SHOP).has_value());

  stepWorld(world);

  const std::vector<EntityKey> shops = shopBoxes(world);
  CHECK(sourcesOf(steppedSlots(world)) == shops);
  for (const EntityKey shop : shops) {
    CAPTURE(shop);
    CHECK(placesOf(steppedSlots(world), shop) == placesOf(resolvedSlots(world), shop));
  }
  CHECK(offersOf(steppedSlots(world), CUT_OFF_SHOP) ==
        std::optional<Offers>(offeredAt(world, CUT_OFF_SHOP, OfferEntry{})));
  CHECK(offersOf(steppedSlots(world), LOOSE_SHOP) == std::optional<Offers>(Offers{}));
}

TEST_CASE("A supplied shop's stepped offer is {MEAL_RELIEF, T_n - r, true}, T_n being when the "
          "guest behind its n queued ones is taken, from its supply units in order: stock, "
          "shipments in ascending arrival, then an order placed in the cycle") {
  World world = offerWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
  const uint64_t ordered = ORDER_DELAY + shipDelay(world, SHOP);
  uint64_t expected = 0;

  SECTION("a shop with nothing queued, held, or shipped waits for the order it places") {
    // t = 0 and r = 1: T_0 = max(r, 0, t + ORDER_DELAY + D).
    stepWorld(world);
    expected = ordered - 1;
  }
  SECTION("guests queued behind the one served in the cycle, taken from stock at the service "
          "rate") {
    hold<Supplies>(world, SHOP, SHOP, 5);
    queueGuests(world, SHOP, 3);
    // A visit of a guest that is gone is not queued.
    hold<GuestVisits>(world, SHOP, GONE, 1);
    stepWorld(world);
    // n = 2 and F = SERVICE_INTERVAL: T_i = (i + 1) * SERVICE_INTERVAL.
    expected = (3 * SERVICE_INTERVAL) - 1;
  }
  SECTION("guests waiting on shipments in ascending arrival, and then on an order placed in the "
          "cycle") {
    hold<Supplies>(world, SHOP, SHOP, 1);
    carry<Supplies>(world, shipment(2, 200));
    carry<Supplies>(world, shipment(1, 100));
    // A shipment under the shop's handle heading back to the depot brings the shop nothing.
    carry<Supplies>(world, FlowPacket{.Arrival = 50,
                                      .From = SHOP,
                                      .To = DEPOT,
                                      .Handle = SHOP,
                                      .Units = 5,
                                      .Delay = 50,
                                      .Returning = true});
    queueGuests(world, SHOP, 5);
    stepWorld(world);
    // The shop serves from its one unit of stock, so n = 4, F = SERVICE_INTERVAL, and units 0 to
    // 2 arrive at 100, 200, and 200, and units 3 and 4 at t + ORDER_DELAY + D.
    const uint64_t taken0 = std::max<uint64_t>(SERVICE_INTERVAL, 100);
    const uint64_t taken1 = std::max<uint64_t>(taken0 + SERVICE_INTERVAL, 200);
    const uint64_t taken2 = std::max<uint64_t>(taken1 + SERVICE_INTERVAL, 200);
    const uint64_t taken3 = std::max(taken2 + SERVICE_INTERVAL, ordered);
    const uint64_t taken4 = std::max(taken3 + SERVICE_INTERVAL, ordered);
    expected = taken4 - 1;
  }
  SECTION("a shop that served in an earlier cycle is free SERVICE_INTERVAL after it") {
    // Above the reorder point, so the stock is all the shop has.
    hold<Supplies>(world, SHOP, SHOP, 12);
    queueGuests(world, SHOP, 1);
    stepUntil(world, 10);
    REQUIRE(unitsCreated<Meals>(world) == 1);
    queueGuests(world, SHOP, 2);
    stepWorld(world);
    // t = 10, r = 11, n = 2, and F = 0 + SERVICE_INTERVAL: T_2 = F + 2 * SERVICE_INTERVAL.
    expected = (3 * SERVICE_INTERVAL) - 11;
  }
  SECTION("a shop with nothing on order still counts units beyond its shipments as coming from an "
          "order placed in the cycle") {
    // Its inventory position is 9, above the reorder point, so it orders nothing.
    hold<Supplies>(world, SHOP, SHOP, 1);
    carry<Supplies>(world, shipment(8, 1000));
    queueGuests(world, SHOP, 11);
    stepWorld(world);
    REQUIRE(packetsFrom<SupplyOrders>(world, SHOP).empty());
    // n = 10: units 0 to 7 arrive at 1000, and units 8 to 10 at t + ORDER_DELAY + D, after them in
    // order whenever they arrive.
    const uint64_t taken7 = 1000 + (7 * SERVICE_INTERVAL);
    const uint64_t taken8 = std::max(taken7 + SERVICE_INTERVAL, ordered);
    const uint64_t taken9 = std::max(taken8 + SERVICE_INTERVAL, ordered);
    const uint64_t taken10 = std::max(taken9 + SERVICE_INTERVAL, ordered);
    expected = taken10 - 1;
  }

  CHECK(offersOf(steppedSlots(world), SHOP) ==
        std::optional<Offers>(offeredAt(
            world, SHOP, OfferEntry{.Relief = MEAL_RELIEF, .Wait = expected, .Supplied = true})));
}

// Prediction.

// Every fixture below waits well under this, so a wrong wait fails rather than stepping for ever.
constexpr uint64_t LONGEST_WAIT = 2000;

// The wait of the supplied offer sampled at the shop's offer place.
uint64_t sampledWait(const World &world, EntityKey shop) {
  const std::optional<Place> at = offerPlace(world, shop);
  REQUIRE(at.has_value());
  const std::vector<SampledEntry<OfferEntry>> sampled =
      sampleField<FoodOffer>(world, parkNetwork(world, PathKind::Guest), at.value_or(Place{}));
  REQUIRE(sampled.size() == 1);
  REQUIRE(sampled.front().Source == shop);
  REQUIRE(sampled.front().Value.Supplied);
  return sampled.front().Value.Wait;
}

// Samples the shop's wait, hands the shop one more visit from a new guest, and checks that
// stepping with no command, the shop serves the guest in the cycle stepping t + wait.
void checkServedAfterWait(World world, EntityKey shop) {
  const uint64_t wait = sampledWait(world, shop);
  REQUIRE(wait <= LONGEST_WAIT);
  const uint64_t tick = world.Tick;
  const EntityKey guest = world.createEntity();
  hold<GuestVisits>(world, shop, guest, 1);

  stepUntil(world, tick + wait);
  CHECK(addressedTo<Meals>(world, guest).Packets.empty());
  stepWorld(world);
  CHECK(addressedTo<Meals>(world, guest).Packets.size() == 1);
}

TEST_CASE("A shop's offered wait predicts the cycle in which it serves the next guest to arrive") {
  SECTION("a shop box placed by the last cycle's command") {
    World world = offerWorld({depotAt(DEPOT, 32.0)});
    const AddBox add{.Kind = BoxKind::Shop, .At = shopAt(SHOP, 0.0).At};
    REQUIRE(isAccepted(world, add));
    CommandQueue queue;
    queueEdit(queue, add);
    stepWorld(world, queue);
    const std::vector<EntityKey> shops = shopBoxes(world);
    REQUIRE(shops.size() == 1);
    REQUIRE(nearestDepot(world, shops.front()).has_value());
    checkServedAfterWait(std::move(world), shops.front());
  }
  SECTION("a shop whose queue is served from stock at the service rate") {
    World world = offerWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
    hold<Supplies>(world, SHOP, SHOP, 5);
    queueGuests(world, SHOP, 2);
    stepWorld(world);
    checkServedAfterWait(std::move(world), SHOP);
  }
  SECTION("a shop waiting on a shipment") {
    World world = offerWorld({shopAt(SHOP, 0.0), depotAt(DEPOT, 32.0)});
    carry<Supplies>(world, shipment(3, 200));
    stepWorld(world);
    checkServedAfterWait(std::move(world), SHOP);
  }
}

} // namespace
} // namespace tpj
