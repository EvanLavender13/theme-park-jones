# Implementation Plan: Tick Cycle

## Goal

Give tpj_sim the fixed tick cycle, with registered systems, swap functions, resolvers, and command types, a command queue outside the world, pending resolution, and candidates made by copy, apply, and resolve.

## Approach

WorldSchema, which every world already shares, also holds the cycle's function pointers and command types, so a world cannot be cycled with rules other than its own. Commands are held in std::any beside their type id in a CommandQueue that the caller owns, and each is applied through a captureless function that calls the owning module's applyCommand. The world gains a pending-resolution flag, which the walk covers, and a resolving flag, which only the debug check in createEntity reads.

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Apply the text in FEATURE.md's "Spec changes" section exactly. Replace the sentence "Each call to stepWorld increments World::Tick by one." with the cycle sentences. Append the schema sentence to the end of the paragraph that begins "Component types are registered with a WorldSchema". Insert the three new paragraphs after the paragraph that begins "The walk visits registered types".

### Task 2: Add the command queue

Files:
- Create: `src/sim/command_queue.h`

Step 1: Create the header, with include guard `TPJ_SIM_COMMAND_QUEUE_H`, including `<entt/core/type_info.hpp>`, `<any>`, `<string_view>`, `<type_traits>`, `<utility>`, and `<vector>`, in namespace tpj. These are complete definitions, not stubs:

```cpp
// A command waiting for the next cycle, with its type's id and name.
struct QueuedCommand {
  entt::id_type TypeId = 0;
  std::string_view TypeName;
  std::any Value;
};

// Commands submitted since the last cycle, in submission order. It lives outside the world, so
// copies, hashes, and saves never hold pending commands.
class CommandQueue {
public:
  template <typename T> void push(T command) {
    static_assert(std::is_copy_constructible_v<T>, "a command must be copyable");
    Commands.push_back(
        QueuedCommand{entt::type_id<T>().hash(), entt::type_id<T>().name(), std::move(command)});
  }

  [[nodiscard]] const std::vector<QueuedCommand> &commands() const { return Commands; }
  [[nodiscard]] bool empty() const { return Commands.empty(); }
  void clear() { Commands.clear(); }

private:
  std::vector<QueuedCommand> Commands;
};
```

### Task 3: Declare the cycle's registration

Files:
- Modify: `src/sim/schema.h`
- Modify: `src/sim/schema.cpp`

Step 1: In `src/sim/schema.h`, add `#include <any>` to the system includes. After `namespace tpj {`, add `class World;`. After the `ComponentType` struct, add:

```cpp
// A system, swap function, or resolver. It takes only the world, so it can hold no state outside
// it (principle 10).
using WorldFunction = void (*)(World &world);

struct ResolverType {
  std::string Name;
  WorldFunction Resolve = nullptr;
  std::vector<std::string> Dependencies;
};

// A registered command type. Apply calls the owning module's applyCommand on a queued value.
struct CommandType {
  entt::id_type TypeId = 0;
  std::string_view TypeName;
  void (*Apply)(World &world, const std::any &command) = nullptr;
};

// A command type names the function that applies it, found by argument-dependent lookup,
//   void applyCommand(World &world, const PlacePath &command);
template <typename T>
concept HasApplyCommand = requires(World &world, const T &command) {
  applyCommand(world, command);
};
```

Step 2: In `WorldSchema`'s public section, after `addComponent`, add:

```cpp
  // Every cycle steps the systems, and runs the swap functions, in registration order.
  void addSystem(WorldFunction step);
  void addSwap(WorldFunction swap);
  // Registers a resolver after the resolvers it depends on. Throws std::invalid_argument for a
  // malformed or repeated name, or for a dependency that is not an already registered resolver.
  void addResolver(std::string_view name, WorldFunction resolve,
                   std::vector<std::string> dependencies = {});
  // Registers T as a command type. Throws std::invalid_argument if T is already registered.
  template <typename T> void addCommand();

  [[nodiscard]] const std::vector<WorldFunction> &systems() const { return Systems; }
  [[nodiscard]] const std::vector<WorldFunction> &swaps() const { return Swaps; }
  [[nodiscard]] const std::vector<ResolverType> &resolvers() const { return Resolvers; }
  [[nodiscard]] const std::vector<CommandType> &commands() const { return Commands; }
  [[nodiscard]] const CommandType *findCommand(entt::id_type typeId) const;
```

In the private section, add `void addCommandType(CommandType type);` after `addComponentType`, and these members after `Components`:

```cpp
  std::vector<WorldFunction> Systems;
  std::vector<WorldFunction> Swaps;
  std::vector<ResolverType> Resolvers;
  std::vector<CommandType> Commands;
```

Step 3: At the end of the header, before the namespace closes, add the stub:

```cpp
template <typename T> void WorldSchema::addCommand() {}
```

Step 4: In `src/sim/schema.cpp`, add stub definitions after `sameComponents`:

```cpp
void WorldSchema::addSystem(WorldFunction) {}

void WorldSchema::addSwap(WorldFunction) {}

void WorldSchema::addResolver(std::string_view, WorldFunction, std::vector<std::string>) {}

void WorldSchema::addCommandType(CommandType) {}

const CommandType *WorldSchema::findCommand(entt::id_type) const { return nullptr; }
```

If clang-tidy flags a stub as convertible to static, add `// NOLINT(readability-convert-member-functions-to-static)` to that line; Task 7 removes these.

### Task 4: Declare the cycle on the world

Files:
- Modify: `src/sim/world.h`
- Modify: `src/sim/world.cpp:3,82-85`
- Create: `src/sim/cycle.cpp`

Step 1: In `src/sim/world.h`, add `#include "sim/command_queue.h"` before `#include "sim/entity_key.h"`. In `World`'s public section, after `nextKey()`, add:

```cpp
  // True from construction until the first resolution, and from each applied command until the
  // next.
  [[nodiscard]] bool isResolvePending() const { return ResolvePending; }
```

In the private section, after `KeyByEntity`, add:

```cpp
  bool ResolvePending = true;
  // Set only while resolvers run, so that createEntity can refuse them in debug builds.
  bool Resolving = false;
```

After the existing friend declarations, add:

```cpp
  friend void resolveWorld(World &world);
  friend void stepWorld(World &world, CommandQueue &commands);
  friend World makeCandidate(const World &world, const CommandQueue &commands);
```

Step 2: Replace the declaration `void stepWorld(World &world);` at the end of the header with:

```cpp
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
```

Step 3: In `src/sim/world.cpp`, delete `stepWorld` (lines 82 to 85) and the `#include "core/profile.h"` line.

Step 4: Create `src/sim/cycle.cpp` with stubs:

```cpp
#include "sim/world.h"

#include "core/profile.h"

namespace tpj {

void resolveWorld(World &) {}

void stepWorld(World &world, CommandQueue &) {
  TPJ_PROFILE_ZONE();
  ++world.Tick;
}

void stepWorld(World &world) {
  CommandQueue none;
  stepWorld(world, none);
}

World makeCandidate(const World &world, const CommandQueue &) { return copyWorld(world); }

} // namespace tpj
```

### Task 5: Build the new source

Files:
- Modify: `src/sim/CMakeLists.txt:3-6`

Step 1: Change the source list to:

```cmake
add_library(tpj_sim STATIC
    cycle.cpp
    schema.cpp
    walk.cpp
    world.cpp)
```

Step 2: Build and test.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the build succeeds with no warnings, and the 33 existing tests pass. src/app/main.cpp compiles unchanged, because `stepWorld(world)` still exists.

### Task 6: Test pass

Dispatch the test-writer agent through implementing-features' test pass template. Feature: this directory's FEATURE.md. Specs: `src/sim/SPEC.md`. Public headers: `src/sim/command_queue.h`, `src/sim/schema.h`, `src/sim/world.h`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build. The 33 existing tests pass. The new tests of the cycle order, resolution, registration, candidates, and the resolver check fail against the stubs.

### Task 7: Registration

Files:
- Modify: `src/sim/schema.h` (the stub `addCommand` from Task 3)
- Modify: `src/sim/schema.cpp` (the stubs from Task 3)

Step 1: Replace the stub `addCommand<T>` with:

```cpp
template <typename T> void WorldSchema::addCommand() {
  static_assert(std::is_copy_constructible_v<T>, "a command must be copyable");
  static_assert(HasApplyCommand<T>, "a command type needs an applyCommand function");
  CommandType type;
  type.TypeId = entt::type_id<T>().hash();
  type.TypeName = entt::type_id<T>().name();
  type.Apply = [](World &world, const std::any &command) {
    applyCommand(world, std::any_cast<const T &>(command));
  };
  addCommandType(type);
}
```

Step 2: In `src/sim/schema.cpp`, replace the stubs, and remove any NOLINT added in Task 3:

- `addSystem` appends to Systems, and `addSwap` appends to Swaps.
- `addResolver` builds `ResolverType{std::string(name), resolve, std::move(dependencies)}`. It throws `std::invalid_argument("resolver name '<name>' must be lowercase letters, digits, and hyphens")` when `isValidName` fails, and `std::invalid_argument("resolver name '<name>' is already registered")` when an entry in Resolvers has the same Name. For each dependency with no entry of that Name in Resolvers, it throws `std::invalid_argument("resolver '<name>' depends on '<dependency>', which is not an already registered resolver")`. It appends only after every check has passed.
- `addCommandType` throws `std::invalid_argument("command type <TypeName> is already registered")` when `findCommand(type.TypeId)` is not nullptr, and appends otherwise.
- `findCommand` is `std::find_if` over Commands by TypeId, returning a pointer to the entry or nullptr, like `findComponent`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the registration tests (criterion 4) pass.

### Task 8: Resolution and the resolver check

Files:
- Modify: `src/sim/cycle.cpp`
- Modify: `src/sim/world.cpp` (`createEntity`)

Step 1: Replace the stub `resolveWorld` with:

```cpp
void resolveWorld(World &world) {
  TPJ_PROFILE_ZONE();
  world.Resolving = true;
  try {
    for (const ResolverType &resolver : world.schema().resolvers()) {
      resolver.Resolve(world);
    }
  } catch (...) {
    world.Resolving = false;
    throw;
  }
  world.Resolving = false;
  world.ResolvePending = false;
}
```

Step 2: In `World::createEntity`, before the counter check, add:

```cpp
  if (WORLD_CHECKS && Resolving) {
    throw WorldInvariantError("a resolver called createEntity; resolvers take derived keys");
  }
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the resolver check test (criterion 7) and the resolveWorld part of criterion 2 pass.

### Task 9: Pending resolution in the walk

Files:
- Modify: `src/sim/walk.cpp` (`World::emitWords` and `copyWorld`)

Step 1: In `World::emitWords`, after `sink.word(NextKey);`, add `sink.word(ResolvePending ? 1U : 0U);`.

Step 2: In `copyWorld`, after `copy.NextKey = world.NextKey;`, add `copy.ResolvePending = world.ResolvePending;`.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the test that the walk covers pending resolution (criterion 3) passes, and the 33 earlier tests still pass.

### Task 10: The cycle

Files:
- Modify: `src/sim/cycle.cpp`

Step 1: Add `#include <stdexcept>` and `#include <string>` to the system includes. Before `resolveWorld`, add an anonymous namespace with two helpers:

```cpp
namespace {

// Throws before anything changes when a command's type is not registered with the world's schema.
void requireRegistered(const World &world, const CommandQueue &commands) {
  for (const QueuedCommand &command : commands.commands()) {
    if (world.schema().findCommand(command.TypeId) == nullptr) {
      throw std::invalid_argument("command type " + std::string(command.TypeName) +
                                  " is not registered with the world's schema");
    }
  }
}

void applyCommands(World &world, const CommandQueue &commands) {
  for (const QueuedCommand &command : commands.commands()) {
    world.schema().findCommand(command.TypeId)->Apply(world, command.Value);
  }
}

} // namespace
```

applyCommands cannot set the private flag, so the cycle sets it.

Step 2: Replace the stub two-argument `stepWorld` with:

```cpp
void stepWorld(World &world, CommandQueue &commands) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  if (world.ResolvePending) {
    resolveWorld(world);
  }
  for (const WorldFunction step : world.schema().systems()) {
    step(world);
  }
  ++world.Tick;
  for (const WorldFunction swap : world.schema().swaps()) {
    swap(world);
  }
  if (!commands.empty()) {
    applyCommands(world, commands);
    world.ResolvePending = true;
    commands.clear();
  }
  if (world.ResolvePending) {
    resolveWorld(world);
  }
}
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the cycle order, resolution, and unregistered-command tests for stepWorld (criteria 1, 2, and 5's stepWorld part) pass.

### Task 11: Candidates

Files:
- Modify: `src/sim/cycle.cpp`

Step 1: Replace the stub `makeCandidate` with:

```cpp
World makeCandidate(const World &world, const CommandQueue &commands) {
  TPJ_PROFILE_ZONE();
  requireRegistered(world, commands);
  World candidate = copyWorld(world);
  if (!commands.empty()) {
    applyCommands(candidate, commands);
    candidate.ResolvePending = true;
  }
  if (candidate.ResolvePending) {
    resolveWorld(candidate);
  }
  return candidate;
}
```

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: every test passes.

### Task 12: Verify on both builds

Step 1: Run the full checks.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):" ; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

Run: `cmake.exe --build --preset windows-debug`
Expected: the build succeeds, which shows the app still compiles against the new world header.

### Task 13: Commit

Step 1: Commit the feature once through the commit-hygiene skill, with the subject `Sim: Add the tick cycle, commands, and candidates`.
