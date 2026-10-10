#ifndef TPJ_LEGIBLE_AVAILABILITY_H
#define TPJ_LEGIBLE_AVAILABILITY_H

#include "sim/clock.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/world.h"

#include <array>
#include <optional>
#include <span>
#include <stdint.h>
#include <vector>

namespace tpj {

class World;

// The meters guests walk in a game minute at their own speed, so an effective time is what a guest
// would feel, on the park's clock.
inline constexpr double REFERENCE_SPEED =
    WALK_SPEED * SIM_TICK_SECONDS * 60.0 / static_cast<double>(GAME_SECONDS_PER_TICK);
// How much of a relief counts after an effective time in game minutes (decision 0020), for every
// need: half for each 20 minutes, and none from two hours.
inline constexpr std::array<CurvePoint, 6> DISCOUNT_CURVE{
    {{0.0, 1.0}, {20.0, 0.5}, {40.0, 0.25}, {60.0, 0.125}, {80.0, 0.0625}, {120.0, 0.0}}};

// A source's offer at a node of a network: its own first entry of one offer field at the node, or
// none.
using OfferAtNode = std::optional<OfferEntry> (*)(const World &world, const Network &network,
                                                  uint32_t node, EntityKey source);

// What an availability measures: the offer it reads, the curve that discounts a relief by the
// effective time in game minutes, at least one point ascending in X, and the positive availability
// that an overlay of it shades halfway up its ramp.
struct AvailabilityDefinition {
  OfferAtNode Offer = nullptr;
  std::span<const CurvePoint> Curve;
  double Half = 1.0;
};

// One source's part of an availability at a place: its offer's relief, its route distance in
// meters, its offer's wait and the effective time in game minutes, and its term, the relief
// discounted by that time.
struct AvailabilityContribution {
  EntityKey Source = NULL_KEY;
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
