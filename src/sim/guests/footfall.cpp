#include "sim/guests/footfall.h"

#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/guests/internal/footfall.h"
#include "sim/guests/internal/guest.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/routes/networks.h"
#include "sim/schema.h"
#include "sim/sim_math.h"
#include "sim/world.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

namespace {

// The field's entity, the source of its kept entries.
constexpr EntityKey FOOTFALL_SOURCE = fieldKey(HungryFootfall::Name);

// The index in edges() of the stretch the place lies in: the one of its own carrier whose
// FromDistance it is at or above and whose ToDistance it is below, or the carrier's last at its
// length. None when the network lacks the carrier or the distance is off it.
std::optional<size_t> stretchOf(const Network &network, const Place &place) {
  const std::vector<NetworkEdge> &edges = network.edges();
  const auto first = std::ranges::lower_bound(edges, place.Carrier, {}, &NetworkEdge::Carrier);
  const auto last =
      std::ranges::upper_bound(first, edges.end(), place.Carrier, {}, &NetworkEdge::Carrier);
  if (first == last || !(place.Distance >= first->FromDistance) ||
      place.Distance > std::prev(last)->ToDistance) {
    return std::nullopt;
  }
  const auto after =
      std::ranges::upper_bound(first, last, place.Distance, {}, &NetworkEdge::FromDistance);
  return static_cast<size_t>(std::prev(after) - edges.begin());
}

// The place of the stretch's carrier halfway between its stops, where its value is kept.
Place midpointOf(const NetworkEdge &edge) {
  return Place{edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2.0};
}

// A guest's stretch, as its index in edges(), and its hunger.
struct GuestOnStretch {
  size_t Stretch = 0;
  double Hunger = 0.0;
};

// A kept entry carried to a stretch of the new network, as its index in edges().
struct CarriedEntry {
  size_t Stretch = 0;
  const KeptEntry *Entry = nullptr;
};

// One entry at the stretch's midpoint for the entries carried into it: a lone entry keeps its
// value and tick, and several hold their values read at the tick, added in held order, with the
// tick.
KeptEntry combineCarried(std::span<const CarriedEntry> group, const Place &midpoint,
                         uint64_t tick) {
  if (group.size() == 1) {
    return {.At = midpoint, .Value = group.front().Entry->Value, .Tick = group.front().Entry->Tick};
  }
  double sum = 0.0;
  for (const CarriedEntry &carried : group) {
    sum += readKeptEntry<HungryFootfall>(*carried.Entry, tick);
  }
  return {.At = midpoint, .Value = sum, .Tick = tick};
}

// Moves the value of each stretch some guest stands in toward the summed hunger of the guests
// there, and visits no other stretch.
void stepFootfall(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  // In ascending key order, then grouped by stretch with that order kept within each.
  std::vector<GuestOnStretch> onStretches;
  for (const EntityKey key : parkGuests(world)) {
    const Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (const std::optional<size_t> stretch = stretchOf(network, guest.At)) {
      onStretches.push_back({.Stretch = *stretch, .Hunger = guest.Hunger});
    }
  }
  std::ranges::stable_sort(onStretches, {}, &GuestOnStretch::Stretch);
  for (auto group = onStretches.begin(); group != onStretches.end();) {
    const size_t stretch = group->Stretch;
    double hunger = 0.0;
    for (; group != onStretches.end() && group->Stretch == stretch; ++group) {
      hunger += group->Hunger;
    }
    const Place midpoint = midpointOf(network.edges()[stretch]);
    const double held = keptValue<HungryFootfall>(world, FOOTFALL_SOURCE, midpoint).value_or(0.0);
    keepEntry<HungryFootfall>(world, FOOTFALL_SOURCE, midpoint,
                              held + ((hunger - held) / static_cast<double>(FOOTFALL_TIME)));
  }
}

// Carries each kept value from the guest network before the resolution to the new one, holding
// one entry per stretch at its midpoint and dropping values whose places are retired.
void carryFootfall(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  if (before == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  std::vector<CarriedEntry> carried;
  for (const KeptEntry &entry : keptEntries<HungryFootfall>(world, FOOTFALL_SOURCE)) {
    const std::optional<Place> at = carryOver(entry.At, *before, after);
    if (const std::optional<size_t> stretch = at ? stretchOf(after, *at) : std::nullopt) {
      carried.push_back({.Stretch = *stretch, .Entry = &entry});
    }
  }
  std::ranges::stable_sort(carried, {}, &CarriedEntry::Stretch);
  std::vector<KeptEntry> entries;
  for (auto group = carried.begin(); group != carried.end();) {
    const size_t stretch = group->Stretch;
    const auto next = std::ranges::find_if(
        group, carried.end(), [stretch](const CarriedEntry &c) { return c.Stretch != stretch; });
    entries.push_back(combineCarried(std::span<const CarriedEntry>(group, next),
                                     midpointOf(after.edges()[stretch]), world.Tick));
    group = next;
  }
  replaceKeptEntries<HungryFootfall>(world, FOOTFALL_SOURCE, std::move(entries));
}

} // namespace

std::vector<double> HungryFootfall::sampleEdge(const EdgeSample<double> &sample) {
  std::vector<double> values;
  values.reserve(sample.Along.size());
  for (const EdgeEntry<double> &entry : sample.Along) {
    values.push_back(entry.Value);
  }
  return values;
}

double HungryFootfall::readKept(double value, uint64_t ticks) {
  const double perTick = 1.0 - (1.0 / static_cast<double>(FOOTFALL_TIME));
  return value * simExp(static_cast<double>(ticks) * simLog(perTick));
}

std::vector<double> HungryFootfall::sampleNode(const NodeSample &sample) {
  double sum = 0.0;
  for (const NodeEnd &end : sample.Ends) {
    double stretch = 0.0;
    for (const EdgeEntry<double> &entry : end.Along) {
      stretch += entry.Value;
    }
    sum += stretch;
  }
  return {sum / static_cast<double>(sample.Ends.size())};
}

void addFootfall(WorldSchema &schema) {
  addKeptField<HungryFootfall>(schema);
  schema.addSystem(&stepFootfall);
  schema.addFinisher(&carryFootfall);
}

} // namespace tpj
