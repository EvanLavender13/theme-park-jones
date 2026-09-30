#ifndef TPJ_TESTS_SIM_SUPPORT_GUEST_PARKS_H
#define TPJ_TESTS_SIM_SUPPORT_GUEST_PARKS_H

#include "support/park_worlds.h"

#include "sim/command_queue.h"
#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/guests/footfall.h"
#include "sim/guests/guests.h"
#include "sim/medium/field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/world.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <stdint.h>
#include <string_view>
#include <utility>
#include <variant>
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

// The legs park: an entrance whose door joins a near leg, a guest path 4 m long, whose far end
// starts a far leg, a guest path 20 m long running on in line with it. The far leg starts 6 m from
// the door, beyond CONNECTION_REACH, so deleting the near leg stops arrivals. The near leg is short
// so that few guests are behind the first when it reaches the far leg.
inline constexpr EntityKey LEGS_GATE{1};
inline constexpr EntityKey NEAR_LEG{2};
inline constexpr EntityKey FAR_LEG{3};

inline std::vector<ParkPoint> nearLegPoints() { return {{0.0, 123.0}, {0.0, 119.0}}; }

inline World legsWorld() {
  return resolvedWorld(ParkIntent{.Entrances = {northGate(LEGS_GATE, 0.0)},
                                  .Paths = {guestPath(NEAR_LEG, nearLegPoints()),
                                            guestPath(FAR_LEG, {{0.0, 119.0}, {0.0, 99.0}})},
                                  .Boxes = {}});
}

// Steps the legs park, each cycle by step(world, commands), until its first guest stands on the far
// leg, and gives that guest's key. Guests behind it are still on the near leg or the entrance's
// connector then.
template <typename Step> EntityKey walkOntoFarLeg(World &world, Step step) {
  while (world.Tick < FIRST_ARRIVAL) {
    // step takes the queue by non-const reference, which the check cannot see through Step.
    CommandQueue none; // NOLINT(misc-const-correctness)
    step(world, none);
  }
  const EntityKey guest{world.nextKey()};
  bool onFarLeg = false;
  for (int cycle = 0; cycle < 1000 && !onFarLeg; ++cycle) {
    // step takes the queue by non-const reference, which the check cannot see through Step.
    CommandQueue none; // NOLINT(misc-const-correctness)
    step(world, none);
    onFarLeg = recordOf(world, guest).At.Carrier == FAR_LEG;
  }
  REQUIRE(onFarLeg);
  return guest;
}

inline void stepWith(World &world, CommandQueue &commands) { stepWorld(world, commands); }

inline EntityKey walkOntoFarLeg(World &world) { return walkOntoFarLeg(world, stepWith); }

// Steps the legs park, each cycle by step(world, commands), until its first guest stands on the far
// leg. Then deletes the near leg, which strands that guest there with no way home and leaves the
// entrance nothing to admit guests onto, and steps until the guest's stay is over and it walks
// away from the junction, so that the far leg's dead end is the next node it reaches. Then draws
// the near leg again, and gives the guest's key.
template <typename Step> EntityKey strandPastStay(World &world, Step step) {
  const EntityKey guest = walkOntoFarLeg(world, step);
  CommandQueue cut;
  cut.push(DeletePath{NEAR_LEG});
  step(world, cut);

  const uint64_t stayUntil = recordOf(world, guest).StayUntil;
  bool outward = false;
  while (world.Tick <= stayUntil || !outward) {
    REQUIRE(world.Tick <= stayUntil + 2000);
    const double distance = recordOf(world, guest).At.Distance;
    // step takes the queue by non-const reference, which the check cannot see through Step.
    CommandQueue none; // NOLINT(misc-const-correctness)
    step(world, none);
    REQUIRE(recordOf(world, guest).At.Carrier == FAR_LEG);
    outward = recordOf(world, guest).At.Distance > distance;
  }

  CommandQueue rejoin;
  rejoin.push(AddPath{PathKind::Guest, nearLegPoints()});
  step(world, rejoin);
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

// The eating park: an entrance on the north edge whose door joins a gate path, which runs on as a
// spine south to a dead end, crossed near that end by a cross path. Deleting the gate path leaves
// the entrance no path within reach, so the park admits no more guests, and drawing it again lets
// them head home. Beside the spine stand a near shop and a far shop, which a depot supplies over a
// backstage path, and across it a starved shop, whose back door reaches no backstage path. Every
// edge is far longer than WALK_STEP.
inline constexpr EntityKey EATING_GATE{1};
inline constexpr EntityKey EATING_SPINE{2};
inline constexpr EntityKey EATING_CROSS{3};
inline constexpr EntityKey EATING_BACKSTAGE{4};
inline constexpr EntityKey NEAR_SHOP{5};
inline constexpr EntityKey FAR_SHOP{6};
inline constexpr EntityKey STARVED_SHOP{7};
inline constexpr EntityKey EATING_DEPOT{8};
inline constexpr EntityKey EATING_GATE_PATH{9};

inline std::vector<ParkPoint> eatingGatePoints() { return {{0.0, 123.0}, {0.0, 119.0}}; }
inline std::vector<ParkPoint> eatingBackstagePoints() { return {{12.0, 122.0}, {12.0, 96.0}}; }

inline World eatingWorld() {
  const auto box = [](EntityKey key, BoxKind kind, Pose at) {
    return ParkBox{.Key = key, .Kind = kind, .At = at};
  };
  return resolvedWorld(
      ParkIntent{.Entrances = {northGate(EATING_GATE, 0.0)},
                 .Paths = {guestPath(EATING_GATE_PATH, eatingGatePoints()),
                           guestPath(EATING_SPINE, {{0.0, 119.0}, {0.0, 60.0}}),
                           guestPath(EATING_CROSS, {{-20.0, 70.0}, {20.0, 70.0}}),
                           ParkPath{.Key = EATING_BACKSTAGE,
                                    .Kind = PathKind::Backstage,
                                    .Points = eatingBackstagePoints()}},
                 .Boxes = {box(NEAR_SHOP, BoxKind::Shop, Pose{6.5, 115.0, -1.0, 0.0}),
                           box(FAR_SHOP, BoxKind::Shop, Pose{6.5, 104.0, -1.0, 0.0}),
                           box(STARVED_SHOP, BoxKind::Shop, Pose{-6.5, 112.0, 1.0, 0.0}),
                           box(EATING_DEPOT, BoxKind::Depot, Pose{12.0, 89.0, 0.0, 1.0})}});
}

// The key of the eating park's gate path, drawn first or again, or none while it is deleted.
inline std::optional<EntityKey> eatingGatePath(const World &world) {
  for (const ParkPath &path : parkPaths(world)) {
    if (path.Kind == PathKind::Guest && path.Points == eatingGatePoints()) {
      return path.Key;
    }
  }
  return std::nullopt;
}

// Guest route distance's entries at the place, sources ascending.
inline std::vector<SampledEntry<RouteEntry>> routesAt(const World &world, const Place &place) {
  return sampleField<RouteDistance<PathKind::Guest>>(world, guestNetwork(world), place);
}

// The source's entry among the sampled entries, or none.
inline std::optional<RouteEntry> routeFrom(const std::vector<SampledEntry<RouteEntry>> &routes,
                                           EntityKey source) {
  const auto found = std::ranges::find(routes, source, &SampledEntry<RouteEntry>::Source);
  return found == routes.end() ? std::nullopt : std::optional<RouteEntry>(found->Value);
}

// The node place of the entity's lowest anchored node on the guest network, or none.
inline std::optional<Place> anchorPlace(const World &world, EntityKey entity) {
  const std::vector<uint32_t> nodes = guestNetwork(world).anchoredNodes(entity);
  return nodes.empty() ? std::nullopt
                       : std::optional<Place>(guestNetwork(world).nodePlace(nodes.front()));
}

// The source's food offer: the first of its own entries at its anchor place, or none.
inline std::optional<OfferEntry> offerOf(const World &world, EntityKey source) {
  const std::optional<Place> anchor = anchorPlace(world, source);
  if (!anchor.has_value()) {
    return std::nullopt;
  }
  for (const SampledEntry<OfferEntry> &sampled :
       sampleField<FoodOffer>(world, guestNetwork(world), anchor.value_or(Place{}))) {
    if (sampled.Source == source) {
      return sampled.Value;
    }
  }
  return std::nullopt;
}

// The offer of each box and entrance, the entities that can be a source of route distance, keyed
// by the entity. A check reads it from the world before a cycle, since shops publish their offers
// as they step.
using ParkOffers = std::map<EntityKey, std::optional<OfferEntry>>;

inline ParkOffers parkOffers(const World &world) {
  ParkOffers offers;
  for (const ParkBox &box : parkBoxes(world)) {
    offers.emplace(box.Key, offerOf(world, box.Key));
  }
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    offers.emplace(entrance.Key, offerOf(world, entrance.Key));
  }
  return offers;
}

// The source's offer among the offers, or none.
inline std::optional<OfferEntry> offerAmong(const ParkOffers &offers, EntityKey source) {
  const auto found = offers.find(source);
  return found == offers.end() ? std::nullopt : found->second;
}

// Whether the source has an entry among the route entries and an offer that says it has meals.
inline bool isOfferReachable(const ParkOffers &offers,
                             const std::vector<SampledEntry<RouteEntry>> &routes,
                             EntityKey source) {
  const std::optional<OfferEntry> offer = offerAmong(offers, source);
  return routeFrom(routes, source).has_value() && offer.has_value() &&
         offer.value_or(OfferEntry{}).Supplied;
}

inline bool isOfferReachable(const World &world,
                             const std::vector<SampledEntry<RouteEntry>> &routes,
                             EntityKey source) {
  return isOfferReachable(parkOffers(world), routes, source);
}

inline bool isEntrance(const World &world, EntityKey key) {
  const std::vector<ParkEntrance> entrances = parkEntrances(world);
  return std::ranges::any_of(entrances,
                             [key](const ParkEntrance &entrance) { return entrance.Key == key; });
}

// The least entrance entry among the route entries, ties to the lower source key, or none.
inline std::optional<RouteEntry> homeEntry(const World &world,
                                           const std::vector<SampledEntry<RouteEntry>> &routes) {
  std::optional<RouteEntry> least;
  for (const SampledEntry<RouteEntry> &sampled : routes) {
    if (isEntrance(world, sampled.Source) &&
        (!least.has_value() || sampled.Value.Distance < least.value_or(RouteEntry{}).Distance)) {
      least = sampled.Value;
    }
  }
  return least;
}

inline bool isAtNode(const World &world, const Place &place) {
  const std::optional<NetworkPosition> position = guestNetwork(world).resolve(place);
  return position.has_value() && std::holds_alternative<NodePosition>(position.value());
}

// The place of the edge's carrier halfway between its two stops.
inline Place midpointOf(const NetworkEdge &edge) {
  return Place{edge.Carrier, (edge.FromDistance + edge.ToDistance) / 2.0};
}

// Hungry footfall's value at the place on the guest network.
inline double footfallAt(const World &world, const Place &place) {
  return fieldValue<HungryFootfall>(world, guestNetwork(world), place);
}

// The tick of the guest's last choice, or none before its first.
inline std::optional<uint64_t> choiceTick(const GuestRecord &record) {
  return record.LastChoice.has_value()
             ? std::optional<uint64_t>(record.LastChoice.value_or(GuestChoice{}).Tick)
             : std::nullopt;
}

} // namespace tpj::test

#endif
