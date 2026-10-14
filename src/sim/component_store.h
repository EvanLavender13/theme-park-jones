#ifndef TPJ_SIM_COMPONENT_STORE_H
#define TPJ_SIM_COMPONENT_STORE_H

#include "sim/entity_key.h"

#include <iterator>
#include <map>
#include <memory>
#include <stddef.h>
#include <string>
#include <vector>

namespace tpj {

class TextCursor;
class WordSink;

// Receives a store's keys, one at a time.
class KeyVisitor {
public:
  KeyVisitor() = default;
  KeyVisitor(const KeyVisitor &) = delete;
  KeyVisitor &operator=(const KeyVisitor &) = delete;
  KeyVisitor(KeyVisitor &&) = delete;
  KeyVisitor &operator=(KeyVisitor &&) = delete;
  virtual ~KeyVisitor() = default;

  virtual void key(EntityKey key) = 0;
};

// One registered type's components, held by EntityKey in ascending key order. World owns one per
// registered type, and the walk and save reach it through this interface.
class ComponentStore {
public:
  ComponentStore() = default;
  ComponentStore(const ComponentStore &) = delete;
  ComponentStore &operator=(const ComponentStore &) = delete;
  ComponentStore(ComponentStore &&) = delete;
  ComponentStore &operator=(ComponentStore &&) = delete;
  virtual ~ComponentStore() = default;

  [[nodiscard]] virtual std::unique_ptr<ComponentStore> clone() const = 0;
  // Makes this store hold exactly the other's components, which has the same type, reusing its
  // memory.
  virtual void assignFrom(const ComponentStore &other) = 0;
  [[nodiscard]] virtual bool has(EntityKey key) const = 0;
  [[nodiscard]] virtual size_t size() const = 0;
  // The held keys, ascending.
  [[nodiscard]] virtual std::vector<EntityKey> keys() const = 0;
  // Gives the visitor each held key, ascending. Allocates nothing.
  virtual void visitKeys(KeyVisitor &visitor) const = 0;
  virtual void erase(EntityKey key) = 0;
  // The held component's fields as words. An empty type emits none.
  virtual void emit(EntityKey key, WordSink &sink) const = 0;
  // The held component's fields for its save line, each as a space and name=value.
  virtual void write(EntityKey key, std::string &line) const = 0;
  // Reads what write wrote and adds the component under key. Throws LoadError.
  virtual void read(TextCursor &cursor, EntityKey key) = 0;
};

// Makes room in a list of kept nodes before one more is added. When the list is full, it makes room
// for every node the map still holds as well, so it grows again only after every value held then
// has been removed.
template <typename Node> void keepSpare(std::vector<Node> &spare, size_t held) {
  if (spare.size() == spare.capacity()) {
    spare.reserve(held + spare.size() + 1);
  }
}

// Makes mine hold exactly the other's values. It assigns in place the value of each key both hold,
// so the value's lists keep their memory, moves the node of each key only mine holds into spare,
// and takes a node from spare for each key only the other holds. Allocates nothing once mine and
// spare have grown.
template <typename Value>
void assignKeyed(std::map<EntityKey, Value> &mine, const std::map<EntityKey, Value> &other,
                 std::vector<typename std::map<EntityKey, Value>::node_type> &spare) {
  auto at = mine.begin();
  auto from = other.begin();
  while (at != mine.end() || from != other.end()) {
    if (from == other.end() || (at != mine.end() && at->first < from->first)) {
      const auto next = std::next(at);
      keepSpare(spare, mine.size());
      spare.push_back(mine.extract(at));
      at = next;
    } else if (at == mine.end() || from->first < at->first) {
      if (spare.empty()) {
        mine.emplace_hint(at, from->first, from->second);
      } else {
        auto node = std::move(spare.back());
        spare.pop_back();
        node.key() = from->first;
        node.mapped() = from->second;
        mine.insert(at, std::move(node));
      }
      ++from;
    } else {
      at->second = from->second;
      ++at;
      ++from;
    }
  }
}

} // namespace tpj

#endif
