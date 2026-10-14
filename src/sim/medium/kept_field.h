#ifndef TPJ_SIM_MEDIUM_KEPT_FIELD_H
#define TPJ_SIM_MEDIUM_KEPT_FIELD_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

// A kept field's entry: a value at a place, and the World::Tick at which the value became
// readable.
struct KeptEntry {
  Place At;
  double Value = 0.0;
  uint64_t Tick = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, KeptEntry &entry) {
  visitor.field("at", entry.At);
  visitor.field("value", entry.Value);
  visitor.field("tick", entry.Tick);
}

// One source's kept entries, in the order their places were first kept.
struct KeptSlot {
  EntityKey Source = NULL_KEY;
  std::vector<KeptEntry> Entries;
  // The positions of Entries ordered by place, made the first time it is needed and kept as the
  // swap adds entries. Derived from Entries' places, which nothing changes, and never visited, so
  // saves, hashes, and comparisons never see it.
  mutable std::vector<uint32_t> ByPlace;
  mutable bool Ordered = false;
};

template <typename Visitor> void visitFields(Visitor &visitor, KeptSlot &slot) {
  visitor.field("source", slot.Source);
  visitor.field("entries", slot.Entries);
}

// A change a system made during the tick, applied by the swap that ends it.
struct KeptChange {
  EntityKey Source = NULL_KEY;
  Place At;
  double Value = 0.0;
};

template <typename Visitor> void visitFields(Visitor &visitor, KeptChange &change) {
  visitor.field("source", change.Source);
  visitor.field("at", change.At);
  visitor.field("value", change.Value);
}

// A kept field's entries. State. Slots holds the readable entries, sources in ascending key order,
// and Pending the changes made during the current tick, in the order made.
template <KeptFieldDefinition F> struct KeptEntries {
  std::vector<KeptSlot> Slots;
  std::vector<KeptChange> Pending;
};

template <typename Visitor, typename F> void visitFields(Visitor &visitor, KeptEntries<F> &kept) {
  visitor.field("slots", kept.Slots);
  visitor.field("pending", kept.Pending);
}

// One end, at a sampled node, of an edge meeting it, with the source's entries strictly inside the
// edge.
struct NodeEnd {
  NetworkEdge Edge;
  // Whether the node is the edge's From end; false for its To end.
  bool AtFrom = true;
  std::vector<EdgeEntry<double>> Along;
};

// What a kept field's node rule is given for one source at a node.
struct NodeSample {
  uint32_t Node = 0;
  std::vector<double> AtNode;
  std::vector<NodeEnd> Ends;
};

// A kept field whose owner reads nodes by its own rule, appending its values to out:
//   static void sampleNode(const NodeSample &sample, std::vector<double> &out);
template <typename F>
concept HasNodeRule =
    KeptFieldDefinition<F> && requires(const NodeSample &sample, std::vector<double> &out) {
      { F::sampleNode(sample, out) } -> std::same_as<void>;
    };

// The lists sampling a kept field reuses: the slots, the sample's working lists, the node rule's
// sample, the node ends that sample isn't using, the node rule's values, and the entries
// sampleField gives. Keep one to sample a kept field many times without allocating once its lists
// have grown.
struct KeptFieldSample {
  std::vector<const KeptSlot *> Slots;
  SampleLists<double> Lists;
  NodeSample Node;
  // Ends taken out of Node, their lists kept for the next node's ends.
  std::vector<NodeEnd> SpareEnds;
  std::vector<double> Values;
  std::vector<SampledEntry<double>> Entries;
};

// The field's kept entries, or null when the world holds none.
template <KeptFieldDefinition F> const KeptEntries<F> *findKeptEntries(const World &world) {
  return world.tryGet<KeptEntries<F>>(fieldKey(F::Name));
}

template <KeptFieldDefinition F> KeptEntries<F> *findKeptEntries(World &world) {
  return world.tryGet<KeptEntries<F>>(fieldKey(F::Name));
}

// The source's slot among slots in ascending source order, or null.
inline const KeptSlot *findKeptSlot(const std::vector<KeptSlot> &slots, EntityKey source) {
  const auto at = std::ranges::lower_bound(slots, source, {}, &KeptSlot::Source);
  return at != slots.end() && at->Source == source ? &*at : nullptr;
}

// The entry's value read at the tick by the field's rule, with 0 ticks when the entry's tick is
// later.
template <KeptFieldDefinition F> double readKeptEntry(const KeptEntry &entry, uint64_t tick) {
  return F::readKept(entry.Value, tick > entry.Tick ? tick - entry.Tick : 0);
}

// The field's resolver: creates the field's entity holding empty kept entries when it holds none.
template <KeptFieldDefinition F> void emptyKeptEntries(World &world, TickBuffers & /*buffers*/) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  static_cast<void>(world.getOrEmplace<KeptEntries<F>>(key));
}

// The field's swap: applies the tick's changes in the order made, then empties them.
template <KeptFieldDefinition F> void applyKeptChanges(World &world, TickBuffers & /*buffers*/) {
  KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    return;
  }
  for (const KeptChange &change : kept->Pending) {
    auto at = std::ranges::lower_bound(kept->Slots, change.Source, {}, &KeptSlot::Source);
    if (at == kept->Slots.end() || at->Source != change.Source) {
      KeptSlot added;
      added.Source = change.Source;
      at = kept->Slots.insert(at, std::move(added));
    }
    KeptSlot &slot = *at;
    const std::span<const uint32_t> found = positionsAt(slot, change.At);
    if (!found.empty()) {
      KeptEntry &entry = slot.Entries[found.front()];
      entry.Value = change.Value;
      entry.Tick = world.Tick;
      continue;
    }
    const auto position = static_cast<uint32_t>(slot.Entries.size());
    slot.Entries.push_back({.At = change.At, .Value = change.Value, .Tick = world.Tick});
    // An order made is kept; one not yet made is made from the entries when next needed.
    if (slot.Ordered) {
      std::vector<uint32_t> &order = slot.ByPlace;
      const auto placeOf = [&slot](uint32_t i) -> const Place & { return slot.Entries[i].At; };
      order.insert(std::ranges::upper_bound(order, change.At, placeBefore, placeOf), position);
    }
  }
  kept->Pending.clear();
}

// Registers the kept field's state component, <name>-kept, its resolver, <name>-field, and its
// swap. Throws std::invalid_argument when a name is malformed or already registered.
template <KeptFieldDefinition F> void addKeptField(WorldSchema &schema) {
  const std::string name(F::Name);
  schema.addComponent<KeptEntries<F>>(name + "-kept", DataKind::State);
  schema.addResolver(name + "-field", &emptyKeptEntries<F>);
  schema.addSwap(&applyKeptChanges<F>);
}

// Sets the source's entry at the place to the value when the swap ending this tick runs. Systems
// only. Throws std::logic_error when the world is not stepping or holds no kept entries for the
// field, and std::invalid_argument for the null key or a NaN distance.
template <KeptFieldDefinition F>
void keepEntry(World &world, EntityKey source, const Place &place, double value) {
  // Messages are built only when throwing, since systems call this for each mover every tick.
  if (!world.isStepping()) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " takes kept changes only from systems");
  }
  KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " holds no kept entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entries from the null key");
  }
  if (std::isnan(place.Distance)) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entry at a NaN distance");
  }
  kept->Pending.push_back({.Source = source, .At = place, .Value = value});
}

// The read value of the source's first entry at exactly the place, or none, and none at a NaN
// distance.
template <KeptFieldDefinition F>
std::optional<double> keptValue(const World &world, EntityKey source, const Place &place) {
  // A NaN distance is at no place, and would match a whole carrier's run in the order.
  if (std::isnan(place.Distance)) {
    return std::nullopt;
  }
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  const KeptSlot *slot = kept == nullptr ? nullptr : findKeptSlot(kept->Slots, source);
  if (slot == nullptr) {
    return std::nullopt;
  }
  // Positions at a place ascend, so the first is the source's first entry there.
  const std::span<const uint32_t> found = positionsAt(*slot, place);
  if (found.empty()) {
    return std::nullopt;
  }
  return readKeptEntry<F>(slot->Entries[found.front()], world.Tick);
}

// The source's kept entries as held, unread, in its order, or an empty list.
template <KeptFieldDefinition F>
std::span<const KeptEntry> keptEntries(const World &world, EntityKey source) {
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  const KeptSlot *slot = kept == nullptr ? nullptr : findKeptSlot(kept->Slots, source);
  return slot == nullptr ? std::span<const KeptEntry>() : std::span<const KeptEntry>(slot->Entries);
}

// Makes the source's kept entries the given ones, in the given order; an empty list removes the
// source. Finishers only. Throws std::logic_error when the finishers are not running or the world
// holds no kept entries for the field, and std::invalid_argument for the null key, an entry whose
// distance is NaN or whose tick is later than the world's, or two entries at one place.
template <KeptFieldDefinition F>
void replaceKeptEntries(World &world, EntityKey source, std::vector<KeptEntry> entries) {
  const std::string name(F::Name);
  if (!world.isFinishing()) {
    throw std::logic_error("field " + name + " takes kept replacements only from finishers");
  }
  KeptEntries<F> *kept = findKeptEntries<F>(world);
  if (kept == nullptr) {
    throw std::logic_error("field " + name + " holds no kept entries; resolve the world first");
  }
  if (source == NULL_KEY) {
    throw std::invalid_argument("field " + name + " takes no entries from the null key");
  }
  for (const KeptEntry &entry : entries) {
    if (std::isnan(entry.At.Distance)) {
      throw std::invalid_argument("field " + name + " takes no entry at a NaN distance");
    }
    if (entry.Tick > world.Tick) {
      throw std::invalid_argument("field " + name + " takes no entry stamped after tick " +
                                  std::to_string(world.Tick));
    }
  }
  std::vector<uint32_t> order;
  orderItemsByPlace(std::span<const KeptEntry>(entries), order);
  // The order ascends, so two entries at one place are neighbours in it.
  for (size_t i = 1; i < order.size(); ++i) {
    if (!placeBefore(entries[order[i - 1]].At, entries[order[i]].At)) {
      throw std::invalid_argument("field " + name + " takes one entry at a place");
    }
  }
  auto at = std::ranges::lower_bound(kept->Slots, source, {}, &KeptSlot::Source);
  const bool held = at != kept->Slots.end() && at->Source == source;
  if (entries.empty()) {
    if (held) {
      kept->Slots.erase(at);
    }
    return;
  }
  if (!held) {
    KeptSlot added;
    added.Source = source;
    at = kept->Slots.insert(at, std::move(added));
  }
  at->Entries = std::move(entries);
  at->ByPlace = std::move(order);
  at->Ordered = true;
}

// Appends what the field's node rule gives for the source at the node, when the source has an
// entry at the node's stop places or strictly inside an edge meeting it, each value read by read.
// Fills the kept sample's node sample, reusing the ends it holds and its spare ends.
template <KeptFieldDefinition F, typename Read>
  requires HasNodeRule<F>
void sampleKeptSlotAtNode(const Network &network, const KeptSlot &slot, uint32_t node,
                          SampleScratch<double> &scratch, KeptFieldSample &kept,
                          std::vector<SampledEntry<double>> &sampled, Read read) {
  NodeSample &sample = kept.Node;
  sample.Node = node;
  sample.AtNode.clear();
  // Moving an end keeps its list's memory for a later end.
  while (!sample.Ends.empty()) {
    kept.SpareEnds.push_back(std::move(sample.Ends.back()));
    sample.Ends.pop_back();
  }
  positionsAtAny(slot, network.stopPlaces(node), scratch.Positions);
  for (const uint32_t i : scratch.Positions) {
    sample.AtNode.push_back(read(i));
  }
  bool any = !sample.AtNode.empty();
  for (const EdgeEnd &end : network.edgeEnds(node)) {
    const NetworkEdge &edge = network.edges()[end.Edge];
    if (kept.SpareEnds.empty()) {
      sample.Ends.emplace_back();
    } else {
      sample.Ends.push_back(std::move(kept.SpareEnds.back()));
      kept.SpareEnds.pop_back();
    }
    NodeEnd &nodeEnd = sample.Ends.back();
    nodeEnd.Edge = edge;
    nodeEnd.AtFrom = end.AtFrom;
    nodeEnd.Along.clear();
    std::vector<uint32_t> &inside = scratch.Inside;
    inside.clear();
    positionsInside(slot, edge.Carrier, edge.FromDistance, edge.ToDistance, inside);
    std::ranges::sort(inside);
    for (const uint32_t i : inside) {
      const Place &at = slot.Entries[i].At;
      nodeEnd.Along.push_back(
          {at.Distance - edge.FromDistance, edge.ToDistance - at.Distance, read(i)});
    }
    any = any || !nodeEnd.Along.empty();
  }
  if (!any) {
    return;
  }
  kept.Values.clear();
  F::sampleNode(sample, kept.Values);
  for (const double value : kept.Values) {
    sampled.push_back({slot.Source, value});
  }
}

// As sampleKeptFieldAt, keeping its working lists in the sample. Allocates nothing once they have
// grown.
template <KeptFieldDefinition F, typename Visit>
void sampleKeptFieldAt(const World &world, const Network &network, std::span<const Place> places,
                       Visit visit, KeptFieldSample &sample) {
  const KeptEntries<F> *kept = findKeptEntries<F>(world);
  sample.Slots.clear();
  if (kept != nullptr) {
    for (const KeptSlot &slot : kept->Slots) {
      sample.Slots.push_back(&slot);
    }
  }
  sampleSlotsAt<F>(
      network, places, std::span<const KeptSlot *const>(sample.Slots),
      [&world](const KeptSlot &slot) {
        return
            [&slot, &world](uint32_t i) { return readKeptEntry<F>(slot.Entries[i], world.Tick); };
      },
      [&network, &sample](const KeptSlot &slot, uint32_t node, SampleScratch<double> &scratch,
                          std::vector<SampledEntry<double>> &sampled, auto read) {
        if constexpr (HasNodeRule<F>) {
          sampleKeptSlotAtNode<F>(network, slot, node, scratch, sample, sampled, read);
        } else {
          sampleSlotAtNode(network, slot, node, scratch, sampled, read);
        }
      },
      visit, sample.Lists);
}

template <KeptFieldDefinition F, typename Visit>
void sampleKeptFieldAt(const World &world, const Network &network, std::span<const Place> places,
                       Visit visit) {
  KeptFieldSample sample;
  sampleKeptFieldAt<F>(world, network, places, visit, sample);
}

// Fills the sample's Entries, which it clears first, with what sampleField gives for the kept
// field at the place. Allocates nothing once the sample's lists have grown.
template <KeptFieldDefinition F>
void sampleField(const World &world, const Network &network, const Place &place,
                 KeptFieldSample &sample) {
  sample.Entries.clear();
  sampleKeptFieldAt<F>(
      world, network, std::span<const Place>(&place, 1),
      [&sample](size_t /*index*/, const SampledEntry<double> &entry) {
        sample.Entries.push_back(entry);
      },
      sample);
}

// The kept field's value at the place, as fieldValue gives it, using the sample's lists.
// Allocates nothing once they have grown.
template <KeptFieldDefinition F>
double fieldValue(const World &world, const Network &network, const Place &place,
                  KeptFieldSample &sample) {
  sampleField<F>(world, network, place, sample);
  double value = 0.0;
  for (const SampledEntry<double> &entry : sample.Entries) {
    value += entry.Value;
  }
  return value;
}

template <KeptFieldDefinition F>
std::vector<SampledEntry<double>> sampleKeptField(const World &world, const Network &network,
                                                  const Place &place) {
  KeptFieldSample sample;
  sampleField<F>(world, network, place, sample);
  return std::move(sample.Entries);
}

} // namespace tpj

#endif
