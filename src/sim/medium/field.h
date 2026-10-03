#ifndef TPJ_SIM_MEDIUM_FIELD_H
#define TPJ_SIM_MEDIUM_FIELD_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <iterator>
#include <optional>
#include <span>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

// Whether a field's entries are only listed, or also summed into a value.
enum class FieldKind { Entry, Scalar };

// An entry published at a place.
template <typename Entry> struct PlacedEntry {
  Place At;
  Entry Value{};
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, PlacedEntry<Entry> &entry) {
  visitor.field("at", entry.At);
  visitor.field("value", entry.Value);
}

// One source's entries in a field, in the order it published them.
template <typename Entry> struct FieldSlot {
  EntityKey Source = NULL_KEY;
  std::vector<PlacedEntry<Entry>> Entries;
  // The positions of Entries ordered by place, made on the slot's first sample. Derived from
  // Entries, which nothing changes in place, and never visited, so saves, hashes, and comparisons
  // never see it.
  mutable std::optional<std::vector<uint32_t>> ByPlace = std::nullopt;
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, FieldSlot<Entry> &slot) {
  visitor.field("source", slot.Source);
  visitor.field("entries", slot.Entries);
}

// Whether a's place orders before b's: by carrier key, then distance.
inline bool placeBefore(const Place &a, const Place &b) {
  return a.Carrier != b.Carrier ? a.Carrier < b.Carrier : a.Distance < b.Distance;
}

// The positions of the entries whose distance is not NaN, each once, ordered by carrier key, then
// distance, then position.
template <typename Entry>
std::vector<uint32_t> orderByPlace(std::span<const PlacedEntry<Entry>> entries) {
  std::vector<uint32_t> order;
  order.reserve(entries.size());
  for (uint32_t i = 0; i < entries.size(); ++i) {
    // A NaN distance equals no place and lies inside no edge, and would break the ordering.
    if (!std::isnan(entries[i].At.Distance)) {
      order.push_back(i);
    }
  }
  std::ranges::stable_sort(order, [entries](uint32_t a, uint32_t b) {
    return placeBefore(entries[a].At, entries[b].At);
  });
  return order;
}

// The slot's entries ordered by place, as orderByPlace gives them, made on the first call and kept
// in the slot.
template <typename Entry> std::span<const uint32_t> entriesByPlace(const FieldSlot<Entry> &slot) {
  if (!slot.ByPlace) {
    slot.ByPlace = orderByPlace(std::span<const PlacedEntry<Entry>>(slot.Entries));
  }
  return *slot.ByPlace;
}

// An entry sampled at a place, with the source that published it.
template <typename Entry> struct SampledEntry {
  EntityKey Source = NULL_KEY;
  Entry Value{};
};

// An entry strictly inside the sampled edge, with its distances along the carrier to the edge's
// two ends.
template <typename Entry> struct EdgeEntry {
  double FromOffset = 0.0;
  double ToOffset = 0.0;
  Entry Value{};
};

// What an owner's rule is given for one source at a place strictly inside an edge.
template <typename Entry> struct EdgeSample {
  NetworkEdge Edge;
  double FromOffset = 0.0;
  double ToOffset = 0.0;
  std::vector<Entry> AtFrom;
  std::vector<Entry> AtTo;
  std::vector<EdgeEntry<Entry>> Along;
};

// A field's definition, a type its owning module supplies:
//   struct HungryFootfall {
//     using Entry = double;
//     static constexpr std::string_view Name = "hungry-footfall";
//     static constexpr FieldKind Kind = FieldKind::Scalar;
//   };
template <typename F>
concept FieldDefinition = requires {
  typename F::Entry;
  { F::Name } -> std::convertible_to<std::string_view>;
  { F::Kind } -> std::convertible_to<FieldKind>;
};

// A field whose owner reads places inside an edge by its own rule:
//   static std::vector<Entry> sampleEdge(const EdgeSample<Entry> &sample);
template <typename F>
concept HasEdgeRule = FieldDefinition<F> && requires(const EdgeSample<typename F::Entry> &sample) {
  { F::sampleEdge(sample) } -> std::same_as<std::vector<typename F::Entry>>;
};

inline constexpr uint64_t FIELD_PURPOSE = hashName("field");

// The key of the entity holding the named field's entries.
constexpr EntityKey fieldKey(std::string_view name) {
  return deriveKey(NULL_KEY, FIELD_PURPOSE, hashName(name));
}

// A field's resolved entries, sources in ascending key order. Derived data. From the field's
// resolver to its finisher, Previous holds the previous resolution's slots and HasPrevious says
// there was one. Both are empty between resolutions.
template <FieldDefinition F> struct ResolvedEntries {
  std::vector<FieldSlot<typename F::Entry>> Slots;
  bool HasPrevious = false;
  std::vector<FieldSlot<typename F::Entry>> Previous;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, ResolvedEntries<F> &resolved) {
  visitor.field("slots", resolved.Slots);
  visitor.field("has-previous", resolved.HasPrevious);
  visitor.field("previous", resolved.Previous);
}

// A field's stepped entries, sources in ascending key order. State. Readable holds what the last
// swap made readable, and Pending what systems publish during the current tick.
template <FieldDefinition F> struct SteppedEntries {
  std::vector<FieldSlot<typename F::Entry>> Readable;
  std::vector<FieldSlot<typename F::Entry>> Pending;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, SteppedEntries<F> &stepped) {
  visitor.field("readable", stepped.Readable);
  visitor.field("pending", stepped.Pending);
}

// Collects the words the walk emits for a value, so values compare as worlds do.
class WordCollector : public WordSink {
public:
  void word(uint64_t value) override { Words.push_back(value); }
  void real(std::string_view /*field*/, double /*value*/) override {}
  [[nodiscard]] const std::vector<uint64_t> &words() const { return Words; }

private:
  std::vector<uint64_t> Words;
};

// The walk's words for a source's entries among the slots, an empty list when it has no slot.
template <typename Entry>
std::vector<uint64_t> entryWords(const std::vector<FieldSlot<Entry>> &slots, EntityKey source) {
  std::vector<PlacedEntry<Entry>> entries;
  const auto at = std::ranges::lower_bound(slots, source, {}, &FieldSlot<Entry>::Source);
  if (at != slots.end() && at->Source == source) {
    entries = at->Entries;
  }
  WordCollector collector;
  emitField(collector, "entries", entries);
  return collector.words();
}

// The field's resolver: empties its resolved entries, keeping the previous ones for its finisher,
// and creates the entity that holds both layers, with an empty stepped layer if it has none.
template <FieldDefinition F> void emptyResolvedEntries(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  const entt::entity entity = world.findEntity(key);
  ResolvedEntries<F> next;
  if (auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity)) {
    next.HasPrevious = true;
    next.Previous = std::move(resolved->Slots);
  }
  world.Registry.emplace_or_replace<ResolvedEntries<F>>(entity, std::move(next));
  static_cast<void>(world.Registry.get_or_emplace<SteppedEntries<F>>(entity));
}

// The field's swap: makes the entries published during the tick readable, replacing the last.
template <FieldDefinition F> void swapSteppedEntries(World &world) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *stepped =
      entity == entt::null ? nullptr : world.Registry.try_get<SteppedEntries<F>>(entity);
  if (stepped == nullptr) {
    return;
  }
  stepped->Readable = std::move(stepped->Pending);
  stepped->Pending.clear();
}

// The field's finisher: after a resolution with a previous one to compare with, clears the
// readable stepped entries of each source whose resolved entries changed, then drops the previous.
template <FieldDefinition F> void settleSteppedEntries(World &world) {
  using Slot = FieldSlot<typename F::Entry>;
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *resolved =
      entity == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr || !resolved->HasPrevious) {
    return;
  }
  auto &stepped = world.Registry.get<SteppedEntries<F>>(entity);
  std::erase_if(stepped.Readable, [resolved](const Slot &slot) {
    return entryWords(resolved->Previous, slot.Source) != entryWords(resolved->Slots, slot.Source);
  });
  resolved->HasPrevious = false;
  resolved->Previous.clear();
}

// Registers the field's derived component, <name>-resolved, its state component, <name>-stepped,
// its resolver, <name>-field, its swap, and its finisher. Throws std::invalid_argument when a name
// is malformed or already registered.
template <FieldDefinition F> void addField(WorldSchema &schema) {
  static_assert(F::Kind != FieldKind::Scalar || std::is_same_v<typename F::Entry, double>,
                "a scalar field's entries are doubles");
  const std::string name(F::Name);
  schema.addComponent<ResolvedEntries<F>>(name + "-resolved", DataKind::Derived);
  schema.addComponent<SteppedEntries<F>>(name + "-stepped", DataKind::State);
  schema.addResolver(name + "-field", &emptyResolvedEntries<F>);
  schema.addSwap(&swapSteppedEntries<F>);
  schema.addFinisher(&settleSteppedEntries<F>);
}

// Gives a source's entries for this resolution. Resolvers only. Throws std::logic_error when the
// world is not resolving or holds no resolved entries for the field, and std::invalid_argument for
// the null key or a source that has already published into the field in this resolution.
template <FieldDefinition F>
void publishResolved(World &world, EntityKey source,
                     std::vector<PlacedEntry<typename F::Entry>> entries) {
  using Slot = FieldSlot<typename F::Entry>;
  const std::string name(F::Name);
  if (!world.isResolving()) {
    throw std::logic_error("field " + name + " takes resolved entries only from resolvers");
  }
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *resolved =
      entity == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr) {
    throw std::logic_error("field " + name +
                           " holds no resolved entries; register it before its publishers");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + name + " takes no entries from the null key");
  }
  std::vector<Slot> &slots = resolved->Slots;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at != slots.end() && at->Source == source) {
    throw std::invalid_argument("source " + std::to_string(static_cast<uint64_t>(source)) +
                                " has already published into field " + name);
  }
  slots.insert(at, Slot{.Source = source, .Entries = std::move(entries)});
}

// Gives a source's entries for this tick, readable after the swap that ends it. Systems only.
// Throws std::logic_error when the world is not stepping or holds no stepped entries for the
// field, and std::invalid_argument for the null key or a source that has already published into
// the field in this tick.
template <FieldDefinition F>
void publishStepped(World &world, EntityKey source,
                    std::vector<PlacedEntry<typename F::Entry>> entries) {
  using Slot = FieldSlot<typename F::Entry>;
  const std::string name(F::Name);
  if (!world.isStepping()) {
    throw std::logic_error("field " + name + " takes stepped entries only from systems");
  }
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  auto *stepped =
      entity == entt::null ? nullptr : world.Registry.try_get<SteppedEntries<F>>(entity);
  if (stepped == nullptr) {
    throw std::logic_error("field " + name + " holds no stepped entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + name + " takes no entries from the null key");
  }
  std::vector<Slot> &slots = stepped->Pending;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at != slots.end() && at->Source == source) {
    throw std::invalid_argument("source " + std::to_string(static_cast<uint64_t>(source)) +
                                " has already published into field " + name + " this tick");
  }
  slots.insert(at, Slot{.Source = source, .Entries = std::move(entries)});
}

// The positions of the slot's entries at exactly the place, ascending.
template <typename Entry>
std::span<const uint32_t> positionsAt(const FieldSlot<Entry> &slot, const Place &place) {
  const std::span<const uint32_t> order = entriesByPlace(slot);
  const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
  const auto found = std::ranges::equal_range(order, place, placeBefore, placeOf);
  return {found.begin(), found.end()};
}

// Appends the positions of the slot's entries on the carrier strictly between the two distances.
template <typename Entry>
void positionsInside(const FieldSlot<Entry> &slot, EntityKey carrier, double from, double to,
                     std::vector<uint32_t> &positions) {
  const std::span<const uint32_t> order = entriesByPlace(slot);
  const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
  const auto first = std::ranges::upper_bound(order, Place{carrier, from}, placeBefore, placeOf);
  const auto last = std::ranges::lower_bound(order, Place{carrier, to}, placeBefore, placeOf);
  if (first < last) {
    positions.insert(positions.end(), first, last);
  }
}

// Clears positions and fills it with those of the slot's entries at any of the places, ascending
// and once each, so entries come out in the source's order.
template <typename Entry>
void positionsAtAny(const FieldSlot<Entry> &slot, std::span<const Place> places,
                    std::vector<uint32_t> &positions) {
  positions.clear();
  for (const Place &place : places) {
    const std::span<const uint32_t> found = positionsAt(slot, place);
    positions.insert(positions.end(), found.begin(), found.end());
  }
  std::ranges::sort(positions);
  const auto repeats = std::ranges::unique(positions);
  positions.erase(repeats.begin(), repeats.end());
}

// The working lists a sample reuses for each of its sources, so its sources' lookups allocate only
// while the lists grow.
template <typename Entry> struct SampleScratch {
  std::vector<uint32_t> Positions;
  std::vector<uint32_t> Inside;
  EdgeSample<Entry> Edge;
};

// Appends the source's entries whose places resolve to the node: exactly those at its stop
// places, found through the slot's order by place.
template <typename Entry>
void sampleSlotAtNode(const Network &network, const FieldSlot<Entry> &slot, uint32_t node,
                      SampleScratch<Entry> &scratch, std::vector<SampledEntry<Entry>> &sampled) {
  positionsAtAny(slot, network.stopPlaces(node), scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    sampled.push_back({slot.Source, slot.Entries[i].Value});
  }
}

// Appends the source's entries at a place strictly inside an edge: those at the same place, or
// what the field's own rule gives when it has one.
template <FieldDefinition F>
void sampleSlotInEdge(const Network &network, const FieldSlot<typename F::Entry> &slot,
                      [[maybe_unused]] const Place &place,
                      [[maybe_unused]] const EdgePosition &position,
                      [[maybe_unused]] SampleScratch<typename F::Entry> &scratch,
                      std::vector<SampledEntry<typename F::Entry>> &sampled) {
  using Entry = typename F::Entry;
  if constexpr (HasEdgeRule<F>) {
    const NetworkEdge &edge = network.edges()[position.Edge];
    const std::span<const Place> fromStops = network.stopPlaces(edge.From);
    const std::span<const Place> toStops = network.stopPlaces(edge.To);
    EdgeSample<Entry> &sample = scratch.Edge;
    sample.AtFrom.clear();
    sample.AtTo.clear();
    sample.Along.clear();
    sample.Edge = edge;
    sample.FromOffset = position.FromOffset;
    sample.ToOffset = position.ToOffset;
    // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
    // between its stops, and to one of its nodes exactly when it is at one of that node's stop
    // places, so no entry is resolved. The offsets are resolve's own subtractions.
    std::vector<uint32_t> &inside = scratch.Inside;
    inside.clear();
    positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, inside);
    std::ranges::sort(inside);
    for (const uint32_t i : inside) {
      const PlacedEntry<Entry> &entry = slot.Entries[i];
      sample.Along.push_back({entry.At.Distance - edge.FromDistance,
                              edge.ToDistance - entry.At.Distance, entry.Value});
    }
    // An entry inside the edge is not also at an end, as resolving it gives the inside.
    positionsAtAny(slot, fromStops, scratch.Positions);
    for (const uint32_t i : scratch.Positions) {
      if (!std::ranges::binary_search(inside, i)) {
        sample.AtFrom.push_back(slot.Entries[i].Value);
      }
    }
    positionsAtAny(slot, toStops, scratch.Positions);
    for (const uint32_t i : scratch.Positions) {
      if (!std::ranges::binary_search(inside, i)) {
        sample.AtTo.push_back(slot.Entries[i].Value);
      }
    }
    if (sample.AtFrom.empty() && sample.AtTo.empty() && sample.Along.empty()) {
      return;
    }
    for (const Entry &value : F::sampleEdge(sample)) {
      sampled.push_back({slot.Source, value});
    }
  } else {
    for (const uint32_t i : positionsAt(slot, place)) {
      sampled.push_back({slot.Source, slot.Entries[i].Value});
    }
  }
}

// Each source's slot, in ascending key order: its readable stepped slot when it has one, and
// otherwise its resolved one.
template <FieldDefinition F>
std::vector<const FieldSlot<typename F::Entry> *> layeredSlots(const World &world,
                                                               entt::entity entity) {
  using Slot = FieldSlot<typename F::Entry>;
  const std::vector<Slot> none;
  const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity);
  const auto *stepped = world.Registry.try_get<SteppedEntries<F>>(entity);
  const std::vector<Slot> &fromResolved = resolved != nullptr ? resolved->Slots : none;
  const std::vector<Slot> &fromStepped = stepped != nullptr ? stepped->Readable : none;
  std::vector<const Slot *> slots;
  auto r = fromResolved.begin();
  auto s = fromStepped.begin();
  while (r != fromResolved.end() || s != fromStepped.end()) {
    if (s == fromStepped.end() || (r != fromResolved.end() && r->Source < s->Source)) {
      slots.push_back(&*r);
      ++r;
    } else {
      if (r != fromResolved.end() && r->Source == s->Source) {
        ++r;
      }
      slots.push_back(&*s);
      ++s;
    }
  }
  return slots;
}

// The source's slot as the layer rule chooses it: its readable stepped slot when it has one, and
// otherwise its resolved one, or none. Allocates nothing.
template <FieldDefinition F>
const FieldSlot<typename F::Entry> *sourceSlot(const World &world, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  if (entity == entt::null) {
    return nullptr;
  }
  // Both lists keep their slots in ascending source order.
  const auto find = [source](const std::vector<Slot> &slots) -> const Slot * {
    const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
    return at != slots.end() && at->Source == source ? &*at : nullptr;
  };
  if (const auto *stepped = world.Registry.try_get<SteppedEntries<F>>(entity)) {
    if (const Slot *slot = find(stepped->Readable)) {
      return slot;
    }
  }
  const auto *resolved = world.Registry.try_get<ResolvedEntries<F>>(entity);
  return resolved != nullptr ? find(resolved->Slots) : nullptr;
}

// The entries of the slots at the place on the network, in the slots' order, by the default rule
// or the field's sampleEdge.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleSlots(const Network &network, const Place &place,
            const std::vector<const FieldSlot<typename F::Entry> *> &slots) {
  std::vector<SampledEntry<typename F::Entry>> sampled;
  const std::optional<NetworkPosition> position = network.resolve(place);
  if (!position) {
    return sampled;
  }
  SampleScratch<typename F::Entry> scratch;
  for (const FieldSlot<typename F::Entry> *slot : slots) {
    if (const auto *node = std::get_if<NodePosition>(&*position)) {
      sampleSlotAtNode(network, *slot, node->Node, scratch, sampled);
    } else {
      sampleSlotInEdge<F>(network, *slot, place, std::get<EdgePosition>(*position), scratch,
                          sampled);
    }
  }
  return sampled;
}

// The field's entries at the place on the network, each with its source, sources in ascending key
// order, by the default rule or the field's sampleEdge.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>> sampleField(const World &world, const Network &network,
                                                         const Place &place) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  if (entity == entt::null) {
    return {};
  }
  return sampleSlots<F>(network, place, layeredSlots<F>(world, entity));
}

// The first of the source's entries that sampleField gives at the node's nodePlace, or none.
// Throws std::out_of_range for a node not below the node count. Allocates nothing but the slot's
// order by place, when it is the slot's first reader.
template <FieldDefinition F>
std::optional<typename F::Entry> sourceEntryAtNode(const World &world, const Network &network,
                                                   uint32_t node, EntityKey source) {
  // The stops come first, so a node out of range throws whatever the world holds.
  const std::span<const Place> stops = network.stopPlaces(node);
  const FieldSlot<typename F::Entry> *slot = sourceSlot<F>(world, source);
  if (slot == nullptr) {
    return std::nullopt;
  }
  // Each place's positions ascend, so the least first position among the stops is the source's
  // first entry at the node.
  std::optional<uint32_t> first;
  for (const Place &stop : stops) {
    const std::span<const uint32_t> found = positionsAt(*slot, stop);
    if (!found.empty() && (!first || found.front() < *first)) {
      first = found.front();
    }
  }
  if (!first) {
    return std::nullopt;
  }
  return slot->Entries[*first].Value;
}

// The field's entries at the place, as sampleField gives them, but choosing each source's resolved
// entries whatever its stepped ones. Resolvers sample with it, since stepped entries are state.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleResolvedField(const World &world, const Network &network, const Place &place) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  const auto *resolved =
      entity == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr) {
    return {};
  }
  std::vector<const FieldSlot<typename F::Entry> *> slots;
  slots.reserve(resolved->Slots.size());
  for (const FieldSlot<typename F::Entry> &slot : resolved->Slots) {
    slots.push_back(&slot);
  }
  return sampleSlots<F>(network, place, slots);
}

// A scalar field's value at the place: 0.0 with each sampled entry added in turn.
template <FieldDefinition F>
  requires(F::Kind == FieldKind::Scalar)
double fieldValue(const World &world, const Network &network, const Place &place) {
  double value = 0.0;
  for (const SampledEntry<double> &entry : sampleField<F>(world, network, place)) {
    value += entry.Value;
  }
  return value;
}

} // namespace tpj

#endif
