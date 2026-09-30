#ifndef TPJ_SIM_MEDIUM_FIELD_H
#define TPJ_SIM_MEDIUM_FIELD_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <concepts>
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
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, FieldSlot<Entry> &slot) {
  visitor.field("source", slot.Source);
  visitor.field("entries", slot.Entries);
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
  slots.insert(at, Slot{source, std::move(entries)});
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
  slots.insert(at, Slot{source, std::move(entries)});
}

// Appends the source's entries whose places resolve to the node: exactly those among its stop
// places.
template <typename Entry>
void sampleSlotAtNode(const Network &network, const FieldSlot<Entry> &slot, uint32_t node,
                      std::vector<SampledEntry<Entry>> &sampled) {
  const std::span<const Place> stops = network.stopPlaces(node);
  for (const PlacedEntry<Entry> &entry : slot.Entries) {
    if (std::ranges::find(stops, entry.At) != stops.end()) {
      sampled.push_back({slot.Source, entry.Value});
    }
  }
}

// Appends the source's entries at a place strictly inside an edge: those at the same place, or
// what the field's own rule gives when it has one.
template <FieldDefinition F>
void sampleSlotInEdge(const Network &network, const FieldSlot<typename F::Entry> &slot,
                      [[maybe_unused]] const Place &place,
                      [[maybe_unused]] const EdgePosition &position,
                      std::vector<SampledEntry<typename F::Entry>> &sampled) {
  using Entry = typename F::Entry;
  if constexpr (HasEdgeRule<F>) {
    const NetworkEdge &edge = network.edges()[position.Edge];
    const std::span<const Place> fromStops = network.stopPlaces(edge.From);
    const std::span<const Place> toStops = network.stopPlaces(edge.To);
    EdgeSample<Entry> sample;
    sample.Edge = edge;
    sample.FromOffset = position.FromOffset;
    sample.ToOffset = position.ToOffset;
    // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
    // between its stops, and to one of its nodes exactly when it is among that node's stop places,
    // so no entry is resolved. The offsets are resolve's own subtractions.
    for (const PlacedEntry<Entry> &entry : slot.Entries) {
      const Place &at = entry.At;
      if (at.Carrier == edge.Carrier && at.Distance > edge.FromDistance &&
          at.Distance < edge.ToDistance) {
        sample.Along.push_back(
            {at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, entry.Value});
        continue;
      }
      if (std::ranges::find(fromStops, at) != fromStops.end()) {
        sample.AtFrom.push_back(entry.Value);
      }
      if (std::ranges::find(toStops, at) != toStops.end()) {
        sample.AtTo.push_back(entry.Value);
      }
    }
    if (sample.AtFrom.empty() && sample.AtTo.empty() && sample.Along.empty()) {
      return;
    }
    for (const Entry &value : F::sampleEdge(sample)) {
      sampled.push_back({slot.Source, value});
    }
  } else {
    for (const PlacedEntry<Entry> &entry : slot.Entries) {
      if (entry.At == place) {
        sampled.push_back({slot.Source, entry.Value});
      }
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
  for (const FieldSlot<typename F::Entry> *slot : slots) {
    if (const auto *node = std::get_if<NodePosition>(&*position)) {
      sampleSlotAtNode(network, *slot, node->Node, sampled);
    } else {
      sampleSlotInEdge<F>(network, *slot, place, std::get<EdgePosition>(*position), sampled);
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
