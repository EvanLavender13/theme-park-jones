# Implementation Plan: Eating Guests

## Goal

Guests score the food offers they can reach against carrying on and heading home at every node, pick by softmax, walk to the shop they pick, send it a visit and wait, eat the meal that comes back, and publish their target, last meal, and last choice, with the Debug panel counting waiting guests and meals eaten.

## Approach

The guests module gains a public hunger curve and softmax pick, and the guest's state gains its target, the relief it expects, its meals, and its last choice. The walk loop samples route distance at each place it stands, drops a target it can no longer reach, sends the visit at its target's anchor, and chooses at every node it leaves and whenever it must, then steps by the target's or home's Next, or wanders. stepGuests checks a waiting guest's stocks for its returned visit and meal before walking, and no longer switches a guest home when its stay ends.

## Tasks

### Task 1: Guests spec

Files:
- Modify: `src/sim/guests/SPEC.md` (whole file)

Step 1: Replace the file's contents with:

```
# guests

Believable guests (plans/believable-guests): the park's visitors. It is part of tpj_sim and follows its contract (src/sim/SPEC.md). A guest reads the park only through the medium and intent: the guest network through parkNetwork and the Network type's queries, guest route distance and the food offer through sampleField, its own stocks of guest-visits and meals through unitsHeld, and the entrances through parkEntrances (decision 0025). It sends visits and consumes what comes back through the flow ledger. A guest never reads another entity's state, and no other module reads a guest's, so a guest's hunger never leaves it (principles 3 and 6).

## Registration

addGuests registers the state component type Guest, named guest, and then the system stepGuests. addPark calls it after addOperations, whose flow kinds and food offer guests use, so guests step after shops and depots. Guest is private to the module, declared in guests/internal/guest.h. It holds At, the guest's place on the guest network; Forward, whether it walks toward higher distances along At's carrier; Activity, one of wandering, heading-to-shop, waiting, and heading-home; Hunger; HungerRate; StayUntil, the tick its stay ends; Target, the shop it is heading to or waiting at, or NULL_KEY; MealRelief, the Relief of the offer it last picked; MealsEaten, how many meals it has eaten; LastMeal; and LastChoice. parkGuests gives the keys of the entities holding one, ascending.

## Stepping

Each cycle, stepGuests takes each guest in ascending key order, with t the tick being stepped and N the guest network, parkNetwork(world, PathKind::Guest). A guest whose place does not resolve on N, as after an edit deletes its path, leaves the park. Otherwise its Hunger becomes the lesser of 1 and Hunger + HungerRate. A waiting guest then checks whether its visit has come back (Visits and meals), and one still waiting ends its step. Every other guest walks. A guest leaves the park when its entity is destroyed, which happens after every guest has stepped. Then guests arrive.

## Arrivals

In the cycle stepping t, when t + 1 is a multiple of ARRIVAL_INTERVAL, 60 ticks, each entrance of parkEntrances that anchors a node of N admits one guest, in entrance key order. The guest is a new entity from createEntity, at nodePlace of the entrance's lowest anchored node, with Forward true, Activity wandering, and Target NULL_KEY. For a purpose p, let u(p) be drawUniform(drawKey(world, the guest's key, hashName(p), 0)). Its StayUntil is t + STAY_MIN + floor(u("guest-stay") * (STAY_MAX - STAY_MIN)), with STAY_MIN 1800 and STAY_MAX 3600. Its HungerRate is HUNGER_RATE_MIN + u("guest-hunger-rate") * (HUNGER_RATE_MAX - HUNGER_RATE_MIN), with HUNGER_RATE_MIN 1/2700 and HUNGER_RATE_MAX 1/1350 per tick, so hunger rises from 0 to 1 in 45 to 90 s. Its Hunger is u("guest-starting-hunger") * STARTING_HUNGER_MAX, 0.4. Hunger is on the scale from 0 to 1 that MEAL_RELIEF assumes (sim/operations/SPEC.md). A new guest first walks in the next cycle. An entrance with no guest connector admits no guests, a legitimate state (principle 2).

## Choice

A guest chooses what to do by scoring its options and picking one by softmax (decision 0019). Let R be the entries sampleField of guest route distance on N gives at the guest's place. A source's offer is the first of its own entries that sampleField of food-offer on N gives at the nodePlace of the source's lowest anchored node on N, or none, and the offer is reachable when the source has an entry in R and the offer exists and has Supplied true. So a guest finds offers only through route distance, and an entrance, which publishes no offer, is never one. The guest's options are, in order:

- an offer for each source of R whose offer is reachable, in ascending key order;
- carrying on;
- heading home, when t is StayUntil or later and R has an entry whose source is an entrance of parkEntrances.

An offer's terms come from its source's entry E in R and its offer O. Its Relief is RELIEF_WEIGHT times hungerCurve(Hunger) times O.Relief, multiplied in that order. Its Distance is DISTANCE_WEIGHT times E.Distance. Its Wait is WAIT_WEIGHT times O.Wait in seconds, the double of O.Wait times SIM_TICK_SECONDS. Its Commitment is COMMITMENT_BONUS when its source is the guest's Target and 0 otherwise. Its Score is Relief + Distance + Wait + Commitment, added in that order. RELIEF_WEIGHT is 4, DISTANCE_WEIGHT -0.01 per meter, WAIT_WEIGHT -0.02 per second, and COMMITMENT_BONUS 0.3. Carrying on has the Score CARRY_ON_SCORE, 0.5, and heading home HEAD_HOME_SCORE, 10, each with every term 0. An offer of MEAL_RELIEF scores at most 2.3, so heading home dominates once a stay is over.

hungerCurve is the authored piecewise-linear curve through the points of HUNGER_CURVE (decision 0020): (0, 0), (0.3, 0.1), (0.7, 0.8), and (1, 1), over hunger's whole range. A hunger below 0 counts as 0, and one above 1 as 1. At a point's X it is that point's Y, and strictly between consecutive points a and b it is a.Y + (h - a.X) * (b.Y - a.Y) / (b.X - a.X), computed in that order.

softmaxPick(key, options) sets each option's Probability and returns the index it picks. With m the greatest Score, an option's weight is simExp((Score - m) / CHOICE_TEMPERATURE), with CHOICE_TEMPERATURE 0.25, and its Probability is its weight divided by the total of the weights, added in order from 0.0. It returns drawPick of the key over the weights, the double overload, so it picks each option with its Probability. The option with the greatest Score weighs exactly 1, so the total is at least 1. It throws std::invalid_argument, changing nothing, for an empty list or a Score that is not finite.

The guest picks with softmaxPick keyed drawKey(world, its key, hashName("guest-choice"), c), where c counts the choices it has made in the cycle from 0. Picking an offer makes it heading-to-shop, with the offer's source as Target and O.Relief as MealRelief. Picking carrying on makes it wandering, and heading home makes it heading-home, each with Target NULL_KEY. Its LastChoice becomes a GuestChoice: t, its place, its Hunger, the options with their terms, Scores, and Probabilities, and the index picked.

## Walking

A guest walks WALK_STEP, WALK_SPEED 1.3 m/s times SIM_TICK_SECONDS, each cycle, along the carriers of N (principle 4). A step is a carrier and two distances along it, From and To, walked from From toward To. The walk repeats the following, with the distance left starting at WALK_STEP and R sampled at the guest's place each time.

1. A guest heading to a shop whose Target's offer is not reachable, because its Target has no entry in R or its offer says no meals, drops its target: it becomes wandering with Target NULL_KEY, and must choose. A guest heading home when R has no entrance entry becomes wandering, and must choose.
2. A guest heading to a shop whose Target's entry in R has a Next whose carrier is NULL_KEY stands at the shop's guest anchor. It sends its visit (Visits and meals), becomes waiting, and ends its walk. A guest heading home whose least entrance entry in R, ties to the lower source key, has a Next whose carrier is NULL_KEY stands at that entrance's anchor. It leaves the park, ending its walk.
3. Unless it has chosen in this walk since it last walked a step, a guest chooses, and goes back to 1, when it must choose, or when its place resolves to a node and distance is left.
4. When no distance is left, the walk ends.
5. Otherwise the guest walks a step. When its place resolves to a node, it moves to the step's carrier at the step's From, and inside an edge it stays where it is. It walks toward the step's To, setting Forward to whether To lies above its distance, by the lesser of the distance left and the gap between its distance and To, which it subtracts from the distance left. Reaching To, it stands exactly at To, and otherwise its distance never passes To.

So a guest decides at every node it leaves, as soon as it loses its target, and when its visit comes back, and its speed does not depend on how the network is cut into edges. It chooses at most once between steps, so a node reached with no distance left is decided once, in the next cycle.

A guest heading to a shop steps by the Next of its Target's entry in R, and a guest heading home by the Next of the least entrance entry. A wandering guest wanders. Inside an edge, a wandering guest's step runs from its distance to the stop ahead of it, the edge's ToDistance when Forward and its FromDistance otherwise. At a node, the node's steps are, for each edge in edges() order, {carrier, FromDistance, ToDistance} when the edge starts at the node, then {carrier, ToDistance, FromDistance} when it ends there. The guest's way back is the step whose carrier is At's, whose From is At's distance, and which runs toward higher distances exactly when Forward is false: the edge it came in on. A wandering guest picks among the node's other steps by drawPick over integer weights of 1, one for each, keyed drawKey(world, its key, hashName("guest-wander"), i), where i counts the picks it has made in the cycle from 0, and takes the way back when there is no other step, as at the end of a path or at an anchor. A new guest has no way back, so it takes its entrance's connector.

## Visits and meals

A guest visits a shop through the guest-visits flow, and eats the meal the shop sends back through the meals flow (sim/operations/SPEC.md, Service). At its Target's guest anchor, a guest creates one guest-visits unit under its own key as handle, and sends it to its Target with that handle and the delay VISIT_DELAY, 1 tick. It sends a visit nowhere else, and only when it is not already waiting, so it never has two outstanding.

A waiting guest stays where it stands, keeping its Target, until its visit comes back, whatever its Target's offer, route distance, or its stay says meanwhile. Its visit has come back when it holds guest-visits units under its own key. It consumes them with the cause finished, FINISHED_CAUSE. When it also holds meals units under its own key, it was served: it consumes them with the cause eaten, EATEN_CAUSE, its Hunger falls by MealRelief to no lower than 0, MealsEaten rises by 1, and LastMeal becomes the GuestMeal of t and its Hunger before and after. Served or not, it becomes wandering with Target NULL_KEY and walks, so it chooses at the anchor in the same cycle.

## Inspection record

A guest publishes an inspection record about itself for display and tests (decision 0025). No system or resolver reads one. guestRecord(world, key) gives none when key holds no guest, and otherwise its GuestRecord: its Activity, At, Position, groundPoint of At on N or none where At does not resolve, Hunger, StayUntil, Target, MealsEaten, LastMeal, none until it has eaten, and LastChoice, none until it has chosen. The record is computed from the guest's state, so it adds nothing to a save.
```

Step 2: Check the sections.

Run: `grep -c "^## " src/sim/guests/SPEC.md`
Expected: `7`

### Task 2: Sim, scenarios, and app specs

Files:
- Modify: `src/sim/SPEC.md` (the addPark paragraph)
- Modify: `src/scenarios/SPEC.md` (the park-edits sentence)
- Modify: `src/app/SPEC.md` (the Tooling UI section's guest line)

Step 1: In src/sim/SPEC.md, replace "which admit and walk its guests" with "which admit, walk, and feed its guests".

Step 2: In src/scenarios/SPEC.md, replace "and the walks of the guests its entrance admits, are compared across builds" with "and the walks, choices, and meals of the guests its entrance admits, are compared across builds".

Step 3: In src/app/SPEC.md, after the sentence ending "or `Guests 0` when there are none.", add:

```
Below that it shows `Waiting <w>, meals eaten <e>`, with w the number of parkGuests whose records show them waiting and e the meals units consumed with the cause eaten (sim/guests/SPEC.md), each in decimal.
```

Step 4: Check.

Run: `grep -c "admit, walk, and feed" src/sim/SPEC.md; grep -c "choices, and meals" src/scenarios/SPEC.md; grep -c "meals eaten" src/app/SPEC.md`
Expected: `1`, `1`, and `1`

### Task 3: Guests interface

Files:
- Modify: `src/sim/guests/guests.h` (whole file)
- Modify: `src/sim/guests/internal/guest.h` (the Guest struct and its visitFields)
- Modify: `src/sim/guests/guests.cpp` (add two stubs before addGuests)

Step 1: Replace src/sim/guests/guests.h with:

```cpp
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

template <typename Visitor> void visitFields(Visitor &visitor, ChoiceOption &option) {
  visitor.field("kind", option.Kind);
  visitor.field("shop", option.Shop);
  visitor.field("relief", option.Relief);
  visitor.field("distance", option.Distance);
  visitor.field("wait", option.Wait);
  visitor.field("commitment", option.Commitment);
  visitor.field("score", option.Score);
  visitor.field("probability", option.Probability);
}

// A choice a guest made: the tick, where it stood, how hungry it was, its options, and the index
// of the one it picked.
struct GuestChoice {
  uint64_t Tick = 0;
  Place At;
  double Hunger = 0.0;
  std::vector<ChoiceOption> Options;
  uint64_t Picked = 0;

  bool operator==(const GuestChoice &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, GuestChoice &choice) {
  visitor.field("tick", choice.Tick);
  visitor.field("at", choice.At);
  visitor.field("hunger", choice.Hunger);
  visitor.field("options", choice.Options);
  visitor.field("picked", choice.Picked);
}

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
// hungry it is, the tick its stay ends, the shop it is heading to or waiting at, the meals it has
// eaten and the last of them, and its last choice. Nothing in the park reads it.
struct GuestRecord {
  GuestActivity Activity = GuestActivity::Wandering;
  Place At;
  std::optional<GroundPoint> Position;
  double Hunger = 0.0;
  uint64_t StayUntil = 0;
  EntityKey Target = NULL_KEY;
  uint64_t MealsEaten = 0;
  std::optional<GuestMeal> LastMeal;
  std::optional<GuestChoice> LastChoice;

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
// The hunger curve's value at the hunger, clamped to [0, 1].
double hungerCurve(double hunger);
// Sets each option's softmax probability from the scores and returns the index the draw picks.
// Throws std::invalid_argument for no options or a score that is not finite.
size_t softmaxPick(const DrawKey &key, std::span<ChoiceOption> options);
// Registers the guest state and the system that steps guests and admits new ones. The routes and
// operations modules' registrations come first.
void addGuests(WorldSchema &schema);

} // namespace tpj

#endif
```

Step 2: In src/sim/guests/internal/guest.h, replace the Guest comment, struct, and visitFields with:

```cpp
// State, on a guest's entity: where it stands on the guest network, whether it walks toward
// higher distances along that place's carrier, what it is doing, how hungry it is and how fast
// that rises, the tick its stay ends, the shop it is heading to or waiting at, the relief of the
// offer it last picked, the meals it has eaten and the last of them, and its last choice, whose
// options are empty until it first chooses.
struct Guest {
  Place At;
  bool Forward = true;
  GuestActivity Activity = GuestActivity::Wandering;
  double Hunger = 0.0;
  double HungerRate = 0.0;
  uint64_t StayUntil = 0;
  EntityKey Target = NULL_KEY;
  double MealRelief = 0.0;
  uint64_t MealsEaten = 0;
  GuestMeal LastMeal;
  GuestChoice LastChoice;
};

template <typename Visitor> void visitFields(Visitor &visitor, Guest &guest) {
  visitor.field("at", guest.At);
  visitor.field("forward", guest.Forward);
  visitor.field("activity", guest.Activity);
  visitor.field("hunger", guest.Hunger);
  visitor.field("hunger-rate", guest.HungerRate);
  visitor.field("stay-until", guest.StayUntil);
  visitor.field("target", guest.Target);
  visitor.field("meal-relief", guest.MealRelief);
  visitor.field("meals-eaten", guest.MealsEaten);
  visitor.field("last-meal", guest.LastMeal);
  visitor.field("last-choice", guest.LastChoice);
}
```

Add `#include "sim/entity_key.h"` to its includes.

Step 3: In src/sim/guests/guests.cpp, before `void addGuests(WorldSchema &schema) {`, add the stubs:

```cpp
double hungerCurve(double /*hunger*/) { return 0.0; }

size_t softmaxPick(const DrawKey & /*key*/, std::span<ChoiceOption> /*options*/) { return 0; }
```

Step 4: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 4: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/guests/SPEC.md, src/sim/SPEC.md, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, src/sim/park/SPEC.md, src/sim/operations/SPEC.md, src/app/SPEC.md, and src/scenarios/SPEC.md, and the public headers src/sim/guests/guests.h, src/sim/operations/operations.h, src/sim/medium/flow.h, src/sim/medium/field.h, src/sim/routes/route_distance.h, src/sim/draw.h, and src/sim/sim_math.h.

### Task 5: Hunger curve and softmax pick

Files:
- Modify: `src/sim/guests/guests.cpp` (replace the two stubs; includes)

Step 1: Add `#include "sim/sim_math.h"`, `#include <limits>`, and `#include <stdexcept>` to the includes.

Step 2: Replace the stubs with:

```cpp
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
  return drawPick(key, std::span<const double>(weights));
}
```

Step 3: Build and run the guests tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_sim_tests -# "[#<file>]"` for each test pass file covering criteria 1 and 2.
Expected: no build output; those tests pass.

### Task 6: Record

Files:
- Modify: `src/sim/guests/guests.cpp` (guestRecord)

Step 1: Replace guestRecord's final `return GuestRecord{...};` with:

```cpp
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
  if (!state->LastChoice.Options.empty()) {
    record.LastChoice = state->LastChoice;
  }
  return record;
```

Step 2: Build.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 7: Choosing and walking to a target

Files:
- Modify: `src/sim/guests/guests.cpp` (the anonymous namespace: homeStep, walk, and stepGuests; includes)

Step 1: Add `#include "sim/medium/flow.h"`, `#include "sim/operations/operations.h"`, and `#include <utility>` to the includes. After `using GuestRouteDistance = RouteDistance<PathKind::Guest>;` add:

```cpp
using RouteSample = std::vector<SampledEntry<RouteEntry>>;
```

Step 2: Replace homeStep with:

```cpp
// The source's entry in the sample, or none.
const RouteEntry *entryOf(const RouteSample &routes, EntityKey source) {
  for (const SampledEntry<RouteEntry> &entry : routes) {
    if (entry.Source == source) {
      return &entry.Value;
    }
  }
  return nullptr;
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
  const std::vector<uint32_t> anchored = network.anchoredNodes(source);
  if (anchored.empty()) {
    return std::nullopt;
  }
  for (const SampledEntry<OfferEntry> &entry :
       sampleField<FoodOffer>(world, network, network.nodePlace(anchored.front()))) {
    if (entry.Source == source) {
      if (!entry.Value.Supplied) {
        return std::nullopt;
      }
      return entry.Value;
    }
  }
  return std::nullopt;
}
```

Step 3: After wanderStep, add:

```cpp
// Scores the guest's options where it stands, picks one by softmax, and takes it up, recording
// the choice.
void choose(const World &world, EntityKey key, Guest &guest, const Network &network,
            const RouteSample &routes, const std::vector<EntityKey> &entrances,
            uint64_t &choices) {
  GuestChoice choice;
  choice.Tick = world.Tick;
  choice.At = guest.At;
  choice.Hunger = guest.Hunger;
  const double curve = hungerCurve(guest.Hunger);
  // Each offer's relief, by option index, for the meal it would give.
  std::vector<double> reliefs;
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
    choice.Options.push_back(option);
    reliefs.push_back(offer->Relief);
  }
  ChoiceOption carryOn;
  carryOn.Kind = ChoiceKind::CarryOn;
  carryOn.Score = CARRY_ON_SCORE;
  choice.Options.push_back(carryOn);
  if (world.Tick >= guest.StayUntil && homeEntry(routes, entrances) != nullptr) {
    ChoiceOption headHome;
    headHome.Kind = ChoiceKind::HeadHome;
    headHome.Score = HEAD_HOME_SCORE;
    choice.Options.push_back(headHome);
  }
  const size_t picked =
      softmaxPick(drawKey(world, key, hashName("guest-choice"), choices++), choice.Options);
  choice.Picked = picked;
  const ChoiceOption &option = choice.Options[picked];
  guest.Target = NULL_KEY;
  if (option.Kind == ChoiceKind::Offer) {
    guest.Activity = GuestActivity::HeadingToShop;
    guest.Target = option.Shop;
    guest.MealRelief = reliefs[picked];
  } else if (option.Kind == ChoiceKind::HeadHome) {
    guest.Activity = GuestActivity::HeadingHome;
  } else {
    guest.Activity = GuestActivity::Wandering;
  }
  guest.LastChoice = std::move(choice);
}

// Sends the guest's visit to the shop: one unit created under its own key, arriving after
// VISIT_DELAY.
void sendVisit(World &world, EntityKey key, EntityKey shop) {
  createUnits<GuestVisits>(world, key, key, 1);
  sendUnits<GuestVisits>(world, key, shop, key, 1, VISIT_DELAY);
}
```

Step 4: Replace walk with:

```cpp
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
    const RouteSample routes = sampleField<GuestRouteDistance>(world, network, guest.At);
    const RouteEntry *target = nullptr;
    if (guest.Activity == GuestActivity::HeadingToShop) {
      target = entryOf(routes, guest.Target);
      if (target == nullptr || !suppliedOffer(world, network, guest.Target)) {
        guest.Activity = GuestActivity::Wandering;
        guest.Target = NULL_KEY;
        target = nullptr;
        mustChoose = true;
      }
    }
    const RouteEntry *home = nullptr;
    if (guest.Activity == GuestActivity::HeadingHome) {
      home = homeEntry(routes, entrances);
      if (home == nullptr) {
        guest.Activity = GuestActivity::Wandering;
        mustChoose = true;
      }
    }
    if (target != nullptr && target->Next.Carrier == NULL_KEY) {
      sendVisit(world, key, guest.Target);
      guest.Activity = GuestActivity::Waiting;
      return true;
    }
    if (home != nullptr && home->Next.Carrier == NULL_KEY) {
      return false;
    }
    const bool atNode = std::holds_alternative<NodePosition>(*position);
    if (!chosen && (mustChoose || (atNode && left > 0.0))) {
      choose(world, key, guest, network, routes, entrances, choices);
      chosen = true;
      mustChoose = false;
      continue;
    }
    if (left <= 0.0) {
      return true;
    }
    RouteStep step;
    if (target != nullptr) {
      step = target->Next;
    } else if (home != nullptr) {
      step = home->Next;
    } else {
      step = wanderStep(world, key, guest, network, *position, picks);
    }
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
    chosen = false;
  }
}
```

Step 5: In stepGuests, delete:

```cpp
    if (world.Tick >= guest.StayUntil) {
      guest.Activity = GuestActivity::HeadingHome;
    }
```

and update its comment to: "Each guest, in ascending key order, leaves when its place no longer resolves, and otherwise gets hungrier and walks. Guests that leave go after all have stepped, and then the entrances admit new ones."

Step 6: Build and run the guests tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_sim_tests -# "[#<file>]"` for each test pass file under tests/sim/guests/, one per invocation.
Expected: no build output. The tests of criteria 1 to 4 pass; tests that need a waiting guest's visit to come back fail until Task 8.

### Task 8: Waiting and eating

Files:
- Modify: `src/sim/guests/guests.cpp` (a new function before stepGuests; stepGuests)

Step 1: Before stepGuests, add:

```cpp
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
```

Step 2: In stepGuests, after `guest.Hunger = std::min(1.0, guest.Hunger + guest.HungerRate);`, add:

```cpp
    if (guest.Activity == GuestActivity::Waiting && awaitVisit(world, key, guest)) {
      continue;
    }
```

and change its comment to: "Each guest, in ascending key order, leaves when its place no longer resolves, and otherwise gets hungrier, and unless it is still waiting for its visit to come back, walks. Guests that leave go after all have stepped, and then the entrances admit new ones."

Step 3: Build and run the sim tests.

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_sim_tests`
Expected: no build output; all pass. A failure is a deviation: compare against the Choice, Walking, and Visits and meals sections of src/sim/guests/SPEC.md; for a test outside tests/sim/guests/, report it as a disputed test.

### Task 9: The food-shop scenario's causes

Files:
- Modify: `src/scenarios/food_shop.cpp:1-43`

Step 1: Add `#include "sim/guests/guests.h"` to the includes, after `#include "sim/entity_key.h"`, and delete the scenario's own constants:

```cpp
constexpr std::string_view FINISHED_CAUSE = "finished";
constexpr std::string_view EATEN_CAUSE = "eaten";
```

so its synthetic guests consume their visits and meals with the guests module's causes.

Step 2: Build and run the scenario tests.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_scenarios_tests`
Expected: no build output; the scenario tests pass.

### Task 10: The Debug panel's waiting and meals line

Files:
- Modify: `src/app/debug_panel.h` (DebugStats and drawDebugPanel's comment)
- Modify: `src/app/debug_panel.cpp:31-37`
- Modify: `src/app/main.cpp` (includes; drawPanels, the guest loop)

Step 1: In DebugStats, after `double MeanHunger = 0.0;`, add:

```cpp
  uint64_t Waiting = 0;
  int64_t MealsEaten = 0;
```

and end drawDebugPanel's comment with "the guest count and mean hunger, and the guests waiting and meals eaten. Call between ImGui::NewFrame and ImGui::Render."

Step 2: In drawDebugPanel, after the guests `if`/`else`, add:

```cpp
    ImGui::Text("Waiting %llu, meals eaten %lld", static_cast<unsigned long long>(stats.Waiting),
                static_cast<long long>(stats.MealsEaten));
```

Step 3: In src/app/main.cpp, add `#include "sim/medium/flow.h"` after `#include "sim/guests/guests.h"`. In drawPanels's guest loop, after `hunger += record->Hunger;`, add:

```cpp
      if (record->Activity == tpj::GuestActivity::Waiting) {
        ++stats.Waiting;
      }
```

and after the `stats.MeanHunger = ...;` line, add:

```cpp
  stats.MealsEaten = tpj::unitsConsumed<tpj::Meals>(world, tpj::EATEN_CAUSE);
```

Step 4: Build.

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 11: Capture

Run: `cmake.exe --build --preset windows-debug > /dev/null; cd build/windows-debug && timeout 60 ./ThemeParkJones.exe --park ../../tests/parks/supply.park --ticks 1800 --capture supply.bmp; echo $?`. Convert the capture to PNG in the scratchpad and view it with the Read tool.
Expected: `0`. The capture shows guests on the paths and some standing at the shops' doors, and the Debug panel shows `Waiting` and `meals eaten` followed by a number above 0.

### Task 12: Confirm the acceptance criteria

Run:

```
git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i
cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"
ctest --preset linux-debug
cmake.exe --build --preset windows-debug
ctest.exe --preset windows-debug
scripts/cross-build-check.sh
```

Expected: no warnings; every test passes on both builds; the cross-build check passes.

### Task 13: Review and commit

Stage src, tests, and plans (`git add src tests plans`), dispatch the reviewer as implementing-features describes, and give Evan its findings verbatim. Then commit once via the commit-hygiene skill, with the subject `Guests: Choose food by softmax, visit shops, and eat`.
