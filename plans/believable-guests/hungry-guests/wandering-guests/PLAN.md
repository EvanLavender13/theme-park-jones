# Implementation Plan: Wandering Guests

## Goal

Guests arrive at the entrance, get hungrier, wander the guest network, head home and leave when their stay is over, publish inspection records, and are drawn in the scene colored by hunger, with a guest line in the Debug panel.

## Approach

A new module, src/sim/guests, holds one private state component per guest: its place, the way it walks, its activity, hunger, hunger rate, and stay. One system steps every guest in key order and then admits arrivals. A guest's walk is a loop that chooses a step, from route distance when heading home or from a keyed pick among a node's other steps when wandering, and walks it until the tick's distance is used. The renderer gains a guest mesh built from the guests' records, and the app rebuilds it whenever the tick changes.

## Tasks

### Task 1: Guests spec

Files:
- Create: `src/sim/guests/SPEC.md`

Step 1: Write the file:

```
# guests

Believable guests (plans/believable-guests): the park's visitors. It is part of tpj_sim and follows its contract (src/sim/SPEC.md). A guest reads the park only through the medium and intent: the guest network through parkNetwork and the Network type's queries, guest route distance through sampleField, and the entrances through parkEntrances (decision 0025). A guest never reads another entity's state, and no other module reads a guest's (principles 3 and 6).

## Registration

addGuests registers the state component type Guest, named guest, and then the system stepGuests. addPark calls it after addOperations, so guests step after shops and depots. Guest is private to the module, declared in guests/internal/guest.h. It holds At, the guest's place on the guest network; Forward, whether it walks toward higher distances along At's carrier; Activity, wandering or heading-home; Hunger; HungerRate; and StayUntil, the tick its stay ends. parkGuests gives the keys of the entities holding one, ascending.

## Stepping

Each cycle, stepGuests takes each guest in ascending key order, with t the tick being stepped and N the guest network, parkNetwork(world, PathKind::Guest). A guest whose place does not resolve on N, as after an edit deletes its path, leaves the park. Otherwise its Hunger becomes the lesser of 1 and Hunger + HungerRate, its Activity becomes heading-home once t is StayUntil or later, and it walks. A guest leaves the park when its entity is destroyed, which happens after every guest has stepped. Then guests arrive.

## Arrivals

In the cycle stepping t, when t + 1 is a multiple of ARRIVAL_INTERVAL, 60 ticks, each entrance of parkEntrances that anchors a node of N admits one guest, in entrance key order. The guest is a new entity from createEntity, at nodePlace of the entrance's lowest anchored node, with Forward true and Activity wandering. For a purpose p, let u(p) be drawUniform(drawKey(world, the guest's key, hashName(p), 0)). Its StayUntil is t + STAY_MIN + floor(u("guest-stay") * (STAY_MAX - STAY_MIN)), with STAY_MIN 1800 and STAY_MAX 3600. Its HungerRate is HUNGER_RATE_MIN + u("guest-hunger-rate") * (HUNGER_RATE_MAX - HUNGER_RATE_MIN), with HUNGER_RATE_MIN 1/2700 and HUNGER_RATE_MAX 1/1350 per tick, so hunger rises from 0 to 1 in 45 to 90 s. Its Hunger is u("guest-starting-hunger") * STARTING_HUNGER_MAX, 0.4. Hunger is on the scale from 0 to 1 that MEAL_RELIEF assumes (sim/operations/SPEC.md). A new guest first walks in the next cycle. An entrance with no guest connector admits no guests, a legitimate state (principle 2).

## Walking

A guest walks WALK_STEP, WALK_SPEED 1.3 m/s times SIM_TICK_SECONDS, each cycle, along the carriers of N (principle 4). A step is a carrier and two distances along it, From and To, walked from From toward To. The walk repeats these, with the distance left starting at WALK_STEP. A guest heading home samples guest route distance at its place, and when the least Distance among the entries whose source is an entrance, ties to the lower source key, has a Next whose carrier is NULL_KEY, it stands at that entrance's anchor and leaves the park, ending its walk. Otherwise, when no distance is left, the walk ends. Otherwise the guest chooses a step. When its place resolves to a node, it moves to the step's carrier at the step's From, and inside an edge it stays where it is. It walks toward the step's To, setting Forward to whether To lies above its distance, by the lesser of the distance left and the gap between its distance and To, which it subtracts from the distance left. Reaching To, it stands exactly at To, and otherwise its distance never passes To. So it decides at every node it reaches within the cycle, and its speed does not depend on how the network is cut into edges.

A guest heading home steps by the Next of that least entrance entry. A wandering guest, or one heading home with no entrance entry at its place, wanders. Inside an edge, a wandering guest's step runs from its distance to the stop ahead of it, the edge's ToDistance when Forward and its FromDistance otherwise. At a node, the node's steps are, for each edge in edges() order, {carrier, FromDistance, ToDistance} when the edge starts at the node, then {carrier, ToDistance, FromDistance} when it ends there. The guest's way back is the step whose carrier is At's, whose From is At's distance, and which runs toward higher distances exactly when Forward is false: the edge it came in on. A wandering guest picks among the node's other steps by drawPick with a weight of 1 each, keyed drawKey(world, its key, hashName("guest-wander"), i), where i counts the picks it has made in the cycle from 0, and takes the way back when there is no other step, as at the end of a path or at an anchor. A new guest has no way back, so it takes its entrance's connector.

## Inspection record

A guest publishes an inspection record about itself for display and tests (decision 0025). No system or resolver reads one. guestRecord(world, key) gives none when key holds no guest, and otherwise its GuestRecord: its Activity, At, Position, groundPoint of At on N or none where At does not resolve, Hunger, and StayUntil. The record is computed from the guest's state, so it adds nothing to a save.
```

Step 2: Check the text.

Run: `grep -c "^## " src/sim/guests/SPEC.md`
Expected: `5`

### Task 2: Sim and scenarios specs

Files:
- Modify: `src/sim/SPEC.md`
- Modify: `src/scenarios/SPEC.md`

Step 1: In src/sim/SPEC.md, replace "and then the operations module's flow kinds, food offer, and systems, which run its shops and depots (sim/operations/SPEC.md)." with "then the operations module's flow kinds, food offer, and systems, which run its shops and depots (sim/operations/SPEC.md), and then the guests module's state and system, which admit and walk its guests (sim/guests/SPEC.md)."

Step 2: In src/scenarios/SPEC.md, replace "So the physical-validity check's answers, and the orders and shipments of the shops and depots it places, are compared across builds." with "So the physical-validity check's answers, the orders and shipments of the shops and depots it places, and the walks of the guests its entrance admits, are compared across builds."

Step 3: Check the text.

Run: `grep -c "sim/guests/SPEC.md" src/sim/SPEC.md; grep -c "walks of the guests" src/scenarios/SPEC.md`
Expected: `1` and `1`

### Task 3: Render spec

Files:
- Modify: `src/render/SPEC.md`

Step 1: In the Contract section, after the paragraph that begins "setGhostMesh uploads", insert:

```
setGuestMesh uploads the guests' mesh, replacing the one drawn before; an empty mesh draws nothing. drawFrame draws it after the park mesh and before the ghost mesh, through the park mesh's pipeline, so guests are lit, opaque, and depth tested as boxes are.
```

Step 2: Insert a section before "## Graph overlay":

```
## Guests

guest_mesh.h builds the guests' mesh on the CPU, so it can be tested without a GPU. It reads guests only through parkGuests and guestRecord (sim/guests/SPEC.md), and changes nothing. guestColor(hunger) clamps the hunger to [0, 1], converts it to float as h, and gives, for each of red, green, and blue, s + (g - s) * h in float arithmetic, where s is GUEST_SATED_COLOR's channel and g GUEST_HUNGRY_COLOR's, with alpha 1. So a sated guest is blue and a hungry one pink. appendGuest(mesh, point, hunger) adds exactly what appendBox adds for a Pose at the point's x and z with the default facing, GUEST_SIZE, 0.6 m by 0.6 m, GUEST_HEIGHT, 1.8 m, and guestColor(hunger). buildGuestMesh gives a world's guest mesh: appendGuest for each guest of parkGuests whose guestRecord has a Position, in key order, at that Position with its Hunger. The two guest colors are opaque and differ in red, green, and blue.
```

Step 3: Check the text.

Run: `grep -c "^## Guests" src/render/SPEC.md; grep -c "setGuestMesh" src/render/SPEC.md`
Expected: `1` and `1`

### Task 4: App spec

Files:
- Modify: `src/app/SPEC.md`

Step 1: In the Park section, insert before the sentence that begins "When it builds the first mesh": "Each frame, after its ticks, when the world's tick differs from the tick the guests were last drawn at, or the world has been replaced since, it builds buildGuestMesh for the world and gives it to the renderer with setGuestMesh, so guests move as the world steps."

Step 2: In the Tooling UI section, after the sentence that ends "each number in decimal and limit its limitingFactorName.", insert: "Below them, after a separator, each frame, the panel shows `Guests <n>, mean hunger <m>`, with n the number of parkGuests in decimal and m the mean of their records' Hunger to two decimals, or `Guests 0` when there are none."

Step 3: Check the text.

Run: `grep -c "buildGuestMesh" src/app/SPEC.md; grep -c "mean hunger" src/app/SPEC.md`
Expected: `1` and `1`

### Task 5: Guests interface

Files:
- Modify: `src/sim/medium/network.h`
- Create: `src/sim/guests/guests.h`
- Create: `src/sim/guests/internal/guest.h`
- Create: `src/sim/guests/guests.cpp`
- Modify: `src/sim/CMakeLists.txt`
- Modify: `src/sim/park_schema.cpp`

Step 1: In network.h, give GroundPoint a comparison:

```cpp
struct GroundPoint {
  double X = 0.0;
  double Z = 0.0;

  bool operator==(const GroundPoint &) const = default;
};
```

Step 2: Write src/sim/guests/guests.h:

```cpp
#ifndef TPJ_SIM_GUESTS_GUESTS_H
#define TPJ_SIM_GUESTS_GUESTS_H

#include "sim/entity_key.h"
#include "sim/medium/network.h"

#include <array>
#include <optional>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// What a guest is doing: wandering the paths, or heading home once its stay is over.
enum class GuestActivity : uint8_t { Wandering, HeadingHome };

constexpr std::array<std::string_view, 2> enumNames(GuestActivity /*value*/) {
  return {"wandering", "heading-home"};
}

// What a guest publishes about itself for display and tests (decision 0025): what it is doing,
// where it stands, where that is on the ground (none while its place does not resolve), how
// hungry it is, and the tick its stay ends. Nothing in the park reads it.
struct GuestRecord {
  GuestActivity Activity = GuestActivity::Wandering;
  Place At;
  std::optional<GroundPoint> Position;
  double Hunger = 0.0;
  uint64_t StayUntil = 0;

  bool operator==(const GuestRecord &) const = default;
};

// Ticks between arrivals at each entrance.
inline constexpr uint64_t ARRIVAL_INTERVAL = 60;
// A guest's stay, in ticks, is drawn from STAY_MIN up to STAY_MAX.
inline constexpr uint64_t STAY_MIN = 1800;
inline constexpr uint64_t STAY_MAX = 3600;
// A guest's hunger rises by a rate per tick drawn from these, so from 0 to 1 in 45 to 90 s.
inline constexpr double HUNGER_RATE_MIN = 1.0 / 2700.0;
inline constexpr double HUNGER_RATE_MAX = 1.0 / 1350.0;
// A guest arrives with a hunger drawn from 0 up to this.
inline constexpr double STARTING_HUNGER_MAX = 0.4;
// Meters per second a guest walks.
inline constexpr double WALK_SPEED = 1.3;

// The keys of every guest, ascending.
std::vector<EntityKey> parkGuests(const World &world);
// The guest's inspection record, or none when the key holds no guest.
std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest);
// Registers the guest state and the system that steps guests and admits new ones. The routes and
// operations modules' registrations come first.
void addGuests(WorldSchema &schema);

} // namespace tpj

#endif
```

Step 3: Write src/sim/guests/internal/guest.h:

```cpp
#ifndef TPJ_SIM_GUESTS_INTERNAL_GUEST_H
#define TPJ_SIM_GUESTS_INTERNAL_GUEST_H

#include "sim/guests/guests.h"
#include "sim/medium/network.h"

#include <stdint.h>

namespace tpj {

// State, on a guest's entity: where it stands on the guest network, whether it walks toward
// higher distances along that place's carrier, what it is doing, how hungry it is and how fast
// that rises, and the tick its stay ends.
struct Guest {
  Place At;
  bool Forward = true;
  GuestActivity Activity = GuestActivity::Wandering;
  double Hunger = 0.0;
  double HungerRate = 0.0;
  uint64_t StayUntil = 0;
};

template <typename Visitor> void visitFields(Visitor &visitor, Guest &guest) {
  visitor.field("at", guest.At);
  visitor.field("forward", guest.Forward);
  visitor.field("activity", guest.Activity);
  visitor.field("hunger", guest.Hunger);
  visitor.field("hunger-rate", guest.HungerRate);
  visitor.field("stay-until", guest.StayUntil);
}

} // namespace tpj

#endif
```

Step 4: Write src/sim/guests/guests.cpp with stubs:

```cpp
#include "sim/guests/guests.h"

#include "sim/guests/internal/guest.h"
#include "sim/schema.h"
#include "sim/world.h"

namespace tpj {

std::vector<EntityKey> parkGuests(const World & /*world*/) { return {}; }

std::optional<GuestRecord> guestRecord(const World & /*world*/, EntityKey /*guest*/) {
  return std::nullopt;
}

void addGuests(WorldSchema & /*schema*/) {}

} // namespace tpj
```

Step 5: In src/sim/CMakeLists.txt, add `guests/guests.cpp` to tpj_sim's sources after `draw.cpp`.

Step 6: In src/sim/park_schema.cpp, add `#include "sim/guests/guests.h"` among the includes, in order, call `addGuests(schema);` after `addOperations(schema);`, and end the comment's last sentence with "then the operations that run its shops and depots, then the guests who visit them."

Step 7: Build.

Run: `cmake --build --preset linux-debug --target tpj_sim 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 6: Guest mesh interface

Files:
- Create: `src/render/guest_mesh.h`
- Create: `src/render/guest_mesh.cpp`
- Modify: `src/render/CMakeLists.txt`

Step 1: Write src/render/guest_mesh.h:

```cpp
#ifndef TPJ_RENDER_GUEST_MESH_H
#define TPJ_RENDER_GUEST_MESH_H

#include "render/park_mesh.h"
#include "sim/medium/network.h"
#include "sim/park/intent.h"
#include "sim/world.h"

namespace tpj {

// A guest is drawn as an upright box of this footprint and height, colored from sated to hungry.
inline constexpr FootprintSize GUEST_SIZE{0.6, 0.6};
inline constexpr float GUEST_HEIGHT = 1.8f;
inline constexpr Rgba GUEST_SATED_COLOR{0.30f, 0.75f, 0.95f, 1.0f};
inline constexpr Rgba GUEST_HUNGRY_COLOR{0.95f, 0.20f, 0.50f, 1.0f};

// The sated color moved toward the hungry one by the hunger, clamped to [0, 1].
Rgba guestColor(double hunger);
// Adds a guest's box standing at the point, in its hunger's color.
void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger);
// Every guest whose place resolves, in key order.
ParkMesh buildGuestMesh(const World &world);

} // namespace tpj

#endif
```

Step 2: Write src/render/guest_mesh.cpp with stubs:

```cpp
#include "render/guest_mesh.h"

namespace tpj {

Rgba guestColor(double /*hunger*/) { return {}; }

void appendGuest(ParkMesh & /*mesh*/, GroundPoint /*point*/, double /*hunger*/) {}

ParkMesh buildGuestMesh(const World & /*world*/) { return {}; }

} // namespace tpj
```

Step 3: In src/render/CMakeLists.txt, add `guest_mesh.cpp` to tpj_render's sources after `graph_overlay.cpp`.

Step 4: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 7: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/guests/SPEC.md, src/sim/SPEC.md, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, src/sim/park/SPEC.md, src/sim/operations/SPEC.md, src/render/SPEC.md, src/app/SPEC.md, and src/scenarios/SPEC.md, and the public headers src/sim/guests/guests.h, src/render/guest_mesh.h, and src/sim/medium/network.h.

### Task 8: Registration and records

Files:
- Modify: `src/sim/guests/guests.cpp`

Step 1: Replace the file with the full module below. Its stepGuests is the whole step; Tasks 9 and 10 verify its parts.

```cpp
#include "sim/guests/guests.h"

#include "sim/draw.h"
#include "sim/guests/internal/guest.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <variant>
#include <vector>

namespace tpj {

namespace {

using GuestRouteDistance = RouteDistance<PathKind::Guest>;

// Meters a guest walks each tick.
constexpr double WALK_STEP = WALK_SPEED * SIM_TICK_SECONDS;

// The keys of the entrances, ascending.
std::vector<EntityKey> entranceKeys(const World &world) {
  std::vector<EntityKey> keys;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    keys.push_back(entrance.Key);
  }
  return keys;
}

// The first step home from the place: the Next of the least entrance entry guest route distance
// gives there, ties to the lower key, or none without one.
std::optional<RouteStep> homeStep(const World &world, const Network &network, const Place &place,
                                  const std::vector<EntityKey> &entrances) {
  std::optional<RouteEntry> nearest;
  // Sources come in ascending key order, so keeping only a strictly less distance breaks ties
  // to the lower key.
  for (const SampledEntry<RouteEntry> &entry :
       sampleField<GuestRouteDistance>(world, network, place)) {
    if (std::ranges::binary_search(entrances, entry.Source) &&
        (!nearest || entry.Value.Distance < nearest->Distance)) {
      nearest = entry.Value;
    }
  }
  if (!nearest) {
    return std::nullopt;
  }
  return nearest->Next;
}

// The steps out of the node, in edge order: each edge starting at it walked forward, then each
// ending at it walked back.
std::vector<RouteStep> stepsFrom(const Network &network, uint32_t node) {
  std::vector<RouteStep> steps;
  for (const NetworkEdge &edge : network.edges()) {
    if (edge.From == node) {
      steps.push_back({edge.Carrier, edge.FromDistance, edge.ToDistance});
    }
    if (edge.To == node) {
      steps.push_back({edge.Carrier, edge.ToDistance, edge.FromDistance});
    }
  }
  return steps;
}

// Whether the step at a node goes back along the edge the guest came in on: its own carrier, from
// where it stands, against the way it walks.
bool isWayBack(const RouteStep &step, const Guest &guest) {
  return step.Carrier == guest.At.Carrier && step.From == guest.At.Distance &&
         (step.To > step.From) != guest.Forward;
}

// A wandering guest's step: on to the stop ahead inside an edge, and at a node a keyed pick among
// the steps other than the way back, or the way back when there is no other.
RouteStep wanderStep(const World &world, EntityKey key, const Guest &guest,
                     const Network &network, const NetworkPosition &position, uint64_t &picks) {
  if (const auto *inside = std::get_if<EdgePosition>(&position)) {
    const NetworkEdge &edge = network.edges()[inside->Edge];
    return {guest.At.Carrier, guest.At.Distance,
            guest.Forward ? edge.ToDistance : edge.FromDistance};
  }
  std::vector<RouteStep> onward;
  RouteStep back;
  for (const RouteStep &step : stepsFrom(network, std::get<NodePosition>(position).Node)) {
    if (isWayBack(step, guest)) {
      back = step;
    } else {
      onward.push_back(step);
    }
  }
  if (onward.empty()) {
    return back;
  }
  const std::vector<uint64_t> weights(onward.size(), 1);
  return onward[drawPick(drawKey(world, key, hashName("guest-wander"), picks++), weights)];
}

// Walks the guest WALK_STEP along the guest network, choosing a step at the start and at every
// node it reaches. Returns false when the guest stands at its entrance heading home, and leaves.
bool walk(const World &world, EntityKey key, Guest &guest, const Network &network,
          const std::vector<EntityKey> &entrances) {
  double left = WALK_STEP;
  uint64_t picks = 0;
  for (;;) {
    const std::optional<NetworkPosition> position = network.resolve(guest.At);
    if (!position) {
      return false;
    }
    std::optional<RouteStep> step;
    if (guest.Activity == GuestActivity::HeadingHome) {
      step = homeStep(world, network, guest.At, entrances);
      if (step && step->Carrier == NULL_KEY) {
        return false;
      }
    }
    if (left <= 0.0) {
      return true;
    }
    if (!step) {
      step = wanderStep(world, key, guest, network, *position, picks);
    }
    if (std::holds_alternative<NodePosition>(*position)) {
      guest.At = {step->Carrier, step->From};
    }
    guest.Forward = step->To > guest.At.Distance;
    const double gap = std::abs(step->To - guest.At.Distance);
    if (left < gap) {
      // Rounding never carries the guest past the stop it walks toward.
      const double walked = guest.Forward ? guest.At.Distance + left : guest.At.Distance - left;
      guest.At.Distance = guest.Forward ? std::min(walked, step->To) : std::max(walked, step->To);
      left = 0.0;
    } else {
      guest.At.Distance = step->To;
      left -= gap;
    }
  }
}

// Each entrance with a guest connector admits a guest at the end of every ARRIVAL_INTERVAL
// ticks, with its stay, hunger rate, and starting hunger drawn on its key.
void admitGuests(World &world, const Network &network) {
  if ((world.Tick + 1) % ARRIVAL_INTERVAL != 0) {
    return;
  }
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    const std::vector<uint32_t> anchored = network.anchoredNodes(entrance.Key);
    if (anchored.empty()) {
      continue;
    }
    const EntityKey key = world.createEntity();
    const auto draw = [&world, key](const char *purpose) {
      return drawUniform(drawKey(world, key, hashName(purpose), 0));
    };
    Guest guest;
    guest.At = network.nodePlace(anchored.front());
    guest.StayUntil = world.Tick + STAY_MIN +
                      static_cast<uint64_t>(draw("guest-stay") *
                                            static_cast<double>(STAY_MAX - STAY_MIN));
    guest.HungerRate =
        HUNGER_RATE_MIN + draw("guest-hunger-rate") * (HUNGER_RATE_MAX - HUNGER_RATE_MIN);
    guest.Hunger = draw("guest-starting-hunger") * STARTING_HUNGER_MAX;
    world.Registry.emplace<Guest>(world.findEntity(key), guest);
  }
}

// Each guest, in ascending key order, leaves when its place no longer resolves, and otherwise
// gets hungrier, starts home once its stay is over, and walks. Guests that leave go after all
// have stepped, and then the entrances admit new ones.
void stepGuests(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const std::vector<EntityKey> entrances = entranceKeys(world);
  std::vector<EntityKey> leaving;
  for (const EntityKey key : parkGuests(world)) {
    Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (!network.resolve(guest.At)) {
      leaving.push_back(key);
      continue;
    }
    guest.Hunger = std::min(1.0, guest.Hunger + guest.HungerRate);
    if (world.Tick >= guest.StayUntil) {
      guest.Activity = GuestActivity::HeadingHome;
    }
    if (!walk(world, key, guest, network, entrances)) {
      leaving.push_back(key);
    }
  }
  for (const EntityKey key : leaving) {
    world.destroyEntity(key);
  }
  admitGuests(world, network);
}

} // namespace

std::vector<EntityKey> parkGuests(const World &world) {
  std::vector<EntityKey> keys;
  for (const EntityKey key : world.keys()) {
    if (world.Registry.all_of<Guest>(world.findEntity(key))) {
      keys.push_back(key);
    }
  }
  return keys;
}

std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest) {
  const entt::entity entity = world.findEntity(guest);
  if (entity == entt::null) {
    return std::nullopt;
  }
  const Guest *state = world.Registry.try_get<Guest>(entity);
  if (state == nullptr) {
    return std::nullopt;
  }
  return GuestRecord{state->Activity, state->At,
                     parkNetwork(world, PathKind::Guest).groundPoint(state->At), state->Hunger,
                     state->StayUntil};
}

void addGuests(WorldSchema &schema) {
  schema.addComponent<Guest>("guest", DataKind::State);
  schema.addSystem(&stepGuests);
}

} // namespace tpj
```

Step 2: Build and run the guests tests' registration and record cases.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_sim_tests -# "[#<file>]"` for each of the test pass's files under tests/sim/guests/, one per invocation.
Expected: no build output. The registration, arrival, and record tests (criteria 1 and 2) pass; failures in the others go to Tasks 9 and 10.

### Task 9: Hunger and stays

Files:
- Modify: `src/sim/guests/guests.cpp` only if a criterion 3 or 5 test fails.

Step 1: Run the tests for hunger, stays, and heading home (criteria 3 and 5).

Run: `build/linux-debug/tpj_sim_tests -# "[#<file>]"` for each test pass file covering criteria 3 and 5.
Expected: all pass. A failure is a deviation: compare stepGuests and admitGuests against the Stepping and Arrivals sections of src/sim/guests/SPEC.md.

### Task 10: Walking and leaving

Files:
- Modify: `src/sim/guests/guests.cpp` only if a criterion 4 or 6 test fails.

Step 1: Run the walking, wandering, and edit-sequence tests (criteria 4 and 6), and every sim test, since parks with entrances now admit guests.

Run: `build/linux-debug/tpj_sim_tests`
Expected: all pass. A failure is a deviation: compare walk and wanderStep against the Walking section of src/sim/guests/SPEC.md; for a test outside tests/sim/guests/, report it as a disputed test.

### Task 11: Guest mesh

Files:
- Modify: `src/render/guest_mesh.cpp`

Step 1: Replace the file with:

```cpp
#include "render/guest_mesh.h"

#include "sim/guests/guests.h"

#include <algorithm>
#include <optional>

namespace tpj {

Rgba guestColor(double hunger) {
  const auto h = static_cast<float>(std::clamp(hunger, 0.0, 1.0));
  const auto mix = [h](float sated, float hungry) { return sated + (hungry - sated) * h; };
  return {mix(GUEST_SATED_COLOR.R, GUEST_HUNGRY_COLOR.R),
          mix(GUEST_SATED_COLOR.G, GUEST_HUNGRY_COLOR.G),
          mix(GUEST_SATED_COLOR.B, GUEST_HUNGRY_COLOR.B), 1.0f};
}

void appendGuest(ParkMesh &mesh, GroundPoint point, double hunger) {
  Pose pose;
  pose.X = point.X;
  pose.Z = point.Z;
  appendBox(mesh, pose, GUEST_SIZE, GUEST_HEIGHT, guestColor(hunger));
}

ParkMesh buildGuestMesh(const World &world) {
  ParkMesh mesh;
  for (const EntityKey key : parkGuests(world)) {
    const std::optional<GuestRecord> record = guestRecord(world, key);
    if (record && record->Position) {
      appendGuest(mesh, *record->Position, record->Hunger);
    }
  }
  return mesh;
}

} // namespace tpj
```

Step 2: Build and run the render tests.

Run: `cmake --build --preset linux-debug --target tpj_render_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_render_tests`
Expected: no build output; all pass.

### Task 12: Renderer guest mesh

Files:
- Modify: `src/render/renderer.h`
- Modify: `src/render/renderer.cpp`

Step 1: In renderer.h, add to Renderer after `uint32_t ParkIndexCount = 0;`:

```cpp
  SDL_GPUBuffer *GuestVertices = nullptr;
  SDL_GPUBuffer *GuestIndices = nullptr;
  uint32_t GuestIndexCount = 0;
```

and after the setGhostMesh declaration:

```cpp
// Uploads the guests' mesh drawn from now on, replacing the one before. An empty mesh draws
// nothing. Returns false and logs through SDL on failure.
bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh);
```

Step 2: In renderer.cpp's destroyRenderer, after `SDL_ReleaseGPUBuffer(renderer.Device, renderer.ParkIndices);`, add:

```cpp
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GuestVertices);
  SDL_ReleaseGPUBuffer(renderer.Device, renderer.GuestIndices);
```

Step 3: After setGhostMesh's definition, add:

```cpp
bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh) {
  return replaceMesh(renderer, mesh, renderer.GuestVertices, renderer.GuestIndices,
                     renderer.GuestIndexCount);
}
```

Step 4: In drawScene, after the block that draws the park mesh and before the ghost comment, add:

```cpp
  // Guests are opaque boxes, drawn as the park's are.
  if (renderer.GuestIndexCount > 0) {
    SDL_BindGPUGraphicsPipeline(pass, renderer.ParkPipeline);
    const SDL_GPUBufferBinding guestVertexBinding = {renderer.GuestVertices, 0};
    SDL_BindGPUVertexBuffers(pass, 0, &guestVertexBinding, 1);
    const SDL_GPUBufferBinding guestIndexBinding = {renderer.GuestIndices, 0};
    SDL_BindGPUIndexBuffer(pass, &guestIndexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    SDL_DrawGPUIndexedPrimitives(pass, renderer.GuestIndexCount, 1, 0, 0, 0);
  }
```

Step 5: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 13: Guests in the app

Files:
- Modify: `src/app/debug_panel.h`
- Modify: `src/app/debug_panel.cpp`
- Modify: `src/app/main.cpp`

Step 1: In debug_panel.h, add to DebugStats after `std::vector<ShopLine> Shops;`:

```cpp
  uint64_t Guests = 0;
  double MeanHunger = 0.0;
```

and end drawDebugPanel's comment with "a line for each shop's record, and the guest count and mean hunger."

Step 2: In debug_panel.cpp, after the loop over stats.Shops, add:

```cpp
    ImGui::Separator();
    if (stats.Guests == 0) {
      ImGui::Text("Guests 0");
    } else {
      ImGui::Text("Guests %llu, mean hunger %.2f", static_cast<unsigned long long>(stats.Guests),
                  stats.MeanHunger);
    }
```

Step 3: In main.cpp, add `#include "render/guest_mesh.h"` and `#include "sim/guests/guests.h"` among the includes, in order. In drawPanels, after the loop over parkBoxes, add:

```cpp
  double hunger = 0.0;
  for (const tpj::EntityKey guest : tpj::parkGuests(world)) {
    if (const std::optional<tpj::GuestRecord> record = tpj::guestRecord(world, guest)) {
      ++stats.Guests;
      hunger += record->Hunger;
    }
  }
  stats.MeanHunger = stats.Guests == 0 ? 0.0 : hunger / static_cast<double>(stats.Guests);
```

Step 4: In main.cpp, after updateGhostMesh's definition, add:

```cpp
// Rebuilds the guests' mesh when the world's tick differs from the one last drawn. Returns false
// if the upload failed.
bool updateGuestMesh(tpj::Renderer &renderer, const tpj::World &world,
                     std::optional<uint64_t> &drawnTick) {
  if (drawnTick && *drawnTick == world.Tick) {
    return true;
  }
  drawnTick = world.Tick;
  return tpj::setGuestMesh(renderer, tpj::buildGuestMesh(world));
}
```

Step 5: In runLoop, declare `std::optional<uint64_t> guestTick;` after `std::optional<DrawnGhost> drawnGhost;`, add `guestTick.reset();` after `drawn.reset();` in the file request branch, and after the updateParkMesh check add:

```cpp
    if (!updateGuestMesh(renderer, world, guestTick)) {
      return false;
    }
```

Step 6: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 14: Capture

Step 1: Build Windows and capture supply.park after 900 ticks from build/windows-debug, so a saved imgui.ini at the repository root does not apply.

Run: `cmake.exe --build --preset windows-debug > /dev/null; cd build/windows-debug && timeout 60 ./ThemeParkJones.exe --park ../../tests/parks/supply.park --ticks 900 --capture supply.bmp; echo $?`. Convert the capture to PNG in the scratchpad and view it with the Read tool.
Expected: `0`. The capture shows small upright boxes, blue to pink, standing on the guest paths and walkways, and the Debug panel shows `Guests 15, mean hunger` followed by a value between 0 and 1.

### Task 15: Confirm the acceptance criteria

Run:
- `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
- `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
- `ctest --preset linux-debug`
- `cmake.exe --build --preset windows-debug`
- `ctest.exe --preset windows-debug`
- `scripts/cross-build-check.sh`

Expected: no warnings; every test passes on both builds; the cross-build check passes.

### Task 16: Review and commit

Step 1: Review the staged diff via the reviewing skill, as implementing-features step 7 describes, with FEATURE.md, the changed SPEC.md files, and docs/principles.md as context.

Step 2: Commit once via commit-hygiene, staging `src tests plans` by path:

```
Guests: Admit guests who wander, get hungry, and go home
```
