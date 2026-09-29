#ifndef TPJ_SIM_OPERATIONS_OPERATIONS_H
#define TPJ_SIM_OPERATIONS_OPERATIONS_H

#include "sim/entity_key.h"

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

inline constexpr std::string_view FULFILLED_CAUSE = "fulfilled";
inline constexpr std::string_view UNFILLED_CAUSE = "unfilled";
inline constexpr std::string_view CANCELLED_CAUSE = "cancelled";
inline constexpr std::string_view RETURNED_CAUSE = "returned";
inline constexpr std::string_view SERVED_CAUSE = "served";
inline constexpr std::string_view ABANDONED_CAUSE = "abandoned";

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
// The supplies the shop holds or has coming, and the orders on their way to or held by a depot.
int64_t inventoryPosition(const World &world, EntityKey shop);
// Registers the flow kinds supply-orders, supplies, guest-visits, and meals, the shop-service
// state, then the systems that step shops and then depots.
void addOperations(WorldSchema &schema);

} // namespace tpj

#endif
