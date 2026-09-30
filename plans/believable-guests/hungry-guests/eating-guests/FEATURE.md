# Feature: Eating Guests

## Summary

eating-guests makes guests choose, and eat. At every node it leaves, a guest scores each food offer it can reach through guest route distance, carrying on, and, once its stay is over, heading home, and picks one by softmax with a keyed draw and simExp (decision 0019). An offer's score is the sum of four terms: relief shaped by the guest's hunger through an authored curve (decision 0020), route distance, wait, and a commitment bonus for its current target. A guest that picks a shop walks there by route distance's Next steps, dropping the target and choosing again at once if the shop becomes unreachable or says no meals. At the shop's guest anchor it sends one visit and waits, whatever happens meanwhile, until the visit comes back. A served guest eats the meal that came with it, and its hunger falls by the offer's relief. Heading home becomes a choice that dominates once the stay is over, replacing wandering-guests' switch. The inspection record gains the guest's target, the hunger before and after its last meal, and its last choice with every option's terms and probability. The Debug panel adds how many guests are waiting and how many meals have been eaten.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema between cycles, t the tick the cycle that follows steps, N is parkNetwork(W, PathKind::Guest), R(p) the entries sampleField of guest route distance on N gives in W at a place p, a source's offer and its reachability are as the Choice section of the guests module's spec defines them, and "the guests module's spec" is src/sim/guests/SPEC.md as Spec changes gives it. A guest chooses in a cycle when its record's LastChoice after the cycle has Tick t.

1. hungerCurve passes through each point of HUNGER_CURVE, is the spec's linear interpolation strictly between consecutive points, and gives, for a hunger below 0 or above 1, its value at 0 or at 1.
2. softmaxPick(key, options) sets each option's Probability to its weight over the weights' total, each weight simExp((Score - m) / CHOICE_TEMPERATURE) with m the greatest Score, and returns drawPick(key, the weights). Over 10000 keys differing only in index, each option's pick count is within 4.5 * sqrt(10000 * p * (1 - p)) + 1 of 10000 * p, p its Probability. It throws std::invalid_argument, leaving the options unchanged, for an empty list or a Score that is not finite.
3. When a guest chooses in a cycle, its LastChoice gives t, a place p, and its Hunger after the cycle. Its options are, in order, an offer for each source of R(p) whose offer in W is reachable, ascending, then carrying on, then heading home exactly when t is at least its StayUntil and R(p) has an entrance's entry. Each offer's terms are the spec's, from the source's entry in R(p), its offer in W, and the guest's Hunger and Target when it chose, which are its Hunger after the cycle and its Target in W, except that a guest whose visit came back in the cycle, that dropped its target, or that chose more than once in the cycle chose with the Target those left it, NULL_KEY for the first two. Every Score equals its terms added in the spec's order exactly, CARRY_ON_SCORE for carrying on and HEAD_HOME_SCORE for heading home. The Probabilities are those softmaxPick sets for the options, and Picked indexes them. After the cycle the guest's Activity and Target are those the picked option gives, except that an offer picked at its source's anchor leaves it waiting there with that Target, and heading home picked at an entrance's anchor removes it.
4. A guest that is not waiting chooses in every cycle in which it stands at a node at the start of its walk, or its Target's offer is not reachable at its place in W, or it is heading home and R of its place has no entrance entry. A guest that walks to a node with no distance left does not choose there in that cycle. A guest heading to a shop that keeps its target walks the Target's Next steps, so the Target's Distance in R of its place falls by WALK_STEP in each such cycle until it reaches the anchor.
5. A guest sends a guest-visits unit only in a cycle that ends with it waiting at the nodePlace of its Target's anchored node, and then exactly one, created under its own key, sent to its Target with its own key as handle and the delay VISIT_DELAY. So every guest-visits packet from a guest goes to its record's Target, and no guest has more than one guest-visits unit addressed to it outstanding. A waiting guest whose visit has not come back keeps its place, Activity, and Target, and sends nothing, whatever its Target's offer, route distance, or stay, for as long as its place resolves.
6. In the cycle in which a waiting guest holds a guest-visits unit under its own key, it consumes every such unit as finished, and when it holds a meal, consumes every meals unit it holds as eaten, and its MealsEaten rises by 1 and its LastMeal gives t, Before its Hunger after this cycle's rise, and After the greater of 0 and Before less the Relief of the offer it picked, which for every shop's offer is MEAL_RELIEF. Then it chooses in that cycle. So every served guest's hunger fell, and in every cycle, meals consumed as eaten rise by the number of guests whose MealsEaten rose.
7. makeParkSchema's worlds keep the wandering-guests criteria for arrivals, hunger, wandering, and records. A guest heads home only by choosing it, and one heading home walks the least entrance entry's Next steps and leaves at the entrance's anchor. In every cycle of randomized park edit sequences (tests/sim/support/route_edits.h) from a park with an entrance, shops, a depot, and guests walking, waiting, and eating, nothing throws, a copy of the world equals it, its save loads back and resolves equal to it, a candidate made with an edit equals the world that queues the same edit, and guest-visits and meals each conserve their units.
8. The Debug panel shows the number of waiting guests and the meals consumed as eaten. A --capture of tests/parks/supply.park after 1800 ticks shows guests on the paths and at the shops, and the panel shows meals eaten above 0. The cross-build check passes, now running guests that choose and eat in every tests/parks file.

## Medium

- Route distance (navigable-networks): sampled at the guest's place for every choice, the offers' Distance terms, the target's and home's Next steps, and drops.
- Food offer (plausible-operations): sampled at each reachable source's lowest anchored node's place, for Relief, Wait, and Supplied.
- Guest visits (this feature supplies, plausible-operations draws): one unit created under the guest's key and sent to its target shop at the shop's guest anchor, and consumed by the guest as finished when it comes back.
- Meals (plausible-operations supplies, this feature draws): the guest consumes the meal that comes back with its visit as eaten.
- Networks (navigable-networks, shared-medium): resolve, edges, nodePlace, anchoredNodes, and groundPoint, as before.
- Park intent (decision 0025): the entrances through parkEntrances, as before.
- Keyed draws and simExp (deterministic-simulation): the softmax pick.
- Inspection records (decision 0025): read by tests and the app's Debug panel.

No guest reads a shop's state, record, or queue, and no shop reads a guest's hunger, target, or choice (principle 6). The Debug panel reads the meals ledger's consumed count, which is tooling.

## Principle checks

- Principle 1: criterion 7. A guest's target, meal relief, meals eaten, last meal, and last choice are state that saves hold, and a loaded world explains its guests' choices exactly as the saved one did.
- Principle 2: criteria 4, 5, and 7. A shop cut from its depot, a deleted shop or path ahead, a guest waiting at a shop that is cut, and a guest with no route home are each legitimate, and randomized edits never throw.
- Principle 3: criteria 5 and 6. Guests reach shops only through visits and meals, whose units conserve, and every visit goes to the guest's own target.
- Principle 4: criteria 3 and 4. The Distance term and the walk to a target use route distance.
- Principle 5: criterion 3. Hunger acts through a curve against carrying on, with no threshold.
- Principle 6: guests read only route distance, the food offer, their own stocks, the network, and the entrances. Review checks that nothing in the guests module reads a shop's or depot's components or records.
- Principle 8: criteria 3 and 6. The recorded terms reproduce every score, the probabilities are the softmax of the scores, and the last meal gives the hunger before and after.
- Principle 10: criteria 2 and 7, and the cross-build check in criterion 8. Picks come from keyed draws and simExp.

## Spec changes

- src/sim/guests/SPEC.md: rewritten. The preamble adds the food offer and the guest's own stocks, Registration adds the new fields, Stepping replaces the stay's switch to heading home with the waiting check, a new Choice section and a new Visits and meals section are added, Walking adds the target and home checks and when a guest chooses, and the Inspection record adds the new fields. PLAN.md's Task 1 gives the text.
- src/sim/SPEC.md: addPark's guests module admits, walks, and feeds its guests.
- src/app/SPEC.md: the Debug panel's guest lines.
- src/scenarios/SPEC.md: park-edits compares the guests' choices and meals across builds.

Public interface, src/sim/guests/guests.h gains, and changes GuestActivity and GuestRecord to:

```cpp
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

// A meal a guest ate: the tick, and its hunger before and after.
struct GuestMeal {
  uint64_t Tick = 0;
  double Before = 0.0;
  double After = 0.0;

  bool operator==(const GuestMeal &) const = default;
};

// A point of an authored curve (decision 0020).
struct CurvePoint {
  double X = 0.0;
  double Y = 0.0;
};

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

// How much an offer's relief matters at each hunger.
inline constexpr std::array<CurvePoint, 4> HUNGER_CURVE{
    {{0.0, 0.0}, {0.3, 0.1}, {0.7, 0.8}, {1.0, 1.0}}};
// An offer's terms: relief per unit of curve times relief, per meter of route, per second of wait,
// and for the guest's current target.
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

// The hunger curve's value at the hunger, clamped to [0, 1].
double hungerCurve(double hunger);
// Sets each option's softmax probability from the scores and returns the index the draw picks.
// Throws std::invalid_argument for no options or a score that is not finite.
size_t softmaxPick(const DrawKey &key, std::span<ChoiceOption> options);
```

with visitFields for ChoiceOption, GuestChoice, and GuestMeal, which the guest's state holds.

## Files affected

- Modify: src/sim/guests/SPEC.md, src/sim/guests/guests.h, src/sim/guests/guests.cpp, src/sim/guests/internal/guest.h, src/sim/SPEC.md, src/scenarios/SPEC.md, src/scenarios/food_shop.cpp, src/app/SPEC.md, src/app/debug_panel.h, src/app/debug_panel.cpp, src/app/main.cpp
- Tests (test pass): tests/sim/guests/, tests/sim/support/guest_parks.h, and their CMakeLists.txt. wandering-guests' tests of heading home when the stay ends change with heading home becoming a choice, and existing tests that step a park with an entrance and a supplied shop for ARRIVAL_INTERVAL cycles or more now see guests visit it; the test pass updates any whose expectations that changes.

## Dependencies

- wandering-guests: the guests module, walking, and the record. Met.
- plausible-operations' supplied-food-shop: the food offer, guest-visits and meals, and service. Met.
- navigable-networks' paths-become-routes: guest route distance with Next steps from every anchored entity. Met.
- deterministic-simulation's world-as-value: keyed draws, drawPick's double overload, and simExp. Met.

## Out of scope

- Carrying places across edits, and stranded guests moving to the nearest point: carried-guests. Until then a guest whose place stops resolving leaves, even while waiting.
- Hungry footfall: hungry-footfall.
- A queue drawn as a line of guests, instead of waiting guests standing together at the anchor (MILESTONE.md's deepening candidates).
- Clicking a guest to see its last choice: legible-simulation's guest inspector.
- A general authored-curve type shared across modules. The hunger curve is the first, and its points stay in guests.h until a second curve needs them.

## Test pass decisions

- A guest that chooses more than once in a cycle is left to the spec and review. The record keeps only the last choice, and the draw index c is not visible, so choice tests use parks whose edges are longer than one cycle's walk, where an edit-free cycle has at most one choice, drawn with index 0.
- Criterion 4's node reached with no distance left needs the distance left to equal the remaining gap to the bit, which no fixture sets up without repeating the walk's arithmetic. The test checks it where it happens, and the spec's step 3 defines it.
- Criterion 5's delay is shown by the Target holding the visit at the end of the cycle that sends it, since a VISIT_DELAY of 1 delivers the packet at that cycle's swap, so no packet is ever in transit between cycles.
- Criterion 8's Debug panel line, capture, and cross-build check are checked by a --capture run of supply.park and by scripts/cross-build-check.sh.
- Principle 6, that guests read no shop's or depot's internals, is checked by review.

## Open questions

None.
