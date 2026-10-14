#ifndef TPJ_SIM_UNITS_H
#define TPJ_SIM_UNITS_H

#include "sim/entity_key.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/type_id.h"
#include "sim/unit_space.h"
#include "sim/world.h"

#include <algorithm>
#include <concepts>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <stdint.h>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace tpj {

// What a unit's derivation reads. Each read is recorded, by table and key.
class UnitReads {
public:
  // Made by resolution for each derivation of a unit of the table, recording into reads.
  UnitReads(const World &world, const UnitSpace &space, TickBuffers &scratch, size_t table,
            std::vector<UnitRead> &reads)
      : Read(&world), Space(&space), Scratch(&scratch), Table(table), Recorded(&reads) {}

  // The entity's T, or null. T must be one of the kind's Reads.
  template <typename T> const T *component(EntityKey key);
  // The unit of an earlier kind for the key, or null when there is none.
  template <typename K> const typename K::Value *unit(EntityKey key);
  // The keys the index files under the key, ascending.
  template <typename I> std::span<const EntityKey> filed(EntityKey key);
  // Lists the derivation reuses from one unit to the next, never read by resolution. Clear a list
  // before reading it.
  TickBuffers &scratch() { return *Scratch; }

private:
  // The position of an earlier unit kind or index, named by its type, in the schema's unit tables.
  size_t earlierTable(TypeId id) const;

  const World *Read;
  const UnitSpace *Space;
  TickBuffers *Scratch;
  size_t Table;
  std::vector<UnitRead> *Recorded;
};

template <typename T> struct IsTuple : std::false_type {};
template <typename... Types> struct IsTuple<std::tuple<Types...>> : std::true_type {};

// A unit kind: a Name, an intent component Over that each unit's entity holds, a Value with a
// visitFields function, compared by the walk's words, Reads, a std::tuple of the intent types its
// derivation reads, and derive(UnitReads &, EntityKey, Value &). The value derive is given may hold
// another unit's value, so derive sets every field.
template <typename K>
concept UnitKind = std::convertible_to<decltype(K::Name), std::string_view> &&
                   std::is_default_constructible_v<typename K::Value> &&
                   HasVisitFields<typename K::Value> && IsTuple<typename K::Reads>::value &&
                   requires(UnitReads &reads, EntityKey key, typename K::Value &value,
                            typename K::Over *over) { K::derive(reads, key, value); };

// An index: a Name, an intent component Over, and keysOf(const Over &, std::vector<EntityKey> &),
// which clears the list and puts in the keys to file the entity under.
template <typename I>
concept UnitIndex =
    std::convertible_to<decltype(I::Name), std::string_view> &&
    requires(const typename I::Over &over, std::vector<EntityKey> &keys) { I::keysOf(over, keys); };

template <UnitIndex I> void addUnitIndex(WorldSchema &schema);
template <UnitKind K> void addUnits(WorldSchema &schema);
template <UnitKind K> const typename K::Value *unitValue(const World &world, EntityKey key);
template <UnitKind K> bool unitsCurrent(const World &world);

// The lists an index's update reuses: the keys to file again, and the keys one entity is filed
// under now.
struct IndexUpdateLists {
  std::vector<EntityKey> Dirty;
  std::vector<EntityKey> Keys;
};

// Collects a value's words, as the walk emits them, in a list it keeps.
class KeptWords final : public WordSink {
public:
  void word(uint64_t value) override { Words.push_back(value); }
  void real(std::string_view, double) override {}

  std::vector<uint64_t> Words;
};

// The lists a kind's update reuses: the keys to derive again, the value a derivation fills, what
// it read, and the words two values are compared by.
template <typename Value> struct KindUpdateLists {
  std::vector<EntityKey> Dirty;
  Value Derived;
  std::vector<UnitRead> Reads;
  KeptWords Left;
  KeptWords Right;
};

// True when both values emit the same words, as the walk compares them. The lists are kept.
template <typename Value>
bool sameWords(const Value &left, const Value &right, KeptWords &leftWords, KeptWords &rightWords) {
  leftWords.Words.clear();
  rightWords.Words.clear();
  // emitField takes a mutable reference so that one function can also decode. Emitting only reads.
  emitField(leftWords, "", const_cast<Value &>(left));
  emitField(rightWords, "", const_cast<Value &>(right));
  return leftWords.Words == rightWords.Words;
}

// Sorts the keys and leaves out repeats.
void sortUnique(std::vector<EntityKey> &keys);
// Sorts the reads and leaves out repeats.
void sortUnique(std::vector<UnitRead> &reads);
// Removes the key from the ascending list, if it holds it.
void eraseSorted(std::vector<EntityKey> &keys, EntityKey key);
// Adds the key to the ascending list, if it doesn't hold it.
void insertSorted(std::vector<EntityKey> &keys, EntityKey key);

// The position of a registered unit kind or index in the schema's unit tables. Throws
// std::logic_error when T isn't registered.
template <typename T> size_t tablePosition(const WorldSchema &schema) {
  const std::optional<size_t> position = schema.unitTableOf(typeIdOf<T>());
  if (!position) {
    throw std::logic_error("a unit kind or index isn't registered with the world's schema");
  }
  return *position;
}

// Files again each entity whose Over a command touched, or every entity before the first update,
// and puts each entry that changed on the change list.
template <UnitIndex I> void updateIndex(World &world, TickBuffers &buffers) {
  using Over = typename I::Over;
  const World &read = world;
  UnitSpace &space = unitSpaceOf(world);
  const WorldSchema &schema = read.schema();
  const size_t position = tablePosition<I>(schema);
  const size_t table = space.StoreCount + position;
  auto &index = static_cast<IndexTable &>(*space.Tables[position]);
  auto &lists = buffers.get<IndexUpdateLists>();
  if (index.Counter) {
    const size_t over = schema.unitTables()[position].Over;
    copyGrowing(lists.Dirty, std::span<const EntityKey>(space.Logs[over]));
    sortUnique(lists.Dirty);
  } else {
    read.keysWith<Over>(lists.Dirty);
  }
  for (const EntityKey key : lists.Dirty) {
    lists.Keys.clear();
    if (const Over *component = read.tryGet<Over>(key)) {
      I::keysOf(*component, lists.Keys);
      sortUnique(lists.Keys);
    }
    std::vector<EntityKey> *filed = index.Filed.find(key);
    if (filed != nullptr && *filed == lists.Keys) {
      continue;
    }
    if (filed != nullptr) {
      for (const EntityKey old : *filed) {
        if (std::ranges::binary_search(lists.Keys, old)) {
          continue;
        }
        std::vector<EntityKey> &entry = *index.Entries.find(old);
        eraseSorted(entry, key);
        if (entry.empty()) {
          index.Entries.erase(old);
        }
        space.Changes.push_back(UnitRead{table, old});
      }
    }
    for (const EntityKey now : lists.Keys) {
      if (filed != nullptr && std::ranges::binary_search(*filed, now)) {
        continue;
      }
      auto [entry, added] = index.Entries.getOrAdd(now);
      if (added) {
        entry.clear();
      }
      insertSorted(entry, key);
      space.Changes.push_back(UnitRead{table, now});
    }
    if (lists.Keys.empty()) {
      index.Filed.erase(key);
    } else {
      std::vector<EntityKey> &kept = index.Filed.getOrAdd(key).first;
      copyGrowing(kept, std::span<const EntityKey>(lists.Keys));
    }
  }
  index.Counter = space.Counter;
}

// Derives again each unit of K whose entity a command touched, or whose reads this resolution
// changed, or every unit before the first update, in ascending key order. Puts each unit that was
// added, removed, or derived a different value on the change list.
template <UnitKind K> void updateUnits(World &world, TickBuffers &buffers) {
  using Over = typename K::Over;
  using Value = typename K::Value;
  const World &read = world;
  UnitSpace &space = unitSpaceOf(world);
  const WorldSchema &schema = read.schema();
  const size_t position = tablePosition<K>(schema);
  const UnitRead table{space.StoreCount + position, NULL_KEY};
  auto &units = static_cast<KindTable<Value> &>(*space.Tables[position]);
  auto &lists = buffers.get<KindUpdateLists<Value>>();
  if (units.Counter) {
    const size_t over = schema.unitTables()[position].Over;
    copyGrowing(lists.Dirty, std::span<const EntityKey>(space.Logs[over]));
    for (const UnitRead &change : space.Changes) {
      if (const std::vector<UnitRead> *readers = space.Readers[change.Table].find(change.Key)) {
        for (const UnitRead &reader : *readers) {
          if (reader.Table == table.Table) {
            lists.Dirty.push_back(reader.Key);
          }
        }
      }
    }
    sortUnique(lists.Dirty);
  } else {
    read.keysWith<Over>(lists.Dirty);
  }
  for (const EntityKey key : lists.Dirty) {
    const UnitRead unit{table.Table, key};
    if (!read.has<Over>(key)) {
      if (const KindUnit<Value> *gone = units.Units.find(key)) {
        space.removeReader(unit, gone->Reads);
        units.Units.erase(key);
        space.Changes.push_back(unit);
      }
      continue;
    }
    lists.Reads.clear();
    UnitReads reads(read, space, buffers, position, lists.Reads);
    K::derive(reads, key, lists.Derived);
    sortUnique(lists.Reads);
    auto [held, added] = units.Units.getOrAdd(key);
    if (added || !sameWords(held.Held, lists.Derived, lists.Left, lists.Right)) {
      std::swap(held.Held, lists.Derived);
      space.Changes.push_back(unit);
    }
    if (added || held.Reads != lists.Reads) {
      if (!added) {
        space.removeReader(unit, held.Reads);
      }
      space.addReader(unit, lists.Reads);
      copyGrowing(held.Reads, std::span<const UnitRead>(lists.Reads));
    }
  }
  units.Counter = space.Counter;
}

// The schema's entry for a unit kind or index, with the intent types it reads.
template <typename T, typename... Reads>
UnitTableType unitTableType(std::unique_ptr<UnitTable> (*makeTable)()) {
  UnitTableType type;
  type.Name = std::string(T::Name);
  type.Id = typeIdOf<T>();
  type.OverId = typeIdOf<typename T::Over>();
  type.ReadIds = {typeIdOf<Reads>()...};
  type.MakeTable = makeTable;
  return type;
}

template <typename T> struct ReadIdsOf;
template <typename... Reads> struct ReadIdsOf<std::tuple<Reads...>> {
  template <typename K> static UnitTableType typeOf() {
    return unitTableType<K, Reads...>([]() -> std::unique_ptr<UnitTable> {
      return std::make_unique<KindTable<typename K::Value>>();
    });
  }
};

template <typename T> const T *UnitReads::component(EntityKey key) {
  const WorldSchema &schema = Read->schema();
  const std::optional<size_t> store = schema.indexOf(typeIdOf<T>());
  if constexpr (WORLD_CHECKS) {
    const std::vector<size_t> &allowed = schema.unitTables()[Table].Reads;
    if (!store || std::ranges::find(allowed, *store) == allowed.end()) {
      throw std::logic_error("unit kind '" + schema.unitTables()[Table].Name +
                             "' read a component its Reads don't name");
    }
  }
  Recorded->push_back(UnitRead{*store, key});
  return Read->tryGet<T>(key);
}

template <typename K> const typename K::Value *UnitReads::unit(EntityKey key) {
  const size_t position = earlierTable(typeIdOf<K>());
  Recorded->push_back(UnitRead{Space->StoreCount + position, key});
  const auto &table = static_cast<const KindTable<typename K::Value> &>(*Space->Tables[position]);
  const KindUnit<typename K::Value> *found = table.Units.find(key);
  return found == nullptr ? nullptr : &found->Held;
}

template <typename I> std::span<const EntityKey> UnitReads::filed(EntityKey key) {
  const size_t position = earlierTable(typeIdOf<I>());
  Recorded->push_back(UnitRead{Space->StoreCount + position, key});
  const auto &index = static_cast<const IndexTable &>(*Space->Tables[position]);
  const std::vector<EntityKey> *entry = index.Entries.find(key);
  return entry == nullptr ? std::span<const EntityKey>() : std::span<const EntityKey>(*entry);
}

template <UnitIndex I> void addUnitIndex(WorldSchema &schema) {
  schema.addUnitTable(unitTableType<I>([]() -> std::unique_ptr<UnitTable> {
                        return std::make_unique<IndexTable>();
                      }),
                      &updateIndex<I>);
}

template <UnitKind K> void addUnits(WorldSchema &schema) {
  schema.addUnitTable(ReadIdsOf<typename K::Reads>::template typeOf<K>(), &updateUnits<K>);
}

template <UnitKind K> const typename K::Value *unitValue(const World &world, EntityKey key) {
  const std::optional<size_t> position = world.schema().unitTableOf(typeIdOf<K>());
  if (!position) {
    return nullptr;
  }
  const auto &table =
      static_cast<const KindTable<typename K::Value> &>(*unitSpaceOf(world).Tables[*position]);
  const KindUnit<typename K::Value> *found = table.Units.find(key);
  return found == nullptr ? nullptr : &found->Held;
}

template <UnitKind K> bool unitsCurrent(const World &world) {
  const std::optional<size_t> position = world.schema().unitTableOf(typeIdOf<K>());
  if (!position) {
    return false;
  }
  const UnitSpace &space = unitSpaceOf(world);
  return space.Tables[*position]->Counter == space.Counter;
}

} // namespace tpj

#endif
