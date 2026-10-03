#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/save.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {
namespace {

// A key no entity of these worlds holds.
constexpr EntityKey ABSENT_SOURCE{0x7777};
constexpr EntityKey ABSENT_CARRIER{0x7778};

std::string routesText() {
  std::ifstream file(TPJ_PARKS_DIR "/routes.park", std::ios::binary);
  REQUIRE(file.is_open());
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

using EntryBits = std::array<uint64_t, 4>;

std::optional<EntryBits> bitsOf(const std::optional<RouteEntry> &entry) {
  if (!entry) {
    return std::nullopt;
  }
  return EntryBits{
      std::bit_cast<uint64_t>(entry->Distance), static_cast<uint64_t>(entry->Next.Carrier),
      std::bit_cast<uint64_t>(entry->Next.From), std::bit_cast<uint64_t>(entry->Next.To)};
}

// The source's entries in sampleField of the kind's route distance field at the place.
std::vector<RouteEntry> sampledFor(const World &world, PathKind kind, const Network &network,
                                   const Place &place, EntityKey source) {
  const std::vector<SampledEntry<RouteEntry>> sample =
      kind == PathKind::Guest
          ? sampleField<RouteDistance<PathKind::Guest>>(world, network, place)
          : sampleField<RouteDistance<PathKind::Backstage>>(world, network, place);
  std::vector<RouteEntry> entries;
  for (const SampledEntry<RouteEntry> &entry : sample) {
    if (entry.Source == source) {
      entries.push_back(entry.Value);
    }
  }
  return entries;
}

// Every entity anchoring a node of either kind's network, so each network is also asked about
// sources anchored only on the other.
std::vector<EntityKey> anchoringEntities(const World &world) {
  std::set<EntityKey> entities;
  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const Network &network = parkNetwork(world, kind);
    for (uint32_t node = 0; node < network.nodeCount(); ++node) {
      if (network.nodeAnchor(node) != NULL_KEY) {
        entities.insert(network.nodeAnchor(node));
      }
    }
  }
  return {entities.begin(), entities.end()};
}

// Every stop place of every node, since a node is found from any of them, not only its nodePlace;
// a middle and a quarter of each edge, since the two ends may tie at the middle; and places that
// do not resolve.
std::vector<Place> placesOn(const Network &network) {
  std::vector<Place> places;
  for (uint32_t node = 0; node < network.nodeCount(); ++node) {
    const std::span<const Place> stops = network.stopPlaces(node);
    places.insert(places.end(), stops.begin(), stops.end());
  }
  for (const NetworkEdge &edge : network.edges()) {
    places.push_back(
        {.Carrier = edge.Carrier, .Distance = edge.FromDistance + (edge.length() / 2)});
    places.push_back(
        {.Carrier = edge.Carrier, .Distance = edge.FromDistance + (edge.length() / 4)});
  }
  places.push_back({.Carrier = ABSENT_CARRIER, .Distance = 0.0});
  if (!network.carriers().empty()) {
    const Carrier &first = network.carriers().front();
    places.push_back({.Carrier = first.Key, .Distance = -1.0});
    places.push_back({.Carrier = first.Key, .Distance = first.Points.back().Distance + 1.0});
    places.push_back({.Carrier = first.Key, .Distance = std::numeric_limits<double>::quiet_NaN()});
  }
  return places;
}

// Where routeEntryAt gave some source an entry.
struct Found {
  int AtNodes = 0;
  int InEdges = 0;
};

// Compares routeEntryAt with sampleField for each source at each place, by bits.
Found checkAgainstSample(const World &world, PathKind kind, const Network &network,
                         const std::vector<EntityKey> &sources) {
  Found found;
  for (const Place &place : placesOn(network)) {
    const std::optional<NetworkPosition> position = network.resolve(place);
    for (const EntityKey source : sources) {
      CAPTURE(static_cast<int>(kind), place.Carrier, place.Distance, source);
      const std::vector<RouteEntry> sampled = sampledFor(world, kind, network, place, source);
      // Route distance gives a source at most one entry at a place.
      REQUIRE(sampled.size() <= 1);
      const std::optional<RouteEntry> expected =
          sampled.empty() ? std::nullopt : std::optional<RouteEntry>(sampled.front());
      CHECK(bitsOf(routeEntryAt(world, kind, network, place, source)) == bitsOf(expected));
      if (expected) {
        const bool atNode = position && std::holds_alternative<NodePosition>(*position);
        (atNode ? found.AtNodes : found.InEdges) += 1;
      }
    }
  }
  return found;
}

// Gives each source with resolved guest route distance a readable stepped slot: the first source
// its resolved entries with every distance raised, the rest an empty slot, so the layer rule
// changes what is sampled.
void layerSteppedEntries(World &world) {
  using Field = RouteDistance<PathKind::Guest>;
  const entt::entity holder = world.findEntity(fieldKey(Field::Name));
  REQUIRE(holder != entt::null);
  const auto &resolved = world.Registry.get<ResolvedEntries<Field>>(holder);
  REQUIRE(resolved.Slots.size() >= 2);
  auto &stepped = world.Registry.get<SteppedEntries<Field>>(holder);
  stepped.Readable.clear();
  for (size_t index = 0; index < resolved.Slots.size(); ++index) {
    FieldSlot<RouteEntry> slot{.Source = resolved.Slots[index].Source, .Entries = {}};
    if (index == 0) {
      slot.Entries = resolved.Slots[index].Entries;
      for (PlacedEntry<RouteEntry> &entry : slot.Entries) {
        entry.Value.Distance += 100.0;
      }
    }
    stepped.Readable.push_back(std::move(slot));
  }
}

TEST_CASE("routeEntryAt gives, bit for bit, the source's entry in sampleField of the kind's route "
          "distance field at the place, and none when that sample gives the source none") {
  World resolved = loadWorld(makeParkSchema(), routesText());
  const World unresolved = copyWorld(resolved);
  resolveWorld(resolved);
  World layered = copyWorld(resolved);
  layerSteppedEntries(layered);
  std::vector<EntityKey> sources = anchoringEntities(resolved);
  REQUIRE(sources.size() >= 3);
  sources.push_back(ABSENT_SOURCE);
  sources.push_back(NULL_KEY);

  for (const PathKind kind : {PathKind::Guest, PathKind::Backstage}) {
    const Network &network = parkNetwork(resolved, kind);
    const Found inResolved = checkAgainstSample(resolved, kind, network, sources);
    CHECK(inResolved.AtNodes > 0);
    CHECK(inResolved.InEdges > 0);
    // The stepped layer chooses a source's entries at nodes and inside edges alike.
    const Found inLayered = checkAgainstSample(layered, kind, network, sources);
    CHECK(inLayered.AtNodes > 0);
    CHECK(inLayered.InEdges > 0);
    // A world holding no route distance entries, sampled on a network it does not hold.
    const Found inUnresolved = checkAgainstSample(unresolved, kind, network, sources);
    CHECK(inUnresolved.AtNodes == 0);
    CHECK(inUnresolved.InEdges == 0);
  }
}

} // namespace
} // namespace tpj
