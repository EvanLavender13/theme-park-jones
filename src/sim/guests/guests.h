#ifndef TPJ_SIM_GUESTS_GUESTS_H
#define TPJ_SIM_GUESTS_GUESTS_H

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/furniture/furniture.h"
#include "sim/medium/network.h"
#include "sim/wayfinding/best_seats.h"

#include <array>
#include <optional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <vector>

namespace tpj {

class World;
class WorldSchema;

// What a guest is doing: wandering the paths, heading to a shop, bench, or toilet block, waiting
// there or at a piece of furniture for its visit to come back, heading home, heading to a piece of
// furniture, or seated there.
enum class GuestActivity : uint8_t {
  Wandering,
  HeadingToFacility,
  Waiting,
  HeadingHome,
  HeadingToSeat,
  Seated
};

constexpr std::array<std::string_view, 6> enumNames(GuestActivity /*value*/) {
  return {"wandering",    "heading-to-facility", "waiting",
          "heading-home", "heading-to-seat",     "seated"};
}

// A need a guest has, which one kind of offer answers: hunger by meals, thirst by drinks,
// tiredness by rests, and the bladder by toilet uses.
enum class GuestNeed : uint8_t { Hunger, Thirst, Tiredness, Bladder };

constexpr std::array<std::string_view, 4> enumNames(GuestNeed /*value*/) {
  return {"hunger", "thirst", "tiredness", "bladder"};
}

// Every need, in the order a guest scores a facility's offers.
inline constexpr std::array<GuestNeed, 4> NEEDS{GuestNeed::Hunger, GuestNeed::Thirst,
                                                GuestNeed::Tiredness, GuestNeed::Bladder};

// Why a guest heads home: one of its needs, or closing time. The needs come in NEEDS order, with
// the bladder as the toilet.
enum class HomeReason : uint8_t { Hunger, Thirst, Tiredness, Toilet, Closing };

constexpr std::array<std::string_view, 5> enumNames(HomeReason /*value*/) {
  return {"hunger", "thirst", "tiredness", "toilet", "closing"};
}

// Every reason, in the order heading home adds their terms.
inline constexpr std::array<HomeReason, 5> HOME_REASONS{HomeReason::Hunger, HomeReason::Thirst,
                                                        HomeReason::Tiredness, HomeReason::Toilet,
                                                        HomeReason::Closing};

// What a choice's option does: go to a shop for its offer, carry on wandering, head home, go to a
// piece of furniture to sit, or eat the meal it carries now.
enum class ChoiceKind : uint8_t { Offer, CarryOn, HeadHome, Seat, EatNow };

constexpr std::array<std::string_view, 5> enumNames(ChoiceKind /*value*/) {
  return {"offer", "carry-on", "head-home", "seat", "eat-now"};
}

// One option of a choice: its kind, the facility and need for an offer, or the piece and need for a
// seat, its terms, its score, and the probability the softmax gave it. An offer's terms are Relief,
// Distance, Wait, and Commitment. A seat's are Relief, Distance, and Commitment. Heading home's are
// Base, Reasons, and Commitment, and its reason is the first largest of Reasons. Eating now's only
// term is Relief. Carrying on has no terms.
struct ChoiceOption {
  ChoiceKind Kind = ChoiceKind::CarryOn;
  EntityKey Facility = NULL_KEY;
  GuestNeed Need = GuestNeed::Hunger;
  double Relief = 0.0;
  double Distance = 0.0;
  double Wait = 0.0;
  double Commitment = 0.0;
  double Base = 0.0;
  std::array<double, HOME_REASONS.size()> Reasons{};
  HomeReason Reason = HomeReason::Hunger;
  double Score = 0.0;
  double Probability = 0.0;

  bool operator==(const ChoiceOption &) const = default;
};

// A meal a guest ate: the tick, its hunger before and after, and whether it ate seated.
struct GuestMeal {
  uint64_t Tick = 0;
  double Before = 0.0;
  double After = 0.0;
  bool Seated = false;

  bool operator==(const GuestMeal &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, GuestMeal &meal) {
  visitor.field("tick", meal.Tick);
  visitor.field("before", meal.Before);
  visitor.field("after", meal.After);
  visitor.field("seated", meal.Seated);
}

// A piece of furniture a guest found full, and the tick it found it full. An empty one has Piece
// NULL_KEY.
struct FullPiece {
  EntityKey Piece = NULL_KEY;
  uint64_t Found = 0;

  bool operator==(const FullPiece &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, FullPiece &piece) {
  visitor.field("piece", piece.Piece);
  visitor.field("found", piece.Found);
}

// How many guests have walked out of an entrance since the park last opened, by the reason they
// left, in HOME_REASONS order.
struct DepartureTally {
  std::array<uint64_t, HOME_REASONS.size()> ByReason{};

  bool operator==(const DepartureTally &) const = default;
};

// A point of an authored curve (decision 0020).
struct CurvePoint {
  double X = 0.0;
  double Y = 0.0;
};

// What a guest publishes about itself for display and tests (decision 0025): what it is doing,
// its place on the guest network, the ground position it stands on, its four needs, the tick it
// arrived, the facility or piece it is heading to or waiting at and the need it goes there for,
// the reason it last recorded for heading home, whether it is carrying food, the seat place of the
// piece it sits at while it is seated there and the piece is still laid, the pieces it remembers as
// full, with empty entries where it remembers none, and the meals it has eaten and the last of
// them. Nothing in the park reads it.
struct GuestRecord {
  GuestActivity Activity = GuestActivity::Wandering;
  Place At;
  std::optional<GroundPoint> Position;
  double Hunger = 0.0;
  double Thirst = 0.0;
  double Tiredness = 0.0;
  double Bladder = 0.0;
  uint64_t Arrived = 0;
  EntityKey Target = NULL_KEY;
  GuestNeed TargetNeed = GuestNeed::Hunger;
  HomeReason Reason = HomeReason::Hunger;
  bool Carrying = false;
  std::optional<SeatPlace> Seat;
  std::array<FullPiece, SEAT_CHOICES> FullPieces{};
  uint64_t MealsEaten = 0;
  std::optional<GuestMeal> LastMeal;

  bool operator==(const GuestRecord &) const = default;
};

// The arrival curve: guests per game hour at each entrance, at fractions of the opening hours
// that have passed. A few come at opening, the most from 0.15 to 0.3, and none from 0.75.
inline constexpr std::array<CurvePoint, 6> ARRIVAL_CURVE{
    {{0.0, 5.0}, {0.15, 15.0}, {0.3, 15.0}, {0.5, 6.0}, {0.75, 0.0}, {1.0, 0.0}}};
// A guest's hunger rises by a rate per tick drawn from these, so from 0 to 1 in 4 to 6 game hours.
inline constexpr double HUNGER_RATE_MIN = 1.0 / 10800.0;
inline constexpr double HUNGER_RATE_MAX = 1.0 / 7200.0;
// A guest arrives with a hunger drawn from 0 up to this.
inline constexpr double STARTING_HUNGER_MAX = 0.4;
// A guest's thirst rises by a rate per tick drawn from these, so from 0 to 1 in 3 to 4.5 game
// hours.
inline constexpr double THIRST_RATE_MIN = 1.0 / 8100.0;
inline constexpr double THIRST_RATE_MAX = 1.0 / 5400.0;
// A guest arrives with a thirst drawn from 0 up to this.
inline constexpr double STARTING_THIRST_MAX = 0.4;
// A guest's tiredness rises by a rate per meter walked drawn from these, so from 0 to 1 in 400
// to 600 m. It arrives with none.
inline constexpr double TIREDNESS_RATE_MIN = 1.0 / 600.0;
inline constexpr double TIREDNESS_RATE_MAX = 1.0 / 400.0;
// A guest arrives with a bladder drawn from 0 up to this.
inline constexpr double STARTING_BLADDER_MAX = 0.2;
// How much each meal and each drink a guest takes fills its bladder.
inline constexpr double BLADDER_PER_MEAL = 0.2;
inline constexpr double BLADDER_PER_DRINK = 0.35;
// The mean speed guests walk at, in meters per second.
inline constexpr double WALK_SPEED = 1.3;
// A guest's speed is drawn uniformly from WALK_SPEED less this up to WALK_SPEED plus this.
inline constexpr double WALK_SPEED_SPREAD = 0.3;
// Where every guest's side starts, across the width its box can use seen the way it walks, from
// 0 at the left to 1 at the right. Its share of the rest puts it between here and the right.
inline constexpr double SIDE_BIAS = 0.5;
// Meters a guest keeps from every line bounding the paving it walks on: half the diagonal of its
// 0.6 m box, so the box stays on the paving at any angle.
inline constexpr double GUEST_CLEARANCE = 0.45;
// Meters around where a guest stands within which it senses other guests in guest presence.
inline constexpr double SIDESTEP_RADIUS = 3.0;
// Seconds ahead within which a guest steps aside from a nearest approach.
inline constexpr double SIDESTEP_HORIZON = 3.0;
// Meters apart, centre to centre, that a guest steps aside to pass another at.
inline constexpr double SIDESTEP_DISTANCE = 0.9;
// The fastest a guest steps aside, in meters per second.
inline constexpr double SIDESTEP_SPEED = 0.6;
// Meters of route ahead of its place that a guest looks along for its aim.
inline constexpr double AIM_LOOKAHEAD = 10.0;
// Meters before a node, along its route, at which a guest makes its choice for it.
inline constexpr double TURN_LOOKAHEAD = 10.0;
// How much an offer's relief matters at each level of the need it answers.
inline constexpr std::array<CurvePoint, 4> NEED_CURVE{
    {{0.0, 0.0}, {0.3, 0.1}, {0.7, 0.8}, {1.0, 1.0}}};
// An offer's terms: relief per unit of curve times relief, per meter of route, per second of
// wait, and for the guest's current target.
inline constexpr double RELIEF_WEIGHT = 4.0;
inline constexpr double DISTANCE_WEIGHT = -0.01;
inline constexpr double WAIT_WEIGHT = -0.02;
inline constexpr double COMMITMENT_BONUS = 0.3;
// The score of carrying on.
inline constexpr double CARRY_ON_SCORE = 0.5;
// Heading home's base, and its weight per unit of each need's curve.
inline constexpr double HOME_BASE = -2.0;
inline constexpr double HOME_NEED_WEIGHT = 1.5;
// The game seconds before closing over which heading home's closing term rises.
inline constexpr uint64_t CLOSING_LEAD = 3600;
// Heading home's closing term from closing until the park opens.
inline constexpr double CLOSING_SCORE = 10.0;
// The softmax temperature: lower picks the best option more surely.
inline constexpr double CHOICE_TEMPERATURE = 0.25;
// The hunger a meal relieves when a guest eats it now, standing.
inline constexpr double STANDING_MEAL_RELIEF = 0.25;
// Ticks a guest remembers a piece of furniture as full.
inline constexpr uint64_t FULL_MEMORY_TIME = 900;
// Ticks a visit takes from the guest at a shop's anchor to the shop.
inline constexpr uint32_t VISIT_DELAY = 1;
// The causes a guest consumes its returned visit and each serving with.
inline constexpr std::string_view FINISHED_CAUSE = "finished";
inline constexpr std::string_view EATEN_CAUSE = "eaten";
inline constexpr std::string_view DRUNK_CAUSE = "drunk";
inline constexpr std::string_view RESTED_CAUSE = "rested";
inline constexpr std::string_view USED_CAUSE = "used";

// The keys of every guest, ascending.
std::vector<EntityKey> parkGuests(const World &world);
// The guest's inspection record, or none when the key holds no guest.
std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest);
// The options the guest would weigh if it chose now, each with its terms, score, and softmax
// probability, or none when the key holds no guest or its place does not resolve on the guest
// network. Draws nothing and changes nothing; it costs one choice, so call it for one guest.
std::optional<std::vector<ChoiceOption>> guestOptions(const World &world, EntityKey guest);
// The park's departure tally, or 0 for every reason before the park is first resolved.
DepartureTally departureTally(const World &world);
// The rate, in guests per game hour, at which each entrance connected to the guest network admits
// guests at the second of the day, below DAY_SECONDS, under the park's opening hours:
// ARRIVAL_CURVE at the fraction of the hours passed since opening, or 0 while the park is closed.
// If the park is open all day, the fraction runs over the whole day from the opening time. Reads
// only the hours, and draws nothing.
double arrivalCurve(const World &world, uint64_t second);
// Creates a guest standing at the place, arriving at the world's tick, with its needs, rates,
// speed, and share drawn on its key, and gives its key. Throws std::invalid_argument, changing
// nothing, when the place does not resolve on the guest network.
EntityKey addGuest(World &world, const Place &place);
// The need curve's value at the level, clamped to [0, 1].
double needCurve(double level);
// Sets each option's softmax probability from the scores and returns the index the draw picks,
// filling weights, which it clears first, with the weights the probabilities came from. Throws
// std::invalid_argument for no options or a score that is not finite.
size_t softmaxPick(const DrawKey &key, std::span<ChoiceOption> options,
                   std::vector<double> &weights);
// Registers the guest state, guest presence, the system that steps guests and admits new ones, and
// the finisher that carries their places across each resolution, and then hungry footfall's field,
// state, system, and finisher. The routes and operations modules' registrations come first, and
// addDropPreviousNetworks after.
void addGuests(WorldSchema &schema);

} // namespace tpj

#endif
