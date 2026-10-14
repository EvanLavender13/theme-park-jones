#ifndef TPJ_SIM_MEDIUM_GROUND_FIELD_H
#define TPJ_SIM_MEDIUM_GROUND_FIELD_H

#include "sim/entity_key.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpj {

// A ground field's definition, a type its owning module supplies:
//   struct GuestPresence {
//     using Entry = Stride;
//     static constexpr std::string_view Name = "guest-presence";
//   };
template <typename F>
concept GroundFieldDefinition = requires {
  typename F::Entry;
  { F::Name } -> std::convertible_to<std::string_view>;
};

// The side of the square cells a ground field's cell order groups entries by, in meters.
constexpr double GROUND_CELL_SIZE = 4.0;

// An entity's entry in a ground field: where it is, its value, and the World::Tick of its last
// change. Dropped marks a drop in the tick's working list, and is false in readable entries.
template <typename Entry> struct GroundEntry {
  EntityKey Key = NULL_KEY;
  GroundPoint At;
  Entry Value{};
  uint64_t Changed = 0;
  bool Dropped = false;
};

template <typename Visitor, typename Entry>
void visitFields(Visitor &visitor, GroundEntry<Entry> &entry) {
  visitor.field("key", entry.Key);
  visitor.field("x", entry.At.X);
  visitor.field("z", entry.At.Z);
  visitor.field("value", entry.Value);
  visitor.field("changed", entry.Changed);
  visitor.field("dropped", entry.Dropped);
}

// One readable entry's packed cell and its position among the readable entries.
struct GroundCell {
  uint64_t Cell = 0;
  uint32_t Position = 0;
};

// A ground field's entries. State. Readable holds the readable entries in ascending key order.
// From the tick's first change to the swap, Changing is set and Next holds the entries the swap
// will make readable, in ascending key order, with drops marked.
template <GroundFieldDefinition F> struct GroundEntries {
  std::vector<GroundEntry<typename F::Entry>> Readable;
  std::vector<GroundEntry<typename F::Entry>> Next;
  bool Changing = false;
  // The readable entries ordered by cell, then position, made on the first sample after each swap
  // and reusing its storage. Derived from Readable and never visited, so saves, hashes, and
  // comparisons never see it.
  mutable std::vector<GroundCell> ByCell;
  mutable bool CellsCurrent = false;
};

template <typename Visitor, typename F>
void visitFields(Visitor &visitor, GroundEntries<F> &entries) {
  visitor.field("readable", entries.Readable);
  visitor.field("next", entries.Next);
  visitor.field("changing", entries.Changing);
}

// Makes mine equal to other, keeping its lists' memory, as copying a world refills it.
template <GroundFieldDefinition F>
void keepAssign(GroundEntries<F> &mine, const GroundEntries<F> &other) {
  using Entry = GroundEntry<typename F::Entry>;
  copyGrowing(mine.Readable, std::span<const Entry>(other.Readable));
  copyGrowing(mine.Next, std::span<const Entry>(other.Next));
  mine.Changing = other.Changing;
  copyGrowing(mine.ByCell, std::span<const GroundCell>(other.ByCell));
  mine.CellsCurrent = other.CellsCurrent;
}

// An entry a sample found: its entity's key, its position, and its value.
template <typename Entry> struct GroundSample {
  EntityKey Key = NULL_KEY;
  GroundPoint At;
  Entry Value{};
};

// The field's entries, or null when the world holds none.
template <GroundFieldDefinition F> const GroundEntries<F> *findGroundEntries(const World &world) {
  return world.tryGet<GroundEntries<F>>(fieldKey(F::Name));
}

template <GroundFieldDefinition F> GroundEntries<F> *findGroundEntries(World &world) {
  return world.tryGet<GroundEntries<F>>(fieldKey(F::Name));
}

// The field's resolver: creates the field's entity holding empty entries when it holds none, and
// leaves entries it holds as they are.
template <GroundFieldDefinition F>
void emptyGroundEntries(World &world, TickBuffers & /*buffers*/) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FIELD_PURPOSE, hashName(F::Name));
  static_cast<void>(world.getOrEmplace<GroundEntries<F>>(key));
}

// The field's swap: makes the tick's changes readable, and removes dropped entries and those of
// entities that are gone.
template <GroundFieldDefinition F> void swapGroundEntries(World &world, TickBuffers & /*buffers*/) {
  GroundEntries<F> *entries = findGroundEntries<F>(world);
  if (entries == nullptr) {
    return;
  }
  if (entries->Changing) {
    entries->Readable.swap(entries->Next);
    entries->Next.clear();
    entries->Changing = false;
  }
  std::erase_if(entries->Readable, [&world](const GroundEntry<typename F::Entry> &entry) {
    return entry.Dropped || !world.isLive(entry.Key);
  });
  entries->CellsCurrent = false;
}

// Registers the field's state component, <name>-ground, its resolver, <name>-field, and its swap.
// Every kind of field registers a resolver named <name>-field, so a name another field has is
// refused. Throws std::invalid_argument when a name is malformed or already registered.
template <GroundFieldDefinition F> void addGroundField(WorldSchema &schema) {
  const std::string name(F::Name);
  schema.addComponent<GroundEntries<F>>(name + "-ground", DataKind::State);
  schema.addResolver(name + "-field", &emptyGroundEntries<F>);
  schema.addSwap(&swapGroundEntries<F>);
}

// Checks a change to the entity's entry, starts the tick's working list on its first change, and
// gives the position of the entity's entry in it, or where it would go.
template <GroundFieldDefinition F>
size_t groundChangeAt(World &world, GroundEntries<F> *entries, EntityKey entity) {
  // Messages are built only when throwing, since systems call this for each mover every tick.
  if (!world.isStepping()) {
    throw std::logic_error("field " + std::string(F::Name) + " takes changes only from systems");
  }
  if (entries == nullptr) {
    throw std::logic_error("field " + std::string(F::Name) +
                           " holds no ground entries; resolve the world first");
  }
  if (entity == NULL_KEY) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entry for the null key");
  }
  if (!entries->Changing) {
    copyGrowing(entries->Next, std::span<const GroundEntry<typename F::Entry>>(entries->Readable));
    entries->Changing = true;
  }
  const auto at =
      std::ranges::lower_bound(entries->Next, entity, {}, &GroundEntry<typename F::Entry>::Key);
  if (at != entries->Next.end() && at->Key == entity && at->Changed == world.Tick) {
    throw std::invalid_argument("entity " + std::to_string(static_cast<uint64_t>(entity)) +
                                " has already changed its entry in field " + std::string(F::Name) +
                                " this tick");
  }
  return static_cast<size_t>(at - entries->Next.begin());
}

// Gives the entity's entry the ground position and value when the swap ending this tick runs.
// Systems only. Throws std::logic_error when the world is not stepping or holds no entries for the
// field, and std::invalid_argument for the null key, a position that isn't finite, or an entity
// whose entry already changed this tick.
template <GroundFieldDefinition F>
void setGroundEntry(World &world, EntityKey entity, GroundPoint at, typename F::Entry value) {
  if (!std::isfinite(at.X) || !std::isfinite(at.Z)) {
    throw std::invalid_argument("field " + std::string(F::Name) +
                                " takes no entry at a position that isn't finite");
  }
  GroundEntries<F> *entries = findGroundEntries<F>(world);
  const size_t position = groundChangeAt<F>(world, entries, entity);
  std::vector<GroundEntry<typename F::Entry>> &next = entries->Next;
  if (position < next.size() && next[position].Key == entity) {
    GroundEntry<typename F::Entry> &entry = next[position];
    entry.At = at;
    entry.Value = std::move(value);
    entry.Changed = world.Tick;
    entry.Dropped = false;
    return;
  }
  next.insert(next.begin() + static_cast<ptrdiff_t>(position),
              GroundEntry<typename F::Entry>{.Key = entity,
                                             .At = at,
                                             .Value = std::move(value),
                                             .Changed = world.Tick,
                                             .Dropped = false});
}

// Removes the entity's entry when the swap ending this tick runs. A drop counts as the entity's
// change for the tick even when it has no entry. Systems only. Throws as setGroundEntry does.
template <GroundFieldDefinition F> void dropGroundEntry(World &world, EntityKey entity) {
  GroundEntries<F> *entries = findGroundEntries<F>(world);
  const size_t position = groundChangeAt<F>(world, entries, entity);
  std::vector<GroundEntry<typename F::Entry>> &next = entries->Next;
  if (position < next.size() && next[position].Key == entity) {
    next[position].Changed = world.Tick;
    next[position].Dropped = true;
    return;
  }
  next.insert(next.begin() + static_cast<ptrdiff_t>(position),
              GroundEntry<typename F::Entry>{
                  .Key = entity, .At = {}, .Value = {}, .Changed = world.Tick, .Dropped = true});
}

// The index of the cell holding the coordinate, clamped to the range of int32_t.
inline int64_t groundCellIndex(double coordinate) {
  constexpr double LOWEST = -2147483648.0;
  constexpr double HIGHEST = 2147483647.0;
  return static_cast<int64_t>(
      std::clamp(std::floor(coordinate / GROUND_CELL_SIZE), LOWEST, HIGHEST));
}

// The cell's key, ordering cells by x, then z, so one x's cells from one z to another are
// consecutive.
inline uint64_t packGroundCell(int64_t x, int64_t z) {
  constexpr int64_t BIAS = int64_t{1} << 31;
  return (static_cast<uint64_t>(x + BIAS) << 32U) | static_cast<uint64_t>(z + BIAS);
}

// Fills sampled with the entries whose position is within the radius of the point, the radius
// included, in ascending key order, leaving out entities that are gone. Gives nothing for a
// negative or NaN radius or a point that isn't finite. Allocates only while sampled or the field's
// cell order grows.
template <GroundFieldDefinition F>
void sampleGroundField(const World &world, GroundPoint point, double radius,
                       std::vector<GroundSample<typename F::Entry>> &sampled) {
  using Entry = typename F::Entry;
  sampled.clear();
  const GroundEntries<F> *entries = findGroundEntries<F>(world);
  if (!(radius >= 0.0) || !std::isfinite(point.X) || !std::isfinite(point.Z) ||
      entries == nullptr) {
    return;
  }
  const std::vector<GroundEntry<Entry>> &readable = entries->Readable;
  const auto take = [&](const GroundEntry<Entry> &entry) {
    const double dx = entry.At.X - point.X;
    const double dz = entry.At.Z - point.Z;
    if (dx * dx + dz * dz <= radius * radius && world.isLive(entry.Key)) {
      sampled.push_back({entry.Key, entry.At, entry.Value});
    }
  };
  const int64_t xFrom = groundCellIndex(point.X - radius);
  const int64_t xTo = groundCellIndex(point.X + radius);
  const int64_t zFrom = groundCellIndex(point.Z - radius);
  const int64_t zTo = groundCellIndex(point.Z + radius);
  const double cells = static_cast<double>(xTo - xFrom + 1) * static_cast<double>(zTo - zFrom + 1);
  // A box of more cells than entries costs more to search than a scan, which is in key order.
  if (cells > static_cast<double>(readable.size())) {
    for (const GroundEntry<Entry> &entry : readable) {
      take(entry);
    }
    return;
  }
  std::vector<GroundCell> &byCell = entries->ByCell;
  if (!entries->CellsCurrent) {
    byCell.clear();
    for (uint32_t i = 0; i < readable.size(); ++i) {
      byCell.push_back(
          {packGroundCell(groundCellIndex(readable[i].At.X), groundCellIndex(readable[i].At.Z)),
           i});
    }
    std::ranges::sort(byCell, [](const GroundCell &a, const GroundCell &b) {
      return a.Cell != b.Cell ? a.Cell < b.Cell : a.Position < b.Position;
    });
    entries->CellsCurrent = true;
  }
  for (int64_t x = xFrom; x <= xTo; ++x) {
    const auto first =
        std::ranges::lower_bound(byCell, packGroundCell(x, zFrom), {}, &GroundCell::Cell);
    const auto last =
        std::ranges::upper_bound(byCell, packGroundCell(x, zTo), {}, &GroundCell::Cell);
    for (auto at = first; at < last; ++at) {
      take(readable[at->Position]);
    }
  }
  std::ranges::sort(sampled, {}, &GroundSample<Entry>::Key);
}

} // namespace tpj

#endif
