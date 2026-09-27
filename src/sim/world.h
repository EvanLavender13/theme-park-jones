#ifndef TPJ_SIM_WORLD_H
#define TPJ_SIM_WORLD_H

#include "sim/command_queue.h"
#include "sim/entity_key.h"
#include "sim/schema.h"

#include <entt/entity/registry.hpp>

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>
#include <unordered_map>
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

// A world the walk cannot cover fully: an unregistered component, an entity without a key, a NaN
// in registered state, or two derived origins sharing a key. Also thrown when a world is created
// under a floating-point environment that would change the simulation's arithmetic.
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
  // Create and destroy entities through the functions below, never directly on the registry.
  entt::registry Registry;

  [[nodiscard]] const WorldSchema &schema() const { return *Schema; }
  [[nodiscard]] uint64_t nextKey() const { return NextKey; }
  // True from construction until the first resolution, and from each applied command until the
  // next.
  [[nodiscard]] bool isResolvePending() const { return ResolvePending; }
  // True only while the resolvers run.
  [[nodiscard]] bool isResolving() const { return Resolving; }

  // A new entity keyed from the counter.
  EntityKey createEntity();
  // The entity keyed deriveKey(owner, purpose, index), created if no entity holds that key.
  EntityKey createDerivedEntity(EntityKey owner, uint64_t purpose, uint64_t index);
  // Removes the entity and its components. False, with no change, when the key is not live.
  bool destroyEntity(EntityKey key);
  [[nodiscard]] entt::entity findEntity(EntityKey key) const;
  [[nodiscard]] EntityKey keyOf(entt::entity entity) const;
  // Live keys, ascending.
  [[nodiscard]] std::vector<EntityKey> keys() const;

private:
  struct DerivedOrigin {
    EntityKey Owner = NULL_KEY;
    uint64_t Purpose = 0;
    uint64_t Index = 0;
    bool operator==(const DerivedOrigin &) const = default;
  };
  struct EntityRecord {
    entt::entity Entity = entt::null;
    std::optional<DerivedOrigin> Origin;
  };

  entt::entity addEntity(EntityKey key);
  void emitWords(WordSink &sink) const;

  std::shared_ptr<const WorldSchema> Schema;
  uint64_t NextKey = 1;
  std::map<EntityKey, EntityRecord> ByKey;
  std::unordered_map<entt::entity, EntityKey> KeyByEntity;
  bool ResolvePending = true;
  // Set only while resolvers run, so that createEntity can refuse them in debug builds.
  bool Resolving = false;

  friend World copyWorld(const World &world);
  friend bool worldsEqual(const World &left, const World &right);
  friend uint64_t hashWorld(const World &world);
  friend void validateWorld(const World &world);
  friend void resolveWorld(World &world);
  friend void stepWorld(World &world, CommandQueue &commands);
  friend World makeCandidate(const World &world, const CommandQueue &commands);
  friend std::string saveWorld(const World &world);
  friend World loadWorld(std::shared_ptr<const WorldSchema> schema, std::string_view text);
};

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
// One cycle: resolve if pending, step the systems, advance Tick, run the swaps, apply the queued
// commands in submission order, and resolve if pending. Empties the queue. Throws
// std::invalid_argument, changing nothing, if a queued command's type is not registered.
void stepWorld(World &world, CommandQueue &commands);
// One cycle with no commands.
void stepWorld(World &world);
// A copy of the world with the commands applied and resolved, not stepped. The world is unchanged.
// Throws std::invalid_argument if a command's type is not registered.
World makeCandidate(const World &world, const CommandQueue &commands);

} // namespace tpj

#endif
