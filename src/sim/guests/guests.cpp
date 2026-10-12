#include "sim/guests/guests.h"

#include "sim/clock.h"
#include "sim/draw.h"
#include "sim/furniture/furniture.h"
#include "sim/guests/internal/departures.h"
#include "sim/guests/internal/footfall.h"
#include "sim/guests/internal/ground.h"
#include "sim/guests/internal/guest.h"
#include "sim/guests/internal/presence.h"
#include "sim/guests/internal/route_ahead.h"
#include "sim/medium/field.h"
#include "sim/medium/flow.h"
#include "sim/medium/ground_field.h"
#include "sim/medium/network.h"
#include "sim/mix.h"
#include "sim/operations/facilities.h"
#include "sim/operations/operations.h"
#include "sim/operations/seats.h"
#include "sim/park/intent.h"
#include "sim/routes/networks.h"
#include "sim/routes/route_distance.h"
#include "sim/schema.h"
#include "sim/sim_math.h"
#include "sim/wayfinding/best_seats.h"
#include "sim/world.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace tpj {

namespace {

using GuestRouteDistance = RouteDistance<PathKind::Guest>;
using RouteSample = std::vector<SampledEntry<RouteEntry>>;

// An aim that moves less than this, in meters, when it is advanced, can come no closer to the end
// of its route, so halving can't creep it on for ever.
constexpr double AIM_SETTLED = 0.01;

// The purpose the departure tally's entity is derived with.
constexpr uint64_t DEPARTURES_PURPOSE = hashName("departure-tally");
// The purpose an entrance's arrival draw is keyed with.
constexpr uint64_t ARRIVAL_PURPOSE = hashName("guest-arrival");
// The game seconds in a game hour.
constexpr double HOUR_SECONDS = 3600.0;

// The authored curve's value at x, clamped to its first and last X, linear between its points.
double curveAt(std::span<const CurvePoint> curve, double x) {
  const double h = std::clamp(x, curve.front().X, curve.back().X);
  for (size_t i = 1; i < curve.size(); ++i) {
    const CurvePoint &a = curve[i - 1];
    const CurvePoint &b = curve[i];
    if (h == a.X) {
      return a.Y;
    }
    if (h < b.X) {
      return a.Y + (h - a.X) * (b.Y - a.Y) / (b.X - a.X);
    }
  }
  return curve.back().Y;
}

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

// What every guest's step reads from the park and the clock: the entrance keys ascending, heading
// home's closing term, and whether the clock has just passed the closing or the opening time.
struct ParkMoment {
  std::vector<EntityKey> Entrances;
  double Closing = 0.0;
  bool Closes = false;
  bool Opens = false;
};

// Heading home's closing term at the second of the day under the hours: 0 until CLOSING_LEAD
// before closing, then rising linearly to CLOSING_SCORE at closing, and CLOSING_SCORE while the
// park is closed. Always 0 when the park is open all day.
double closingTerm(const OpeningHours &hours, uint64_t second) {
  if (hours.Opening == hours.Closing) {
    return 0.0;
  }
  if (!isOpenAt(hours, second)) {
    return CLOSING_SCORE;
  }
  const uint64_t left = (hours.Closing + DAY_SECONDS - second) % DAY_SECONDS;
  if (left >= CLOSING_LEAD) {
    return 0.0;
  }
  return CLOSING_SCORE * static_cast<double>(CLOSING_LEAD - left) /
         static_cast<double>(CLOSING_LEAD);
}

// The park's moment at the world's tick, judged by its current hours. The clock passes a time when
// the park is open at one tick and not the tick before, or the other way round.
ParkMoment parkMoment(const World &world) {
  ParkMoment moment;
  moment.Entrances = entranceKeys(world);
  const OpeningHours hours = parkHours(world);
  moment.Closing = closingTerm(hours, parkTime(world.Tick).Second);
  if (world.Tick > 0) {
    const bool now = isOpenAt(hours, parkTime(world.Tick).Second);
    const bool before = isOpenAt(hours, parkTime(world.Tick - 1).Second);
    moment.Closes = before && !now;
    moment.Opens = !before && now;
  }
  return moment;
}

// The kinds each need uses: the offer field that answers it, the visits a guest sends for it, the
// servings that come back, the cause a guest consumes a serving with, and how much each serving
// fills the bladder.
template <GuestNeed N> struct NeedKinds;

template <> struct NeedKinds<GuestNeed::Hunger> {
  using Offer = FoodOffer;
  using Visits = GuestVisits;
  using Servings = Meals;
  static constexpr std::string_view Cause = EATEN_CAUSE;
  static constexpr double BladderFill = BLADDER_PER_MEAL;
};

template <> struct NeedKinds<GuestNeed::Thirst> {
  using Offer = DrinkOffer;
  using Visits = DrinkVisits;
  using Servings = Drinks;
  static constexpr std::string_view Cause = DRUNK_CAUSE;
  static constexpr double BladderFill = BLADDER_PER_DRINK;
};

template <> struct NeedKinds<GuestNeed::Tiredness> {
  using Offer = RestOffer;
  using Visits = RestVisits;
  using Servings = Rests;
  static constexpr std::string_view Cause = RESTED_CAUSE;
  static constexpr double BladderFill = 0.0;
};

template <> struct NeedKinds<GuestNeed::Bladder> {
  using Offer = ToiletOffer;
  using Visits = ToiletVisits;
  using Servings = ToiletUses;
  static constexpr std::string_view Cause = USED_CAUSE;
  static constexpr double BladderFill = 0.0;
};

template <typename Action, size_t... At>
void forEachNeedAt(Action &action, std::index_sequence<At...> /*at*/) {
  (action.template operator()<NEEDS[At]>(), ...);
}

// Calls the action's operator()<N>() for each need N of NEEDS, in order.
template <typename Action> void forEachNeed(Action &&action) {
  forEachNeedAt(action, std::make_index_sequence<NEEDS.size()>{});
}

// The guest's level of the need.
template <typename G> auto &needLevel(G &guest, GuestNeed need) {
  switch (need) {
  case GuestNeed::Thirst:
    return guest.Thirst;
  case GuestNeed::Tiredness:
    return guest.Tiredness;
  case GuestNeed::Bladder:
    return guest.Bladder;
  case GuestNeed::Hunger:
    break;
  }
  return guest.Hunger;
}

// The source's entry in the offer field at the node when it says the source can serve, or none.
template <typename Offer>
std::optional<OfferEntry> offerAt(const World &world, const Network &network, uint32_t node,
                                  EntityKey source) {
  const std::optional<OfferEntry> offer = sourceEntryAtNode<Offer>(world, network, node, source);
  if (!offer || !offer->Supplied) {
    return std::nullopt;
  }
  return offer;
}

// The source's offer for the need when it says it can serve: its first entry in the need's offer
// field at its lowest anchored node, or none. With an entry in route distance at the guest's place,
// the offer is reachable.
std::optional<OfferEntry> needOffer(const World &world, const Network &network, EntityKey source,
                                    GuestNeed need) {
  const std::optional<uint32_t> anchor = network.firstAnchoredNode(source);
  if (!anchor) {
    return std::nullopt;
  }
  std::optional<OfferEntry> offer;
  forEachNeed([&]<GuestNeed N>() {
    if (N == need) {
      offer = offerAt<typename NeedKinds<N>::Offer>(world, network, *anchor, source);
    }
  });
  return offer;
}

// Lists walking reuses from guest to guest within a step, so it allocates nothing per guest once
// they have grown.
struct WalkLists {
  RouteAhead Route;
  std::vector<RouteStep> Steps;
  std::vector<uint64_t> Weights;
  std::vector<GroundSample<Stride>> Near;
  SeatSample Seats;
};

// Whether the guest remembers the piece as full at the tick.
bool remembersFull(const Guest &guest, EntityKey piece, uint64_t tick) {
  return std::ranges::any_of(guest.Full, [piece, tick](const FullPiece &full) {
    return full.Piece == piece && tick < full.Found + FULL_MEMORY_TIME;
  });
}

// Remembers the piece as full from the tick, in the first entry that is empty or has expired, or
// else in place of the one found full first.
void rememberFull(Guest &guest, EntityKey piece, uint64_t tick) {
  FullPiece *slot = &guest.Full.front();
  for (FullPiece &full : guest.Full) {
    if (full.Piece == NULL_KEY || tick >= full.Found + FULL_MEMORY_TIME) {
      slot = &full;
      break;
    }
    if (full.Found < slot->Found) {
      slot = &full;
    }
  }
  *slot = {piece, tick};
}

// The piece's entry in the use's best seats field at the place, or none.
std::optional<BestPlace> seatEntryAt(const World &world, const Network &network, const Place &place,
                                     SeatUse use, EntityKey piece, WalkLists &lists) {
  bestSeatsAt(world, network, place, use, lists.Seats.Scratch, lists.Seats.Places);
  const auto found = std::ranges::find(lists.Seats.Places, piece, &BestPlace::Key);
  if (found == lists.Seats.Places.end()) {
    return std::nullopt;
  }
  return *found;
}

// The piece's seat place, from its segment's seat places, or none when the segment no longer lays
// it.
const SeatPlace *seatPlaceOf(const World &world, EntityKey segment, EntityKey piece) {
  const FieldSlot<SeatPlace> *slot = sourceSlot<SeatPlaces>(world, segment);
  if (slot == nullptr) {
    return nullptr;
  }
  for (const PlacedEntry<SeatPlace> &entry : slot->Entries) {
    if (entry.Value.Piece == piece) {
      return &entry.Value;
    }
  }
  return nullptr;
}

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
// relief by option index, for the serving it would give.
struct ScoredOptions {
  std::vector<ChoiceOption> Options;
  std::vector<double> Reliefs;
  // The segment that lays each seat's piece by option index, and NULL_KEY for other options.
  std::vector<EntityKey> Owners;
};

// Appends an option for each piece best rest seats gives at the guest's place, other than the
// pieces it remembers as full, with the piece's rest relief and segment.
void appendSeatOptions(const World &world, const Guest &guest, const Network &network,
                       WalkLists &lists, ScoredOptions &scored) {
  bestSeatsAt(world, network, guest.At, SeatUse::Rest, lists.Seats.Scratch, lists.Seats.Places);
  for (const BestPlace &seat : lists.Seats.Places) {
    if (remembersFull(guest, seat.Key, world.Tick)) {
      continue;
    }
    ChoiceOption option;
    option.Kind = ChoiceKind::Seat;
    option.Facility = seat.Key;
    option.Need = GuestNeed::Tiredness;
    option.Relief = RELIEF_WEIGHT * needCurve(guest.Tiredness) * seat.Value;
    option.Distance = DISTANCE_WEIGHT * seat.Distance;
    option.Commitment = guest.Activity == GuestActivity::HeadingToSeat && seat.Key == guest.Target
                            ? COMMITMENT_BONUS
                            : 0.0;
    option.Score = option.Relief + option.Distance + option.Commitment;
    scored.Options.push_back(option);
    scored.Reliefs.push_back(seat.Value);
    scored.Owners.push_back(seat.Owner);
  }
}

// Heading home's option: its base, each reason's term, and its commitment, with the first largest
// reason as its reason.
ChoiceOption headHomeOption(const Guest &guest, const ParkMoment &moment) {
  ChoiceOption headHome;
  headHome.Kind = ChoiceKind::HeadHome;
  headHome.Base = HOME_BASE;
  // HomeReason lists the needs in NEEDS order, so a need's index is its reason's.
  forEachNeed([&]<GuestNeed N>() {
    headHome.Reasons[static_cast<size_t>(N)] = HOME_NEED_WEIGHT * needCurve(needLevel(guest, N));
  });
  headHome.Reasons[static_cast<size_t>(HomeReason::Closing)] = moment.Closing;
  headHome.Commitment = guest.Activity == GuestActivity::HeadingHome ? COMMITMENT_BONUS : 0.0;
  headHome.Score = headHome.Base;
  size_t largest = 0;
  for (size_t i = 0; i < headHome.Reasons.size(); ++i) {
    headHome.Score += headHome.Reasons[i];
    if (headHome.Reasons[i] > headHome.Reasons[largest]) {
      largest = i;
    }
  }
  headHome.Score += headHome.Commitment;
  headHome.Reason = HOME_REASONS[largest];
  return headHome;
}

ScoredOptions scoreOptions(const World &world, const Guest &guest, const Network &network,
                           const RouteSample &routes, const ParkMoment &moment, WalkLists &lists) {
  ScoredOptions scored;
  for (const SampledEntry<RouteEntry> &entry : routes) {
    const std::optional<uint32_t> anchor = network.firstAnchoredNode(entry.Source);
    if (!anchor) {
      continue;
    }
    forEachNeed([&]<GuestNeed N>() {
      const std::optional<OfferEntry> offer =
          offerAt<typename NeedKinds<N>::Offer>(world, network, *anchor, entry.Source);
      if (!offer) {
        return;
      }
      ChoiceOption option;
      option.Kind = ChoiceKind::Offer;
      option.Facility = entry.Source;
      option.Need = N;
      option.Relief = RELIEF_WEIGHT * needCurve(needLevel(guest, N)) * offer->Relief;
      option.Distance = DISTANCE_WEIGHT * entry.Value.Distance;
      option.Wait = WAIT_WEIGHT * (static_cast<double>(offer->Wait) * SIM_TICK_SECONDS);
      option.Commitment =
          entry.Source == guest.Target && N == guest.TargetNeed ? COMMITMENT_BONUS : 0.0;
      option.Score = option.Relief + option.Distance + option.Wait + option.Commitment;
      scored.Options.push_back(option);
      scored.Reliefs.push_back(offer->Relief);
      scored.Owners.push_back(NULL_KEY);
    });
  }
  appendSeatOptions(world, guest, network, lists, scored);
  ChoiceOption carryOn;
  carryOn.Kind = ChoiceKind::CarryOn;
  carryOn.Score = CARRY_ON_SCORE;
  scored.Options.push_back(carryOn);
  if (homeEntry(routes, moment.Entrances) != nullptr) {
    scored.Options.push_back(headHomeOption(guest, moment));
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
            const ParkMoment &moment, uint64_t &choices, WalkLists &lists) {
  const RouteSample routes = sampleField<GuestRouteDistance>(world, network, guest.At);
  ScoredOptions scored = scoreOptions(world, guest, network, routes, moment, lists);
  const size_t picked =
      softmaxPick(drawKey(world, key, hashName("guest-choice"), choices++), scored.Options);
  const ChoiceOption &option = scored.Options[picked];
  guest.Target = NULL_KEY;
  guest.TargetOwner = NULL_KEY;
  if (option.Kind == ChoiceKind::Seat) {
    guest.Activity = GuestActivity::HeadingToSeat;
    guest.Target = option.Facility;
    guest.TargetNeed = option.Need;
    guest.ServingRelief = scored.Reliefs[picked];
    guest.TargetOwner = scored.Owners[picked];
  } else if (option.Kind == ChoiceKind::Offer) {
    guest.Activity = GuestActivity::HeadingToFacility;
    guest.Target = option.Facility;
    guest.TargetNeed = option.Need;
    guest.ServingRelief = scored.Reliefs[picked];
  } else if (option.Kind == ChoiceKind::HeadHome) {
    guest.Activity = GuestActivity::HeadingHome;
    guest.Reason = option.Reason;
  } else {
    guest.Activity = GuestActivity::Wandering;
  }
}

// Sends the guest's visit for the need to the facility: one unit of the need's visits created under
// its own key, arriving after VISIT_DELAY.
void sendVisit(World &world, EntityKey key, EntityKey facility, GuestNeed need) {
  forEachNeed([&]<GuestNeed N>() {
    if (N == need) {
      using Visits = typename NeedKinds<N>::Visits;
      createUnits<Visits>(world, key, key, 1);
      sendUnits<Visits>(world, key, facility, key, 1, VISIT_DELAY);
    }
  });
}

// Where the guest's heading takes it from its place: its target shop's route entry, its target
// piece's entry in best rest seats, or the least entrance entry, whichever it heads for. A guest
// heading to a shop whose offer it can no longer reach, or to a piece the field at its place no
// longer gives, drops the target, and one heading home with no entrance entry stops, each becoming
// wandering; Dropped says it must choose.
struct Heading {
  std::optional<RouteEntry> Target;
  std::optional<BestPlace> Seat;
  std::optional<RouteEntry> Home;
  bool Dropped = false;
};

Heading headingOf(const World &world, Guest &guest, const Network &network,
                  const std::vector<EntityKey> &entrances, WalkLists &lists) {
  Heading heading;
  if (guest.Activity == GuestActivity::HeadingToSeat) {
    heading.Seat = seatEntryAt(world, network, guest.At, SeatUse::Rest, guest.Target, lists);
    if (!heading.Seat) {
      guest.Activity = GuestActivity::Wandering;
      guest.Target = NULL_KEY;
      guest.TargetOwner = NULL_KEY;
      heading.Dropped = true;
    }
  }
  if (guest.Activity == GuestActivity::HeadingToFacility) {
    heading.Target = routeEntryAt(world, PathKind::Guest, network, guest.At, guest.Target);
    if (!heading.Target || !needOffer(world, network, guest.Target, guest.TargetNeed)) {
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
// along it is too. On a pass that stops short of the aim, the guest steps aside from the guests
// sampled near it, where the lines to the stepped point and from it to the aim are both clear, so
// the line to its aim stays clear. The guest's place becomes the spot of the route nearest where it
// now stands, never past the aim, and it walks the way that spot's stretch runs.
WalkedPass walkTowardAim(Guest &guest, EntityKey key, const GuestGround &ground, uint32_t group,
                         const RouteAhead &route, std::span<const GroundSample<Stride>> near,
                         double &left) {
  WalkedPass pass;
  pass.Aimed = spotOf(route, guest.Aim).value_or(RouteSpot{});
  pass.Aim = advanceAim(ground, group, guest.Ground, route, pass.Aimed);
  const GroundPoint toward = pointAt(ground, group, route, pass.Aim);
  const double gapX = toward.X - guest.Ground.X;
  const double gapZ = toward.Z - guest.Ground.Z;
  const double gap = std::sqrt((gapX * gapX) + (gapZ * gapZ));
  if (left >= gap) {
    guest.Ground = toward;
    left -= gap;
  } else {
    const GroundDirection heading{gapX / gap, gapZ / gap};
    const GroundPoint ahead{guest.Ground.X + (heading.X * left),
                            guest.Ground.Z + (heading.Z * left)};
    const GroundPoint aside = sidestep(key, guest.Ground, heading, guest.Speed, near);
    const GroundPoint stepped{ahead.X + aside.X, ahead.Z + aside.Z};
    if (aside != GroundPoint{} && ground.clearLine(group, guest.Ground, stepped) &&
        ground.clearLine(group, stepped, toward)) {
      guest.Ground = stepped;
    } else {
      guest.Ground = ahead;
    }
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

// Whether the guest has reached the end of its route: it stands on it, on its side, or it stands at
// its aim with no node of the route between the aim and the end, and the pass moved the aim less
// than AIM_SETTLED, so its box can come no closer. A door's end lies along its face, so where the
// connector meets it at a slant, or its last part is short, the nearest the box can come may lie
// well back from the door.
bool reachedEnd(const Guest &guest, const GuestGround &ground, uint32_t group,
                const RouteAhead &route, const WalkedPass &pass) {
  if (!route.Ends) {
    return false;
  }
  const size_t last = route.Points.size() - 1;
  if (guest.Ground == pointAt(ground, group, route, {last, 1.0})) {
    return true;
  }
  const GroundPoint aim = pointAt(ground, group, route, pass.Aim);
  const GroundPoint aimed = pointAt(ground, group, route, pass.Aimed);
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
Pass walkPass(Guest &guest, EntityKey key, const GuestGround &ground, uint32_t group,
              const RouteAhead &route, std::span<const GroundSample<Stride>> near, double &left) {
  const Place was = guest.At;
  const GroundPoint stood = guest.Ground;
  const WalkedPass pass = walkTowardAim(guest, key, ground, group, route, near, left);
  if (reachedEnd(guest, ground, group, route, pass)) {
    guest.At = placeAt(route, {route.Points.size() - 1, 1.0});
    guest.Aim = guest.At;
    return Pass::ReachedEnd;
  }
  if (guest.At != was) {
    return Pass::PlaceMoved;
  }
  return guest.Ground != stood ? Pass::GroundMoved : Pass::Stayed;
}

// Traces the guest's route ahead: by its target's entries toward its shop, by its piece's entries
// in best rest seats toward its seat place, by the entrances' toward home, or wandering on from
// its first wander step with its turn.
void traceGuestRoute(const World &world, const Guest &guest, const Network &network,
                     const NetworkPosition &position, const Heading &heading,
                     const std::vector<EntityKey> &entrances, WalkLists &lists) {
  RouteAhead &route = lists.Route;
  if (heading.Seat) {
    traceSeatRoute(world, network, guest.At, heading.Seat->Next, SeatUse::Rest, guest.Target,
                   lists.Seats, route);
  } else if (heading.Target) {
    traceRoute(world, network, guest.At, heading.Target->Next,
               std::span<const EntityKey>(&guest.Target, 1), guest.Turn, route);
  } else if (heading.Home) {
    traceRoute(world, network, guest.At, heading.Home->Next, entrances, guest.Turn, route);
  } else {
    traceRoute(world, network, guest.At, firstWanderStep(guest, network, position), {}, guest.Turn,
               route);
  }
}

// Moves the last stretch of a route that ends at the guest's target piece's seat place toward the
// piece, so the guest stops on the paving's edge beside it: by half the stretch's width less
// GUEST_CLEARANCE, across from the seat place toward the piece. Changes nothing when the piece is
// no longer laid, the route is the place alone, or the stretch is too narrow.
void shiftTowardPiece(const World &world, const Guest &guest, RouteAhead &route) {
  const SeatPlace *seat = seatPlaceOf(world, guest.TargetOwner, guest.Target);
  if (seat == nullptr || route.Points.size() < 2) {
    return;
  }
  RoutePoint &last = route.Points.back();
  const double x = seat->At.X - last.At.X;
  const double z = seat->At.Z - last.At.Z;
  const double length = std::sqrt((x * x) + (z * z));
  const double reach = (0.5 * last.Width) - GUEST_CLEARANCE;
  if (length == 0.0 || reach <= 0.0) {
    return;
  }
  last.Shift = {x / length * reach, z / length * reach};
}

// How a guest's walk in a step ended: still in the park, out of an entrance heading home, or lost
// because its place no longer resolves.
enum class WalkEnd : uint8_t { Stays, WalkedOut, Lost };

// How far a guest's walk has got within a step: the distance it has left, its draws so far for
// wandering picks and for choices, whether it has chosen since its place last moved, and whether it
// must choose.
struct WalkProgress {
  double Left = 0.0;
  uint64_t Picks = 0;
  uint64_t Choices = 0;
  bool Chosen = false;
  bool MustChoose = false;
};

// When the guest has arrived where its heading ends, sends its visit to its facility or piece and
// starts it waiting, or walks it out of the entrance, and gives how its walk ends. None when it
// hasn't arrived.
std::optional<WalkEnd> arrive(World &world, EntityKey key, Guest &guest, const Heading &heading) {
  if (heading.Target && heading.Target->Next.Carrier == NULL_KEY) {
    sendVisit(world, key, guest.Target, guest.TargetNeed);
    guest.Activity = GuestActivity::Waiting;
    return WalkEnd::Stays;
  }
  if (heading.Seat && heading.Seat->Next.Carrier == NULL_KEY) {
    createUnits<SeatVisits>(world, key, key, 1);
    sendUnits<SeatVisits>(world, key, guest.Target, key, 1, VISIT_DELAY);
    guest.Sent = world.Tick;
    guest.Activity = GuestActivity::Waiting;
    return WalkEnd::Stays;
  }
  if (heading.Home && heading.Home->Next.Carrier == NULL_KEY) {
    return WalkEnd::WalkedOut;
  }
  return std::nullopt;
}

// Makes the guest's choice for the route's node at the index. It chooses unless it has since its
// place last moved, and then its aim starts again from its place. It then sets its turn out of the
// node: a wanderer's pick, or for any other guest the empty step at the node.
void chooseFor(World &world, EntityKey key, Guest &guest, const Network &network,
               const ParkMoment &moment, size_t index, WalkProgress &progress, WalkLists &lists) {
  const RoutePoint &point = lists.Route.Points[index];
  const Place at{point.Carrier, point.To};
  const bool forward = index == 0 ? guest.Forward : point.To > point.From;
  const uint32_t node = point.NodeIndex;
  if (!progress.Chosen) {
    choose(world, key, guest, network, moment, progress.Choices, lists);
    progress.Chosen = true;
    guest.Aim = guest.At;
  }
  guest.Turn = guest.Activity == GuestActivity::Wandering
                   ? pickTurn(world, key, network, node, at, forward, progress.Picks, lists)
                   : RouteStep{at.Carrier, at.Distance, at.Distance};
}

// Walks the guest as far as its speed takes it in a tick across the guest paving, straight toward
// its aim along its route moved to its side. It drops a target it can no longer reach, and sends
// its visit when it reaches the end of its route at its target. It makes its choice for each node
// of its route, other than a shaping point and the node it is heading to, when it comes within
// TURN_LOOKAHEAD of it or stands on it, and chooses when it must, at most once until its place
// moves, and it must choose when the clock passes the closing time. Its turn clears once its place
// passes the turn's node.
WalkEnd walk(World &world, EntityKey key, Guest &guest, const Network &network,
             const GuestGround &ground, const ParkMoment &moment, bool mustChoose,
             WalkLists &lists) {
  WalkProgress progress;
  progress.Left = guest.Speed * SIM_TICK_SECONDS;
  progress.MustChoose = moment.Closes || mustChoose;
  sampleGroundField<GuestPresence>(world, guest.Ground, SIDESTEP_RADIUS, lists.Near);
  for (;;) {
    const std::optional<NetworkPosition> position = network.resolve(guest.At);
    if (!position) {
      return WalkEnd::Lost;
    }
    const Heading heading = headingOf(world, guest, network, moment.Entrances, lists);
    progress.MustChoose = progress.MustChoose || heading.Dropped;
    // Before arriving, so a guest at its facility's door or its entrance as the park closes still
    // chooses first.
    if (!progress.Chosen && progress.MustChoose) {
      choose(world, key, guest, network, moment, progress.Choices, lists);
      progress.Chosen = true;
      progress.MustChoose = false;
      // A new choice can change the route, so the aim starts again from the place.
      guest.Aim = guest.At;
      guest.Turn = {};
      continue;
    }
    if (const std::optional<WalkEnd> end = arrive(world, key, guest, heading)) {
      return *end;
    }
    if (progress.Left <= 0.0) {
      return WalkEnd::Stays;
    }
    const std::optional<uint32_t> group = ground.groupOf(guest.At.Carrier);
    if (!group) {
      return WalkEnd::Stays;
    }
    traceGuestRoute(world, guest, network, *position, heading, moment.Entrances, lists);
    moveToSide(SIDE_BIAS + ((1.0 - SIDE_BIAS) * guest.Share), lists.Route);
    if (heading.Seat && lists.Route.Ends) {
      shiftTowardPiece(world, guest, lists.Route);
    }
    const RouteAhead &route = lists.Route;
    if (route.Points.empty()) {
      return WalkEnd::Stays;
    }
    const std::optional<size_t> node = turnNode(
        route, heading.Target.has_value() || heading.Seat.has_value() || heading.Home.has_value());
    if (node && !startsAt(network, guest.Turn, route.Points[*node].NodeIndex)) {
      chooseFor(world, key, guest, network, moment, *node, progress, lists);
      continue;
    }

    const Pass pass = walkPass(guest, key, ground, *group, route, lists.Near, progress.Left);
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
      return WalkEnd::Stays;
    }
  }
}

// The guest's uniform draw for the purpose with the index, on its key and the world's seed and
// tick.
double guestDraw(const World &world, EntityKey guest, const char *purpose, uint64_t index = 0) {
  return drawUniform(drawKey(world, guest, hashName(purpose), index));
}

// Each entrance with a guest connector admits a guest through addGuest when its draw for the tick
// falls below the chance of an arrival in a tick at arrivalCurve's rate.
void admitGuests(World &world, const Network &network) {
  const double rate = arrivalCurve(world, parkTime(world.Tick).Second);
  if (rate <= 0.0) {
    return;
  }
  const double chance = rate * static_cast<double>(GAME_SECONDS_PER_TICK) / HOUR_SECONDS;
  for (const ParkEntrance &entrance : parkEntrances(world)) {
    const std::vector<uint32_t> anchored = network.anchoredNodes(entrance.Key);
    if (anchored.empty()) {
      continue;
    }
    if (drawUniform(drawKey(world, entrance.Key, ARRIVAL_PURPOSE, 0)) < chance) {
      addGuest(world, network.nodePlace(anchored.front()));
    }
  }
}

// Takes back the guest's visit for the need when it holds it: it finishes the visit, and takes any
// serving that came with it, which lowers the need by the offer's relief and fills the bladder by
// the need's fill. A meal is also counted and recorded. Returns whether the visit was back.
template <GuestNeed N> bool takeVisit(World &world, EntityKey key, Guest &guest) {
  using Kinds = NeedKinds<N>;
  const int64_t visits = unitsHeld<typename Kinds::Visits>(world, key, key);
  if (visits == 0) {
    return false;
  }
  consumeUnits<typename Kinds::Visits>(world, key, key, visits, FINISHED_CAUSE);
  const int64_t servings = unitsHeld<typename Kinds::Servings>(world, key, key);
  if (servings > 0) {
    consumeUnits<typename Kinds::Servings>(world, key, key, servings, Kinds::Cause);
    double &level = needLevel(guest, N);
    const double before = level;
    level = std::max(0.0, before - guest.ServingRelief);
    guest.Bladder =
        std::min(1.0, guest.Bladder + (Kinds::BladderFill * static_cast<double>(servings)));
    if constexpr (N == GuestNeed::Hunger) {
      ++guest.MealsEaten;
      guest.LastMeal = {world.Tick, before, level};
    }
  }
  return true;
}

// A waiting guest's visit has come back when it holds the visit for the need it went for: it takes
// the visit and any serving, and stops waiting. It forgets any choice it made at the facility's
// door, so it chooses again there. Returns whether it is still waiting.
bool awaitVisit(World &world, EntityKey key, Guest &guest) {
  bool back = false;
  forEachNeed([&]<GuestNeed N>() {
    if (N == guest.TargetNeed) {
      back = takeVisit<N>(world, key, guest);
    }
  });
  if (!back) {
    return true;
  }
  guest.Activity = GuestActivity::Wandering;
  guest.Target = NULL_KEY;
  guest.Turn = {};
  return false;
}

// What awaiting a seat visit found: the guest still waits, or its visit came back and it must
// choose.
enum class SeatWait : uint8_t { Waiting, Back };

// A guest waiting at a piece of furniture is seated once its visit hasn't come back within the
// turn-away time. When the visit comes back, it takes the visit and any rest, which lowers
// tiredness by the seat's rest relief. A visit back with no rest means the piece was full, and the
// guest remembers it. Either way it stops waiting and must choose.
SeatWait awaitSeat(World &world, EntityKey key, Guest &guest) {
  const int64_t visits = unitsHeld<SeatVisits>(world, key, key);
  if (visits == 0) {
    if (guest.Activity == GuestActivity::Waiting &&
        world.Tick >= guest.Sent + VISIT_DELAY + RETURN_DELAY) {
      guest.Activity = GuestActivity::Seated;
    }
    return SeatWait::Waiting;
  }
  consumeUnits<SeatVisits>(world, key, key, visits, FINISHED_CAUSE);
  const int64_t rests = unitsHeld<Rests>(world, key, key);
  if (rests > 0) {
    consumeUnits<Rests>(world, key, key, rests, RESTED_CAUSE);
    guest.Tiredness = std::max(0.0, guest.Tiredness - guest.ServingRelief);
  } else {
    rememberFull(guest, guest.Target, world.Tick);
  }
  guest.Activity = GuestActivity::Wandering;
  guest.Target = NULL_KEY;
  guest.TargetOwner = NULL_KEY;
  guest.Turn = {};
  return SeatWait::Back;
}

// Tires the guest by its rate for each meter between where it stood and where it stands.
void tire(Guest &guest, GroundPoint stood) {
  const double x = guest.Ground.X - stood.X;
  const double z = guest.Ground.Z - stood.Z;
  guest.Tiredness =
      std::min(1.0, guest.Tiredness + (guest.TirednessRate * std::sqrt((x * x) + (z * z))));
}

// Each guest, in ascending key order, leaves when its place does not resolve, which carrying
// leaves only when the guest network has no carrier, and otherwise gets hungrier and thirstier, and
// unless it is still waiting for its visit to come back, walks and tires with the ground it walks.
// A guest that walks sets its entry in guest presence, where it stands and how it moved, and one
// that starts waiting drops it. Guests that leave drop their entries and go after all have stepped.
// Those that walked out of an entrance are counted in the departure tally by their reason, which
// starts again from 0 in the tick the clock passes the opening time. Then the entrances admit new
// ones.
void stepGuests(World &world) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  const GuestGround &ground = guestGround(world);
  const ParkMoment moment = parkMoment(world);
  std::vector<EntityKey> leaving;
  std::array<uint64_t, HOME_REASONS.size()> walkedOut{};
  WalkLists lists;
  for (const EntityKey key : parkGuests(world)) {
    Guest &guest = world.get<Guest>(key);
    if (!network.resolve(guest.At)) {
      leaving.push_back(key);
      continue;
    }
    guest.Hunger = std::min(1.0, guest.Hunger + guest.HungerRate);
    guest.Thirst = std::min(1.0, guest.Thirst + guest.ThirstRate);
    bool seatBack = false;
    if (guest.Activity == GuestActivity::Waiting || guest.Activity == GuestActivity::Seated) {
      if (guest.TargetOwner != NULL_KEY) {
        if (awaitSeat(world, key, guest) == SeatWait::Waiting) {
          continue;
        }
        seatBack = true;
      } else if (awaitVisit(world, key, guest)) {
        continue;
      }
    }
    const GroundPoint stood = guest.Ground;
    const WalkEnd end = walk(world, key, guest, network, ground, moment, seatBack, lists);
    if (end == WalkEnd::WalkedOut) {
      ++walkedOut[static_cast<size_t>(guest.Reason)];
    }
    if (end != WalkEnd::Stays) {
      leaving.push_back(key);
      continue;
    }
    tire(guest, stood);
    if (guest.Activity == GuestActivity::Waiting) {
      dropGroundEntry<GuestPresence>(world, key);
    } else {
      setGroundEntry<GuestPresence>(world, key, guest.Ground,
                                    Stride{(guest.Ground.X - stood.X) / SIM_TICK_SECONDS,
                                           (guest.Ground.Z - stood.Z) / SIM_TICK_SECONDS});
    }
  }
  // The resolver has made the tally before any step.
  Departures &tally = world.get<Departures>(deriveKey(NULL_KEY, DEPARTURES_PURPOSE, 0));
  if (moment.Opens) {
    tally.ByReason = {};
  }
  for (size_t i = 0; i < walkedOut.size(); ++i) {
    tally.ByReason[i] += walkedOut[i];
  }
  for (const EntityKey key : leaving) {
    dropGroundEntry<GuestPresence>(world, key);
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
    Guest &guest = world.get<Guest>(key);
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

// The departure tally's resolver: creates its entity holding no departures when it holds none,
// and leaves a tally it holds as it is.
void ensureDepartures(World &world) {
  static_cast<void>(
      world.getOrEmplace<Departures>(world.createDerivedEntity(NULL_KEY, DEPARTURES_PURPOSE, 0)));
}

} // namespace

std::vector<EntityKey> parkGuests(const World &world) { return world.keysWith<Guest>(); }

std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest) {
  const Guest *state = world.tryGet<Guest>(guest);
  if (state == nullptr) {
    return std::nullopt;
  }
  GuestRecord record;
  record.Activity = state->Activity;
  record.At = state->At;
  record.Position = state->Ground;
  record.Hunger = state->Hunger;
  record.Thirst = state->Thirst;
  record.Tiredness = state->Tiredness;
  record.Bladder = state->Bladder;
  record.Arrived = state->Arrived;
  record.Target = state->Target;
  record.TargetNeed = state->TargetNeed;
  record.Reason = state->Reason;
  if (state->Activity == GuestActivity::Seated) {
    if (const SeatPlace *seat = seatPlaceOf(world, state->TargetOwner, state->Target)) {
      record.Seat = *seat;
    }
  }
  for (size_t i = 0; i < state->Full.size(); ++i) {
    const FullPiece &full = state->Full[i];
    if (full.Piece != NULL_KEY && world.Tick < full.Found + FULL_MEMORY_TIME) {
      record.FullPieces[i] = full;
    }
  }
  record.MealsEaten = state->MealsEaten;
  if (state->MealsEaten > 0) {
    record.LastMeal = state->LastMeal;
  }
  return record;
}

std::optional<std::vector<ChoiceOption>> guestOptions(const World &world, EntityKey guest) {
  const Guest *state = world.tryGet<Guest>(guest);
  if (state == nullptr) {
    return std::nullopt;
  }
  const Network &network = parkNetwork(world, PathKind::Guest);
  if (!network.resolve(state->At)) {
    return std::nullopt;
  }
  const RouteSample routes = sampleField<GuestRouteDistance>(world, network, state->At);
  WalkLists lists;
  ScoredOptions scored = scoreOptions(world, *state, network, routes, parkMoment(world), lists);
  softmaxWeights(scored.Options);
  return std::move(scored.Options);
}

EntityKey addGuest(World &world, const Place &place) {
  const std::optional<GroundPoint> ground = parkNetwork(world, PathKind::Guest).groundPoint(place);
  if (!ground) {
    throw std::invalid_argument("addGuest: the place is not on the guest network");
  }
  const EntityKey key = world.createEntity();
  Guest guest;
  guest.At = place;
  guest.Ground = *ground;
  guest.Aim = place;
  guest.Arrived = world.Tick;
  guest.HungerRate = HUNGER_RATE_MIN + guestDraw(world, key, "guest-hunger-rate") *
                                           (HUNGER_RATE_MAX - HUNGER_RATE_MIN);
  guest.Hunger = guestDraw(world, key, "guest-starting-hunger") * STARTING_HUNGER_MAX;
  guest.ThirstRate = THIRST_RATE_MIN + guestDraw(world, key, "guest-thirst-rate") *
                                           (THIRST_RATE_MAX - THIRST_RATE_MIN);
  guest.Thirst = guestDraw(world, key, "guest-starting-thirst") * STARTING_THIRST_MAX;
  guest.TirednessRate = TIREDNESS_RATE_MIN + guestDraw(world, key, "guest-tiredness-rate") *
                                                 (TIREDNESS_RATE_MAX - TIREDNESS_RATE_MIN);
  guest.Bladder = guestDraw(world, key, "guest-starting-bladder") * STARTING_BLADDER_MAX;
  guest.Speed = WALK_SPEED - WALK_SPEED_SPREAD +
                guestDraw(world, key, "guest-speed") * 2.0 * WALK_SPEED_SPREAD;
  guest.Share =
      0.5 * (guestDraw(world, key, "guest-share", 0) + guestDraw(world, key, "guest-share", 1));
  world.emplace<Guest>(key, guest);
  return key;
}

double needCurve(double level) { return curveAt(NEED_CURVE, level); }

DepartureTally departureTally(const World &world) {
  const std::vector<EntityKey> keys = world.keysWith<Departures>();
  if (keys.empty()) {
    return DepartureTally{};
  }
  return DepartureTally{world.get<Departures>(keys.front()).ByReason};
}

double arrivalCurve(const World &world, uint64_t second) {
  const OpeningHours hours = parkHours(world);
  if (!isOpenAt(hours, second)) {
    return 0.0;
  }
  const uint64_t span = hours.Opening == hours.Closing
                            ? DAY_SECONDS
                            : (hours.Closing + DAY_SECONDS - hours.Opening) % DAY_SECONDS;
  const uint64_t passed = (second + DAY_SECONDS - hours.Opening) % DAY_SECONDS;
  return curveAt(ARRIVAL_CURVE, static_cast<double>(passed) / static_cast<double>(span));
}

size_t softmaxPick(const DrawKey &key, std::span<ChoiceOption> options) {
  const std::vector<double> weights = softmaxWeights(options);
  return drawPick(key, std::span<const double>(weights));
}

void addGuests(WorldSchema &schema) {
  schema.addComponent<Guest>("guest", DataKind::State);
  schema.addComponent<Departures>("departure-tally", DataKind::State);
  addGroundField<GuestPresence>(schema);
  addGuestGround(schema);
  schema.addResolver("departure-tally", &ensureDepartures);
  schema.addSystem(&stepGuests);
  schema.addFinisher(&carryGuests);
  addFootfall(schema);
}

} // namespace tpj
