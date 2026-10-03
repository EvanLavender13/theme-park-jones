#include "sim/guests/guests.h"

#include "sim/draw.h"
#include "sim/guests/internal/footfall.h"
#include "sim/guests/internal/guest.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/operations/operations.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/sim_math.h"
#include "sim/world.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

using GuestRouteDistance = RouteDistance<PathKind::Guest>;
using RouteSample = std::vector<SampledEntry<RouteEntry>>;

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

// The least entrance entry in the sample, ties to the lower key, or none.
const RouteEntry *homeEntry(const RouteSample &routes, const std::vector<EntityKey> &entrances) {
  const RouteEntry *nearest = nullptr;
  // Sources come in ascending key order, so keeping only a strictly less distance breaks ties
  // to the lower key.
  for (const SampledEntry<RouteEntry> &entry : routes) {
    if (std::ranges::binary_search(entrances, entry.Source) &&
        (nearest == nullptr || entry.Value.Distance < nearest->Distance)) {
      nearest = &entry.Value;
    }
  }
  return nearest;
}

// The least entrance entry at the place, ties to the lower key, or none, as homeEntry finds it in a
// sample there.
std::optional<RouteEntry> homeEntryAt(const World &world, const Network &network,
                                      const Place &place, const std::vector<EntityKey> &entrances) {
  std::optional<RouteEntry> nearest;
  // Entrances come in ascending key order, so keeping only a strictly less distance breaks ties
  // to the lower key.
  for (const EntityKey entrance : entrances) {
    const std::optional<RouteEntry> entry =
        routeEntryAt(world, PathKind::Guest, network, place, entrance);
    if (entry && (!nearest || entry->Distance < nearest->Distance)) {
      nearest = entry;
    }
  }
  return nearest;
}

// The source's offer when it says meals are supplied: its first food-offer entry at the place of
// its lowest anchored node, or none. With an entry in route distance at the guest's place, the
// offer is reachable.
std::optional<OfferEntry> suppliedOffer(const World &world, const Network &network,
                                        EntityKey source) {
  const std::optional<uint32_t> anchor = network.firstAnchoredNode(source);
  if (!anchor) {
    return std::nullopt;
  }
  const std::optional<OfferEntry> offer =
      sourceEntryAtNode<FoodOffer>(world, network, *anchor, source);
  if (!offer || !offer->Supplied) {
    return std::nullopt;
  }
  return offer;
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

// The options a guest weighs where it stands, scored as a choice scores them, and each offer's
// relief by option index, for the meal it would give.
struct ScoredOptions {
  std::vector<ChoiceOption> Options;
  std::vector<double> Reliefs;
};

ScoredOptions scoreOptions(const World &world, const Guest &guest, const Network &network,
                           const RouteSample &routes, const std::vector<EntityKey> &entrances) {
  ScoredOptions scored;
  const double curve = hungerCurve(guest.Hunger);
  for (const SampledEntry<RouteEntry> &entry : routes) {
    const std::optional<OfferEntry> offer = suppliedOffer(world, network, entry.Source);
    if (!offer) {
      continue;
    }
    ChoiceOption option;
    option.Kind = ChoiceKind::Offer;
    option.Shop = entry.Source;
    option.Relief = RELIEF_WEIGHT * curve * offer->Relief;
    option.Distance = DISTANCE_WEIGHT * entry.Value.Distance;
    option.Wait = WAIT_WEIGHT * (static_cast<double>(offer->Wait) * SIM_TICK_SECONDS);
    option.Commitment = entry.Source == guest.Target ? COMMITMENT_BONUS : 0.0;
    option.Score = option.Relief + option.Distance + option.Wait + option.Commitment;
    scored.Options.push_back(option);
    scored.Reliefs.push_back(offer->Relief);
  }
  ChoiceOption carryOn;
  carryOn.Kind = ChoiceKind::CarryOn;
  carryOn.Score = CARRY_ON_SCORE;
  scored.Options.push_back(carryOn);
  if (world.Tick >= guest.StayUntil && homeEntry(routes, entrances) != nullptr) {
    ChoiceOption headHome;
    headHome.Kind = ChoiceKind::HeadHome;
    headHome.Score = HEAD_HOME_SCORE;
    scored.Options.push_back(headHome);
  }
  return scored;
}

// Sets each option's softmax probability and gives the weights they came from. Throws
// std::invalid_argument for no options or a score that is not finite.
std::vector<double> softmaxWeights(std::span<ChoiceOption> options) {
  if (options.empty()) {
    throw std::invalid_argument("a choice needs at least one option");
  }
  double greatest = -std::numeric_limits<double>::infinity();
  for (const ChoiceOption &option : options) {
    if (!std::isfinite(option.Score)) {
      throw std::invalid_argument("a choice's score is not finite");
    }
    greatest = std::max(greatest, option.Score);
  }
  // Scores are shifted so the greatest weighs exactly 1: no weight overflows, and the total is at
  // least 1.
  std::vector<double> weights;
  double total = 0.0;
  for (const ChoiceOption &option : options) {
    weights.push_back(simExp((option.Score - greatest) / CHOICE_TEMPERATURE));
    total += weights.back();
  }
  for (size_t i = 0; i < options.size(); ++i) {
    options[i].Probability = weights[i] / total;
  }
  return weights;
}

// Samples route distance where the guest stands, scores its options there, picks one by softmax,
// and takes it up.
void choose(const World &world, EntityKey key, Guest &guest, const Network &network,
            const std::vector<EntityKey> &entrances, uint64_t &choices) {
  const RouteSample routes = sampleField<GuestRouteDistance>(world, network, guest.At);
  ScoredOptions scored = scoreOptions(world, guest, network, routes, entrances);
  const size_t picked =
      softmaxPick(drawKey(world, key, hashName("guest-choice"), choices++), scored.Options);
  const ChoiceOption &option = scored.Options[picked];
  guest.Target = NULL_KEY;
  if (option.Kind == ChoiceKind::Offer) {
    guest.Activity = GuestActivity::HeadingToShop;
    guest.Target = option.Shop;
    guest.MealRelief = scored.Reliefs[picked];
  } else if (option.Kind == ChoiceKind::HeadHome) {
    guest.Activity = GuestActivity::HeadingHome;
  } else {
    guest.Activity = GuestActivity::Wandering;
  }
}

// Sends the guest's visit to the shop: one unit created under its own key, arriving after
// VISIT_DELAY.
void sendVisit(World &world, EntityKey key, EntityKey shop) {
  createUnits<GuestVisits>(world, key, key, 1);
  sendUnits<GuestVisits>(world, key, shop, key, 1, VISIT_DELAY);
}

// Walks the guest along the step, by the lesser of the distance left and the gap to the step's
// To, which it subtracts from the distance left. At a node it joins the step's carrier at From.
void walkStep(Guest &guest, const RouteStep &step, bool atNode, double &left) {
  if (atNode) {
    guest.At = {step.Carrier, step.From};
  }
  guest.Forward = step.To > guest.At.Distance;
  const double gap = std::abs(step.To - guest.At.Distance);
  if (left < gap) {
    // Rounding never carries the guest past the stop it walks toward.
    const double walked = guest.Forward ? guest.At.Distance + left : guest.At.Distance - left;
    guest.At.Distance = guest.Forward ? std::min(walked, step.To) : std::max(walked, step.To);
    left = 0.0;
  } else {
    guest.At.Distance = step.To;
    left -= gap;
  }
}

// Where the guest's heading takes it from its place: its target shop's route entry or the least
// entrance entry, whichever it heads for. A guest heading to a shop whose offer it can no longer
// reach drops the target, and one heading home with no entrance entry stops, each becoming
// wandering; Dropped says it must choose.
struct Heading {
  std::optional<RouteEntry> Target;
  std::optional<RouteEntry> Home;
  bool Dropped = false;
};

Heading headingOf(const World &world, Guest &guest, const Network &network,
                  const std::vector<EntityKey> &entrances) {
  Heading heading;
  if (guest.Activity == GuestActivity::HeadingToShop) {
    heading.Target = routeEntryAt(world, PathKind::Guest, network, guest.At, guest.Target);
    if (!heading.Target || !suppliedOffer(world, network, guest.Target)) {
      guest.Activity = GuestActivity::Wandering;
      guest.Target = NULL_KEY;
      heading.Target.reset();
      heading.Dropped = true;
    }
  }
  if (guest.Activity == GuestActivity::HeadingHome) {
    heading.Home = homeEntryAt(world, network, guest.At, entrances);
    if (!heading.Home) {
      guest.Activity = GuestActivity::Wandering;
      heading.Dropped = true;
    }
  }
  return heading;
}

// Walks the guest WALK_STEP along the guest network. It drops a target it can no longer reach,
// sends its visit at its target's anchor, and chooses when it must and at every node it leaves,
// at most once between steps. Returns false when the guest reaches its entrance heading home,
// and leaves.
bool walk(World &world, EntityKey key, Guest &guest, const Network &network,
          const std::vector<EntityKey> &entrances) {
  double left = WALK_STEP;
  uint64_t picks = 0;
  uint64_t choices = 0;
  bool chosen = false;
  bool mustChoose = false;
  for (;;) {
    const std::optional<NetworkPosition> position = network.resolve(guest.At);
    if (!position) {
      return false;
    }
    const Heading heading = headingOf(world, guest, network, entrances);
    mustChoose = mustChoose || heading.Dropped;
    if (heading.Target && heading.Target->Next.Carrier == NULL_KEY) {
      sendVisit(world, key, guest.Target);
      guest.Activity = GuestActivity::Waiting;
      return true;
    }
    if (heading.Home && heading.Home->Next.Carrier == NULL_KEY) {
      return false;
    }
    const bool atNode = std::holds_alternative<NodePosition>(*position);
    if (!chosen && (mustChoose || (atNode && left > 0.0))) {
      choose(world, key, guest, network, entrances, choices);
      chosen = true;
      mustChoose = false;
      continue;
    }
    if (left <= 0.0) {
      return true;
    }
    RouteStep step;
    if (heading.Target) {
      step = heading.Target->Next;
    } else if (heading.Home) {
      step = heading.Home->Next;
    } else {
      step = wanderStep(world, key, guest, network, *position, picks);
    }
    walkStep(guest, step, atNode, left);
    chosen = false;
  }
}

// The guest's uniform draw for the purpose, on its key and the world's seed and tick.
double guestDraw(const World &world, EntityKey guest, const char *purpose) {
  return drawUniform(drawKey(world, guest, hashName(purpose), 0));
}

// Each entrance with a guest connector admits a guest at the end of every ARRIVAL_INTERVAL
// ticks, through addGuest, with its stay drawn on the key addGuest gives it.
void admitGuests(World &world, const Network &network) {
  if ((world.Tick + 1) % ARRIVAL_INTERVAL != 0) {
    return;
  }
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    const std::vector<uint32_t> anchored = network.anchoredNodes(entrance.Key);
    if (anchored.empty()) {
      continue;
    }
    const EntityKey key{world.nextKey()};
    const uint64_t stayUntil = world.Tick + STAY_MIN +
                               static_cast<uint64_t>(guestDraw(world, key, "guest-stay") *
                                                     static_cast<double>(STAY_MAX - STAY_MIN));
    addGuest(world, network.nodePlace(anchored.front()), stayUntil);
  }
}

// A waiting guest's visit has come back when it holds it: it finishes the visit, eats any meal
// that came with it, and stops waiting. Returns whether it is still waiting.
bool awaitVisit(World &world, EntityKey key, Guest &guest) {
  const int64_t visits = unitsHeld<GuestVisits>(world, key, key);
  if (visits == 0) {
    return true;
  }
  consumeUnits<GuestVisits>(world, key, key, visits, FINISHED_CAUSE);
  const int64_t meals = unitsHeld<Meals>(world, key, key);
  if (meals > 0) {
    consumeUnits<Meals>(world, key, key, meals, EATEN_CAUSE);
    const double before = guest.Hunger;
    guest.Hunger = std::max(0.0, before - guest.MealRelief);
    ++guest.MealsEaten;
    guest.LastMeal = {world.Tick, before, guest.Hunger};
  }
  guest.Activity = GuestActivity::Wandering;
  guest.Target = NULL_KEY;
  return false;
}

// Each guest, in ascending key order, leaves when its place does not resolve, which carrying
// leaves only when the guest network has no carrier, and otherwise gets hungrier, and unless it is
// still waiting for its visit to come back, walks. Guests that leave go after all have stepped,
// and then the entrances admit new ones.
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
    if (guest.Activity == GuestActivity::Waiting && awaitVisit(world, key, guest)) {
      continue;
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

// Carries each guest's place from the guest network before the resolution to the one it derived:
// by carryOver, and for a retired place, to the new network's nearest place to where the guest
// stood. A place with neither is left as it is, and its guest leaves when it next steps. Nothing
// changes in a world's first resolution, which has no network before.
void carryGuests(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  if (before == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  for (const EntityKey key : parkGuests(world)) {
    Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    if (const std::optional<Place> carried = carryOver(guest.At, *before, after)) {
      guest.At = *carried;
      continue;
    }
    const std::optional<GroundPoint> stood = before->groundPoint(guest.At);
    if (!stood) {
      continue;
    }
    if (const std::optional<Place> nearest = after.nearestPlace(*stood)) {
      guest.At = *nearest;
    }
  }
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
  GuestRecord record;
  record.Activity = state->Activity;
  record.At = state->At;
  record.Position = parkNetwork(world, PathKind::Guest).groundPoint(state->At);
  record.Hunger = state->Hunger;
  record.StayUntil = state->StayUntil;
  record.Target = state->Target;
  record.MealsEaten = state->MealsEaten;
  if (state->MealsEaten > 0) {
    record.LastMeal = state->LastMeal;
  }
  return record;
}

std::optional<std::vector<ChoiceOption>> guestOptions(const World &world, EntityKey guest) {
  const entt::entity entity = world.findEntity(guest);
  if (entity == entt::null) {
    return std::nullopt;
  }
  const Guest *state = world.Registry.try_get<Guest>(entity);
  if (state == nullptr) {
    return std::nullopt;
  }
  const Network &network = parkNetwork(world, PathKind::Guest);
  if (!network.resolve(state->At)) {
    return std::nullopt;
  }
  const RouteSample routes = sampleField<GuestRouteDistance>(world, network, state->At);
  ScoredOptions scored = scoreOptions(world, *state, network, routes, entranceKeys(world));
  softmaxWeights(scored.Options);
  return std::move(scored.Options);
}

EntityKey addGuest(World &world, const Place &place, uint64_t stayUntil) {
  if (!parkNetwork(world, PathKind::Guest).resolve(place)) {
    throw std::invalid_argument("addGuest: the place is not on the guest network");
  }
  const EntityKey key = world.createEntity();
  Guest guest;
  guest.At = place;
  guest.StayUntil = stayUntil;
  guest.HungerRate = HUNGER_RATE_MIN + guestDraw(world, key, "guest-hunger-rate") *
                                           (HUNGER_RATE_MAX - HUNGER_RATE_MIN);
  guest.Hunger = guestDraw(world, key, "guest-starting-hunger") * STARTING_HUNGER_MAX;
  world.Registry.emplace<Guest>(world.findEntity(key), guest);
  return key;
}

double hungerCurve(double hunger) {
  const double h = std::clamp(hunger, HUNGER_CURVE.front().X, HUNGER_CURVE.back().X);
  for (size_t i = 1; i < HUNGER_CURVE.size(); ++i) {
    const CurvePoint &a = HUNGER_CURVE[i - 1];
    const CurvePoint &b = HUNGER_CURVE[i];
    if (h == a.X) {
      return a.Y;
    }
    if (h < b.X) {
      return a.Y + (h - a.X) * (b.Y - a.Y) / (b.X - a.X);
    }
  }
  return HUNGER_CURVE.back().Y;
}

size_t softmaxPick(const DrawKey &key, std::span<ChoiceOption> options) {
  const std::vector<double> weights = softmaxWeights(options);
  return drawPick(key, std::span<const double>(weights));
}

void addGuests(WorldSchema &schema) {
  schema.addComponent<Guest>("guest", DataKind::State);
  schema.addSystem(&stepGuests);
  schema.addFinisher(&carryGuests);
  addFootfall(schema);
}

} // namespace tpj
