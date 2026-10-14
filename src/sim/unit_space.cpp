#include "sim/unit_space.h"

#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/units.h"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace tpj {

std::unique_ptr<UnitTable> IndexTable::clone() const {
  auto copy = std::make_unique<IndexTable>();
  copy->Counter = Counter;
  copy->Entries = Entries.clone();
  copy->Filed = Filed.clone();
  return copy;
}

void IndexTable::assignFrom(const UnitTable &other) {
  const auto &table = static_cast<const IndexTable &>(other);
  Counter = table.Counter;
  Entries.assignFrom(table.Entries);
  Filed.assignFrom(table.Filed);
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
    copy->Readers.push_back(readers.clone());
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
}

void UnitSpace::beginResolution() {
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
