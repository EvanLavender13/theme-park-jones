# Implementation Plan: Flow Ledger

## Goal

Add the flow ledger to the medium: flow kinds registered with addFlow, create, send, and consume operations while stepping, a swap that delivers, returns, and settles removed endpoints, queries, and the stalls scenario that puts fields and flows in the cross-build check.

## Approach

Each kind gets a state component, FlowLedger<K>, which derives from a kind-independent Ledger holding packets, stocks, the created count, and consumption by cause, on an entity keyed flowKey(name) that the kind's resolver creates. Thin templates in src/sim/medium/flow.h find the ledger, check stepping, and call kind-independent functions in src/sim/medium/flow.cpp, which validate before changing anything, keep packets and stocks sorted by content, and do the swap. The stalls scenario, in src/scenarios/stalls.cpp, derives a one-carrier track network and has a depot, stalls, and visitors publish two fields and exchange goods and visits.

## Tasks

### Task 1: Update the sim spec

Files:
- Modify: `src/sim/SPEC.md`

Step 1: Make the edit in FEATURE.md's Spec changes for src/sim/SPEC.md, with its exact text.

### Task 2: Update the medium spec

Files:
- Modify: `src/sim/medium/SPEC.md`

Step 1: Make the two edits in FEATURE.md's Spec changes for src/sim/medium/SPEC.md, with their exact text.

### Task 3: Update the scenarios spec

Files:
- Modify: `src/scenarios/SPEC.md`

Step 1: Make the edit in FEATURE.md's Spec changes for src/scenarios/SPEC.md, with its exact text.

### Task 4: Declare the flow ledger

Files:
- Create: `src/sim/medium/flow.h`

Step 1: Create src/sim/medium/flow.h with the include guard TPJ_SIM_MEDIUM_FLOW_H, including sim/entity_key.h, sim/mix.h, sim/schema.h, sim/world.h, `<compare>`, `<concepts>`, `<stdint.h>`, `<string>`, `<string_view>`, and `<vector>`, and in namespace tpj:

```cpp
// A flow kind's definition, a type its owning module supplies:
//   struct Meals {
//     static constexpr std::string_view Name = "meals";
//   };
template <typename K>
concept FlowDefinition = requires {
  { K::Name } -> std::convertible_to<std::string_view>;
};

inline constexpr uint64_t FLOW_PURPOSE = hashName("flow");

// The key of the entity holding the named kind's ledger.
constexpr EntityKey flowKey(std::string_view name) {
  return deriveKey(NULL_KEY, FLOW_PURPOSE, hashName(name));
}

// The causes the ledger itself consumes units with.
inline constexpr std::string_view UNDELIVERABLE_CAUSE = "undeliverable";
inline constexpr std::string_view DISCARDED_CAUSE = "discarded";

// Units in transit from one endpoint to another. Packets are ordered by their fields, in the
// order they are declared.
struct FlowPacket {
  uint64_t Arrival = 0;
  EntityKey From = NULL_KEY;
  EntityKey To = NULL_KEY;
  EntityKey Handle = NULL_KEY;
  int64_t Units = 0;
  uint32_t Delay = 0;
  // Set on a packet going back to its sender, which is then its To.
  bool Returning = false;

  auto operator<=>(const FlowPacket &) const = default;
};
```

Implementation fix: clang-tidy's readability-implicit-bool-conversion rejects the defaulted <=> because it compares Returning as an int. FlowPacket instead defaults operator== and defines operator<=> out of line, comparing a tuple of the fields in order with Returning as 0 or 1.

```cpp
template <typename Visitor> void visitFields(Visitor &visitor, FlowPacket &packet) {
  visitor.field("arrival", packet.Arrival);
  visitor.field("from", packet.From);
  visitor.field("to", packet.To);
  visitor.field("handle", packet.Handle);
  visitor.field("units", packet.Units);
  visitor.field("delay", packet.Delay);
  visitor.field("returning", packet.Returning);
}

// The units an endpoint holds under one handle.
struct FlowStock {
  EntityKey Endpoint = NULL_KEY;
  EntityKey Handle = NULL_KEY;
  int64_t Units = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, FlowStock &stock) {
  visitor.field("endpoint", stock.Endpoint);
  visitor.field("handle", stock.Handle);
  visitor.field("units", stock.Units);
}

// The units consumed with one cause, held as the hashName of its name.
struct FlowConsumption {
  uint64_t Cause = 0;
  int64_t Units = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, FlowConsumption &consumption) {
  visitor.field("cause", consumption.Cause);
  visitor.field("units", consumption.Units);
}

// One kind's ledger. State. Packets in their order, stocks by endpoint and then handle with no
// empty entries, and consumption by cause.
struct Ledger {
  std::vector<FlowPacket> Packets;
  std::vector<FlowStock> Stocks;
  int64_t Created = 0;
  std::vector<FlowConsumption> Consumed;
};

template <typename Visitor> void visitFields(Visitor &visitor, Ledger &ledger) {
  visitor.field("packets", ledger.Packets);
  visitor.field("stocks", ledger.Stocks);
  visitor.field("created", ledger.Created);
  visitor.field("consumed", ledger.Consumed);
}

// The registered component holding kind K's ledger.
template <FlowDefinition K> struct FlowLedger : Ledger {};

template <typename Visitor, typename K> void visitFields(Visitor &visitor, FlowLedger<K> &ledger) {
  visitFields(visitor, static_cast<Ledger &>(ledger));
}

// A handle's units in an endpoint's stock.
struct FlowHolding {
  EntityKey Handle = NULL_KEY;
  int64_t Units = 0;
};
```

Step 2: After those, add the public templates as stubs:

```cpp
// Registers the kind's state component, <name>-ledger, its resolver, <name>-flow, and its swap.
// Throws std::invalid_argument when a name is malformed or already registered.
template <FlowDefinition K> void addFlow(WorldSchema & /*schema*/) {
  // Stub until implemented.
}

// Adds units under the handle to the endpoint's stock. Systems only. Throws as
// sim/medium/SPEC.md's Flows section says, changing nothing.
template <FlowDefinition K>
void createUnits(World & /*world*/, EntityKey /*endpoint*/, EntityKey /*handle*/,
                 int64_t /*units*/) {
  // Stub until implemented.
}

// Sends units of the handle from the endpoint's stock to the destination, arriving after the
// delay. Systems only. Throws as createUnits does.
template <FlowDefinition K>
void sendUnits(World & /*world*/, EntityKey /*from*/, EntityKey /*to*/, EntityKey /*handle*/,
               int64_t /*units*/, uint32_t /*delay*/) {
  // Stub until implemented.
}

// Consumes units of the handle from the endpoint's stock with the cause. Systems only. Throws as
// createUnits does.
template <FlowDefinition K>
void consumeUnits(World & /*world*/, EntityKey /*endpoint*/, EntityKey /*handle*/,
                  int64_t /*units*/, std::string_view /*cause*/) {
  // Stub until implemented.
}

// The units the endpoint holds under the handle.
template <FlowDefinition K>
int64_t unitsHeld(const World & /*world*/, EntityKey /*endpoint*/, EntityKey /*handle*/) {
  return 0; // Stub until implemented.
}

// The units held in every stock.
template <FlowDefinition K> int64_t unitsHeld(const World & /*world*/) {
  return 0; // Stub until implemented.
}

// The endpoint's holdings, handles ascending.
template <FlowDefinition K>
std::vector<FlowHolding> stockOf(const World & /*world*/, EntityKey /*endpoint*/) {
  return {}; // Stub until implemented.
}

template <FlowDefinition K> int64_t unitsInTransit(const World & /*world*/) {
  return 0; // Stub until implemented.
}

template <FlowDefinition K> int64_t unitsCreated(const World & /*world*/) {
  return 0; // Stub until implemented.
}

template <FlowDefinition K> int64_t unitsConsumed(const World & /*world*/) {
  return 0; // Stub until implemented.
}

// The units consumed with the cause, 0 for a cause never used.
template <FlowDefinition K>
int64_t unitsConsumed(const World & /*world*/, std::string_view /*cause*/) {
  return 0; // Stub until implemented.
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and all 195 existing tests pass. Nothing includes flow.h yet, so if clang-tidy does not check it, that is expected; the test pass includes it.

### Task 5: Declare the stalls scenario

Files:
- Create: `src/scenarios/stalls.h`
- Create: `src/scenarios/stalls.cpp`
- Modify: `src/scenarios/synthetic.h`
- Modify: `src/scenarios/scenarios.cpp`
- Modify: `src/scenarios/CMakeLists.txt`

Step 1: Create src/scenarios/stalls.h, guard TPJ_SCENARIOS_STALLS_H, including sim/medium/field.h and `<string_view>`, in namespace tpj:

```cpp
// The stalls scenario's fields and flow kinds, declared here so tests can audit its world.

// Each stall's offer: its distance along the track over 10 when resolved, and the goods it holds
// when stepped.
struct StallOffer {
  using Entry = double;
  static constexpr std::string_view Name = "stall-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// Crowding: 1 at each stall when resolved, and 1 at each visitor when stepped.
struct Crowd {
  using Entry = double;
  static constexpr std::string_view Name = "crowd";
  static constexpr FieldKind Kind = FieldKind::Scalar;
};

// Goods the depot makes and ships to stalls, which sell them.
struct Goods {
  static constexpr std::string_view Name = "goods";
};

// Visits a visitor sends to a stall, addressed to itself, and the stall sends back.
struct Visits {
  static constexpr std::string_view Name = "visits";
};
```

Step 2: In src/scenarios/synthetic.h, after `Scenario beaconsScenario();`, add `Scenario stallsScenario();`.

Step 3: Create src/scenarios/stalls.cpp including scenarios/stalls.h and scenarios/synthetic.h, defining the stub:

```cpp
Scenario stallsScenario() {
  return Scenario{
      .Name = "stalls",
      .Seed = 3003,
      .MakeSchema = []() -> std::shared_ptr<const WorldSchema> {
        return std::make_shared<WorldSchema>(); // Stub until implemented.
      },
      .Populate = [](World & /*world*/) {},
      .QueueCommands = nullptr,
  };
}
```

Step 4: In src/scenarios/scenarios.cpp, make the array `std::array<Scenario, 3>` and append `stallsScenario()` after `beaconsScenario()`.

Step 5: In src/scenarios/CMakeLists.txt, add `stalls.cpp` to tpj_scenarios_lib's sources after `scenarios.cpp`.

Step 6: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and all 195 existing tests pass.

### Task 6: Test pass

Step 1: Dispatch the test-writer agent for this feature, with FEATURE.md, src/sim/medium/SPEC.md, src/sim/SPEC.md, src/scenarios/SPEC.md, and the public headers src/sim/medium/flow.h, src/sim/medium/field.h, src/sim/world.h, src/scenarios/stalls.h, and src/scenarios/scenarios.h. It creates tests/sim/medium/flow_test.cpp, tests/sim/support/synthetic_flows.h, and a stalls test under tests/scenarios, and adds the new files to tests/sim/CMakeLists.txt and tests/scenarios/CMakeLists.txt.

Run: `cmake --build --preset linux-debug && ctest --preset linux-debug`
Expected: the tests build, and the 195 existing tests pass. The new tests of registration, operations, conservation, delivery, returns, removed endpoints, saves, order independence, and the stalls scenario fail against the stubs.

### Task 7: Implement the ledger's operations

Files:
- Modify: `src/sim/medium/flow.h`
- Create: `src/sim/medium/flow.cpp`
- Modify: `src/sim/CMakeLists.txt`

Step 1: In flow.h, before the public templates, declare the kind-independent functions:

```cpp
// The kind-independent work behind the templates below. The kind's name is for messages.
void ledgerCreate(const World &world, Ledger &ledger, std::string_view kind, EntityKey endpoint,
                  EntityKey handle, int64_t units);
void ledgerSend(const World &world, Ledger &ledger, std::string_view kind, EntityKey from,
                EntityKey to, EntityKey handle, int64_t units, uint32_t delay);
void ledgerConsume(const World &world, Ledger &ledger, std::string_view kind, EntityKey endpoint,
                   EntityKey handle, int64_t units, std::string_view cause);
// Delivers due packets, returns those whose destination is gone, and settles the stocks of
// endpoints that are gone, as sim/medium/SPEC.md says. World::Tick is the tick just reached.
void ledgerSwap(const World &world, Ledger &ledger);
[[nodiscard]] int64_t ledgerHeld(const Ledger &ledger, EntityKey endpoint, EntityKey handle);
```

and the helpers that find a kind's ledger:

```cpp
// The kind's ledger, or null when the world holds none.
template <FlowDefinition K> const Ledger *ledgerOf(const World &world) {
  const entt::entity entity = world.findEntity(flowKey(K::Name));
  return entity == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(entity);
}

// The kind's ledger for an operation. Throws std::logic_error when the world is not stepping or
// holds no ledger for the kind.
template <FlowDefinition K> Ledger &steppingLedger(World &world) {
  const std::string name(K::Name);
  if (!world.isStepping()) {
    throw std::logic_error("flow " + name + " takes operations only from systems");
  }
  const entt::entity entity = world.findEntity(flowKey(K::Name));
  auto *ledger = entity == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(entity);
  if (ledger == nullptr) {
    throw std::logic_error("flow " + name + " has no ledger; register it and resolve the world");
  }
  return *ledger;
}
```

Add `<stdexcept>` to flow.h's includes.

Step 2: Replace the bodies of createUnits, sendUnits, and consumeUnits, restoring their parameter names:

```cpp
  ledgerCreate(world, steppingLedger<K>(world), K::Name, endpoint, handle, units);
```

```cpp
  ledgerSend(world, steppingLedger<K>(world), K::Name, from, to, handle, units, delay);
```

```cpp
  ledgerConsume(world, steppingLedger<K>(world), K::Name, endpoint, handle, units, cause);
```

Step 3: Create src/sim/medium/flow.cpp, including sim/medium/flow.h, sim/field_text.h (for isValidName), `<algorithm>`, `<iterator>`, `<limits>`, `<stdexcept>`, `<string>`, `<utility>`, and `<vector>`. In an anonymous namespace inside namespace tpj:

- `bool isLive(const World &world, EntityKey key)`: `key != NULL_KEY && world.findEntity(key) != entt::null`.
- `std::string keyText(EntityKey key)`: `std::to_string(static_cast<uint64_t>(key))`.
- `std::vector<FlowStock>::iterator findStock(std::vector<FlowStock> &stocks, EntityKey endpoint, EntityKey handle)`: `std::ranges::lower_bound` over stocks for `std::pair{endpoint, handle}`, projecting each stock to `std::pair{stock.Endpoint, stock.Handle}`.
- `void addToStock(Ledger &ledger, EntityKey endpoint, EntityKey handle, int64_t units)`: at findStock's position, adds the units to an entry with that endpoint and handle, or inserts `FlowStock{endpoint, handle, units}` there.
- `void takeFromStock(Ledger &ledger, EntityKey endpoint, EntityKey handle, int64_t units)`: subtracts the units from the entry findStock finds, which the caller has checked holds enough, and erases it when it reaches 0.
- `void addConsumed(Ledger &ledger, uint64_t cause, int64_t units)`: lower_bound over Consumed by `&FlowConsumption::Cause`; adds to the matching entry or inserts `FlowConsumption{cause, units}` there.
- `void addPacket(Ledger &ledger, const FlowPacket &packet)`: `ledger.Packets.insert(std::ranges::upper_bound(ledger.Packets, packet), packet)`.
- `void requireEndpoint(const World &world, std::string_view kind, EntityKey endpoint, int64_t units)`: throws `std::invalid_argument` naming the kind, "flow <kind> takes no operation from the null key" for NULL_KEY, "flow <kind> takes no operation from entity <key>, which is not live" when not live, and "flow <kind> takes at least 1 unit" when units is below 1.
- `void requireHeld(const Ledger &ledger, std::string_view kind, EntityKey endpoint, EntityKey handle, int64_t units)`: throws `std::invalid_argument("entity <endpoint> holds fewer than <units> units of flow <kind> under handle <handle>")` when `ledgerHeld(ledger, endpoint, handle) < units`.

After the anonymous namespace, define:

- ledgerHeld: the Units of the entry with that endpoint and handle, found by lower_bound as findStock does over a const vector, or 0.
- ledgerCreate: requireEndpoint; throw `std::invalid_argument("flow <kind>'s created count would exceed the largest int64_t")` when `units > std::numeric_limits<int64_t>::max() - ledger.Created`; then addToStock and `ledger.Created += units`.
- ledgerSend: requireEndpoint for from; throw `std::invalid_argument` for `to == NULL_KEY` ("flow <kind> sends nothing to the null key") and for `delay == 0` ("flow <kind> takes a delay of at least 1 tick"); requireHeld for from and handle; then takeFromStock and addPacket of `FlowPacket{.Arrival = world.Tick + delay, .From = from, .To = to, .Handle = handle, .Units = units, .Delay = delay, .Returning = false}`.
- ledgerConsume: requireEndpoint; throw `std::invalid_argument("flow <kind> takes no malformed cause")` unless `isValidName(cause)`; requireHeld; then takeFromStock and `addConsumed(ledger, hashName(cause), units)`.

Every check runs before the first change, so an operation that throws changes nothing.

Step 4: In src/sim/CMakeLists.txt, add `medium/flow.cpp` to tpj_sim's sources before `medium/network.cpp`.

Step 5: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines. ledgerSwap is declared and not yet defined, which links because nothing calls it yet. The operation tests still fail, since addFlow registers nothing.

### Task 8: Register kinds and implement the swap

Files:
- Modify: `src/sim/medium/flow.h`
- Modify: `src/sim/medium/flow.cpp`

Step 1: In flow.h, before addFlow, add the kind's resolver and swap:

```cpp
// The kind's resolver: creates the entity holding its ledger, with an empty ledger if it has none.
template <FlowDefinition K> void ensureLedger(World &world) {
  const EntityKey key = world.createDerivedEntity(NULL_KEY, FLOW_PURPOSE, hashName(K::Name));
  static_cast<void>(world.Registry.get_or_emplace<FlowLedger<K>>(world.findEntity(key)));
}

// The kind's swap: delivers, returns, and settles, as ledgerSwap does.
template <FlowDefinition K> void swapLedger(World &world) {
  const entt::entity entity = world.findEntity(flowKey(K::Name));
  auto *ledger = entity == entt::null ? nullptr : world.Registry.try_get<FlowLedger<K>>(entity);
  if (ledger != nullptr) {
    ledgerSwap(world, *ledger);
  }
}
```

Replace addFlow's body, restoring its parameter name:

```cpp
  const std::string name(K::Name);
  schema.addComponent<FlowLedger<K>>(name + "-ledger", DataKind::State);
  schema.addResolver(name + "-flow", &ensureLedger<K>);
  schema.addSwap(&swapLedger<K>);
```

Step 2: In flow.cpp, define ledgerSwap:

```cpp
void ledgerSwap(const World &world, Ledger &ledger) {
  // Packets are ordered by arrival first, so the due ones lead.
  const auto due = std::ranges::find_if(
      ledger.Packets, [&world](const FlowPacket &packet) { return packet.Arrival > world.Tick; });
  const std::vector<FlowPacket> arriving(ledger.Packets.begin(), due);
  ledger.Packets.erase(ledger.Packets.begin(), due);
  for (const FlowPacket &packet : arriving) {
    if (isLive(world, packet.To)) {
      addToStock(ledger, packet.To, packet.Handle, packet.Units);
    } else if (!packet.Returning && isLive(world, packet.From)) {
      addPacket(ledger, FlowPacket{.Arrival = world.Tick + packet.Delay,
                                   .From = packet.To,
                                   .To = packet.From,
                                   .Handle = packet.Handle,
                                   .Units = packet.Units,
                                   .Delay = packet.Delay,
                                   .Returning = true});
    } else {
      addConsumed(ledger, hashName(UNDELIVERABLE_CAUSE), packet.Units);
    }
  }
  std::vector<FlowStock> orphaned;
  std::erase_if(ledger.Stocks, [&world, &orphaned](const FlowStock &stock) {
    if (isLive(world, stock.Endpoint)) {
      return false;
    }
    orphaned.push_back(stock);
    return true;
  });
  for (const FlowStock &stock : orphaned) {
    if (isLive(world, stock.Handle)) {
      addToStock(ledger, stock.Handle, stock.Handle, stock.Units);
    } else {
      addConsumed(ledger, hashName(DISCARDED_CAUSE), stock.Units);
    }
  }
}
```

Step 3: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines. Registration, operation, and throw tests pass; tests that read results through the queries still fail.

### Task 9: Implement the queries

Files:
- Modify: `src/sim/medium/flow.h`

Step 1: Replace each query stub's body, restoring its parameter names. Each starts with `const Ledger *ledger = ledgerOf<K>(world);` and gives 0, or `{}`, when it is null. Otherwise:

- unitsHeld of an endpoint and handle: `ledgerHeld(*ledger, endpoint, handle)`.
- unitsHeld of the world: the sum of every stock's Units.
- stockOf: for each stock whose Endpoint is the endpoint, in order, `FlowHolding{stock.Handle, stock.Units}`. Find the first with `std::ranges::lower_bound(ledger->Stocks, endpoint, {}, &FlowStock::Endpoint)` and stop at the first with another endpoint.
- unitsInTransit: the sum of every packet's Units.
- unitsCreated: `ledger->Created`.
- unitsConsumed: the sum of every consumption's Units.
- unitsConsumed with a cause: the Units of the consumption whose Cause is `hashName(cause)`, found by lower_bound on `&FlowConsumption::Cause`, or 0.

Add `<algorithm>` to flow.h's includes.

Step 2: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes except the stalls scenario's.

### Task 10: Build the stalls scenario

Files:
- Modify: `src/scenarios/stalls.cpp`

Step 1: Replace the stub with the scenario below. Includes: scenarios/stalls.h, scenarios/synthetic.h, sim/command_queue.h, sim/draw.h, sim/entity_key.h, sim/medium/field.h, sim/medium/flow.h, sim/medium/network.h, sim/mix.h, sim/schema.h, sim/world.h, `<cmath>`, `<memory>`, `<stdint.h>`, and `<vector>`. Everything but stallsScenario is in an anonymous namespace.

Components, each with a visitFields listing every field under its lowercase hyphenated name:

- `Track {double Length = 0.0; uint32_t Stops = 0;}`, intent, named track.
- `Stall {double Distance = 0.0;}`, intent, named stall.
- `Depot {double Distance = 0.0;}`, state, named depot.
- `Visitor {double Distance = 0.0; uint32_t Age = 0; bool Out = false;}`, state, named visitor.

Commands: `PlaceStall {double Distance = 0.0;}`, whose applyCommand creates an entity with createEntity and emplaces `Stall{command.Distance}` on it; and `RemoveStall {EntityKey Stall = NULL_KEY;}`, whose applyCommand calls `world.destroyEntity(command.Stall)`.

Purposes: `TRACK_PURPOSE = hashName("track-network")`, `SHIP_PURPOSE = hashName("stalls-ship")`, `CHOOSE_PURPOSE = hashName("stalls-choose")`, and `SPAWN_PURPOSE = hashName("stalls-spawn")`.

Helpers:

- `EntityKey trackKey(const World &world)`: the lowest key holding Track, or NULL_KEY.
- `const Network *trackNetwork(const World &world)`: the Network on the entity keyed `deriveKey(NULL_KEY, TRACK_PURPOSE, 0)`, or null.
- `Place trackPlace(const World &world, double distance)`: `Place{trackKey(world), distance}`.
- `std::vector<EntityKey> stallKeys(const World &world)`: keys holding Stall, ascending, from world.keys().
- `uint32_t travelDelay(double from, double to)`: `1 + static_cast<uint32_t>(std::fabs(from - to) / 4.0)`.

Resolvers, registered after the fields and kinds:

- track-network, `resolveTrack`: for the lowest-keyed Track, creates the entity `createDerivedEntity(NULL_KEY, TRACK_PURPOSE, 0)` and emplaces or replaces on it `Network({carrier}, track.Stops, {})`, where the carrier's Key is the track's key, its Points are `{X 0, Z 0, Distance 0}` and `{X Length, Z 0, Distance Length}`, and its Stops are node i at `i == Stops - 1 ? Length : Length * i / (Stops - 1)` for i from 0 to Stops - 1. Copy the Track before creating the entity.
- stall-entries, `resolveStallEntries`, depending on track-network, stall-offer-field, and crowd-field: for each stall in key order, `publishResolved<StallOffer>` of one entry at trackPlace of its distance with value `Distance / 10.0`, and `publishResolved<Crowd>` of one entry there with value 1.0.

Systems, in this order:

- `stepDepot`: for the lowest-keyed Depot, when `world.Tick % 5 == 0`, `createUnits<Goods>(world, depot, NULL_KEY, 2)`. Then, when some stall exists and `unitsHeld<Goods>(world, depot, NULL_KEY) >= 4`, pick a stall with `drawPick(drawKey(world, depot, SHIP_PURPOSE, 0), weights)` over integer weights of 1 per stall in key order, and send every held unit to it with `sendUnits<Goods>(world, depot, stall, NULL_KEY, held, travelDelay(depot's Distance, stall's Distance))`.
- `stepStalls`: for each stall in key order, with goods = `unitsHeld<Goods>(world, stall, NULL_KEY)`, `publishStepped<StallOffer>` of one entry at its place with value `static_cast<double>(goods)`. Then for each holding in `stockOf<Visits>(world, stall)`: when the handle is not live, `consumeUnits<Visits>(world, stall, handle, units, "abandoned")`; otherwise, when goods is at least the holding's units, `consumeUnits<Goods>(world, stall, NULL_KEY, units, "sold")`, lower goods by units, and `sendUnits<Visits>(world, stall, handle, handle, units, 3)`.
- `stepVisitors`: when the track network exists, for each visitor in key order, copied first: Age plus 1; `publishStepped<Crowd>` of one entry at its place with value 1.0; when `unitsHeld<Visits>(world, key, key)` is above 0, consume them all with cause "done" and set Out false; otherwise, when Out is false, `(world.Tick + key) % 9 == 0`, and some stall exists, choose a stall with `drawPick(drawKey(world, key, CHOOSE_PURPOSE, 0), weights)`, where each stall's double weight is 1.0 plus the sum of the values `sampleField<StallOffer>(world, network, stall's place)` gives, then `createUnits<Visits>(world, key, key, 1)`, `sendUnits<Visits>(world, key, stall, key, 1, travelDelay(visitor's Distance, stall's Distance))`, and set Out true. Write the visitor back.
- `turnOverVisitors`: destroys every visitor whose Age is at least 240, then, when `world.Tick % 20 == 0`, creates a visitor at Distance `60.0 * drawUniform(drawKey(world, NULL_KEY, SPAWN_PURPOSE, 0))`.

Schema, in this order: addNetworkComponent; the four components; `addField<StallOffer>`, `addField<Crowd>`, `addFlow<Goods>`, `addFlow<Visits>`; the two resolvers; the four systems; `addCommand<PlaceStall>()` and `addCommand<RemoveStall>()`.

Populate: a Track entity with Length 60 and Stops 7; a Depot at Distance 0; stalls at Distances 15, 32.5, and 50; and six visitors at Distances 5, 13, 21, 29, 37, and 45, with Ages 0, 40, 80, 120, 160, and 200.

QueueCommands: when `world.Tick % 400 == 200` and at least two stalls exist, `RemoveStall{lowest stall key}`; when `world.Tick % 400 == 0` and the tick is above 0, `PlaceStall{static_cast<double>((world.Tick / 400 * 17) % 60)}`.

Step 2: Build and test.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug`
Expected: no diagnostic lines, and every test passes.

### Task 11: Verify both builds

Step 1: Format, then build and test both builds, then run the cross-build check.

Run: `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | xargs clang-format -i && cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; ctest --preset linux-debug && cmake.exe --build --preset windows-debug && ctest.exe --preset windows-debug && scripts/cross-build-check.sh`
Expected: no diagnostic lines, every test passes on both builds, and the cross-build check reports that both builds wrote the same lines.

### Task 12: Review and commit

Step 1: Stage everything with `git add -A` and run the review as implementing-features describes.

Step 2: Commit once via the commit-hygiene skill, with the subject `Medium: Add the flow ledger and the stalls scenario`.
