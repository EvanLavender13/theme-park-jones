#include "legible/availability.h"

#include "sim/medium/field.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <stddef.h>
#include <tuple>
#include <utility>
#include <vector>

namespace tpj {
namespace {

// An offer's wait, in game minutes.
double waitMinutes(uint64_t ticks) {
  return static_cast<double>(ticks) * static_cast<double>(GAME_SECONDS_PER_TICK) / 60.0;
}

// The curve at a time in game minutes: its first point's Y at or before its first X, its last
// point's at or beyond its last X, and its last point's for a NaN.
double discount(std::span<const CurvePoint> curve, double minutes) {
  const double t = std::clamp(minutes, curve.front().X, curve.back().X);
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
  contribution.Source = source;
  contribution.Relief = offer.Relief;
  contribution.Distance = route.Distance;
  contribution.Wait = waitMinutes(offer.Wait);
  contribution.Time = contribution.Distance / REFERENCE_SPEED + contribution.Wait;
  contribution.Term = contribution.Relief * discount(definition.Curve, contribution.Time);
  return contribution;
}

// Each source's supplied offer, found the first time it is asked for, in ascending source order.
class SuppliedOffers {
public:
  SuppliedOffers(const World &world, const AvailabilityDefinition &definition,
                 const Network &network)
      : TheWorld(world), Definition(definition), TheNetwork(network) {}

  std::optional<OfferEntry> of(EntityKey source) {
    const auto at = std::ranges::lower_bound(Found, source, {}, &Entry::first);
    if (at != Found.end() && at->first == source) {
      return at->second;
    }
    return Found.insert(at, {source, suppliedOffer(TheWorld, Definition, TheNetwork, source)})
        ->second;
  }

private:
  const World &TheWorld;
  const AvailabilityDefinition &Definition;
  const Network &TheNetwork;
  using Entry = std::pair<EntityKey, std::optional<OfferEntry>>;
  std::vector<Entry> Found;
};

// A source's least route distance at a stop.
struct StopEntry {
  size_t Stop = 0;
  EntityKey Source = NULL_KEY;
  double Distance = 0.0;
};

// A stretch of a source's route distance along an edge, from Start to End along it: the distance
// at Start plus Slope times how far past Start, Slope 1 when the route leaves by the edge's start
// and -1 when it leaves by its end.
struct Stretch {
  double Start = 0.0;
  double End = 0.0;
  double At = 0.0;
  double Slope = 1.0;
};

// The stretches of a source's route distance along an edge of the length, from its least distance
// at the start, at the end, or both: the better end's distance plus the way to it (sim/routes).
int stretchesOf(std::optional<double> start, std::optional<double> end, double length,
                std::array<Stretch, 2> &out) {
  if (start && end) {
    const double turn = (*end + length - *start) / 2.0;
    if (turn > 0.0 && turn < length) {
      out[0] = {0.0, turn, *start, 1.0};
      out[1] = {turn, length, *start + turn, -1.0};
      return 2;
    }
    out[0] =
        turn <= 0.0 ? Stretch{0.0, length, *end + length, -1.0} : Stretch{0.0, length, *start, 1.0};
    return 1;
  }
  if (start) {
    out[0] = {0.0, length, *start, 1.0};
    return 1;
  }
  if (end) {
    out[0] = {0.0, length, *end + length, -1.0};
    return 1;
  }
  return 0;
}

// Adds to bends, as places on the carrier from the edge's start, each place strictly inside the
// edge where the source's route turns from one end to the other or its effective time reaches a
// point of the curve, with the source's wait in game minutes.
void addBends(EntityKey carrier, double from, double length, std::optional<double> start,
              std::optional<double> end, double wait, std::span<const CurvePoint> curve,
              std::vector<Place> &bends) {
  std::array<Stretch, 2> stretches;
  const int count = stretchesOf(start, end, length, stretches);
  if (count == 2) {
    bends.push_back({carrier, from + stretches[0].End});
  }
  for (const CurvePoint &point : curve) {
    const double distance = (point.X - wait) * REFERENCE_SPEED;
    for (int i = 0; i < count; ++i) {
      const Stretch &stretch = stretches[static_cast<size_t>(i)];
      const double along = stretch.Start + (distance - stretch.At) * stretch.Slope;
      if (along > stretch.Start && along < stretch.End) {
        bends.push_back({carrier, from + along});
      }
    }
  }
}

// Adds a place at each stop of each guest segment's line to places, lines in key order, and gives
// where each line's stops begin in places, with one more entry where the last line's end.
std::vector<size_t> addStops(const World &world, const Network &network,
                             std::vector<Place> &places) {
  std::vector<EntityKey> segments;
  for (const ParkSegment &segment : parkSegments(world)) {
    if (segment.Kind == PathKind::Guest) {
      segments.push_back(segment.Key);
    }
  }
  std::vector<size_t> firstStops;
  for (const Carrier &carrier : network.carriers()) {
    if (!std::ranges::binary_search(segments, carrier.Key)) {
      continue;
    }
    firstStops.push_back(places.size());
    for (const CarrierStop &stop : carrier.Stops) {
      places.push_back({carrier.Key, stop.Distance});
    }
  }
  firstStops.push_back(places.size());
  return firstStops;
}

// Each source's least route distance at each stop, by stop, then by source.
std::vector<StopEntry> leastAtStops(const World &world, const Network &network,
                                    std::span<const Place> stops) {
  std::vector<StopEntry> entries;
  sampleFieldAtPlaces<RouteDistance<PathKind::Guest>>(
      world, network, stops, [&entries](size_t index, const SampledEntry<RouteEntry> &route) {
        entries.push_back({index, route.Source, route.Value.Distance});
      });
  std::ranges::sort(entries, [](const StopEntry &a, const StopEntry &b) {
    return std::tie(a.Stop, a.Source, a.Distance) < std::tie(b.Stop, b.Source, b.Distance);
  });
  const auto [first, last] =
      std::ranges::unique(entries, [](const StopEntry &a, const StopEntry &b) {
        return a.Stop == b.Stop && a.Source == b.Source;
      });
  entries.erase(first, last);
  return entries;
}

// Where each stop's entries begin, with one more entry where the last stop's end.
std::vector<size_t> firstEntries(std::span<const StopEntry> entries, size_t stopCount) {
  std::vector<size_t> first(stopCount + 1, entries.size());
  for (size_t i = entries.size(); i-- > 0;) {
    first[entries[i].Stop] = i;
  }
  for (size_t stop = stopCount; stop-- > 0;) {
    first[stop] = std::min(first[stop], first[stop + 1]);
  }
  return first;
}

// An edge of a line: its carrier, the distance along it of its start, and its length.
struct Edge {
  EntityKey Carrier = NULL_KEY;
  double From = 0.0;
  double Length = 0.0;
};

// Adds the bends inside the edge of each supplied source with an entry at either of its stops,
// whose entries are atStart and atEnd, each in ascending source.
void addEdgeBends(const Edge &edge, std::span<const StopEntry> atStart,
                  std::span<const StopEntry> atEnd, const AvailabilityDefinition &definition,
                  SuppliedOffers &offers, std::vector<Place> &bends) {
  size_t a = 0;
  size_t b = 0;
  while (a < atStart.size() || b < atEnd.size()) {
    const bool fromStart =
        b == atEnd.size() || (a < atStart.size() && atStart[a].Source <= atEnd[b].Source);
    const EntityKey source = fromStart ? atStart[a].Source : atEnd[b].Source;
    std::optional<double> start;
    std::optional<double> end;
    if (a < atStart.size() && atStart[a].Source == source) {
      start = atStart[a++].Distance;
    }
    if (b < atEnd.size() && atEnd[b].Source == source) {
      end = atEnd[b++].Distance;
    }
    if (const std::optional<OfferEntry> offer = offers.of(source)) {
      addBends(edge.Carrier, edge.From, edge.Length, start, end, waitMinutes(offer->Wait),
               definition.Curve, bends);
    }
  }
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
  // Dropped when the call returns.
  SuppliedOffers offers(world, definition, network);
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
          lastOffer = offers.of(route.Source);
        }
        if (lastOffer) {
          values[index] += contributionOf(definition, route.Source, route.Value, *lastOffer).Term;
        }
      });
  return values;
}

std::vector<Place> availabilityBends(const World &world, const AvailabilityDefinition &definition) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::vector<Place> bends;
  const std::vector<size_t> firstStops = addStops(world, network, bends);
  const size_t stopCount = bends.size();
  const std::vector<StopEntry> entries =
      leastAtStops(world, network, std::span<const Place>(bends.data(), stopCount));
  const std::vector<size_t> stopEntries = firstEntries(entries, stopCount);
  SuppliedOffers offers(world, definition, network);
  for (size_t line = 0; line + 1 < firstStops.size(); ++line) {
    for (size_t stop = firstStops[line]; stop + 1 < firstStops[line + 1]; ++stop) {
      const Place start = bends[stop];
      const Edge edge{start.Carrier, start.Distance, bends[stop + 1].Distance - start.Distance};
      addEdgeBends(edge,
                   std::span<const StopEntry>(entries).subspan(
                       stopEntries[stop], stopEntries[stop + 1] - stopEntries[stop]),
                   std::span<const StopEntry>(entries).subspan(
                       stopEntries[stop + 1], stopEntries[stop + 2] - stopEntries[stop + 1]),
                   definition, offers, bends);
    }
  }
  std::ranges::sort(bends, [](const Place &a, const Place &b) {
    return std::tie(a.Carrier, a.Distance) < std::tie(b.Carrier, b.Distance);
  });
  const auto [same, end] = std::ranges::unique(bends);
  bends.erase(same, end);
  return bends;
}

} // namespace tpj
