#ifndef TPJ_SIM_TICK_BUFFERS_H
#define TPJ_SIM_TICK_BUFFERS_H

#include "sim/type_id.h"

#include <algorithm>
#include <memory>
#include <span>
#include <stddef.h>
#include <utility>
#include <vector>

namespace tpj {

// The lists systems, swaps, resolvers, and finishers reuse from one tick or resolution to the next,
// owned by whoever steps or resolves the world. Each is a value of a type a module defines, made on
// first use and kept. A TickBuffers is never part of a world: walks, hashes, saves, and copies
// never see it.
class TickBuffers {
public:
  TickBuffers() = default;
  TickBuffers(const TickBuffers &) = delete;
  TickBuffers &operator=(const TickBuffers &) = delete;
  TickBuffers(TickBuffers &&) noexcept = default;
  TickBuffers &operator=(TickBuffers &&) noexcept = default;
  ~TickBuffers() = default;

  // The kept T, default constructed on first use. Clear a list before reading it.
  template <typename T> T &get();

private:
  struct Held {
    TypeId Id = nullptr;
    std::unique_ptr<void, void (*)(void *)> Value{nullptr, nullptr};
  };
  std::vector<Held> Values;
};

template <typename T> T &TickBuffers::get() {
  const TypeId id = typeIdOf<T>();
  for (const Held &held : Values) {
    if (held.Id == id) {
      return *static_cast<T *>(held.Value.get());
    }
  }
  Held held{id, {new T(), [](void *value) { delete static_cast<T *>(value); }}};
  T &value = *static_cast<T *>(held.Value.get());
  Values.push_back(std::move(held));
  return value;
}

// The fewest values a kept list makes room for when it first grows.
inline constexpr size_t FIRST_KEPT_CAPACITY = 8;

// Copies the values into a list kept from tick to tick. When the list must grow, it at least
// doubles, and first makes room for FIRST_KEPT_CAPACITY values, so a list that grows a few values
// at a time reallocates rarely, where assign would reallocate to the exact size each time.
template <typename T> void copyGrowing(std::vector<T> &kept, std::span<const T> values) {
  if (kept.capacity() < values.size()) {
    kept.reserve(std::max({values.size(), 2 * kept.capacity(), FIRST_KEPT_CAPACITY}));
  }
  kept.assign(values.begin(), values.end());
}

// Gives a kept list the count copies of the value. When the list must grow, it grows as
// copyGrowing does, carrying every value it held, so a growing list stays a growing list and
// never a new one.
template <typename T> void fillGrowing(std::vector<T> &kept, size_t count, const T &value) {
  if (kept.capacity() < count) {
    kept.resize(kept.capacity(), value);
    kept.reserve(std::max({count, 2 * kept.capacity(), FIRST_KEPT_CAPACITY}));
  }
  kept.assign(count, value);
}

// Gives a kept list whose values hold lists of their own count values, keeping every inner list's
// memory: when it shrinks, its extra values move to the spare list, and when it grows, it takes
// values back from the spare list before making new ones. The values it keeps still hold what
// they held, for the caller to overwrite.
template <typename T>
void resizeKeeping(std::vector<T> &kept, size_t count, std::vector<T> &spare) {
  while (kept.size() > count) {
    spare.push_back(std::move(kept.back()));
    // A moved-from value holds no memory, so dropping it frees nothing.
    kept.pop_back();
  }
  kept.reserve(count);
  while (kept.size() < count) {
    if (spare.empty()) {
      kept.emplace_back();
    } else {
      kept.push_back(std::move(spare.back()));
      spare.pop_back();
    }
  }
}

} // namespace tpj

#endif
