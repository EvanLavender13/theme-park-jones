#ifndef TPJ_SIM_GUESTS_GUESTS_H
#define TPJ_SIM_GUESTS_GUESTS_H

#include "sim/draw.h"
#include "sim/entity_key.h"
#include "sim/medium/network.h"

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

// What a guest is doing: wandering the paths, heading to a shop, waiting there for its visit to
// come back, or heading home.
enum class GuestActivity : uint8_t { Wandering, HeadingToShop, Waiting, HeadingHome };

constexpr std::array<std::string_view, 4> enumNames(GuestActivity /*value*/) {
  return {"wandering", "heading-to-shop", "waiting", "heading-home"};
}

// What a choice's option does: go to a shop for its offer, carry on wandering, or head home.
enum class ChoiceKind : uint8_t { Offer, CarryOn, HeadHome };

constexpr std::array<std::string_view, 3> enumNames(ChoiceKind /*value*/) {
  return {"offer", "carry-on", "head-home"};
}

// One option of a choice: its kind, the shop for an offer, its terms, its score, and the
// probability the softmax gave it. Carrying on and heading home have no terms.
struct ChoiceOption {
  ChoiceKind Kind = ChoiceKind::CarryOn;
  EntityKey Shop = NULL_KEY;
  double Relief = 0.0;
  double Distance = 0.0;
  double Wait = 0.0;
  double Commitment = 0.0;
  double Score = 0.0;
  double Probability = 0.0;

  bool operator==(const ChoiceOption &) const = default;
};

// A meal a guest ate: the tick, and its hunger before and after.
struct GuestMeal {
  uint64_t Tick = 0;
  double Before = 0.0;
  double After = 0.0;

  bool operator==(const GuestMeal &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, GuestMeal &meal) {
  visitor.field("tick", meal.Tick);
  visitor.field("before", meal.Before);
  visitor.field("after", meal.After);
}

// A point of an authored curve (decision 0020).
struct CurvePoint {
  double X = 0.0;
  double Y = 0.0;
};

// What a guest publishes about itself for display and tests (decision 0025): what it is doing,
// where it stands, where that is on the ground (none while its place does not resolve), how
// hungry it is, the tick its stay ends, the shop it is heading to or waiting at, and the meals it
// has eaten and the last of them. Nothing in the park reads it.
struct GuestRecord {
  GuestActivity Activity = GuestActivity::Wandering;
  Place At;
  std::optional<GroundPoint> Position;
  double Hunger = 0.0;
  uint64_t StayUntil = 0;
  EntityKey Target = NULL_KEY;
  uint64_t MealsEaten = 0;
  std::optional<GuestMeal> LastMeal;

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
// How much an offer's relief matters at each hunger.
inline constexpr std::array<CurvePoint, 4> HUNGER_CURVE{
    {{0.0, 0.0}, {0.3, 0.1}, {0.7, 0.8}, {1.0, 1.0}}};
// An offer's terms: relief per unit of curve times relief, per meter of route, per second of
// wait, and for the guest's current target.
inline constexpr double RELIEF_WEIGHT = 4.0;
inline constexpr double DISTANCE_WEIGHT = -0.01;
inline constexpr double WAIT_WEIGHT = -0.02;
inline constexpr double COMMITMENT_BONUS = 0.3;
// The scores of carrying on and of heading home once the stay is over.
inline constexpr double CARRY_ON_SCORE = 0.5;
inline constexpr double HEAD_HOME_SCORE = 10.0;
// The softmax temperature: lower picks the best option more surely.
inline constexpr double CHOICE_TEMPERATURE = 0.25;
// Ticks a visit takes from the guest at a shop's anchor to the shop.
inline constexpr uint32_t VISIT_DELAY = 1;
// The causes a guest consumes its returned visit and its meal with.
inline constexpr std::string_view FINISHED_CAUSE = "finished";
inline constexpr std::string_view EATEN_CAUSE = "eaten";

// The keys of every guest, ascending.
std::vector<EntityKey> parkGuests(const World &world);
// The guest's inspection record, or none when the key holds no guest.
std::optional<GuestRecord> guestRecord(const World &world, EntityKey guest);
// The options the guest would weigh if it chose now, each with its terms, score, and softmax
// probability, or none when the key holds no guest or its place does not resolve on the guest
// network. Draws nothing and changes nothing; it costs one choice, so call it for one guest.
std::optional<std::vector<ChoiceOption>> guestOptions(const World &world, EntityKey guest);
// Creates a guest standing at the place, whose stay ends at the tick, with its hunger and hunger
// rate drawn on its key, and gives its key. Throws std::invalid_argument, changing nothing, when
// the place does not resolve on the guest network.
EntityKey addGuest(World &world, const Place &place, uint64_t stayUntil);
// The hunger curve's value at the hunger, clamped to [0, 1].
double hungerCurve(double hunger);
// Sets each option's softmax probability from the scores and returns the index the draw picks.
// Throws std::invalid_argument for no options or a score that is not finite.
size_t softmaxPick(const DrawKey &key, std::span<ChoiceOption> options);
// Registers the guest state, the system that steps guests and admits new ones, and the finisher
// that carries their places across each resolution, and then hungry footfall's field, state,
// system, and finisher. The routes and operations modules' registrations come first, and
// addDropPreviousNetworks after.
void addGuests(WorldSchema &schema);

} // namespace tpj

#endif
