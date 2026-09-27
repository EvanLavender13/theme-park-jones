# Implementation Plan: Registered Walk

## Goal

Give tpj_sim stable entity keys, a schema of registered component types described by visitFields, and copy, equality, and a state hash over one walk, with debug checks for anything the walk cannot cover.

## Approach

World becomes a move-only class holding the EnTT registry, the seed, the key counter, and two maps: key to entity record (ordered, which gives the walk its key order) and entity to key. A WorldSchema holds type-erased functions that each owning module instantiates from its visitFields, so the walk never names a component type. Every world operation is defined by one word stream: equality compares the streams of two worlds, and the hash folds a world's stream through SplitMix64's finalizer.

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Apply the text in FEATURE.md's "Spec changes" section exactly: replace the skeleton sentence in the first paragraph, and insert the three new paragraphs after the Contract's second paragraph (the one about stepWorld and SIM_TICK_SECONDS).

### Task 2: Add the mixing function and name hash

Files:
- Create: `src/sim/mix.h`

Step 1: Create the header, with its include guard `TPJ_SIM_MIX_H`, including `<stdint.h>` and `<string_view>`, in namespace tpj. These are complete definitions, not stubs:

```cpp
// SplitMix64's finalizer (Steele, Lea, and Flood, with the constants of Vigna's splitmix64.c): a
// bijection on 64-bit words whose output depends on every input bit.
constexpr uint64_t mix64(uint64_t x) {
  x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31U);
}

// Folds a sequence of 64-bit words into one, for the state hash, derived keys, and names.
class Hasher {
public:
  constexpr void add(uint64_t word) { State = mix64((State ^ word) + GAMMA); }
  [[nodiscard]] constexpr uint64_t value() const { return State; }

private:
  static constexpr uint64_t GAMMA = 0x9e3779b97f4a7c15ULL;
  uint64_t State = 0x6a09e667f3bcc909ULL;
};

// A stable 64-bit identifier for a name, the same on every build.
constexpr uint64_t hashName(std::string_view name) {
  Hasher hasher;
  for (const char ch : name) {
    hasher.add(static_cast<unsigned char>(ch));
  }
  hasher.add(name.size());
  return hasher.value();
}
```

### Task 3: Add entity keys

Files:
- Create: `src/sim/entity_key.h`

Step 1: Create the header, with its include guard `TPJ_SIM_ENTITY_KEY_H`, including `"sim/mix.h"` and `<stdint.h>`, in namespace tpj. These are complete definitions:

```cpp
// An entity's identity: stable across copies, saves, and loads, unlike EnTT's recycled handles.
// Components refer to other entities by key.
enum class EntityKey : uint64_t {};

constexpr EntityKey NULL_KEY{0};

// Keys from a world's counter lie below this bit. Keys derived by resolvers have it set.
constexpr uint64_t DERIVED_KEY_BIT = uint64_t{1} << 63U;

constexpr bool isDerivedKey(EntityKey key) {
  return (static_cast<uint64_t>(key) & DERIVED_KEY_BIT) != 0;
}

// The key a resolver gives an entity it derives from an owner, for a purpose (usually a hashName)
// and an index. It depends only on its arguments, so resolving the same intent again, in a copy or
// after a load, gives the same key.
constexpr EntityKey deriveKey(EntityKey owner, uint64_t purpose, uint64_t index) {
  Hasher hasher;
  hasher.add(static_cast<uint64_t>(owner));
  hasher.add(purpose);
  hasher.add(index);
  return EntityKey{DERIVED_KEY_BIT | (hasher.value() >> 1U)};
}
```

### Task 4: Declare the schema

Files:
- Create: `src/sim/schema.h`
- Create: `src/sim/schema.cpp`

Step 1: Create `src/sim/schema.h`, with its include guard `TPJ_SIM_SCHEMA_H`, including `"sim/entity_key.h"`, `<entt/entity/registry.hpp>`, `<bit>`, `<stdint.h>`, `<string>`, `<string_view>`, `<type_traits>`, `<utility>`, and `<vector>`, in namespace tpj. Declare, in this order:

```cpp
// What a registered component is to the save (principle 1): intent the player authored, state the
// simulation changes, or data derived from those, which is never saved.
enum class DataKind { Intent, State, Derived };

// Receives a component's fields as 64-bit words, in the order its visitFields lists them.
class WordSink {
public:
  WordSink() = default;
  WordSink(const WordSink &) = delete;
  WordSink &operator=(const WordSink &) = delete;
  WordSink(WordSink &&) = delete;
  WordSink &operator=(WordSink &&) = delete;
  virtual ~WordSink() = default;

  virtual void word(uint64_t value) = 0;
  // Called with each double field, just before its bits are passed to word.
  virtual void real(std::string_view field, double value) = 0;
};

template <typename Field> void emitField(WordSink &sink, std::string_view name, Field &value);

// The visitor the walk passes to a component's visitFields.
class FieldEmitter {
public:
  explicit FieldEmitter(WordSink &sink) : Sink(sink) {}
  template <typename Field> void field(std::string_view name, Field &value) {
    emitField(Sink, name, value);
  }

private:
  WordSink &Sink;
};

// A type lists its fields with a function found by argument-dependent lookup,
//   template <typename Visitor> void visitFields(Visitor &visitor, Shop &shop);
// calling visitor.field("stock", shop.Stock) once per field, in a fixed order.
template <typename T>
concept HasVisitFields = requires(FieldEmitter &emitter, T &value) { visitFields(emitter, value); };

template <typename T> struct IsVector : std::false_type {};
template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename> inline constexpr bool UNSUPPORTED_FIELD = false;

// A registered component type, handled opaquely through functions its owning module instantiates
// (principle 6).
struct ComponentType {
  std::string Name;
  DataKind Kind = DataKind::State;
  entt::id_type TypeId = 0;
  bool (*Has)(const entt::registry &registry, entt::entity entity) = nullptr;
  void (*Copy)(const entt::registry &from, entt::entity source, entt::registry &to,
               entt::entity target) = nullptr;
  void (*Emit)(const entt::registry &registry, entt::entity entity, WordSink &sink) = nullptr;
};

// The component types a world may hold, in registration order. Built once by explicit calls in a
// written order, never by static self-registration, then shared by worlds as a const value.
class WorldSchema {
public:
  // Registers T. The name is lowercase letters, digits, and hyphens. Throws std::invalid_argument
  // for a malformed or repeated name, or for a type already registered.
  template <typename T> void addComponent(std::string_view name, DataKind kind);

  [[nodiscard]] const std::vector<ComponentType> &components() const { return Components; }
  [[nodiscard]] const ComponentType *findComponent(entt::id_type typeId) const;
  // True when both list the same names, kinds, and types in the same order.
  [[nodiscard]] bool sameComponents(const WorldSchema &other) const;

private:
  void addComponentType(ComponentType type);

  std::vector<ComponentType> Components;
};
```

Step 2: Below those declarations, add stub template definitions: `emitField` with an empty body and its parameters marked `[[maybe_unused]]`, and `WorldSchema::addComponent<T>` whose body is `addComponentType(ComponentType{std::string(name), kind, entt::type_id<T>().hash()});`.

Step 3: Create `src/sim/schema.cpp` including `"sim/schema.h"`, with stubs: `addComponentType` pushes the type onto `Components`, `findComponent` returns nullptr, and `sameComponents` returns true.

### Task 5: Declare the world as a value

Files:
- Modify: `src/sim/world.h`
- Modify: `src/sim/world.cpp`
- Create: `src/sim/walk.cpp`

Step 1: Replace `src/sim/world.h` with the declarations below, keeping the guard `TPJ_SIM_WORLD_H`. It includes `"sim/entity_key.h"`, `"sim/schema.h"`, `<entt/entity/registry.hpp>`, `<map>`, `<memory>`, `<optional>`, `<stdexcept>`, `<stdint.h>`, `<unordered_map>`, and `<vector>`. Keep `SIM_TICK_SECONDS` and the `stepWorld` declaration as they are.

```cpp
// Debug builds check a world's invariants before walking it (see validateWorld).
#ifdef NDEBUG
constexpr bool WORLD_CHECKS = false;
#else
constexpr bool WORLD_CHECKS = true;
#endif

// A world the walk cannot cover fully: an unregistered component, an entity without a key, a NaN
// in registered state, or two derived origins sharing a key.
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

  friend World copyWorld(const World &world);
  friend bool worldsEqual(const World &left, const World &right);
  friend uint64_t hashWorld(const World &world);
  friend void validateWorld(const World &world);
};

// A world equal to this one, sharing its schema, and independent of it from then on.
World copyWorld(const World &world);
// True when the walk emits the same words for both worlds.
bool worldsEqual(const World &left, const World &right);
// The walk's words folded into 64 bits.
uint64_t hashWorld(const World &world);
// Throws WorldInvariantError if the walk cannot cover the world fully.
void validateWorld(const World &world);
```

Step 2: In `src/sim/world.cpp`, keep `stepWorld` and add stubs for every World member function: the constructors initialize members only, `createEntity` and `createDerivedEntity` return `NULL_KEY`, `destroyEntity` returns false, `findEntity` returns `entt::null`, `keyOf` returns `NULL_KEY`, `keys` returns an empty vector, and `addEntity` returns `entt::null`.

Step 3: Create `src/sim/walk.cpp` including `"sim/world.h"`, with stubs: `World::emitWords` does nothing, `copyWorld` returns `World(world.Schema, world.Seed)`, `worldsEqual` returns false, `hashWorld` returns 0, and `validateWorld` does nothing.

### Task 6: Build the new sources

Files:
- Modify: `src/sim/CMakeLists.txt:3-4`

Step 1: Change the source list to:

```cmake
add_library(tpj_sim STATIC
    schema.cpp
    walk.cpp
    world.cpp)
```

Step 2: Build.

Run: `cmake --build --preset linux-debug`
Expected: the build succeeds with no warnings. src/app/main.cpp still compiles unchanged, because `World` is default-constructible and keeps a public `Tick`.

Run: `ctest --preset linux-debug`
Expected: the placeholder test passes.

### Task 7: Test pass

Dispatch the test-writer agent through implementing-features' test pass template. Feature: this directory's FEATURE.md. Specs: `src/sim/SPEC.md`. Public headers: `src/sim/mix.h`, `src/sim/entity_key.h`, `src/sim/schema.h`, `src/sim/world.h`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build. The test that each step advances Tick by one passes, since stepWorld is unchanged. Tests of keys, the schema, copy, equality, hash, and validation fail against the stubs.

### Task 8: Emit fields and register types

Files:
- Modify: `src/sim/schema.h` (the stub template definitions from Task 4)
- Modify: `src/sim/schema.cpp`

Step 1: Replace the stub `emitField` with an `if constexpr` chain over the decayed Field type, checked in this order:

- bool: `sink.word(value ? 1U : 0U)`.
- double: `sink.real(name, value)`, then `sink.word(std::bit_cast<uint64_t>(value))`.
- EntityKey: `sink.word(static_cast<uint64_t>(value))`. This case comes before enums, because EntityKey is an enum.
- Other enums: convert to the underlying type in a local variable and call `emitField` on it.
- Signed integers: `sink.word(static_cast<uint64_t>(static_cast<int64_t>(value)))`.
- Unsigned integers: `sink.word(static_cast<uint64_t>(value))`.
- `IsVector<Field>::value`: `sink.word(value.size())`, then `emitField(sink, name, element)` for each element, taken as `auto &&element`. A `std::vector<bool>` element is a proxy type, so it falls through to the static_assert.
- `HasVisitFields<Field>`: construct a `FieldEmitter` on the sink and call `visitFields(emitter, value)`.
- Otherwise: `static_assert(UNSUPPORTED_FIELD<Field>, "unsupported field type; see emitField in sim/schema.h")`.

Step 2: Replace the stub `addComponent<T>`. Start with `static_assert(std::is_copy_constructible_v<T>, ...)` and `static_assert(std::is_empty_v<T> || HasVisitFields<T>, ...)`. Then fill a ComponentType with the name, the kind, `entt::type_id<T>().hash()`, and three captureless lambdas:

- Has returns `registry.all_of<T>(entity)`.
- Copy emplaces a copy with `to.emplace<T>(target, from.get<T>(source))`. For an empty T it uses `to.emplace<T>(target)`, because EnTT stores no instance of empty types.
- Emit does nothing for an empty T. Otherwise it constructs a `FieldEmitter` on the sink and calls `visitFields(emitter, const_cast<T &>(registry.get<T>(entity)))`, with a comment that visitFields takes a mutable reference so that one function can also decode, and that emitting only reads.

End with `addComponentType(std::move(type))`.

Step 3: In `src/sim/schema.cpp`, implement the three functions:

- `addComponentType` throws `std::invalid_argument` when the name is empty or has any character outside `a`–`z`, `0`–`9`, and `-`. It also throws when an existing entry has the same Name, or the same TypeId. Each message quotes the name, and the TypeId message also quotes the existing entry's name. Nothing is added before every check has passed.
- `findComponent` returns a pointer to the entry with that TypeId, or nullptr.
- `sameComponents` is `std::equal` over both vectors, comparing Name, Kind, and TypeId.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the schema tests (criterion 4) pass.

### Task 9: Keys

Files:
- Modify: `src/sim/world.cpp`

Step 1: Implement the constructors. `World()` delegates to `World(std::make_shared<const WorldSchema>(), 0)`. The two-argument constructor stores the schema and seed, and throws `std::invalid_argument("a world needs a schema")` when the schema pointer is null.

Step 2: Implement the key functions:

- `addEntity(key)` calls `Registry.create()`, records `ByKey[key] = {entity, std::nullopt}` and `KeyByEntity[entity] = key`, and returns the entity.
- `createEntity()` throws `WorldInvariantError("the entity key counter is exhausted")` if `NextKey >= DERIVED_KEY_BIT`. Otherwise it takes `EntityKey{NextKey}`, increments NextKey, calls addEntity, and returns the key.
- `createDerivedEntity(owner, purpose, index)` computes `key = deriveKey(owner, purpose, index)` and `origin = {owner, purpose, index}`. If no record holds the key, it calls addEntity and sets the record's Origin. If a record holds it with no Origin, it sets the Origin. That case is an entity text-saves will recreate from a save before resolution. If the record holds a different Origin and `WORLD_CHECKS` is true, it throws `WorldInvariantError` with the message "derived key <key in decimal> is shared by two origins". It returns the key in every case.
- `destroyEntity(key)` returns false if the key is not in ByKey. Otherwise it erases both map entries, calls `Registry.destroy(entity)`, and returns true.
- `findEntity` looks the key up in ByKey, and returns `entt::null` when it is absent. `keyOf` looks the entity up in KeyByEntity, and returns `NULL_KEY` when it is absent. `keys()` collects ByKey's keys in map order.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the key tests (criteria 1 to 3) pass.

### Task 10: Validate

Files:
- Modify: `src/sim/walk.cpp`

Step 1: In an anonymous namespace, add `NanFinder`, a final WordSink. Its `word` ignores its argument. Its `real` records the first field name whose value is NaN, tested with `std::isnan`, which GCC compiles to a comparison with no library call.

Step 2: Implement `validateWorld` with four checks, in this order:

1. For each `(id, storage)` in `world.Registry.storage()`: if the storage is not empty and `world.Schema->findComponent(storage.info().hash())` is nullptr, throw `WorldInvariantError("component type <storage.info().name()> is not registered with the world's schema")`.
2. For each `[entity]` in `world.Registry.storage<entt::entity>()->each()`: if KeyByEntity has no entry for it, throw `WorldInvariantError("an entity was created outside World::createEntity")`.
3. For each record in ByKey: if `!world.Registry.valid(record.Entity)`, throw `WorldInvariantError("entity <key> was destroyed outside World::destroyEntity")`.
4. For each registered type, and each record in ByKey whose entity the type's Has reports: run Emit into a NanFinder. If it found a NaN, throw `WorldInvariantError("NaN in field '<field>' of component '<type name>' on entity <key>")`.

Keys appear in messages in decimal.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the validateWorld tests (criterion 8) pass. The test that copy, equality, and hash validate first (criterion 9) still fails until Tasks 11 and 12.

### Task 11: The word stream, equality, and hash

Files:
- Modify: `src/sim/walk.cpp`

Step 1: Implement `World::emitWords(sink)` to emit, in this order:

1. Tick, Seed, and NextKey.
2. `ByKey.size()`, then for each record in key order: the key, then `1, owner, purpose, index` if it has an Origin, or `0` if not.
3. For each ComponentType in registration order: `hashName(type.Name)`, `static_cast<uint64_t>(type.Kind)`, and the number of records whose entity has the type. Then, for each such record in key order, the key followed by `type.Emit(Registry, record.Entity, sink)`.

Step 2: In the anonymous namespace, add two final WordSinks with empty `real`. `WordList` appends each word to a `std::vector<uint64_t>`. `WordHash` adds each word to a Hasher.

Step 3: Implement `worldsEqual(left, right)`. If `WORLD_CHECKS` is true, validate both worlds first. Return false if `!left.Schema->sameComponents(*right.Schema)`. Otherwise emit both worlds into WordLists and compare the vectors.

Step 4: Implement `hashWorld(world)`. Validate it if `WORLD_CHECKS` is true, emit it into a WordHash, and return the Hasher's value.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the equality, hash, storage-order, and stepping tests (criteria 6, 7, and 10) pass. The copy tests and criterion 9's copy section still fail.

### Task 12: Copy

Files:
- Modify: `src/sim/walk.cpp`

Step 1: Implement `copyWorld(world)`:

1. If `WORLD_CHECKS` is true, validate the world.
2. Construct `World copy(world.Schema, world.Seed)`, and set its Tick and NextKey from the original.
3. For each record in key order, call `copy.addEntity(key)` and copy the record's Origin.
4. For each ComponentType in registration order, and each record in key order whose entity the type's Has reports, call `type.Copy(world.Registry, record.Entity, copy.Registry, copy.ByKey.at(key).Entity)`.
5. Return the copy.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: every test passes, and the build has no warnings.

### Task 13: Verify on both builds

Step 1: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds, which shows the app still compiles with the new World.

### Task 14: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Sim: Make the world a value with keys and a registered walk`.
