#ifndef TPJ_SIM_KEPT_MAP_H
#define TPJ_SIM_KEPT_MAP_H

#include "sim/component_store.h"
#include "sim/entity_key.h"

#include <iterator>
#include <map>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

// Values by key, in ascending key order, whose removed nodes are kept for later adds, as a store's
// are, so that adds and removes allocate nothing once the map has grown. A copy holds the same
// values without the kept nodes. Assigning reuses this map's nodes and their memory.
template <typename Value> class KeptMap {
public:
  using Map = std::map<EntityKey, Value>;

  KeptMap() = default;
  KeptMap(const KeptMap &other) : Items(other.Items) {}
  KeptMap &operator=(const KeptMap &other) {
    if (this != &other) {
      assignFrom(other);
    }
    return *this;
  }
  KeptMap(KeptMap &&) noexcept = default;
  KeptMap &operator=(KeptMap &&) noexcept = default;
  ~KeptMap() = default;

  [[nodiscard]] Value *find(EntityKey key) {
    const auto found = Items.find(key);
    return found == Items.end() ? nullptr : &found->second;
  }
  [[nodiscard]] const Value *find(EntityKey key) const {
    const auto found = Items.find(key);
    return found == Items.end() ? nullptr : &found->second;
  }
  // The key's value, and whether it was added. An added value may still hold what a removed one
  // held, for the caller to overwrite.
  std::pair<Value &, bool> getOrAdd(EntityKey key) {
    const auto found = Items.lower_bound(key);
    if (found != Items.end() && found->first == key) {
      return {found->second, false};
    }
    if (Spare.empty()) {
      return {Items.emplace_hint(found, key, Value{})->second, true};
    }
    auto node = std::move(Spare.back());
    Spare.pop_back();
    node.key() = key;
    return {Items.insert(found, std::move(node))->second, true};
  }
  // Removes the key's value, keeping its node. Returns whether the key held one.
  bool erase(EntityKey key) {
    auto node = Items.extract(key);
    if (!node) {
      return false;
    }
    keepSpare(Spare, Items.size());
    Spare.push_back(std::move(node));
    return true;
  }
  // Makes this map hold exactly the other's values, reusing its nodes and their memory.
  void assignFrom(const KeptMap &other) { assignKeyed(Items, other.Items, Spare); }
  [[nodiscard]] size_t size() const { return Items.size(); }
  [[nodiscard]] bool empty() const { return Items.empty(); }
  // The values in ascending key order, as (key, value) pairs.
  [[nodiscard]] auto begin() const { return Items.cbegin(); }
  [[nodiscard]] auto end() const { return Items.cend(); }
  [[nodiscard]] auto begin() { return Items.begin(); }
  [[nodiscard]] auto end() { return Items.end(); }
  // Removes each value that removed(value) is true for, keeping its node.
  template <typename Removed> void eraseIf(Removed removed) {
    for (auto at = Items.begin(); at != Items.end();) {
      const auto next = std::next(at);
      if (removed(at->second)) {
        keepSpare(Spare, Items.size());
        Spare.push_back(Items.extract(at));
      }
      at = next;
    }
  }

private:
  Map Items;
  std::vector<typename Map::node_type> Spare;
};

} // namespace tpj

#endif
