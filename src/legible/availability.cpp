#include "legible/availability.h"

#include "sim/medium/field.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <algorithm>
#include <optional>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// The curve at a time in seconds: its first point's Y at or before its first X, its last point's
// at or beyond its last X, and its last point's for a NaN.
double discount(std::span<const CurvePoint> curve, double seconds) {
  const double t = std::clamp(seconds, curve.front().X, curve.back().X);
  for (size_t i = 1; i < curve.size(); ++i) {
    const CurvePoint &a = curve[i - 1];
    const CurvePoint &b = curve[i];
    if (t == a.X) {
      return a.Y;
    }
    if (t < b.X) {
      return a.Y + (t - a.X) * (b.Y - a.Y) / (b.X - a.X);
    }
  }
  // Only the last point and a NaN, which no comparison holds for, reach here.
  return curve.back().Y;
}

// The source's offer when it says it is supplied, found as guests find offers: the definition's
// offer at the source's lowest anchored node, or none.
std::optional<OfferEntry> suppliedOffer(const World &world,
                                        const AvailabilityDefinition &definition,
                                        const Network &network, EntityKey source) {
  const std::optional<uint32_t> anchor = network.firstAnchoredNode(source);
  if (!anchor) {
    return std::nullopt;
  }
  const std::optional<OfferEntry> offer = definition.Offer(world, network, *anchor, source);
  if (!offer || !offer->Supplied) {
    return std::nullopt;
  }
  return offer;
}

// The source's contribution through its route entry and its supplied offer. availabilityAt and
// availabilityValues both compute every term here, so a place's value is the same however it is
// asked for.
AvailabilityContribution contributionOf(const AvailabilityDefinition &definition, EntityKey source,
                                        const RouteEntry &route, const OfferEntry &offer) {
  AvailabilityContribution contribution;
  contribution.Shop = source;
  contribution.Relief = offer.Relief;
  contribution.Distance = route.Distance;
  contribution.Wait = static_cast<double>(offer.Wait) * SIM_TICK_SECONDS;
  contribution.Time = contribution.Distance / REFERENCE_SPEED + contribution.Wait;
  contribution.Term = contribution.Relief * discount(definition.Curve, contribution.Time);
  return contribution;
}

} // namespace

Availability availabilityAt(const World &world, const AvailabilityDefinition &definition,
                            const Place &place) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  Availability availability;
  EntityKey previous = NULL_KEY;
  for (const SampledEntry<RouteEntry> &route :
       sampleField<RouteDistance<PathKind::Guest>>(world, network, place)) {
    // Sources come in ascending order, and only a source's first entry counts.
    if (route.Source == previous) {
      continue;
    }
    previous = route.Source;
    const std::optional<OfferEntry> offer = suppliedOffer(world, definition, network, route.Source);
    if (!offer) {
      continue;
    }
    const AvailabilityContribution contribution =
        contributionOf(definition, route.Source, route.Value, *offer);
    availability.Value += contribution.Term;
    availability.Contributions.push_back(contribution);
  }
  return availability;
}

std::vector<double> availabilityValues(const World &world, const AvailabilityDefinition &definition,
                                       std::span<const Place> places) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  // Each source's supplied offer, found the first time a place reaches the source, in ascending
  // source order, and dropped when the call returns.
  using Found = std::pair<EntityKey, std::optional<OfferEntry>>;
  std::vector<Found> found;
  const auto offerOf = [&](EntityKey source) -> std::optional<OfferEntry> {
    const auto at = std::ranges::lower_bound(found, source, {}, &Found::first);
    if (at != found.end() && at->first == source) {
      return at->second;
    }
    return found.insert(at, Found{source, suppliedOffer(world, definition, network, source)})
        ->second;
  };
  std::vector<double> values(places.size(), 0.0);
  // Each place's last counted source, since only a source's first entry at a place counts.
  std::vector<EntityKey> counted(places.size(), NULL_KEY);
  // Along an edge, entries come one source at a time, so the next entry's offer is usually the
  // last one's.
  EntityKey lastSource = NULL_KEY;
  std::optional<OfferEntry> lastOffer;
  sampleFieldAtPlaces<RouteDistance<PathKind::Guest>>(
      world, network, places, [&](size_t index, const SampledEntry<RouteEntry> &route) {
        if (route.Source == counted[index]) {
          return;
        }
        counted[index] = route.Source;
        if (route.Source != lastSource) {
          lastSource = route.Source;
          lastOffer = offerOf(route.Source);
        }
        if (lastOffer) {
          values[index] += contributionOf(definition, route.Source, route.Value, *lastOffer).Term;
        }
      });
  return values;
}

} // namespace tpj
