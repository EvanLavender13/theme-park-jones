#ifndef TPJ_SIM_COMPONENT_STORE_H
#define TPJ_SIM_COMPONENT_STORE_H

#include "sim/entity_key.h"

#include <memory>
#include <stddef.h>
#include <string>
#include <vector>

namespace tpj {

class TextCursor;
class WordSink;

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
  [[nodiscard]] virtual bool has(EntityKey key) const = 0;
  [[nodiscard]] virtual size_t size() const = 0;
  // The held keys, ascending.
  [[nodiscard]] virtual std::vector<EntityKey> keys() const = 0;
  virtual void erase(EntityKey key) = 0;
  // The held component's fields as words. An empty type emits none.
  virtual void emit(EntityKey key, WordSink &sink) const = 0;
  // The held component's fields for its save line, each as a space and name=value.
  virtual void write(EntityKey key, std::string &line) const = 0;
  // Reads what write wrote and adds the component under key. Throws LoadError.
  virtual void read(TextCursor &cursor, EntityKey key) = 0;
};

} // namespace tpj

#endif
