#ifndef TPJ_LEGIBLE_AVAILABILITY_H
#define TPJ_LEGIBLE_AVAILABILITY_H

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"

#include <optional>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

class World;

// The walking speed, in meters per second, that turns a route distance into time: the guests'
// own, so an effective time is what a guest would feel.
inline constexpr double REFERENCE_SPEED = WALK_SPEED;

// A source's offer at a node of a network: its own first entry of one offer field at the node, or
// none.
using OfferAtNode = std::optional<OfferEntry> (*)(const World &world, const Network &network,
                                                  uint32_t node, EntityKey source);

// What an availability measures: the offer it reads, the curve that discounts a relief by the
// effective time in seconds, at least one point ascending in X, and the positive availability at
// which an overlay of it is fully saturated.
struct AvailabilityDefinition {
  OfferAtNode Offer = nullptr;
  std::span<const CurvePoint> Curve;
  double Full = 1.0;
};

// One source's part of an availability at a place: its offer's relief, its route distance in
// meters, its offer's wait and the effective time in seconds, and its term, the relief discounted
// by that time.
struct AvailabilityContribution {
  EntityKey Shop = NULL_KEY;
  double Relief = 0.0;
  double Distance = 0.0;
  double Wait = 0.0;
  double Time = 0.0;
  double Term = 0.0;

  bool operator==(const AvailabilityContribution &) const = default;
};

// An availability at a place and the contributions it is the sum of, in ascending source key.
struct Availability {
  std::vector<AvailabilityContribution> Contributions;
  double Value = 0.0;

  bool operator==(const Availability &) const = default;
};

// The availability at a place on the world's guest network: each reachable source whose offer says
// it is supplied contributes its relief discounted by its effective time, and the value is their
// terms added in order. Changes nothing.
Availability availabilityAt(const World &world, const AvailabilityDefinition &definition,
                            const Place &place);

// The value availabilityAt gives at each of the places, in order, finding each source's offer once
// in the call.
std::vector<double> availabilityValues(const World &world, const AvailabilityDefinition &definition,
                                       std::span<const Place> places);

// The places along each guest segment's line where its availability can bend: each stop of the
// line, and inside each edge each place where a supplied source's route turns from one end to the
// other, or its effective time reaches a point of the curve. In ascending segment key, then
// ascending distance, each once. Between two consecutive bends on a line, availability is linear
// in distance.
std::vector<Place> availabilityBends(const World &world, const AvailabilityDefinition &definition);

} // namespace tpj

#endif
