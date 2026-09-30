#ifndef TPJ_TESTS_SIM_SUPPORT_GUEST_PARKS_H
#define TPJ_TESTS_SIM_SUPPORT_GUEST_PARKS_H

#include "support/park_worlds.h"

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdint.h>
#include <string_view>
#include <utility>
#include <vector>

// Parks with entrances on the park's north edge and guest paths leading in from their doors, and
// what tests read of the guests walking them.
namespace tpj::test {

// Meters a guest walks each cycle.
inline constexpr double WALK_STEP = WALK_SPEED * SIM_TICK_SECONDS;

// An entrance whose back lies on the park's north edge, facing into the park, so its front door
// is at (x, 125).
inline ParkEntrance northGate(EntityKey key, double x) {
  return ParkEntrance{.Key = key, .At = Pose{x, 126.5, 0.0, -1.0}};
}

inline ParkPath guestPath(EntityKey key, std::vector<ParkPoint> points) {
  return ParkPath{.Key = key, .Kind = PathKind::Guest, .Points = std::move(points)};
}

// A world holding exactly the intent, resolved, at tick 0.
inline World resolvedWorld(ParkIntent intent) {
  World world = worldOf(std::move(intent));
  resolveWorld(world);
  return world;
}

inline const Network &guestNetwork(const World &world) {
  return parkNetwork(world, PathKind::Guest);
}

// The place of the entrance's lowest anchored node on the guest network.
inline Place gatePlace(const World &world, EntityKey entrance) {
  const std::vector<uint32_t> nodes = guestNetwork(world).anchoredNodes(entrance);
  REQUIRE_FALSE(nodes.empty());
  return guestNetwork(world).nodePlace(nodes.front());
}

// The least Distance at the place among guest route distance's entries whose source is an
// entrance, or none when no entrance has an entry there.
inline std::optional<double> homeDistance(const World &world, const Place &place) {
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  std::optional<double> least;
  for (const SampledEntry<RouteEntry> &sampled :
       sampleField<RouteDistance<PathKind::Guest>>(world, guestNetwork(world), place)) {
    const bool fromEntrance = std::ranges::any_of(
        entrances, [&](const ParkEntrance &entrance) { return entrance.Key == sampled.Source; });
    if (fromEntrance &&
        sampled.Value.Distance < least.value_or(std::numeric_limits<double>::infinity())) {
      least = sampled.Value.Distance;
    }
  }
  return least;
}

inline GuestRecord recordOf(const World &world, EntityKey guest) {
  const std::optional<GuestRecord> record = guestRecord(world, guest);
  REQUIRE(record.has_value());
  return record.value_or(GuestRecord{});
}

inline bool isLive(const World &world, EntityKey key) {
  const std::vector<EntityKey> keys = world.keys();
  return std::ranges::find(keys, key) != keys.end();
}

// The guest's uniform draw for the purpose, made in the cycle stepping the tick.
inline double guestDraw(const World &world, EntityKey guest, std::string_view purpose,
                        uint64_t tick) {
  return drawUniform(DrawKey{world.Seed, guest, hashName(purpose), tick, 0});
}

// What a guest admitted in the cycle stepping the tick draws from its key.
inline uint64_t drawnStayUntil(const World &world, EntityKey guest, uint64_t tick) {
  const double span = static_cast<double>(STAY_MAX - STAY_MIN);
  return tick + STAY_MIN +
         static_cast<uint64_t>(std::floor(guestDraw(world, guest, "guest-stay", tick) * span));
}

inline double drawnHungerRate(const World &world, EntityKey guest, uint64_t tick) {
  return HUNGER_RATE_MIN +
         (guestDraw(world, guest, "guest-hunger-rate", tick) * (HUNGER_RATE_MAX - HUNGER_RATE_MIN));
}

inline double drawnStartingHunger(const World &world, EntityKey guest, uint64_t tick) {
  return guestDraw(world, guest, "guest-starting-hunger", tick) * STARTING_HUNGER_MAX;
}

// Steps the world until the tick is the one given, with no commands.
inline void stepUntil(World &world, uint64_t tick) {
  while (world.Tick < tick) {
    stepWorld(world);
  }
}

// The tick of the cycle that admits the first guests.
inline constexpr uint64_t FIRST_ARRIVAL = ARRIVAL_INTERVAL - 1;

// The legs park: an entrance whose door joins a near leg, a guest path 20 m long, whose far end
// starts a far leg, a guest path 40 m long running on in line with it.
inline constexpr EntityKey LEGS_GATE{1};
inline constexpr EntityKey NEAR_LEG{2};
inline constexpr EntityKey FAR_LEG{3};

inline std::vector<ParkPoint> nearLegPoints() { return {{0.0, 123.0}, {0.0, 103.0}}; }

inline World legsWorld() {
  return resolvedWorld(ParkIntent{.Entrances = {northGate(LEGS_GATE, 0.0)},
                                  .Paths = {guestPath(NEAR_LEG, nearLegPoints()),
                                            guestPath(FAR_LEG, {{0.0, 103.0}, {0.0, 63.0}})},
                                  .Boxes = {}});
}

// Steps the legs park until its first guest stands on the far leg, and gives that guest's key.
// Guests behind it are still on the near leg or the entrance's connector then.
inline EntityKey walkOntoFarLeg(World &world) {
  stepUntil(world, FIRST_ARRIVAL);
  const EntityKey guest{world.nextKey()};
  bool onFarLeg = false;
  for (int cycle = 0; cycle < 1000 && !onFarLeg; ++cycle) {
    stepWorld(world);
    onFarLeg = recordOf(world, guest).At.Carrier == FAR_LEG;
  }
  REQUIRE(onFarLeg);
  return guest;
}

// A guest after one cycle: the tick the cycle stepped, its record, none once it has left, and the
// least entrance distance at its place.
struct GuestStep {
  uint64_t Stepped = 0;
  std::optional<GuestRecord> Record;
  std::optional<double> Home;
  bool Live = false;
};

// Steps the world with no commands, recording the guest after each cycle, until the guest has left
// or the cycles run out.
inline std::vector<GuestStep> traceGuest(World &world, EntityKey guest, uint64_t cycles) {
  std::vector<GuestStep> trace;
  for (uint64_t cycle = 0; cycle < cycles; ++cycle) {
    const uint64_t stepped = world.Tick;
    stepWorld(world);
    const std::optional<GuestRecord> record = guestRecord(world, guest);
    const std::optional<double> home =
        record.has_value() ? homeDistance(world, record.value_or(GuestRecord{}).At) : std::nullopt;
    const GuestStep step{
        .Stepped = stepped, .Record = record, .Home = home, .Live = isLive(world, guest)};
    trace.push_back(step);
    if (!step.Record.has_value()) {
      break;
    }
  }
  return trace;
}

} // namespace tpj::test

#endif
