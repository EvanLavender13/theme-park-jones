#include "sim/unit_space.h"

#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace tpj {

std::unique_ptr<UnitTable> IndexTable::clone() const {
  auto copy = std::make_unique<IndexTable>();
  copy->Counter = Counter;
  copy->Entries = Entries;
  copy->Filed = Filed;
  return copy;
}

void IndexTable::assignFrom(const UnitTable &other) {
  const auto &table = static_cast<const IndexTable &>(other);
  Counter = table.Counter;
  Entries.assignFrom(table.Entries);
  Filed.assignFrom(table.Filed);
}

bool IndexTable::holds(EntityKey key) const { return Filed.find(key) != nullptr; }

void IndexTable::appendKeys(std::vector<EntityKey> &keys) const {
  for (const auto &[key, filed] : Filed) {
    keys.push_back(key);
  }
}

UnitSpace::UnitSpace(const WorldSchema &schema)
    : StoreCount(schema.components().size()), Logged(StoreCount), Logs(StoreCount),
      Readers(StoreCount + schema.unitTables().size()) {
  for (size_t store = 0; store < StoreCount; ++store) {
    Logged[store] = schema.isLogged(store);
  }
  Tables.reserve(schema.unitTables().size());
  for (const UnitTableType &type : schema.unitTables()) {
    Tables.push_back(type.MakeTable());
  }
}

std::unique_ptr<UnitSpace> UnitSpace::clone() const {
  auto copy = std::unique_ptr<UnitSpace>(new UnitSpace());
  copy->StoreCount = StoreCount;
  copy->Logged = Logged;
  copy->Logs = Logs;
  copy->Counter = Counter;
  copy->Tables.reserve(Tables.size());
  for (const std::unique_ptr<UnitTable> &table : Tables) {
    copy->Tables.push_back(table->clone());
  }
  copy->Readers.reserve(Readers.size());
  for (const KeptMap<std::vector<UnitRead>> &readers : Readers) {
    copy->Readers.push_back(readers);
  }
  copy->Changes = Changes;
  return copy;
}

void UnitSpace::assignFrom(const UnitSpace &other) {
  for (size_t store = 0; store < StoreCount; ++store) {
    copyGrowing(Logs[store], std::span<const EntityKey>(other.Logs[store]));
  }
  Counter = other.Counter;
  for (size_t table = 0; table < Tables.size(); ++table) {
    Tables[table]->assignFrom(*other.Tables[table]);
  }
  for (size_t table = 0; table < Readers.size(); ++table) {
    Readers[table].assignFrom(other.Readers[table]);
  }
  copyGrowing(Changes, std::span<const UnitRead>(other.Changes));
  Derived.clear();
}

void UnitSpace::beginResolution() {
  Derived.clear();
  for (size_t store = 0; store < StoreCount; ++store) {
    for (const EntityKey key : Logs[store]) {
      Changes.push_back(UnitRead{store, key});
    }
  }
}

void UnitSpace::endResolution() {
  for (std::vector<EntityKey> &log : Logs) {
    log.clear();
  }
  Changes.clear();
}

void UnitSpace::addReader(UnitRead unit, const std::vector<UnitRead> &reads) {
  for (const UnitRead &read : reads) {
    auto [readers, added] = Readers[read.Table].getOrAdd(read.Key);
    if (added) {
      readers.clear();
    }
    readers.push_back(unit);
  }
}

void UnitSpace::removeReader(UnitRead unit, const std::vector<UnitRead> &reads) {
  for (const UnitRead &read : reads) {
    std::vector<UnitRead> *readers = Readers[read.Table].find(read.Key);
    if (readers == nullptr) {
      continue;
    }
    const auto found = std::ranges::find(*readers, unit);
    if (found != readers->end()) {
      *found = readers->back();
      readers->pop_back();
    }
    if (readers->empty()) {
      Readers[read.Table].erase(read.Key);
    }
  }
}

void sortUnique(std::vector<EntityKey> &keys) {
  std::ranges::sort(keys);
  keys.erase(std::ranges::unique(keys).begin(), keys.end());
}

void sortUnique(std::vector<UnitRead> &reads) {
  std::ranges::sort(reads);
  reads.erase(std::ranges::unique(reads).begin(), reads.end());
}

void eraseSorted(std::vector<EntityKey> &keys, EntityKey key) {
  const auto found = std::ranges::lower_bound(keys, key);
  if (found != keys.end() && *found == key) {
    keys.erase(found);
  }
}

void insertSorted(std::vector<EntityKey> &keys, EntityKey key) {
  const auto found = std::ranges::lower_bound(keys, key);
  if (found == keys.end() || *found != key) {
    keys.insert(found, key);
  }
}

void fileAgain(IndexTable &index, UnitSpace &space, size_t table, EntityKey key,
               const std::vector<EntityKey> &keys) {
  std::vector<EntityKey> *filed = index.Filed.find(key);
  if (filed != nullptr && *filed == keys) {
    return;
  }
  if (filed != nullptr) {
    for (const EntityKey old : *filed) {
      if (std::ranges::binary_search(keys, old)) {
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
  for (const EntityKey now : keys) {
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
  if (keys.empty()) {
    index.Filed.erase(key);
  } else {
    std::vector<EntityKey> &kept = index.Filed.getOrAdd(key).first;
    copyGrowing(kept, std::span<const EntityKey>(keys));
  }
}

void changedKeys(const UnitSpace &space, size_t table, std::vector<EntityKey> &keys) {
  for (const UnitRead &change : space.Changes) {
    if (change.Table == table) {
      keys.push_back(change.Key);
    }
  }
  sortUnique(keys);
}

void derivedUnits(const World &world, std::string_view kind, std::vector<EntityKey> &keys) {
  const std::vector<UnitTableType> &tables = world.schema().unitTables();
  const auto found = std::ranges::find(tables, kind, &UnitTableType::Name);
  if (found == tables.end()) {
    throw std::invalid_argument("no unit kind or index is registered as '" + std::string(kind) +
                                "'");
  }
  const UnitSpace &space = unitSpaceOf(world);
  const size_t table = space.StoreCount + static_cast<size_t>(found - tables.begin());
  keys.clear();
  for (const UnitRead &derived : space.Derived) {
    if (derived.Table == table) {
      keys.push_back(derived.Key);
    }
  }
  sortUnique(keys);
}

size_t UnitReads::earlierTable(TypeId id) const {
  const std::optional<size_t> position = Read->schema().unitTableOf(id);
  if constexpr (WORLD_CHECKS) {
    if (!position || *position >= Table) {
      throw std::logic_error("unit kind '" + Read->schema().unitTables()[Table].Name +
                             "' read a unit kind or index not registered before it");
    }
  }
  return *position;
}

} // namespace tpj
