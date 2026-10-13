#ifndef TPJ_LEGIBLE_AVAILABILITY_H
#define TPJ_LEGIBLE_AVAILABILITY_H

#include "sim/clock.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/operations/operations.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <array>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <utility>
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

// The lists availabilityBends and availabilityValues reuse. Keep one to compute availabilities many
// times without allocating once its lists have grown.
struct AvailabilityLists {
  // A source's least route distance at a stop.
  struct StopEntry {
    size_t Stop = 0;
    EntityKey Source = NULL_KEY;
    double Distance = 0.0;
  };
  // The guest segments' keys, ascending.
  std::vector<EntityKey> Segments;
  // Where each line's stops begin, with one more entry where the last line's end.
  std::vector<size_t> FirstStops;
  // Each source's least route distance at each stop, by stop, then by source.
  std::vector<StopEntry> Entries;
  // Where each stop's entries begin, with one more entry where the last stop's end.
  std::vector<size_t> StopEntries;
  // Each source's supplied offer found so far, in ascending source order.
  std::vector<std::pair<EntityKey, std::optional<OfferEntry>>> Offers;
  FieldSample<RouteEntry> Routes;
  // Each place's last counted source.
  std::vector<EntityKey> Counted;
};

// The value availabilityAt gives at each of the places, in order, finding each source's offer once
// in the call.
std::vector<double> availabilityValues(const World &world, const AvailabilityDefinition &definition,
                                       std::span<const Place> places);
// As the returning form, into values, which it clears first, and lists the caller keeps.
// Allocates nothing once they have grown.
void availabilityValues(const World &world, const AvailabilityDefinition &definition,
                        std::span<const Place> places, AvailabilityLists &lists,
                        std::vector<double> &values);

// The places along each guest segment's line where its availability can bend: each stop of the
// line, and inside each edge each place where a supplied source's route turns from one end to the
// other, or its effective time reaches a point of the curve. In ascending segment key, then
// ascending distance, each once. Between two consecutive bends on a line, availability is linear
// in distance.
std::vector<Place> availabilityBends(const World &world, const AvailabilityDefinition &definition);
// As the returning form, into bends, which it clears first, and lists the caller keeps. Allocates
// nothing once they have grown.
void availabilityBends(const World &world, const AvailabilityDefinition &definition,
                       AvailabilityLists &lists, std::vector<Place> &bends);

} // namespace tpj

#endif
