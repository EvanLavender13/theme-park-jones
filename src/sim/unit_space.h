#ifndef TPJ_SIM_UNIT_SPACE_H
#define TPJ_SIM_UNIT_SPACE_H

#include "sim/component_store.h"
#include "sim/entity_key.h"
#include "sim/kept_map.h"

#include <compare>
#include <map>
#include <memory>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// Something a unit's derivation read, or something a resolution changed: a table and a key in it.
// Tables are numbered with the component stores first, by store index, then the unit kinds and
// indexes, by their position in the schema's unit tables.
struct UnitRead {
  size_t Table = 0;
  EntityKey Key = NULL_KEY;

  auto operator<=>(const UnitRead &) const = default;
  bool operator==(const UnitRead &) const = default;
};

// One unit kind's or index's table. The schema makes one empty table for each.
class UnitTable {
public:
  UnitTable() = default;
  UnitTable(const UnitTable &) = delete;
  UnitTable &operator=(const UnitTable &) = delete;
  UnitTable(UnitTable &&) = delete;
  UnitTable &operator=(UnitTable &&) = delete;
  virtual ~UnitTable() = default;

  [[nodiscard]] virtual std::unique_ptr<UnitTable> clone() const = 0;
  // Makes this table equal to the other, which has the same kind, reusing its memory.
  virtual void assignFrom(const UnitTable &other) = 0;

  // The space's counter when the table was last updated, or none before its first update.
  std::optional<uint64_t> Counter;
};

// A unit kind's value for one key, and what deriving it read, ascending.
template <typename Value> struct KindUnit {
  Value Held;
  std::vector<UnitRead> Reads;
};

// A unit kind's units by key.
template <typename Value> class KindTable final : public UnitTable {
public:
  [[nodiscard]] std::unique_ptr<UnitTable> clone() const override {
    auto copy = std::make_unique<KindTable>();
    copy->Counter = Counter;
    copy->Units = Units;
    return copy;
  }
  void assignFrom(const UnitTable &other) override {
    const auto &table = static_cast<const KindTable &>(other);
    Counter = table.Counter;
    Units.assignFrom(table.Units);
  }

  KeptMap<KindUnit<Value>> Units;
};

// An index's entries: the keys filed under each key, ascending, and the keys each entity is filed
// under, ascending.
class IndexTable final : public UnitTable {
public:
  [[nodiscard]] std::unique_ptr<UnitTable> clone() const override;
  void assignFrom(const UnitTable &other) override;

  KeptMap<std::vector<EntityKey>> Entries;
  KeptMap<std::vector<EntityKey>> Filed;
};

// The units a world's resolution keeps, apart from its stores, and the changes they start from:
// for each store units read, the keys commands touched since the last resolution; each table; for
// each thing read, the units that read it; and what the running resolution has changed. A world's
// walk, hash, and save never see it.
class UnitSpace {
public:
  explicit UnitSpace(const WorldSchema &schema);

  [[nodiscard]] std::unique_ptr<UnitSpace> clone() const;
  // Makes this space equal to the other, from a world on the same schema, reusing its memory.
  void assignFrom(const UnitSpace &other);

  // Logs the key when the store is one units read, and raises the counter.
  void noteChange(size_t store, EntityKey key) {
    if (Logged[store]) {
      Logs[store].push_back(key);
      ++Counter;
    }
  }
  [[nodiscard]] bool isLogged(size_t store) const { return Logged[store]; }
  // Puts each logged store's log on the change list, as changes to that store, and empties the
  // list of derived units.
  void beginResolution();
  // Empties the logs and the change list.
  void endResolution();
  // Records the unit as a reader of each of the reads.
  void addReader(UnitRead unit, const std::vector<UnitRead> &reads);
  // Stops recording the unit as a reader of each of the reads.
  void removeReader(UnitRead unit, const std::vector<UnitRead> &reads);

  // How many component stores the schema registers. Unit table t is read table StoreCount + t.
  size_t StoreCount = 0;
  // By store index, whether units read the store, and the keys touched since the last resolution.
  std::vector<bool> Logged;
  std::vector<std::vector<EntityKey>> Logs;
  // Raised by each logged touch.
  uint64_t Counter = 0;
  // By position in the schema's unit tables.
  std::vector<std::unique_ptr<UnitTable>> Tables;
  // By read table, the units that read each key, in no order.
  std::vector<KeptMap<std::vector<UnitRead>>> Readers;
  // What the running resolution changed: each logged store's touched keys, each index entry that
  // changed, and each unit that was added, removed, or derived a different value.
  std::vector<UnitRead> Changes;
  // The units the last resolution derived, and those a kind that updates its own units set or
  // removed, in the order it did so. Kept until the next resolution begins. A copy starts with
  // none.
  std::vector<UnitRead> Derived;

private:
  UnitSpace() = default;
};

// The world's units.
UnitSpace &unitSpaceOf(World &world);
const UnitSpace &unitSpaceOf(const World &world);

} // namespace tpj

#endif
