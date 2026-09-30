#include "legible/food.h"

#include "sim/medium/field.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <algorithm>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace tpj {
namespace {

// The source's offer when it says meals are supplied, found as guests find it: its first food-offer
// entry at the place of its lowest anchored node, or none.
std::optional<OfferEntry> suppliedOffer(const World &world, const Network &network,
                                        EntityKey source) {
  const std::vector<uint32_t> anchored = network.anchoredNodes(source);
  if (anchored.empty()) {
    return std::nullopt;
  }
  for (const SampledEntry<OfferEntry> &entry :
       sampleField<FoodOffer>(world, network, network.nodePlace(anchored.front()))) {
    if (entry.Source == source) {
      if (!entry.Value.Supplied) {
        return std::nullopt;
      }
      return entry.Value;
    }
  }
  return std::nullopt;
}

} // namespace

double foodDiscount(double seconds) {
  const double t = std::clamp(seconds, FOOD_DISCOUNT_CURVE.front().X, FOOD_DISCOUNT_CURVE.back().X);
  for (size_t i = 1; i < FOOD_DISCOUNT_CURVE.size(); ++i) {
    const CurvePoint &a = FOOD_DISCOUNT_CURVE[i - 1];
    const CurvePoint &b = FOOD_DISCOUNT_CURVE[i];
    if (t == a.X) {
      return a.Y;
    }
    if (t < b.X) {
      return a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X);
    }
  }
  // Only the last point and a NaN, which no comparison holds for, reach here.
  return FOOD_DISCOUNT_CURVE.back().Y;
}

FoodAvailability foodAvailability(const World &world, const Place &place) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  FoodAvailability availability;
  EntityKey previous = NULL_KEY;
  for (const SampledEntry<RouteEntry> &route :
       sampleField<RouteDistance<PathKind::Guest>>(world, network, place)) {
    // Sources come in ascending order, and only a source's first entry counts.
    if (route.Source == previous) {
      continue;
    }
    previous = route.Source;
    const std::optional<OfferEntry> offer = suppliedOffer(world, network, route.Source);
    if (!offer) {
      continue;
    }
    FoodContribution contribution;
    contribution.Shop = route.Source;
    contribution.Relief = offer->Relief;
    contribution.Distance = route.Value.Distance;
    contribution.Wait = static_cast<double>(offer->Wait) * SIM_TICK_SECONDS;
    contribution.Time = contribution.Distance / REFERENCE_SPEED + contribution.Wait;
    contribution.Term = contribution.Relief * foodDiscount(contribution.Time);
    availability.Value += contribution.Term;
    availability.Contributions.push_back(contribution);
  }
  return availability;
}

std::optional<FoodAvailability> foodNear(const World &world, GroundPoint point, double reach) {
  const std::optional<PathPlace> nearest = nearestGuestPathPlace(world, point);
  if (!nearest || nearest->Distance > reach) {
    return std::nullopt;
  }
  return foodAvailability(world, nearest->At);
}

} // namespace tpj
