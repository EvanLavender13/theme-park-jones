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
#include <stddef.h>
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
  // The positions of Entries ordered by place, made on the slot's first sample when Ordered is
  // false. Derived from Entries, and never visited, so saves, hashes, and comparisons never see
  // it. Whoever changes Entries in place sets Ordered to false.
  mutable std::vector<uint32_t> ByPlace;
  mutable bool Ordered = false;
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, FieldSlot<Entry> &slot) {
  visitor.field("source", slot.Source);
  visitor.field("entries", slot.Entries);
}

// Makes mine equal to other, keeping mine's lists' memory.
template <typename Entry> void keepAssign(FieldSlot<Entry> &mine, const FieldSlot<Entry> &other) {
  mine.Source = other.Source;
  copyGrowing(mine.Entries, std::span<const PlacedEntry<Entry>>(other.Entries));
  copyGrowing(mine.ByPlace, std::span<const uint32_t>(other.ByPlace));
  mine.Ordered = other.Ordered;
}

// Makes mine's slots equal to other's, keeping their lists' memory. Slots mine holds beyond
// other's move to spare with their source and lists emptied, as retired slots are, and slots it
// lacks come from spare first.
template <typename Entry>
void keepAssignSlots(std::vector<FieldSlot<Entry>> &mine,
                     const std::vector<FieldSlot<Entry>> &other,
                     std::vector<FieldSlot<Entry>> &spare) {
  while (mine.size() > other.size()) {
    FieldSlot<Entry> &extra = mine.back();
    extra.Source = NULL_KEY;
    extra.Entries.clear();
    extra.ByPlace.clear();
    extra.Ordered = false;
    spare.push_back(std::move(extra));
    mine.pop_back();
  }
  resizeKeeping(mine, other.size(), spare);
  for (size_t i = 0; i < other.size(); ++i) {
    keepAssign(mine[i], other[i]);
  }
}

// Whether a's place orders before b's: by carrier key, then distance.
inline bool placeBefore(const Place &a, const Place &b) {
  return a.Carrier != b.Carrier ? a.Carrier < b.Carrier : a.Distance < b.Distance;
}

// Fills order, which it clears first, with the positions of the items whose place's distance is
// not NaN, each once, ordered by carrier key, then distance, then position. Items are field entries
// or kept entries. Allocates nothing once the order has grown.
template <typename Item>
void orderItemsByPlace(std::span<const Item> items, std::vector<uint32_t> &order) {
  order.clear();
  for (uint32_t i = 0; i < items.size(); ++i) {
    // A NaN distance equals no place and lies inside no edge, and would break the ordering.
    if (!std::isnan(items[i].At.Distance)) {
      order.push_back(i);
    }
  }
  // Positions break ties, so this gives the stable order without stable_sort's buffer. Entries a
  // resolver keeps in place order are already in it.
  const auto before = [items](uint32_t a, uint32_t b) {
    if (placeBefore(items[a].At, items[b].At)) {
      return true;
    }
    return !placeBefore(items[b].At, items[a].At) && a < b;
  };
  if (!std::ranges::is_sorted(order, before)) {
    std::ranges::sort(order, before);
  }
}

// The positions of the entries whose distance is not NaN, each once, ordered by carrier key, then
// distance, then position.
template <typename Entry>
std::vector<uint32_t> orderByPlace(std::span<const PlacedEntry<Entry>> entries) {
  std::vector<uint32_t> order;
  orderItemsByPlace(entries, order);
  return order;
}

// The slot's entries ordered by place, as orderItemsByPlace gives them, made on the first call and
// kept in the slot. A slot is a field slot or a kept slot.
template <typename Slot> std::span<const uint32_t> entriesByPlace(const Slot &slot) {
  if (!slot.Ordered) {
    orderItemsByPlace(std::span(slot.Entries), slot.ByPlace);
    slot.Ordered = true;
  }
  return slot.ByPlace;
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

// A field whose owner reads places inside an edge by its own rule, which appends the entries at
// the sample's place to out:
//   static void sampleEdge(const EdgeSample<Entry> &sample, std::vector<Entry> &out);
template <typename F>
concept HasEdgeRule = FieldDefinition<F> && requires(const EdgeSample<typename F::Entry> &sample,
                                                     std::vector<typename F::Entry> &out) {
  { F::sampleEdge(sample, out) } -> std::same_as<void>;
};

// A field whose owner may keep a source's stepped entries through a resolution that changes the
// source's resolved entries:
//   static bool keepStepped(const World &world, EntityKey source);
template <typename F>
concept HasKeepRule = FieldDefinition<F> && requires(const World &world, EntityKey source) {
  { F::keepStepped(world, source) } -> std::same_as<bool>;
};

// A kept field: a scalar field whose owner reads a kept entry by its own rule, given the entry's
// value and the ticks since it last changed:
//   static double readKept(double value, uint64_t ticks);
// It registers with addKeptField, in sim/medium/kept_field.h, which holds the rest of it.
template <typename F>
concept KeptFieldDefinition =
    FieldDefinition<F> && (F::Kind == FieldKind::Scalar) && requires(double value, uint64_t ticks) {
      { F::readKept(value, ticks) } -> std::same_as<double>;
    };

// A field whose resolver edits its resolved entries in place and keeps the rest from one
// resolution to the next, through editResolved and dropResolved:
//   static constexpr bool KeepsResolved = true;
template <typename F>
concept KeepsResolvedEntries = FieldDefinition<F> && requires {
  { F::KeepsResolved } -> std::convertible_to<bool>;
} && F::KeepsResolved;

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
  // Slots the finisher took out of Previous, their lists emptied, kept so that the next
  // resolution's publishes reuse their memory. Never visited, so walks, saves, hashes, and
  // comparisons never see them.
  std::vector<FieldSlot<typename F::Entry>> Retired;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, ResolvedEntries<F> &resolved) {
  visitor.field("slots", resolved.Slots);
  visitor.field("has-previous", resolved.HasPrevious);
  visitor.field("previous", resolved.Previous);
}

// Makes mine equal to other, keeping its slots' memory, as copying a world refills it. Retired
// keeps mine's spare slots.
template <typename F> void keepAssign(ResolvedEntries<F> &mine, const ResolvedEntries<F> &other) {
  keepAssignSlots(mine.Slots, other.Slots, mine.Retired);
  mine.HasPrevious = other.HasPrevious;
  keepAssignSlots(mine.Previous, other.Previous, mine.Retired);
}

// A field's stepped entries, sources in ascending key order. State. Readable holds what the last
// swap made readable, and Pending what systems publish during the current tick.
template <FieldDefinition F> struct SteppedEntries {
  std::vector<FieldSlot<typename F::Entry>> Readable;
  std::vector<FieldSlot<typename F::Entry>> Pending;
  // Slots a swap took out of Readable, their lists emptied, kept so that the next publishes reuse
  // their memory. Never visited, so walks, saves, hashes, and comparisons never see them.
  std::vector<FieldSlot<typename F::Entry>> Retired;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, SteppedEntries<F> &stepped) {
  visitor.field("readable", stepped.Readable);
  visitor.field("pending", stepped.Pending);
}

// Makes mine equal to other, keeping its slots' memory, as copying a world refills it. Retired
// keeps mine's spare slots.
template <typename F> void keepAssign(SteppedEntries<F> &mine, const SteppedEntries<F> &other) {
  keepAssignSlots(mine.Readable, other.Readable, mine.Retired);
  keepAssignSlots(mine.Pending, other.Pending, mine.Retired);
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

// Moves the slots to the retired ones, each emptied and holding the null source, and empties the
// list, keeping every list's memory.
template <typename Entry>
void retireSlots(std::vector<FieldSlot<Entry>> &slots, std::vector<FieldSlot<Entry>> &retired) {
  for (FieldSlot<Entry> &slot : slots) {
    slot.Source = NULL_KEY;
    slot.Entries.clear();
    slot.ByPlace.clear();
    slot.Ordered = false;
    retired.push_back(std::move(slot));
  }
  slots.clear();
}

// The field's resolver: empties its resolved entries, keeping the previous ones for its finisher,
// and creates the entity that holds both layers, with an empty stepped layer if it has none. It
// changes the entries in place, so their memory serves the publishes that follow. A field that
// keeps its resolved entries keeps them as they are, with no previous ones until its resolver
// edits a source.
template <FieldDefinition F> void emptyResolvedEntries(World &world, TickBuffers & /*buffers*/) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  if (auto *resolved = world.tryGet<ResolvedEntries<F>>(key)) {
    retireSlots(resolved->Previous, resolved->Retired);
    if constexpr (KeepsResolvedEntries<F>) {
      resolved->HasPrevious = false;
    } else {
      resolved->Previous.swap(resolved->Slots);
      resolved->HasPrevious = true;
    }
  } else {
    world.emplace<ResolvedEntries<F>>(key);
  }
  static_cast<void>(world.getOrEmplace<SteppedEntries<F>>(key));
}

// The field's swap: makes the entries published during the tick readable, replacing the last,
// whose slots it keeps for the next publishes.
template <FieldDefinition F> void swapSteppedEntries(World &world, TickBuffers & /*buffers*/) {
  auto *stepped = world.tryGet<SteppedEntries<F>>(fieldKey(F::Name));
  if (stepped == nullptr) {
    return;
  }
  for (FieldSlot<typename F::Entry> &slot : stepped->Readable) {
    slot.Entries.clear();
    slot.ByPlace.clear();
    slot.Ordered = false;
    stepped->Retired.push_back(std::move(slot));
  }
  stepped->Readable.clear();
  stepped->Readable.swap(stepped->Pending);
}

// The field's finisher: after a resolution with a previous one to compare with, clears the
// readable stepped entries of each source whose resolved entries changed, unless the field's owner
// keeps them, then drops the previous.
template <FieldDefinition F> void settleSteppedEntries(World &world, TickBuffers & /*buffers*/) {
  using Slot = FieldSlot<typename F::Entry>;
  const EntityKey holder = fieldKey(F::Name);
  auto *resolved = world.tryGet<ResolvedEntries<F>>(holder);
  if (resolved == nullptr || !resolved->HasPrevious) {
    return;
  }
  auto &stepped = world.get<SteppedEntries<F>>(holder);
  std::erase_if(stepped.Readable, [&](const Slot &slot) {
    if constexpr (KeepsResolvedEntries<F>) {
      // Previous holds a copy of each source the resolver edited, so a source it lacks kept its
      // resolved entries.
      if (!std::ranges::binary_search(resolved->Previous, slot.Source, {}, &Slot::Source)) {
        return false;
      }
    }
    if (entryWords(resolved->Previous, slot.Source) == entryWords(resolved->Slots, slot.Source)) {
      return false;
    }
    if constexpr (HasKeepRule<F>) {
      return !F::keepStepped(world, slot.Source);
    }
    return true;
  });
  resolved->HasPrevious = false;
  retireSlots(resolved->Previous, resolved->Retired);
}

// Registers the field's derived component, <name>-resolved, its state component, <name>-stepped,
// its resolver, <name>-field, its swap, and its finisher. Throws std::invalid_argument when a name
// is malformed or already registered.
template <FieldDefinition F> void addField(WorldSchema &schema) {
  static_assert(F::Kind != FieldKind::Scalar || std::is_same_v<typename F::Entry, double>,
                "a scalar field's entries are doubles");
  static_assert(!KeptFieldDefinition<F>, "a kept field registers with addKeptField");
  const std::string name(F::Name);
  schema.addComponent<ResolvedEntries<F>>(name + "-resolved", DataKind::Derived);
  schema.addComponent<SteppedEntries<F>>(name + "-stepped", DataKind::State);
  schema.addResolver(name + "-field", &emptyResolvedEntries<F>);
  schema.addSwap(&swapSteppedEntries<F>);
  schema.addFinisher(&settleSteppedEntries<F>);
}

// Gives a source's entries for this resolution, empty, for the resolver to fill. They are readable
// at once. The list stays valid until the next publish into the field. Resolvers only. Throws
// std::logic_error when the world is not resolving or holds no resolved entries for the field, and
// std::invalid_argument for the null key or a source that has already published into the field in
// this resolution. The field's name is made only for the message, so a publish allocates nothing
// once the field's retired slots have grown.
template <FieldDefinition F>
std::vector<PlacedEntry<typename F::Entry>> &publishResolved(World &world, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  if (!world.isResolving()) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " takes resolved entries only from resolvers");
  }
  auto *resolved = world.tryGet<ResolvedEntries<F>>(fieldKey(F::Name));
  if (resolved == nullptr) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " holds no resolved entries; register it before its publishers");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entries from the null key");
  }
  std::vector<Slot> &slots = resolved->Slots;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at != slots.end() && at->Source == source) {
    throw std::invalid_argument("source " + std::to_string(static_cast<uint64_t>(source)) +
                                " has already published into field " + std::string(F::Name));
  }
  Slot slot;
  if (!resolved->Retired.empty()) {
    slot = std::move(resolved->Retired.back());
    resolved->Retired.pop_back();
  }
  slot.Source = source;
  return slots.insert(at, std::move(slot))->Entries;
}

// Throws as publishResolved does when the world is not resolving, holds no resolved entries for
// the field, or the source is the null key.
template <KeepsResolvedEntries F>
ResolvedEntries<F> &editableResolved(World &world, EntityKey source) {
  if (!world.isResolving()) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " takes resolved entries only from resolvers");
  }
  auto *resolved = world.tryGet<ResolvedEntries<F>>(fieldKey(F::Name));
  if (resolved == nullptr) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " holds no resolved entries; register it before its publishers");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entries from the null key");
  }
  return *resolved;
}

// Takes a slot for the source from the retired ones, or a new one.
template <FieldDefinition F>
FieldSlot<typename F::Entry> takeRetired(ResolvedEntries<F> &resolved, EntityKey source) {
  FieldSlot<typename F::Entry> slot;
  if (!resolved.Retired.empty()) {
    slot = std::move(resolved.Retired.back());
    resolved.Retired.pop_back();
  }
  slot.Source = source;
  return slot;
}

// Before the first edit of a source with readable stepped entries in this resolution, puts a copy
// of its resolved entries in Previous, so the finisher can tell whether they changed.
template <KeepsResolvedEntries F>
void keepPrevious(World &world, ResolvedEntries<F> &resolved, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  const auto *stepped = world.tryGet<SteppedEntries<F>>(fieldKey(F::Name));
  if (stepped == nullptr ||
      !std::ranges::binary_search(stepped->Readable, source, {}, &Slot::Source)) {
    return;
  }
  const auto at = std::ranges::lower_bound(resolved.Previous, source, {}, &Slot::Source);
  if (at != resolved.Previous.end() && at->Source == source) {
    return;
  }
  Slot copy = takeRetired(resolved, source);
  const auto current = std::ranges::lower_bound(resolved.Slots, source, {}, &Slot::Source);
  if (current != resolved.Slots.end() && current->Source == source) {
    keepAssign(copy, *current);
  }
  resolved.Previous.insert(at, std::move(copy));
  resolved.HasPrevious = true;
}

// Gives a source's resolved entries for this resolution to change in place, adding an empty list
// for a source that has none. They are readable at once. The list stays valid until the next edit,
// drop, or publish into the field. Resolvers only, for a field that keeps its resolved entries.
// Throws as publishResolved does when the world is not resolving, holds no resolved entries for
// the field, or the source is the null key. Allocates nothing once the field's retired and
// previous slots have grown.
template <KeepsResolvedEntries F>
std::vector<PlacedEntry<typename F::Entry>> &editResolved(World &world, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  ResolvedEntries<F> &resolved = editableResolved<F>(world, source);
  keepPrevious<F>(world, resolved, source);
  std::vector<Slot> &slots = resolved.Slots;
  auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at == slots.end() || at->Source != source) {
    at = slots.insert(at, takeRetired(resolved, source));
  }
  at->Ordered = false;
  return at->Entries;
}

// Removes a source's resolved entries, if it has any. Resolvers only, for a field that keeps its
// resolved entries, and throws as editResolved does.
template <KeepsResolvedEntries F> void dropResolved(World &world, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  ResolvedEntries<F> &resolved = editableResolved<F>(world, source);
  std::vector<Slot> &slots = resolved.Slots;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at == slots.end() || at->Source != source) {
    return;
  }
  keepPrevious<F>(world, resolved, source);
  at->Source = NULL_KEY;
  at->Entries.clear();
  at->ByPlace.clear();
  at->Ordered = false;
  resolved.Retired.push_back(std::move(*at));
  slots.erase(at);
}

// Gives a source's entries for this tick, empty, for the system to fill. They are readable after
// the swap that ends the tick. The list stays valid until the next publish into the field. Systems
// only. Throws std::logic_error when the world is not stepping or holds no stepped entries for the
// field, and std::invalid_argument for the null key or a source that has already published into
// the field in this tick.
template <FieldDefinition F>
std::vector<PlacedEntry<typename F::Entry>> &publishStepped(World &world, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  if (!world.isStepping()) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " takes stepped entries only from systems");
  }
  auto *stepped = world.tryGet<SteppedEntries<F>>(fieldKey(F::Name));
  if (stepped == nullptr) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " holds no stepped entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entries from the null key");
  }
  std::vector<Slot> &slots = stepped->Pending;
  const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
  if (at != slots.end() && at->Source == source) {
    throw std::invalid_argument("source " + std::to_string(static_cast<uint64_t>(source)) +
                                " has already published into field " + std::string(F::Name) +
                                " this tick");
  }
  Slot slot;
  if (!stepped->Retired.empty()) {
    slot = std::move(stepped->Retired.back());
    stepped->Retired.pop_back();
  }
  slot.Source = source;
  return slots.insert(at, std::move(slot))->Entries;
}

// The positions of the slot's entries at exactly the place, ascending.
template <typename Slot>
std::span<const uint32_t> positionsAt(const Slot &slot, const Place &place) {
  const std::span<const uint32_t> order = entriesByPlace(slot);
  const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
  const auto found = std::ranges::equal_range(order, place, placeBefore, placeOf);
  return {found.begin(), found.end()};
}

// Appends the positions of the slot's entries on the carrier strictly between the two distances.
template <typename Slot>
void positionsInside(const Slot &slot, EntityKey carrier, double from, double to,
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
template <typename Slot>
void positionsAtAny(const Slot &slot, std::span<const Place> places,
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
  // The entries the field's rule makes at one place.
  std::vector<Entry> Made;
};

// Appends the source's entries whose places resolve to the node: exactly those at its stop
// places, found through the slot's order by place, each with the value read gives its position.
template <typename Entry, typename Slot, typename Read>
void sampleSlotAtNode(const Network &network, const Slot &slot, uint32_t node,
                      SampleScratch<Entry> &scratch, std::vector<SampledEntry<Entry>> &sampled,
                      Read read) {
  positionsAtAny(slot, network.stopPlaces(node), scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    sampled.push_back({slot.Source, read(i)});
  }
}

// Fills the scratch's edge sample with the source's entries at the edge's two ends and strictly
// inside it, each with the value read gives its position, leaving the sample's offsets as they
// were, and gives whether it found any. They depend on the edge alone, so every place strictly
// inside the edge shares them.
template <typename Entry, typename Slot, typename Read>
bool gatherEdgeSample(const Network &network, const Slot &slot, const NetworkEdge &edge,
                      SampleScratch<Entry> &scratch, Read read) {
  const std::span<const Place> fromStops = network.stopPlaces(edge.From);
  const std::span<const Place> toStops = network.stopPlaces(edge.To);
  EdgeSample<Entry> &sample = scratch.Edge;
  sample.AtFrom.clear();
  sample.AtTo.clear();
  sample.Along.clear();
  sample.Edge = edge;
  // An entry resolves strictly inside the edge exactly when it is on the edge's carrier strictly
  // between its stops, and to one of its nodes exactly when it is at one of that node's stop
  // places, so no entry is resolved. The offsets are resolve's own subtractions.
  std::vector<uint32_t> &inside = scratch.Inside;
  inside.clear();
  positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, inside);
  std::ranges::sort(inside);
  for (const uint32_t i : inside) {
    const Place &at = slot.Entries[i].At;
    sample.Along.push_back(
        {at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, read(i)});
  }
  // An entry inside the edge is not also at an end, as resolving it gives the inside.
  positionsAtAny(slot, fromStops, scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    if (!std::ranges::binary_search(inside, i)) {
      sample.AtFrom.push_back(read(i));
    }
  }
  positionsAtAny(slot, toStops, scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    if (!std::ranges::binary_search(inside, i)) {
      sample.AtTo.push_back(read(i));
    }
  }
  return !sample.AtFrom.empty() || !sample.AtTo.empty() || !sample.Along.empty();
}

// A run of consecutive places strictly inside one edge: their indices and positions, kept across
// runs so they allocate only while they grow.
template <typename Entry> struct SampleRun {
  std::vector<size_t> Indices;
  std::vector<EdgePosition> Positions;
};

// The working lists of sampleSlotsAt: the scratch each source's lookups reuse, the run of places
// inside one edge, and the entries at one node.
template <typename Entry> struct SampleLists {
  SampleScratch<Entry> Scratch;
  SampleRun<Entry> Run;
  std::vector<SampledEntry<Entry>> Single;
};

// Hands visit(index, entry) each entry from the slots at the run's places, one slot at a time and,
// for each slot, the run's places in order, and empties the run: those at exactly the place, or
// what the field's sampleEdge makes of each slot's entries gathered once for the run's edge.
template <FieldDefinition F, typename Slot, typename ReadFor, typename Visit>
void finishRun(const Network &network, std::span<const Place> places,
               std::span<const Slot *const> slots, ReadFor &readFor,
               SampleRun<typename F::Entry> &run, SampleScratch<typename F::Entry> &scratch,
               Visit &visit) {
  using Entry = typename F::Entry;
  if (run.Indices.empty()) {
    return;
  }
  const NetworkEdge &edge = network.edges()[run.Positions.front().Edge];
  for (const Slot *slot : slots) {
    const auto read = readFor(*slot);
    if constexpr (HasEdgeRule<F>) {
      if (!gatherEdgeSample(network, *slot, edge, scratch, read)) {
        continue;
      }
      for (size_t k = 0; k < run.Indices.size(); ++k) {
        scratch.Edge.FromOffset = run.Positions[k].FromOffset;
        scratch.Edge.ToOffset = run.Positions[k].ToOffset;
        scratch.Made.clear();
        F::sampleEdge(scratch.Edge, scratch.Made);
        for (const Entry &value : scratch.Made) {
          visit(run.Indices[k], SampledEntry<Entry>{slot->Source, value});
        }
      }
    } else {
      for (size_t k = 0; k < run.Indices.size(); ++k) {
        for (const uint32_t i : positionsAt(*slot, places[run.Indices[k]])) {
          visit(run.Indices[k], SampledEntry<Entry>{slot->Source, read(i)});
        }
      }
    }
  }
  run.Indices.clear();
  run.Positions.clear();
}

// Hands visit(index, entry) each entry from the slots at each place, with the place's index: at a
// node what atNode appends for each slot, strictly inside an edge those at exactly the place or
// what the field's sampleEdge makes of the gathered entries, and none where the place does not
// resolve. Each place's entries arrive in the slots' order, but along one edge the places' entries
// interleave, one slot at a time. Consecutive places strictly inside one edge share
// each slot's gathered entries, so only the field's rule runs at each of them. readFor(slot)
// gives the function reading a position of that slot.
// This form reuses the lists it is given, so it allocates nothing once they have grown.
template <FieldDefinition F, typename Slot, typename ReadFor, typename AtNode, typename Visit>
void sampleSlotsAt(const Network &network, std::span<const Place> places,
                   std::span<const Slot *const> slots, ReadFor readFor, AtNode atNode, Visit visit,
                   SampleLists<typename F::Entry> &lists) {
  using Entry = typename F::Entry;
  SampleScratch<Entry> &scratch = lists.Scratch;
  SampleRun<Entry> &run = lists.Run;
  std::vector<SampledEntry<Entry>> &single = lists.Single;
  run.Indices.clear();
  run.Positions.clear();
  for (size_t index = 0; index < places.size(); ++index) {
    const std::optional<NetworkPosition> position = network.resolve(places[index]);
    if (position) {
      if (const auto *inside = std::get_if<EdgePosition>(&*position)) {
        if (!run.Indices.empty() && run.Positions.front().Edge != inside->Edge) {
          finishRun<F>(network, places, slots, readFor, run, scratch, visit);
        }
        run.Indices.push_back(index);
        run.Positions.push_back(*inside);
        continue;
      }
    }
    finishRun<F>(network, places, slots, readFor, run, scratch, visit);
    if (position) {
      single.clear();
      const uint32_t node = std::get<NodePosition>(*position).Node;
      for (const Slot *slot : slots) {
        atNode(*slot, node, scratch, single, readFor(*slot));
      }
      for (const SampledEntry<Entry> &entry : single) {
        visit(index, entry);
      }
    }
  }
  finishRun<F>(network, places, slots, readFor, run, scratch, visit);
}

template <FieldDefinition F, typename Slot, typename ReadFor, typename AtNode, typename Visit>
void sampleSlotsAt(const Network &network, std::span<const Place> places,
                   std::span<const Slot *const> slots, ReadFor readFor, AtNode atNode,
                   Visit visit) {
  SampleLists<typename F::Entry> lists;
  sampleSlotsAt<F>(network, places, slots, readFor, atNode, visit, lists);
}

// Fills slots, which it clears first, with each source's slot, in ascending key order: its readable
// stepped slot when it has one, and otherwise its resolved one.
template <FieldDefinition F>
void layeredSlots(const World &world, EntityKey holder,
                  std::vector<const FieldSlot<typename F::Entry> *> &slots) {
  using Slot = FieldSlot<typename F::Entry>;
  static const std::vector<Slot> NONE;
  const auto *resolved = world.tryGet<ResolvedEntries<F>>(holder);
  const auto *stepped = world.tryGet<SteppedEntries<F>>(holder);
  const std::vector<Slot> &fromResolved = resolved != nullptr ? resolved->Slots : NONE;
  const std::vector<Slot> &fromStepped = stepped != nullptr ? stepped->Readable : NONE;
  slots.clear();
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
}

// Each source's slot, in ascending key order, as the filling form gives them.
template <FieldDefinition F>
std::vector<const FieldSlot<typename F::Entry> *> layeredSlots(const World &world,
                                                               EntityKey holder) {
  std::vector<const FieldSlot<typename F::Entry> *> slots;
  layeredSlots<F>(world, holder, slots);
  return slots;
}

// The source's slot as the layer rule chooses it: its readable stepped slot when it has one, and
// otherwise its resolved one, or none. Allocates nothing.
template <FieldDefinition F>
const FieldSlot<typename F::Entry> *sourceSlot(const World &world, EntityKey source) {
  using Slot = FieldSlot<typename F::Entry>;
  const EntityKey holder = fieldKey(F::Name);
  if (!world.isLive(holder)) {
    return nullptr;
  }
  // Both lists keep their slots in ascending source order.
  const auto find = [source](const std::vector<Slot> &slots) -> const Slot * {
    const auto at = std::ranges::lower_bound(slots, source, {}, &Slot::Source);
    return at != slots.end() && at->Source == source ? &*at : nullptr;
  };
  if (const auto *stepped = world.tryGet<SteppedEntries<F>>(holder)) {
    if (const Slot *slot = find(stepped->Readable)) {
      return slot;
    }
  }
  const auto *resolved = world.tryGet<ResolvedEntries<F>>(holder);
  return resolved != nullptr ? find(resolved->Slots) : nullptr;
}

// The slots' entries at each of the places on the network, handed to visit(index, entry) as
// sampleSlotsAt gives them for field slots.
template <FieldDefinition F, typename Visit>
void sampleFieldSlotsAt(const Network &network, std::span<const Place> places,
                        std::span<const FieldSlot<typename F::Entry> *const> slots, Visit visit) {
  using Entry = typename F::Entry;
  using Slot = FieldSlot<Entry>;
  sampleSlotsAt<F>(
      network, places, slots,
      [](const Slot &slot) { return [&slot](uint32_t i) { return slot.Entries[i].Value; }; },
      [&network](const Slot &slot, uint32_t node, SampleScratch<Entry> &scratch,
                 std::vector<SampledEntry<Entry>> &sampled,
                 auto read) { sampleSlotAtNode(network, slot, node, scratch, sampled, read); },
      visit);
}

// The entries of the slots at the place on the network, in the slots' order, by the default rule
// or the field's sampleEdge: the batch of one place.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleSlots(const Network &network, const Place &place,
            const std::vector<const FieldSlot<typename F::Entry> *> &slots) {
  std::vector<SampledEntry<typename F::Entry>> sampled;
  sampleFieldSlotsAt<F>(network, std::span<const Place>(&place, 1), slots,
                        [&sampled](size_t /*index*/, const SampledEntry<typename F::Entry> &entry) {
                          sampled.push_back(entry);
                        });
  return sampled;
}

// A kept field's entries at the place on the network, as sampleField gives them. Defined in
// sim/medium/kept_field.h, which a kept field's definition includes.
template <KeptFieldDefinition F>
std::vector<SampledEntry<double>> sampleKeptField(const World &world, const Network &network,
                                                  const Place &place);

// A kept field's entries at each of the places on the network, handed to visit(index, entry) as
// sampleFieldAtPlaces gives them. Defined in sim/medium/kept_field.h.
template <KeptFieldDefinition F, typename Visit>
void sampleKeptFieldAt(const World &world, const Network &network, std::span<const Place> places,
                       Visit visit);

// The field's entries at the place on the network, each with its source, sources in ascending key
// order, by the default rule or the field's sampleEdge.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>> sampleField(const World &world, const Network &network,
                                                         const Place &place) {
  if constexpr (KeptFieldDefinition<F>) {
    return sampleKeptField<F>(world, network, place);
  } else {
    const EntityKey holder = fieldKey(F::Name);
    if (!world.isLive(holder)) {
      return {};
    }
    return sampleSlots<F>(network, place, layeredSlots<F>(world, holder));
  }
}

// The lists sampleField reuses, and the entries it gives. Keep one to sample the same field many
// times without allocating once its lists have grown.
template <typename Entry> struct FieldSample {
  std::vector<const FieldSlot<Entry> *> Slots;
  SampleLists<Entry> Lists;
  std::vector<SampledEntry<Entry>> Entries;
};

// Fills the sample's Entries, which it clears first, with what sampleField gives at the place.
// Allocates nothing once the sample's lists have grown.
template <FieldDefinition F>
void sampleField(const World &world, const Network &network, const Place &place,
                 FieldSample<typename F::Entry> &sample) {
  using Entry = typename F::Entry;
  using Slot = FieldSlot<Entry>;
  static_assert(!KeptFieldDefinition<F>, "a kept field has no filling form of sampleField");
  sample.Entries.clear();
  const EntityKey holder = fieldKey(F::Name);
  if (!world.isLive(holder)) {
    return;
  }
  layeredSlots<F>(world, holder, sample.Slots);
  sampleSlotsAt<F>(
      network, std::span<const Place>(&place, 1), std::span<const Slot *const>(sample.Slots),
      [](const Slot &slot) { return [&slot](uint32_t i) { return slot.Entries[i].Value; }; },
      [&network](const Slot &slot, uint32_t node, SampleScratch<Entry> &scratch,
                 std::vector<SampledEntry<Entry>> &sampled,
                 auto read) { sampleSlotAtNode(network, slot, node, scratch, sampled, read); },
      [&sample](size_t /*index*/, const SampledEntry<Entry> &entry) {
        sample.Entries.push_back(entry);
      },
      sample.Lists);
}

// The field's entries at each of the places on the network, exactly as sampleField gives them at
// each, handed to visit(index, entry), with entry a const SampledEntry<Entry> &. Each place's
// entries arrive in the order sampleField gives them, but along one edge the places' entries
// interleave, one source at a time, and a place with no entries gets no call. Consecutive places
// strictly inside one edge share each source's lookups of its entries at the edge's ends and along
// it. Keeps nothing after it returns.
template <FieldDefinition F, typename Visit>
void sampleFieldAtPlaces(const World &world, const Network &network, std::span<const Place> places,
                         Visit visit) {
  using Entry = typename F::Entry;
  if constexpr (KeptFieldDefinition<F>) {
    sampleKeptFieldAt<F>(world, network, places, visit);
  } else {
    const EntityKey holder = fieldKey(F::Name);
    const std::vector<const FieldSlot<Entry> *> slots =
        !world.isLive(holder) ? std::vector<const FieldSlot<Entry> *>{}
                              : layeredSlots<F>(world, holder);
    sampleFieldSlotsAt<F>(network, places, slots, visit);
  }
}

// As sampleFieldAtPlaces, keeping its working lists in the sample, for a field that isn't kept.
// Allocates nothing once they have grown.
template <FieldDefinition F, typename Visit>
void sampleFieldAtPlaces(const World &world, const Network &network, std::span<const Place> places,
                         Visit visit, FieldSample<typename F::Entry> &sample) {
  using Entry = typename F::Entry;
  using Slot = FieldSlot<Entry>;
  static_assert(!KeptFieldDefinition<F>, "a kept field samples with a KeptFieldSample");
  const EntityKey holder = fieldKey(F::Name);
  if (world.isLive(holder)) {
    layeredSlots<F>(world, holder, sample.Slots);
  } else {
    sample.Slots.clear();
  }
  sampleSlotsAt<F>(
      network, places, std::span<const Slot *const>(sample.Slots),
      [](const Slot &slot) { return [&slot](uint32_t i) { return slot.Entries[i].Value; }; },
      [&network](const Slot &slot, uint32_t node, SampleScratch<Entry> &scratch,
                 std::vector<SampledEntry<Entry>> &sampled,
                 auto read) { sampleSlotAtNode(network, slot, node, scratch, sampled, read); },
      visit, sample.Lists);
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

// Every source's resolved entries of the field, sources ascending, or none before the field's
// first resolution. Resolvers read with it, since stepped entries are state.
template <FieldDefinition F>
std::span<const FieldSlot<typename F::Entry>> resolvedSlots(const World &world) {
  const auto *resolved = world.tryGet<ResolvedEntries<F>>(fieldKey(F::Name));
  if (resolved == nullptr) {
    return {};
  }
  return resolved->Slots;
}

// The field's entries at the place, as sampleField gives them, but choosing each source's resolved
// entries whatever its stepped ones. Resolvers sample with it, since stepped entries are state.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleResolvedField(const World &world, const Network &network, const Place &place) {
  const auto *resolved = world.tryGet<ResolvedEntries<F>>(fieldKey(F::Name));
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
