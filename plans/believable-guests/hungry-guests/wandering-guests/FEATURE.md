# Feature: Wandering Guests

## Summary

wandering-guests puts guests in the park. It adds the module src/sim/guests. Every ARRIVAL_INTERVAL ticks, each entrance with a guest connector admits a guest at its anchor, with a stay, a hunger rate, and a starting hunger from keyed draws on the guest's key. Each tick a guest gets hungrier, and walks WALK_STEP along the guest network's carriers, deciding at every node it reaches. A wandering guest takes a keyed draw among the node's steps other than the way it came, and turns back at a dead end. Once its stay is over it heads home, following route distance's Next steps to the entrance, and leaves the park at its anchor. With no route home it keeps wandering. Until carried-guests carries places across edits, a guest whose place stops resolving leaves the park. Each guest publishes an inspection record of its activity, place, ground position, hunger, and stay. The scene draws every guest as a small upright box colored from blue to pink by its hunger, and the Debug panel shows the guest count and mean hunger.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema between cycles, t the tick the cycle that follows steps, N is parkNetwork(W, PathKind::Guest), WALK_STEP is WALK_SPEED * SIM_TICK_SECONDS, and "the guests module's spec" is src/sim/guests/SPEC.md as Spec changes gives it.

1. makeParkSchema registers the state component type guest and the system stepGuests after everything addOperations registers, so guests step after shops and depots. parkGuests(W) gives, ascending, the keys of the entities holding a guest, and guestRecord(W, key) is none exactly when key is not one of them.
2. In the cycle, exactly when t + 1 is a multiple of ARRIVAL_INTERVAL, each entrance of parkEntrances(W) that anchors a node of N admits one guest, in entrance key order, after every guest already in W has stepped. The new guest's key comes from the key counter, and its record shows it wandering, at the nodePlace of the entrance's lowest anchored node, with the StayUntil and Hunger the guests module's spec draws from its key and t. No other cycle admits a guest, and an entrance that anchors no node of N admits none.
3. Each cycle, every guest that stays in the park has its Hunger raised by its own constant rate, drawn once when it arrived, from HUNGER_RATE_MIN to HUNGER_RATE_MAX, until Hunger reaches 1, and it never exceeds 1. A new guest's Hunger is from 0 to STARTING_HUNGER_MAX, and its StayUntil is its arrival tick plus STAY_MIN to STAY_MAX.
4. Each cycle, a guest that neither arrives nor leaves walks exactly WALK_STEP along carriers of N, leaving a carrier only at a node. A guest that wanders, one wandering or one heading home with no entrance entry at its place, keeps the direction it walks inside an edge. At a node it leaves by a step other than its way back whenever the node has one, picked by drawPick with a weight of 1 for each such step, keyed as the guests module's spec says, and by its way back at a node with no other step. So a wandering guest turns back only at a dead end, such as the end of a path or an anchor. A guest heading home with an entrance entry follows criterion 5 instead, and may turn back anywhere its route home does.
5. From the cycle in which t reaches its StayUntil, a guest's record shows it heading home. A guest heading home whose place has an entry for an entrance in guest route distance walks that entrance's Next steps, so the least entrance Distance at its place falls by WALK_STEP each cycle, and it leaves the park, its entity destroyed, in the cycle it reaches the entrance's anchor. A guest heading home with no entrance entry at its place wanders as in criterion 4, and heads for the entrance once an entry appears.
6. A guest whose place does not resolve on N at the start of its step leaves the park in that cycle. So after every cycle, every guest's place resolves on N, except the guests of a world whose last cycle applied a command, until the next cycle. In every cycle of randomized park edit sequences (tests/sim/support/route_edits.h) from a park with an entrance and guests walking, nothing throws, a copy of the world equals it, its save loads back and resolves equal to it, and a candidate made with an edit equals the world that queues the same edit.
7. guestColor(hunger) is, in each of red, green, and blue, the sated color's channel plus (the hungry color's channel minus it) times the hunger clamped to [0, 1], with alpha 1. appendGuest(mesh, point, hunger) adds exactly what appendBox adds for a pose at the point with the default facing, GUEST_SIZE, GUEST_HEIGHT, and guestColor(hunger). buildGuestMesh(W) adds appendGuest for each guest of parkGuests(W) whose record has a Position, in key order, at that Position with its Hunger, and nothing else.
8. A --capture of tests/parks/supply.park after 900 ticks shows guests as boxes on its guest paths and walkways, and the Debug panel shows the guest count and mean hunger. The cross-build check passes, now running guests in every tests/parks file.

## Medium

- Route distance (navigable-networks): a guest heading home samples guest-route-distance at its place, and reads the entrances' entries, their Distance and Next.
- Networks (navigable-networks, shared-medium): guests walk parkNetwork(world, Guest) through the Network type's queries: resolve, edges, nodePlace, anchoredNodes, and groundPoint.
- Park intent (decision 0025): the entrances through parkEntrances, whose keys are the anchors guests arrive at and the route distance sources they head home to.
- Keyed draws (deterministic-simulation): stays, hunger rates, starting hunger, and wandering picks.
- Inspection records (decision 0025): read by tests, by buildGuestMesh, and by the app's Debug panel.

Guests emit nothing in this feature, and send and receive no flow units: visits and meals come in eating-guests. No guest reads another guest's state, a shop's, or a depot's, and no other entity reads a guest's.

## Principle checks

- Principle 1: criterion 6. Guests are state that saves hold, and a loaded world equals the world saved.
- Principle 2: criteria 2, 5, and 6. An entrance with no connector, a guest with no route home, and a guest whose path was deleted are each legitimate, and randomized edits never throw.
- Principles 3 and 6: guests read only route distance, the network, and the entrances. Review checks that stepGuests reads nothing else, and that nothing outside the module names Guest.
- Principle 4: criteria 4 and 5. Guests walk along carriers, and heading home follows route distance.
- Principle 8: criterion 1 and the record. Every guest publishes what it is doing and why it will leave.
- Principle 10: criteria 2 and 4. Arrivals and wandering come from keyed draws, and the cross-build check (criterion 8) compares guests' walks in every park file.

## Spec changes

- src/sim/guests/SPEC.md: new. PLAN.md's Task 1 gives the text.
- src/sim/SPEC.md: addPark registers the guests module after the operations module.
- src/render/SPEC.md: a new Guests section defines guestColor, appendGuest, and buildGuestMesh, and the Contract section adds setGuestMesh and where drawFrame draws the guests.
- src/app/SPEC.md: the Park section rebuilds the guest mesh when the tick changes or the world is replaced, and the Tooling UI section adds the Debug panel's guest line.
- src/scenarios/SPEC.md: park-edits' worlds now admit guests, whose walks the cross-build check compares.

Public interface, src/sim/guests/guests.h:

```cpp
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
```

GroundPoint gains a defaulted operator== in src/sim/medium/network.h, so the record compares.

src/render/guest_mesh.h:

```cpp
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
```

src/render/renderer.h gains `bool setGuestMesh(Renderer &renderer, const ParkMesh &mesh);`.

## Files affected

- Create: src/sim/guests/SPEC.md, src/sim/guests/guests.h, src/sim/guests/guests.cpp, src/sim/guests/internal/guest.h, src/render/guest_mesh.h, src/render/guest_mesh.cpp
- Modify: src/sim/CMakeLists.txt, src/sim/park_schema.cpp, src/sim/SPEC.md, src/sim/medium/network.h, src/render/CMakeLists.txt, src/render/renderer.h, src/render/renderer.cpp, src/render/SPEC.md, src/app/main.cpp, src/app/debug_panel.h, src/app/debug_panel.cpp, src/app/SPEC.md, src/scenarios/SPEC.md
- Tests (test pass): tests/sim/guests/, tests/render/, and their CMakeLists.txt. Existing tests that step a park with an entrance and a guest connector for ARRIVAL_INTERVAL cycles or more now see guests arrive; the test pass updates any whose expectations about keys or entities that changes.

## Dependencies

- navigable-networks' paths-become-routes: the guest network, the entrance's connector and anchor, and guest route distance with Next steps. Met.
- deterministic-simulation's world-as-value: keyed draws, createEntity and destroyEntity in systems. Met.
- effortless-building's sketch-a-park: appendBox, the renderer's meshes, the Debug panel, and --capture. Met.

## Out of scope

- Choosing food, visits, meals, and eating: eating-guests.
- Carrying places across edits and moving stranded guests to the nearest point: carried-guests. Until then a guest whose place stops resolving leaves.
- Hungry footfall: hungry-footfall.
- Drawing guests in the ghost, facing guests the way they walk, and interpolating between ticks (MILESTONE.md's deepening candidates).
- Clicking a guest to inspect it: legible-simulation's guest inspector.

## Open questions

None.
