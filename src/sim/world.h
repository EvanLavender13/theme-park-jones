#ifndef TPJ_SIM_WORLD_H
#define TPJ_SIM_WORLD_H

#include "sim/command_queue.h"
#include "sim/component_store.h"
#include "sim/entity_key.h"
#include "sim/schema.h"
#include "sim/tick_buffers.h"
#include "sim/type_id.h"

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tpj {

// Simulation time advances in fixed ticks, independent of the render frame rate.
constexpr double SIM_TICK_SECONDS = 1.0 / 30.0;

// Debug builds check a world's invariants before walking it (see validateWorld).
#ifdef NDEBUG
constexpr bool WORLD_CHECKS = false;
#else
constexpr bool WORLD_CHECKS = true;
#endif

// A world the walk cannot cover fully: a NaN in registered state, or two derived origins sharing a
// key. Also thrown when a component of an unregistered type is added, when a component is added to
// a key that isn't live in debug builds, and when a world is created under a floating-point
// environment that would change the simulation's arithmetic.
class WorldInvariantError : public std::logic_error {
public:
  using std::logic_error::logic_error;
};

// The park as a value: keyed entities whose registered components can be copied, compared, and
// hashed. Move-only; copy it with copyWorld.
class World {
public:
  World();
  World(std::shared_ptr<const WorldSchema> schema, uint64_t seed);
  World(const World &) = delete;
  World &operator=(const World &) = delete;
  World(World &&) noexcept = default;
  World &operator=(World &&) noexcept = default;
  ~World() = default;

  uint64_t Tick = 0;
  uint64_t Seed = 0;

  [[nodiscard]] const WorldSchema &schema() const { return *Schema; }
  [[nodiscard]] uint64_t nextKey() const { return NextKey; }
  // True from construction until the first resolution, and from each applied command until the
  // next.
  [[nodiscard]] bool isResolvePending() const { return ResolvePending; }
  // True only while the resolvers run.
  [[nodiscard]] bool isResolving() const { return Resolving; }
  // True only while the systems step.
  [[nodiscard]] bool isStepping() const { return Stepping; }
  // True only while the finishers run.
  [[nodiscard]] bool isFinishing() const { return Finishing; }

  // A new entity keyed from the counter.
  EntityKey createEntity();
  // The entity keyed deriveKey(owner, purpose, index), created if no entity holds that key.
  EntityKey createDerivedEntity(EntityKey owner, uint64_t purpose, uint64_t index);
  // Removes the entity and its components. False, with no change, when the key is not live.
  bool destroyEntity(EntityKey key);
  // Live keys, ascending.
  [[nodiscard]] std::vector<EntityKey> keys() const;
  // True when an entity holds this key.
  [[nodiscard]] bool isLive(EntityKey key) const;

  // The entity must be live and hold T.
  template <typename T> T &get(EntityKey key);
  template <typename T> const T &get(EntityKey key) const;
  // Nothing when the key isn't live or its entity doesn't hold T.
  template <typename T> T *tryGet(EntityKey key);
  template <typename T> const T *tryGet(EntityKey key) const;
  // False when the key isn't live.
  template <typename T> [[nodiscard]] bool has(EntityKey key) const;
  // The entity must be live and not hold T.
  template <typename T, typename... Args> void emplace(EntityKey key, Args &&...args);
  // The entity must be live.
  template <typename T> void emplaceOrReplace(EntityKey key, T value);
  // The entity must be live. T must not be an empty type.
  template <typename T> T &getOrEmplace(EntityKey key);
  // Removes T if the entity holds it. Nothing happens when the key isn't live.
  template <typename T> void remove(EntityKey key);
  // Keys of the entities that hold T, ascending.
  template <typename T> [[nodiscard]] std::vector<EntityKey> keysWith() const;
  // The same keys, in a list the caller keeps, which it clears first. Allocates nothing once the
  // list has grown.
  template <typename T> void keysWith(std::vector<EntityKey> &keys) const;
  // Calls visit(key, component) for each entity that holds T, in ascending key order. visit must
  // not add or remove a T. Allocates nothing.
  template <typename T, typename Visit> void forEachWith(Visit &&visit) const;
  // True when the entity is live and holds the registered type.
  [[nodiscard]] bool holds(const ComponentType &type, EntityKey key) const;

private:
  struct DerivedOrigin {
    EntityKey Owner = NULL_KEY;
    uint64_t Purpose = 0;
    uint64_t Index = 0;
    bool operator==(const DerivedOrigin &) const = default;
  };
  struct EntityRecord {
    std::optional<DerivedOrigin> Origin;
  };

  void addEntity(EntityKey key);
  // T's store, or nullptr when the schema doesn't register T.
  template <typename T> TypedStore<T> *findStore();
  template <typename T> const TypedStore<T> *findStore() const;
  // T's store for adding a component to the key. Throws WorldInvariantError if the schema doesn't
  // register T, or, in debug builds, if the key isn't live.
  template <typename T> TypedStore<T> &storeToAdd(EntityKey key);
  void emitWords(WordSink &sink) const;

  std::shared_ptr<const WorldSchema> Schema;
  uint64_t NextKey = 1;
  std::map<EntityKey, EntityRecord> ByKey;
  // Nodes of destroyed entities' records, kept for later entities. Never copied, walked, or saved.
  std::vector<std::map<EntityKey, EntityRecord>::node_type> SpareRecords;
  // One store per registered type, in registration order.
  std::vector<std::unique_ptr<ComponentStore>> Stores;
  bool ResolvePending = true;
  // Set only while resolvers run, so that createEntity can refuse them in debug builds.
  bool Resolving = false;
  // Set only while finishers run, so that createEntity can refuse them in debug builds too.
  bool Finishing = false;
  // Set only while the systems step, so that functions meant for systems can refuse others.
  bool Stepping = false;

  friend World copyWorld(const World &world);
  friend bool worldsEqual(const World &left, const World &right);
  friend uint64_t hashWorld(const World &world);
  friend void validateWorld(const World &world);
  friend void resolveWorld(World &world);
  friend void stepWorld(World &world, CommandQueue &commands, TickBuffers &buffers);
  friend World makeCandidate(const World &world, const CommandQueue &commands);
  friend void commitCommands(World &world, CommandQueue &commands);
  friend std::string saveWorld(const World &world);
  friend World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text);
};

template <typename T> TypedStore<T> *World::findStore() {
  const std::optional<size_t> index = Schema->indexOf(typeIdOf<T>());
  return index ? static_cast<TypedStore<T> *>(Stores[*index].get()) : nullptr;
}

template <typename T> const TypedStore<T> *World::findStore() const {
  const std::optional<size_t> index = Schema->indexOf(typeIdOf<T>());
  return index ? static_cast<const TypedStore<T> *>(Stores[*index].get()) : nullptr;
}

template <typename T> TypedStore<T> &World::storeToAdd(EntityKey key) {
  TypedStore<T> *store = findStore<T>();
  if (store == nullptr) {
    throw WorldInvariantError("entity " + std::to_string(static_cast<uint64_t>(key)) +
                              " was given a component of a type the world's schema doesn't "
                              "register");
  }
  if (WORLD_CHECKS && !isLive(key)) {
    throw WorldInvariantError("a component was added to key " +
                              std::to_string(static_cast<uint64_t>(key)) + ", which isn't live");
  }
  return *store;
}

template <typename T> T &World::get(EntityKey key) { return findStore<T>()->Items.at(key); }

template <typename T> const T &World::get(EntityKey key) const {
  return findStore<T>()->Items.at(key);
}

template <typename T> T *World::tryGet(EntityKey key) {
  TypedStore<T> *store = findStore<T>();
  if (store == nullptr) {
    return nullptr;
  }
  const auto found = store->Items.find(key);
  return found == store->Items.end() ? nullptr : &found->second;
}

template <typename T> const T *World::tryGet(EntityKey key) const {
  const TypedStore<T> *store = findStore<T>();
  if (store == nullptr) {
    return nullptr;
  }
  const auto found = store->Items.find(key);
  return found == store->Items.end() ? nullptr : &found->second;
}

template <typename T> bool World::has(EntityKey key) const {
  const TypedStore<T> *store = findStore<T>();
  return store != nullptr && store->Items.contains(key);
}

template <typename T, typename... Args> void World::emplace(EntityKey key, Args &&...args) {
  TypedStore<T> &store = storeToAdd<T>(key);
  if (!store.Items.contains(key)) {
    store.add(key, std::forward<Args>(args)...);
  }
}

template <typename T> void World::emplaceOrReplace(EntityKey key, T value) {
  TypedStore<T> &store = storeToAdd<T>(key);
  const auto found = store.Items.find(key);
  if (found != store.Items.end()) {
    found->second = std::move(value);
  } else {
    store.add(key, std::move(value));
  }
}

template <typename T> T &World::getOrEmplace(EntityKey key) {
  static_assert(!std::is_empty_v<T>, "getOrEmplace needs a type that holds a value");
  TypedStore<T> &store = storeToAdd<T>(key);
  const auto found = store.Items.find(key);
  return found != store.Items.end() ? found->second : store.add(key);
}

template <typename T> void World::remove(EntityKey key) {
  if (TypedStore<T> *store = findStore<T>()) {
    store->erase(key);
  }
}

template <typename T> std::vector<EntityKey> World::keysWith() const {
  const TypedStore<T> *store = findStore<T>();
  return store == nullptr ? std::vector<EntityKey>{} : store->keys();
}

template <typename T> void World::keysWith(std::vector<EntityKey> &keys) const {
  keys.clear();
  if (const TypedStore<T> *store = findStore<T>()) {
    for (const auto &entry : store->Items) {
      keys.push_back(entry.first);
    }
  }
}

template <typename T, typename Visit> void World::forEachWith(Visit &&visit) const {
  if (const TypedStore<T> *store = findStore<T>()) {
    for (const auto &[key, component] : store->Items) {
      visit(key, component);
    }
  }
}

// A world equal to this one, sharing its schema, and independent of it from then on.
World copyWorld(const World &world);
// True when the walk emits the same words for both worlds.
bool worldsEqual(const World &left, const World &right);
// The walk's words folded into 64 bits.
uint64_t hashWorld(const World &world);
// Throws WorldInvariantError if the walk cannot cover the world fully.
void validateWorld(const World &world);

// Calls every registered resolver once, in registration order, and clears the pending resolution.
void resolveWorld(World &world);
// One cycle, passing the buffers to every system and swap: resolve if pending, step the systems,
// advance Tick, run the swaps, apply the queued commands in submission order, and resolve if
// pending. Empties the queue. Throws std::invalid_argument, changing nothing, if a queued command's
// type is not registered.
void stepWorld(World &world, CommandQueue &commands, TickBuffers &buffers);
// One cycle with no commands.
void stepWorld(World &world, TickBuffers &buffers);
// One cycle with buffers of its own, for callers that step once.
void stepWorld(World &world, CommandQueue &commands);
// One cycle with no commands and buffers of its own, for callers that step once.
void stepWorld(World &world);
// A copy of the world with the commands applied and resolved, not stepped. The world is unchanged.
// Throws std::invalid_argument if a command's type is not registered.
World makeCandidate(const World &world, const CommandQueue &commands);
// Applies the queued commands in submission order and resolves, without stepping the systems,
// advancing Tick, or running the swaps, as makeCandidate does to its copy. Empties the queue.
// Changes nothing when the queue is empty and no resolution is pending. Throws
// std::invalid_argument, changing nothing, if a queued command's type is not registered.
void commitCommands(World &world, CommandQueue &commands);

} // namespace tpj

#endif
