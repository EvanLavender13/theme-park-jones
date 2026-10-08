#include "sim/guests/guests.h"

#include "sim/draw.h"
#include "sim/guests/internal/footfall.h"
#include "sim/guests/internal/ground.h"
#include "sim/guests/internal/guest.h"
#include "sim/guests/internal/route_ahead.h"
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
// An aim that moves less than this, in meters, when it is advanced, can come no closer to the end
// of its route, so halving can't creep it on for ever.
constexpr double AIM_SETTLED = 0.01;

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

// Lists walking reuses from guest to guest within a step, so it allocates nothing per guest once
// they have grown.
struct WalkLists {
  RouteAhead Route;
  std::vector<RouteStep> Steps;
  std::vector<uint64_t> Weights;
};

// A wandering guest's first step, which draws nothing: inside an edge, on to the stop ahead; at a
// node, its turn when the turn starts there, or on through a shaping point; at any other node, a
// step with carrier NULL_KEY, so its route is its place alone until it makes its choice there.
RouteStep firstWanderStep(const Guest &guest, const Network &network,
                          const NetworkPosition &position) {
  if (const auto *inside = std::get_if<EdgePosition>(&position)) {
    const NetworkEdge &edge = network.edges()[inside->Edge];
    return {guest.At.Carrier, guest.At.Distance,
            guest.Forward ? edge.ToDistance : edge.FromDistance};
  }
  const uint32_t node = std::get<NodePosition>(position).Node;
  if (startsAt(network, guest.Turn, node)) {
    return guest.Turn;
  }
  if (isShapingPoint(network, node)) {
    return onwardStep(network, node, guest.At, guest.Forward).value_or(RouteStep{});
  }
  return {};
}

// The step a wanderer arriving at the place, walking the way forward gives, picks out of the node:
// a keyed pick among the steps out of it other than the way back, each walked along its edge away
// from the node in the order edgeEnds gives them, or the way back when there is no other.
RouteStep pickTurn(const World &world, EntityKey key, const Network &network, uint32_t node,
                   const Place &at, bool forward, uint64_t &picks, WalkLists &lists) {
  lists.Steps.clear();
  RouteStep back;
  for (const EdgeEnd &end : network.edgeEnds(node)) {
    const RouteStep step = stepAway(network, end);
    if (isWayBack(step, at, forward)) {
      back = step;
    } else {
      lists.Steps.push_back(step);
    }
  }
  if (lists.Steps.empty()) {
    return back;
  }
  lists.Weights.assign(lists.Steps.size(), 1);
  return lists.Steps[drawPick(drawKey(world, key, hashName("guest-wander"), picks++),
                              std::span<const uint64_t>(lists.Weights))];
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
    heading.Home = leastEntryAt(world, network, guest.At, entrances);
    if (!heading.Home) {
      guest.Activity = GuestActivity::Wandering;
      heading.Dropped = true;
    }
  }
  return heading;
}

// Where a pass of a guest's walk left it along its route: the spot of its place, and its aim's spot
// before and after the pass advanced it.
struct WalkedPass {
  RouteSpot At;
  RouteSpot Aimed;
  RouteSpot Aim;
};

// Advances the guest's aim along the route and walks it straight toward the aim, by as much of the
// distance left as that takes, which it subtracts. The line to the aim is clear, so every point
// along it is too. The guest's place becomes the spot of the route nearest where it now stands,
// never past the aim, and it walks the way that spot's stretch runs.
WalkedPass walkTowardAim(Guest &guest, const GuestGround &ground, uint32_t group,
                         const RouteAhead &route, double &left) {
  WalkedPass pass;
  pass.Aimed = spotOf(route, guest.Aim).value_or(RouteSpot{});
  pass.Aim = advanceAim(ground, group, guest.Ground, route, pass.Aimed);
  const GroundPoint toward = pointAt(route, pass.Aim);
  const double gapX = toward.X - guest.Ground.X;
  const double gapZ = toward.Z - guest.Ground.Z;
  const double gap = std::sqrt((gapX * gapX) + (gapZ * gapZ));
  if (left >= gap) {
    guest.Ground = toward;
    left -= gap;
  } else {
    guest.Ground = {guest.Ground.X + (gapX * left / gap), guest.Ground.Z + (gapZ * left / gap)};
    left = 0.0;
  }
  pass.At = nearestSpot(route, guest.Ground, pass.Aim);
  guest.At = placeAt(route, pass.At);
  guest.Aim = placeAt(route, pass.Aim);
  if (pass.At.Point > 0) {
    const RoutePoint &stretch = route.Points[pass.At.Point];
    guest.Forward = stretch.To > stretch.From;
  }
  return pass;
}

// Whether the guest has reached the end of its route: it stands on it, or it stands at its aim with
// no node of the route between the aim and the end, and the pass moved the aim less than
// AIM_SETTLED, so its box can come no closer. A door's end lies along its face, so where the
// connector meets it at a slant, or its last part is short, the nearest the box can come may lie
// well back from the door.
bool reachedEnd(const Guest &guest, const RouteAhead &route, const WalkedPass &pass) {
  if (!route.Ends) {
    return false;
  }
  const size_t last = route.Points.size() - 1;
  if (guest.Ground == route.Points[last].At) {
    return true;
  }
  const GroundPoint aim = pointAt(route, pass.Aim);
  const GroundPoint aimed = pointAt(route, pass.Aimed);
  const double movedX = aim.X - aimed.X;
  const double movedZ = aim.Z - aimed.Z;
  return guest.Ground == aim && !nodeAhead(route, pass.Aim) &&
         (movedX * movedX) + (movedZ * movedZ) < AIM_SETTLED * AIM_SETTLED;
}

// What a pass of a guest's walk did: reached the end of its route, moved its place, moved only
// where it stands, or moved nothing.
enum class Pass : uint8_t { ReachedEnd, PlaceMoved, GroundMoved, Stayed };

// One pass of a guest's walk along its route: it walks toward its aim, and at the end of its route
// its place and aim become the route's end.
Pass walkPass(Guest &guest, const GuestGround &ground, uint32_t group, const RouteAhead &route,
              double &left) {
  const Place was = guest.At;
  const GroundPoint stood = guest.Ground;
  const WalkedPass pass = walkTowardAim(guest, ground, group, route, left);
  if (reachedEnd(guest, route, pass)) {
    guest.At = placeAt(route, {route.Points.size() - 1, 1.0});
    guest.Aim = guest.At;
    return Pass::ReachedEnd;
  }
  if (guest.At != was) {
    return Pass::PlaceMoved;
  }
  return guest.Ground != stood ? Pass::GroundMoved : Pass::Stayed;
}

// Traces the guest's route ahead: by its target's entries toward its shop, by the entrances' toward
// home, or wandering on from its first wander step with its turn.
void traceGuestRoute(const World &world, const Guest &guest, const Network &network,
                     const NetworkPosition &position, const Heading &heading,
                     const std::vector<EntityKey> &entrances, RouteAhead &route) {
  if (heading.Target) {
    traceRoute(world, network, guest.At, heading.Target->Next,
               std::span<const EntityKey>(&guest.Target, 1), guest.Turn, route);
  } else if (heading.Home) {
    traceRoute(world, network, guest.At, heading.Home->Next, entrances, guest.Turn, route);
  } else {
    traceRoute(world, network, guest.At, firstWanderStep(guest, network, position), {}, guest.Turn,
               route);
  }
}

// How far a guest's walk has got within a step: the distance it has left, its draws so far for
// wandering picks and for choices, whether it has chosen since its place last moved, and whether it
// must choose.
struct WalkProgress {
  double Left = WALK_STEP;
  uint64_t Picks = 0;
  uint64_t Choices = 0;
  bool Chosen = false;
  bool MustChoose = false;
};

// Makes the guest's choice for the route's node at the index. It chooses unless it has since its
// place last moved, and then its aim starts again from its place. It then sets its turn out of the
// node: a wanderer's pick, or for any other guest the empty step at the node.
void chooseFor(World &world, EntityKey key, Guest &guest, const Network &network,
               const std::vector<EntityKey> &entrances, size_t index, WalkProgress &progress,
               WalkLists &lists) {
  const RoutePoint &point = lists.Route.Points[index];
  const Place at{point.Carrier, point.To};
  const bool forward = index == 0 ? guest.Forward : point.To > point.From;
  const uint32_t node = point.NodeIndex;
  if (!progress.Chosen) {
    choose(world, key, guest, network, entrances, progress.Choices);
    progress.Chosen = true;
    guest.Aim = guest.At;
  }
  guest.Turn = guest.Activity == GuestActivity::Wandering
                   ? pickTurn(world, key, network, node, at, forward, progress.Picks, lists)
                   : RouteStep{at.Carrier, at.Distance, at.Distance};
}

// Walks the guest WALK_STEP across the guest paving, straight toward its aim along its route. It
// drops a target it can no longer reach, and sends its visit when it reaches the end of its route
// at its target. It makes its choice for each node of its route, other than a shaping point and the
// node it is heading to, when it comes within TURN_LOOKAHEAD of it or stands on it, and chooses
// when it must, at most once until its place moves. Its turn clears once its place passes the
// turn's node. Returns false when the guest reaches its entrance heading home, and leaves.
bool walk(World &world, EntityKey key, Guest &guest, const Network &network,
          const GuestGround &ground, const std::vector<EntityKey> &entrances, WalkLists &lists) {
  WalkProgress progress;
  for (;;) {
    const std::optional<NetworkPosition> position = network.resolve(guest.At);
    if (!position) {
      return false;
    }
    const Heading heading = headingOf(world, guest, network, entrances);
    progress.MustChoose = progress.MustChoose || heading.Dropped;
    if (heading.Target && heading.Target->Next.Carrier == NULL_KEY) {
      sendVisit(world, key, guest.Target);
      guest.Activity = GuestActivity::Waiting;
      return true;
    }
    if (heading.Home && heading.Home->Next.Carrier == NULL_KEY) {
      return false;
    }
    if (!progress.Chosen && progress.MustChoose) {
      choose(world, key, guest, network, entrances, progress.Choices);
      progress.Chosen = true;
      progress.MustChoose = false;
      // A new choice can change the route, so the aim starts again from the place.
      guest.Aim = guest.At;
      guest.Turn = {};
      continue;
    }
    if (progress.Left <= 0.0) {
      return true;
    }
    const std::optional<uint32_t> group = ground.groupOf(guest.At.Carrier);
    if (!group) {
      return true;
    }
    traceGuestRoute(world, guest, network, *position, heading, entrances, lists.Route);
    const RouteAhead &route = lists.Route;
    if (route.Points.empty()) {
      return true;
    }
    const std::optional<size_t> node =
        turnNode(route, heading.Target.has_value() || heading.Home.has_value());
    if (node && !startsAt(network, guest.Turn, route.Points[*node].NodeIndex)) {
      chooseFor(world, key, guest, network, entrances, *node, progress, lists);
      continue;
    }

    const Pass pass = walkPass(guest, ground, *group, route, progress.Left);
    if (node && passedPoint(route, spotOf(route, guest.At).value_or(RouteSpot{}), *node)) {
      guest.Turn = {};
    }
    switch (pass) {
    case Pass::ReachedEnd:
    case Pass::PlaceMoved:
      progress.Chosen = false;
      break;
    case Pass::GroundMoved:
      break;
    case Pass::Stayed:
      return true;
    }
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
// that came with it, and stops waiting. It forgets any choice it made at the shop's door, so it
// chooses again there. Returns whether it is still waiting.
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
  guest.Turn = {};
  return false;
}

// Each guest, in ascending key order, leaves when its place does not resolve, which carrying
// leaves only when the guest network has no carrier, and otherwise gets hungrier, and unless it is
// still waiting for its visit to come back, walks. Guests that leave go after all have stepped,
// and then the entrances admit new ones.
void stepGuests(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const GuestGround &ground = guestGround(world);
  const std::vector<EntityKey> entrances = entranceKeys(world);
  std::vector<EntityKey> leaving;
  WalkLists lists;
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
    if (!walk(world, key, guest, network, ground, entrances, lists)) {
      leaving.push_back(key);
    }
  }
  for (const EntityKey key : leaving) {
    world.destroyEntity(key);
  }
  admitGuests(world, network);
}

// Turns the guest about when the carrier its place is carried to runs against the one it stood
// on, there, so it keeps walking the same way over the ground. If either direction is unknown, or
// the two are square to each other, it keeps its way.
void keepWayOverGround(Guest &guest, const Network &before, const Network &after,
                       const Place &carried) {
  const std::optional<GroundDirection> was = before.directionAt(guest.At);
  const std::optional<GroundDirection> now = after.directionAt(carried);
  if (was && now && (was->X * now->X) + (was->Z * now->Z) < 0.0) {
    guest.Forward = !guest.Forward;
  }
}

// Whether both points are known and lie within ON_GROUND_TOLERANCE of each other.
bool sameGround(const std::optional<GroundPoint> &a, const std::optional<GroundPoint> &b) {
  if (!a || !b) {
    return false;
  }
  const double x = b->X - a->X;
  const double z = b->Z - a->Z;
  return (x * x) + (z * z) <= ON_GROUND_TOLERANCE * ON_GROUND_TOLERANCE;
}

// The place carried across the edit, on whichever carrier passes there, when it stays at the same
// ground position.
std::optional<Place> heldPlace(const Place &place, const Network &before, const Network &after) {
  const std::optional<Place> carried = carryOver(place, before, after);
  if (!carried || !sameGround(before.groundPoint(place), after.groundPoint(*carried))) {
    return std::nullopt;
  }
  return carried;
}

// The turn carried across the edit, or none when the edit moved its node, or moved or split the
// edge it runs along. Ground positions are compared, not carriers or distances, because an edit can
// give part of a carrier a new key, or move its start point and so change every distance along it.
// An edge is found by its middle, which only its own carrier passes, and is kept when its two ends
// stand where the turn's did.
std::optional<RouteStep> carriedTurn(const RouteStep &turn, const Network &before,
                                     const Network &after) {
  if (turn.Carrier == NULL_KEY) {
    return std::nullopt;
  }
  if (turn.From == turn.To) {
    const std::optional<Place> node = heldPlace({turn.Carrier, turn.From}, before, after);
    const std::optional<NetworkPosition> position = node ? after.resolve(*node) : std::nullopt;
    if (!position || !std::holds_alternative<NodePosition>(*position)) {
      return std::nullopt;
    }
    return RouteStep{node->Carrier, node->Distance, node->Distance};
  }
  const std::optional<Place> middle =
      heldPlace({turn.Carrier, turn.From + ((turn.To - turn.From) * 0.5)}, before, after);
  const std::optional<NetworkPosition> position = middle ? after.resolve(*middle) : std::nullopt;
  const auto *inside = position ? std::get_if<EdgePosition>(&*position) : nullptr;
  if (inside == nullptr) {
    return std::nullopt;
  }
  const NetworkEdge &edge = after.edges()[inside->Edge];
  const std::optional<GroundPoint> from = before.groundPoint({turn.Carrier, turn.From});
  const std::optional<GroundPoint> to = before.groundPoint({turn.Carrier, turn.To});
  const std::optional<GroundPoint> low = after.groundPoint({edge.Carrier, edge.FromDistance});
  const std::optional<GroundPoint> high = after.groundPoint({edge.Carrier, edge.ToDistance});
  if (sameGround(from, low) && sameGround(to, high)) {
    return RouteStep{edge.Carrier, edge.FromDistance, edge.ToDistance};
  }
  if (sameGround(from, high) && sameGround(to, low)) {
    return RouteStep{edge.Carrier, edge.ToDistance, edge.FromDistance};
  }
  return std::nullopt;
}

// Carries each guest across a resolution by where it stands. While its box fits on guest paving
// there, it keeps its ground position: on the paving its place is carried to by carryOver when that
// lies under it, and then at that carried place, and otherwise at the nearest place on the carrier
// of the piece it stands on. Otherwise it moves to the nearest point where its box fits, and takes
// its place from the piece there. A guest carried onto another carrier keeps walking the same way
// over the ground. With no guest paving left, its place is left as it is, which no longer
// resolves, and its guest leaves when it next steps. It keeps its turn only where the edit left the
// turn's node, and the edge it picked, where they were. Its aim starts again from its place.
// Nothing changes in a world's first resolution, which has no network before.
void carryGuests(World &world) {
  const Network *before = previousNetwork(world, PathKind::Guest);
  if (before == nullptr) {
    return;
  }
  const Network &after = parkNetwork(world, PathKind::Guest);
  const GuestGround &ground = guestGround(world);
  const auto moveTo = [&](Guest &guest, const Place &place) {
    if (place.Carrier != guest.At.Carrier) {
      keepWayOverGround(guest, *before, after, place);
    }
    guest.At = place;
  };
  for (const EntityKey key : parkGuests(world)) {
    Guest &guest = world.Registry.get<Guest>(world.findEntity(key));
    const std::optional<Place> carried = carryOver(guest.At, *before, after);
    const std::optional<uint32_t> carriedGroup =
        carried ? ground.groupOf(carried->Carrier) : std::nullopt;
    const GroundPiece *piece = ground.pieceAt(guest.Ground, carriedGroup);
    // Leaving door ends out keeps a guest that has just come through the entrance's door there.
    if (piece != nullptr && ground.fits(piece->Group, guest.Ground, false)) {
      if (carried && carriedGroup == piece->Group) {
        moveTo(guest, *carried);
      } else if (const std::optional<Place> nearest =
                     after.nearestPlaceOn(piece->Carrier, guest.Ground)) {
        moveTo(guest, *nearest);
      }
    } else if (const std::optional<GroundSpot> fit = ground.nearestFit(guest.Ground)) {
      guest.Ground = fit->At;
      if (const GroundPiece *on = ground.pieceAt(fit->At, fit->Group)) {
        if (const std::optional<Place> nearest = after.nearestPlaceOn(on->Carrier, fit->At)) {
          moveTo(guest, *nearest);
        }
      }
    }
    guest.Turn = carriedTurn(guest.Turn, *before, after).value_or(RouteStep{});
    guest.Aim = guest.At;
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
  record.Position = state->Ground;
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
  const std::optional<GroundPoint> ground = parkNetwork(world, PathKind::Guest).groundPoint(place);
  if (!ground) {
    throw std::invalid_argument("addGuest: the place is not on the guest network");
  }
  const EntityKey key = world.createEntity();
  Guest guest;
  guest.At = place;
  guest.Ground = *ground;
  guest.Aim = place;
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
  addGuestGround(schema);
  schema.addSystem(&stepGuests);
  schema.addFinisher(&carryGuests);
  addFootfall(schema);
}

} // namespace tpj
