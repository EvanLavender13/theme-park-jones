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

// An index over an earlier unit kind: a Name, the kind Of, and
// keysOf(const typename Of::Value &, std::vector<EntityKey> &), which clears the list and puts in
// the keys to file the unit under.
template <typename I>
concept UnitValueIndex =
    std::convertible_to<decltype(I::Name), std::string_view> && UnitKind<typename I::Of> &&
    requires(const typename I::Of::Value &value, std::vector<EntityKey> &keys) {
      I::keysOf(value, keys);
    };

template <typename Value> class UnitUpdate;

// A kind that updates its own units: a Name, an intent component Over, a Value with a visitFields
// function, and update(UnitUpdate<Value> &).
template <typename K>
concept SelfUpdatingKind =
    std::convertible_to<decltype(K::Name), std::string_view> &&
    std::is_default_constructible_v<typename K::Value> && HasVisitFields<typename K::Value> &&
    requires(UnitUpdate<typename K::Value> &update, typename K::Over *over) { K::update(update); };

template <UnitIndex I> void addUnitIndex(WorldSchema &schema);
template <UnitValueIndex I> void addUnitIndex(WorldSchema &schema);
template <UnitKind K> void addUnits(WorldSchema &schema);
template <SelfUpdatingKind K> void addUnits(WorldSchema &schema);
template <UnitKind K> const typename K::Value *unitValue(const World &world, EntityKey key);
template <UnitKind K> bool unitsCurrent(const World &world);
// Adds to the keys those of the kind's units that the running resolution added, removed, or
// changed, keeping the list ascending without repeats.
template <typename K> void changedUnits(const World &world, std::vector<EntityKey> &keys);
// Puts in keys, clearing it first, the keys of the units of the kind registered under the name that
// the world's last resolution derived, ascending, whether their values came out changed or not.
// For a kind that updates its own units, the keys its update set or removed. An index derives none.
// Throws std::invalid_argument when no unit kind or index has the name.
void derivedUnits(const World &world, std::string_view kind, std::vector<EntityKey> &keys);

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
// Files the entity under the keys, which are ascending without repeats, in place of the keys it
// was filed under, and puts each entry that changed on the change list as one of the table's.
void fileAgain(IndexTable &index, UnitSpace &space, size_t table, EntityKey key,
               const std::vector<EntityKey> &keys);
// Adds to the keys those of the table's entries on the change list, keeping the list ascending
// without repeats.
void changedKeys(const UnitSpace &space, size_t table, std::vector<EntityKey> &keys);
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
    fileAgain(index, space, table, key, lists.Keys);
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
    space.Derived.push_back(unit);
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

// Files again each unit of I::Of on the change list, or every unit before the first update, under
// the keys its value gives, and puts each entry that changed on the change list.
template <UnitValueIndex I> void updateValueIndex(World &world, TickBuffers &buffers) {
  using Value = typename I::Of::Value;
  UnitSpace &space = unitSpaceOf(world);
  const WorldSchema &schema = world.schema();
  const size_t position = tablePosition<I>(schema);
  const size_t of = tablePosition<typename I::Of>(schema);
  auto &index = static_cast<IndexTable &>(*space.Tables[position]);
  const auto &units = static_cast<const KindTable<Value> &>(*space.Tables[of]);
  auto &lists = buffers.get<IndexUpdateLists>();
  lists.Dirty.clear();
  if (index.Counter) {
    changedKeys(space, space.StoreCount + of, lists.Dirty);
  } else {
    for (const auto &[key, unit] : units.Units) {
      lists.Dirty.push_back(key);
    }
  }
  for (const EntityKey key : lists.Dirty) {
    lists.Keys.clear();
    if (const KindUnit<Value> *unit = units.Units.find(key)) {
      I::keysOf(unit->Held, lists.Keys);
      sortUnique(lists.Keys);
    }
    fileAgain(index, space, space.StoreCount + position, key, lists.Keys);
  }
  index.Counter = space.Counter;
}

// What a kind that updates its own units reads and writes in its update.
template <typename Value> class UnitUpdate {
public:
  // Made by resolution for the update of the kind at the position in the schema's unit tables.
  UnitUpdate(World &world, TickBuffers &buffers, size_t position,
             void (*keysOver)(const World &, std::vector<EntityKey> &))
      : Read(&world), Space(&unitSpaceOf(world)), Buffers(&buffers), Position(position),
        Table(&static_cast<KindTable<Value> &>(*Space->Tables[position])), OverKeys(keysOver) {}

  // True at the kind's first update, when it holds no units.
  [[nodiscard]] bool first() const { return !Table->Counter.has_value(); }
  // Puts in keys every key that holds the kind's intent component, ascending, clearing it first.
  void overKeys(std::vector<EntityKey> &keys) const { OverKeys(*Read, keys); }
  // Adds to the keys those of an earlier kind's units, or of an earlier index's entries, that the
  // running resolution changed, keeping the list ascending without repeats.
  template <typename T> void changed(std::vector<EntityKey> &keys) const {
    changedKeys(*Space, Space->StoreCount + earlier<T>(), keys);
  }
  // The unit of an earlier kind for the key, or null when there is none.
  template <typename K> const typename K::Value *unit(EntityKey key) const {
    const auto &table =
        static_cast<const KindTable<typename K::Value> &>(*Space->Tables[earlier<K>()]);
    const KindUnit<typename K::Value> *found = table.Units.find(key);
    return found == nullptr ? nullptr : &found->Held;
  }
  // The keys an earlier index files under the key, ascending.
  template <typename I> std::span<const EntityKey> filed(EntityKey key) const {
    const auto &index = static_cast<const IndexTable &>(*Space->Tables[earlier<I>()]);
    const std::vector<EntityKey> *entry = index.Entries.find(key);
    return entry == nullptr ? std::span<const EntityKey>() : std::span<const EntityKey>(*entry);
  }
  // The kind's own unit for the key, or null.
  [[nodiscard]] const Value *held(EntityKey key) const {
    const KindUnit<Value> *found = Table->Units.find(key);
    return found == nullptr ? nullptr : &found->Held;
  }
  // Sets the key's unit, recording a change when it adds the unit or its words differ.
  void set(EntityKey key, const Value &value) {
    auto &lists = Buffers->get<KindUpdateLists<Value>>();
    Space->Derived.push_back(UnitRead{Space->StoreCount + Position, key});
    auto [unit, added] = Table->Units.getOrAdd(key);
    if (added) {
      unit.Reads.clear();
    }
    if (added || !sameWords(unit.Held, value, lists.Left, lists.Right)) {
      unit.Held = value;
      Space->Changes.push_back(UnitRead{Space->StoreCount + Position, key});
    }
  }
  // Removes the key's unit, recording a change when it held one.
  void remove(EntityKey key) {
    Space->Derived.push_back(UnitRead{Space->StoreCount + Position, key});
    if (Table->Units.erase(key)) {
      Space->Changes.push_back(UnitRead{Space->StoreCount + Position, key});
    }
  }
  // Lists the update reuses, never read by resolution. Clear a list before reading it.
  TickBuffers &scratch() { return *Buffers; }

private:
  // The position of an earlier unit kind or index, named by its type, in the schema's unit tables.
  template <typename T> size_t earlier() const {
    const std::optional<size_t> position = Read->schema().unitTableOf(typeIdOf<T>());
    if constexpr (WORLD_CHECKS) {
      if (!position || *position >= Position) {
        throw std::logic_error("unit kind '" + Read->schema().unitTables()[Position].Name +
                               "' read a unit kind or index not registered before it");
      }
    }
    return *position;
  }

  const World *Read;
  UnitSpace *Space;
  TickBuffers *Buffers;
  size_t Position;
  KindTable<Value> *Table;
  void (*OverKeys)(const World &, std::vector<EntityKey> &);
};

// Puts in keys every key that holds Over, clearing it first.
template <typename Over> void keysHolding(const World &world, std::vector<EntityKey> &keys) {
  world.keysWith<Over>(keys);
}

// Runs the kind's update and marks its table as updated.
template <SelfUpdatingKind K> void updateSelf(World &world, TickBuffers &buffers) {
  const size_t position = tablePosition<K>(world.schema());
  UnitUpdate<typename K::Value> update(world, buffers, position, &keysHolding<typename K::Over>);
  K::update(update);
  UnitSpace &space = unitSpaceOf(world);
  space.Tables[position]->Counter = space.Counter;
}

// The schema's entry for a unit kind or index, with the intent types it reads.
template <typename T, typename Over, typename... Reads>
UnitTableType unitTableType(std::unique_ptr<UnitTable> (*makeTable)()) {
  UnitTableType type;
  type.Name = std::string(T::Name);
  type.Id = typeIdOf<T>();
  type.OverId = typeIdOf<Over>();
  type.ReadIds = {typeIdOf<Reads>()...};
  type.MakeTable = makeTable;
  return type;
}

template <typename T> struct ReadIdsOf;
template <typename... Reads> struct ReadIdsOf<std::tuple<Reads...>> {
  template <typename K> static UnitTableType typeOf() {
    return unitTableType<K, typename K::Over, Reads...>([]() -> std::unique_ptr<UnitTable> {
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
  schema.addUnitTable(unitTableType<I, typename I::Over>([]() -> std::unique_ptr<UnitTable> {
                        return std::make_unique<IndexTable>();
                      }),
                      &updateIndex<I>);
}

template <UnitValueIndex I> void addUnitIndex(WorldSchema &schema) {
  schema.addUnitTable(unitTableType<I, typename I::Of::Over>([]() -> std::unique_ptr<UnitTable> {
                        return std::make_unique<IndexTable>();
                      }),
                      &updateValueIndex<I>);
}

template <UnitKind K> void addUnits(WorldSchema &schema) {
  schema.addUnitTable(ReadIdsOf<typename K::Reads>::template typeOf<K>(), &updateUnits<K>);
}

template <SelfUpdatingKind K> void addUnits(WorldSchema &schema) {
  schema.addUnitTable(unitTableType<K, typename K::Over>([]() -> std::unique_ptr<UnitTable> {
                        return std::make_unique<KindTable<typename K::Value>>();
                      }),
                      &updateSelf<K>);
}

template <typename K> void changedUnits(const World &world, std::vector<EntityKey> &keys) {
  const UnitSpace &space = unitSpaceOf(world);
  changedKeys(space, space.StoreCount + tablePosition<K>(world.schema()), keys);
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
