#ifndef TPJ_SIM_OPERATIONS_OPERATIONS_H
#define TPJ_SIM_OPERATIONS_OPERATIONS_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/park/intent.h"

#include <array>
#include <optional>
#include <stdint.h>
#include <string_view>

namespace tpj {

class World;
class WorldSchema;

// Orders a shop sends a depot, one unit for each supply wanted, addressed to the shop.
struct SupplyOrders {
  static constexpr std::string_view Name = "supply-orders";
};

// Supplies a depot ships to a shop, addressed to the shop.
struct Supplies {
  static constexpr std::string_view Name = "supplies";
};

// A guest's visit to a shop, addressed to the guest, which the shop returns to it.
struct GuestVisits {
  static constexpr std::string_view Name = "guest-visits";
};

// A meal a shop sends the guest it served.
struct Meals {
  static constexpr std::string_view Name = "meals";
};

// Orders for drink a shop sends a depot, one unit for each supply wanted, addressed to the shop.
struct DrinkOrders {
  static constexpr std::string_view Name = "drink-orders";
};

// Drink a depot ships to a shop, addressed to the shop.
struct DrinkSupplies {
  static constexpr std::string_view Name = "drink-supplies";
};

// A guest's visit to a shop for drink, addressed to the guest, which the shop returns to it.
struct DrinkVisits {
  static constexpr std::string_view Name = "drink-visits";
};

// A drink a shop sends the guest it served.
struct Drinks {
  static constexpr std::string_view Name = "drinks";
};

// A shop's offer of a good: the need a serving relieves, on a scale from 0 to 1, the ticks the next
// guest to arrive for the good waits to be taken, and whether the shop has the good. A starved
// shop's offer is OfferEntry{}, which says none.
struct OfferEntry {
  double Relief = 0.0;
  uint64_t Wait = 0;
  bool Supplied = false;

  bool operator==(const OfferEntry &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, OfferEntry &entry) {
  visitor.field("relief", entry.Relief);
  visitor.field("wait", entry.Wait);
  visitor.field("supplied", entry.Supplied);
}

// The food offer field: each shop's food offer, at its guest anchor.
struct FoodOffer {
  using Entry = OfferEntry;
  static constexpr std::string_view Name = "food-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;

  // Keeps a shop's stepped food offer while it no longer sells food but holds or awaits some.
  static bool keepStepped(const World &world, EntityKey source);
};

// The drink offer field: each shop's drink offer, at its guest anchor.
struct DrinkOffer {
  using Entry = OfferEntry;
  static constexpr std::string_view Name = "drink-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;

  // Keeps a shop's stepped drink offer while it no longer sells drink but holds or awaits some.
  static bool keepStepped(const World &world, EntityKey source);
};

// A shop orders when its inventory position falls to the reorder point, up to the order-up-to
// level.
inline constexpr int64_t REORDER_POINT = 8;
inline constexpr int64_t ORDER_UP_TO = 24;
// Ticks an order takes to reach its depot.
inline constexpr uint32_t ORDER_DELAY = 30;
// Meters per second supplies move along the backstage route.
inline constexpr double SUPPLY_SPEED = 2.0;
// Ticks service takes: a served guest's meal and visit arrive this long after the shop takes it,
// and the shop takes no other guest until then.
inline constexpr uint32_t SERVICE_INTERVAL = 90;
// Ticks an unserved visit takes to go back to its guest.
inline constexpr uint32_t RETURN_DELAY = 1;
// The hunger a meal relieves, on a scale from 0 to 1.
inline constexpr double MEAL_RELIEF = 0.5;
// The thirst a drink relieves, on a scale from 0 to 1.
inline constexpr double DRINK_RELIEF = 0.5;

inline constexpr std::string_view FULFILLED_CAUSE = "fulfilled";
inline constexpr std::string_view UNFILLED_CAUSE = "unfilled";
inline constexpr std::string_view CANCELLED_CAUSE = "cancelled";
inline constexpr std::string_view RETURNED_CAUSE = "returned";
inline constexpr std::string_view SERVED_CAUSE = "served";
inline constexpr std::string_view ABANDONED_CAUSE = "abandoned";

// A good a shop sells.
enum class Good : uint8_t { Food, Drink };

constexpr std::array<std::string_view, 2> enumNames(Good /*value*/) { return {"food", "drink"}; }

// Every good, in the order shops order, serve, and offer them.
inline constexpr std::array<Good, 2> GOODS{Good::Food, Good::Drink};

// Whether a shop with the stock setting sells the good.
bool sells(ShopStock stock, Good good);

// What limits a shop's service now: no guests queued, fewer of a good than guests for it, the
// service rate, or no route to a depot.
enum class LimitingFactor : uint8_t { Demand, Supply, ServiceRate, NoSupplyRoute };

// A shop's holding of one good: the supplies it holds, and those on their way or on order.
struct GoodRecord {
  int64_t Stock = 0;
  int64_t OnOrder = 0;

  bool operator==(const GoodRecord &) const = default;
};

// What a shop publishes about itself for display and tests (decision 0025): its holding of each
// good, indexed by the good, the guests queued for any good, whether it has no route to a depot,
// and what limits its service. Nothing in the park reads it.
struct ShopRecord {
  std::array<GoodRecord, GOODS.size()> Goods{};
  int64_t Queue = 0;
  bool Starved = false;
  LimitingFactor Limit = LimitingFactor::Demand;

  bool operator==(const ShopRecord &) const = default;
};

// A shop's nearest depot and the backstage route length to it.
struct DepotRoute {
  EntityKey Depot = NULL_KEY;
  double Distance = 0.0;

  bool operator==(const DepotRoute &) const = default;
};

// The least backstage route distance from the nodes anchored to at to the source, or none.
std::optional<double> supplyRouteLength(const World &world, EntityKey at, EntityKey source);
// The depot box with the least supply route length from the shop, ties to the lower key, or none
// for a starved shop.
std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop);
// The ticks a shipment takes over a backstage route of the distance, at least 1.
uint32_t shipmentDelay(double distance);
// The supplies of the good the shop holds or has coming, and its orders for the good on their way
// to or held by a depot.
int64_t inventoryPosition(const World &world, EntityKey shop, Good good);
// The shop box's inspection record in the world as it stands, or none when the key is not a shop
// box.
std::optional<ShopRecord> shopRecord(const World &world, EntityKey shop);
// The factor's name for display: "demand", "supply", "service rate", or "no supply route".
std::string_view limitingFactorName(LimitingFactor factor);
// Registers the flow kinds supply-orders, supplies, guest-visits, meals, drink-orders,
// drink-supplies, drink-visits, and drinks, the shop-service state, the food-offer and drink-offer
// fields and their resolvers, then the systems that step shops and then depots.
// The routes module's registrations must come first.
void addOperations(WorldSchema &schema);

} // namespace tpj

#endif
