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
RouteStep wanderStep(const World &world, EntityKey key, const Guest &guest, const Network &network,
                     const NetworkPosition &position, uint64_t &picks) {
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
    guest.StayUntil =
        world.Tick + STAY_MIN +
        static_cast<uint64_t>(draw("guest-stay") * static_cast<double>(STAY_MAX - STAY_MIN));
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
