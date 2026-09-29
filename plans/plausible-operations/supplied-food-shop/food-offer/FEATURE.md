# Feature: Food Offer

## Summary

food-offer makes each shop publish what it offers a hungry guest. The entry field food-offer holds, at each shop box's guest anchor, an OfferEntry: the relief a meal gives, MEAL_RELIEF, the exact wait for the next guest to arrive, and whether the shop is supplied. A starved shop offers OfferEntry{}, which says no meals. Resolution publishes every shop's offer as for a shop that has not stepped, from intent and route distance alone, so a candidate or newly placed shop has a complete offer at once. Each cycle republishes it from the shop's state after it serves, and a resolution that changes a shop's resolved offer, such as one that cuts its supply route, clears the stepped one, so the offer says no meals from the tick the route is cut. The wait chains the queue ahead: each guest is taken at the later of the previous guest's take plus SERVICE_INTERVAL and the tick its supply unit becomes available, units coming from stock, then shipments in arrival order, then an order placed in the offer's cycle. The medium gains sampleResolvedField, so the resolver reads route distance's resolved entries alone. The inspection record and the starved mark come in shop-records.

## Acceptance criteria

Throughout, W is any world made with makeParkSchema between cycles, t its tick, and "the cycle" the stepWorld that follows. A shop's guest anchor is the node anchored to it on parkNetwork(W, PathKind::Guest), and its offer place is that node's nodePlace. For a shop with a nearestDepot, D is shipmentDelay of supplyRouteLength(W, depot, shop) for that depot, or of the nearest depot's Distance when that is none. The resolved nearest depot and resolved D are the same, but read from backstage route distance's resolved entries alone, as sampleResolvedField gives them, and they equal nearestDepot and D in every world whose route distance fields hold no stepped entries. Tests state W by stepping and by writing ledgers between cycles, as meal-service's tests do.

1. addOperations registers the flow kinds, then shop-service, then the field FoodOffer, named food-offer, by addField, then the resolver food-offer, which depends on path-networks, route-distance, and food-offer-field, then its systems. So it needs the routes module's registrations before it, as addPark gives, and throws std::invalid_argument into a schema without them.
2. sampleResolvedField gives, at any place on any network, exactly the entries sampleField gives in the same world with every stepped entry of the field removed.
3. After every resolution, food-offer's resolved entries hold one source for each shop box and no other. A shop with a guest anchor has one entry at its offer place, and one without has an empty list. The entry of a shop with a resolved nearest depot is {MEAL_RELIEF, ORDER_DELAY + resolved D, true}, and that of a shop without one is OfferEntry{}.
4. A world's resolved food-offer entries are a function of its intent: after a resolution, they equal those of a new world with the same intent after its first resolution, whatever the world's state, including stepped entries written into the route distance fields.
5. In the cycle, each shop box publishes stepped entries at the places of its resolved ones. A shop with no nearestDepot in W publishes OfferEntry{}. A shop with one publishes {MEAL_RELIEF, wait, true}, where the wait is computed from W and the cycle as follows. r is t + 1. n is the guest-visits units it holds in W under the keys of live entities, less one if it serves in the cycle. F is the tick of the last cycle in which it served, this one included, plus SERVICE_INTERVAL, or 0 when it never has. Its supply units, in order, are: the supplies it holds in W under its own handle, less one if it serves, each at r; then the units of each supplies packet in W addressed to it whose destination is the shop, in ascending arrival, each at its packet's arrival; then any number more, each at t + ORDER_DELAY + D. With A_i the tick of unit i, counting from 0, T_0 = max(r, F, A_0) and T_i = max(T_(i-1) + SERVICE_INTERVAL, A_i). The wait is T_n - r. This covers a shop with nothing queued, held, or shipped, and a shop with nothing on order.
6. The offer predicts service. Take W with a shop that has a nearestDepot, where either the command W's last cycle applied placed the shop box, or W's last cycle applied no command and, in criterion 5's terms for that cycle, units 0 to n all came from stock and shipments, and no supplies packet sent to the shop after W's last cycle began arrives before unit n's tick. Hand the shop one more visit in W: one unit in its stock under the key of a new live entity. Let w be the wait sampled at its offer place in W. Stepping with no command, and with no other visit reaching the shop, the shop serves that guest in the cycle stepping t + w.
7. In every tick of meal-service's randomized runs, park edits (tests/sim/support/route_edits.h) interleaved with synthetic guests, sampling food-offer at a shop's offer place gives exactly one entry, from that shop, whose Supplied is whether nearestDepot gives the shop a depot. So an offer says no meals from the tick a route is cut, and says meals from the tick one is restored. Every world such a run reaches is legitimate, a world loaded from its save and resolved equals it, and a candidate made from it with an edit equals the world that queues the same edit.

## Medium

- Emits food-offer, an entry field of OfferEntry, at each shop box's guest anchor, resolved and stepped. Sampled later by believable-guests, to choose, and by legible-simulation. Tests sample it here.
- Samples backstage-route-distance: through nearestDepot for whether a shop is supplied, and through supplyRouteLength at the nearest depot's anchor, with the shop as source, for the delay of the depot's shipment. The resolver samples its resolved entries alone.
- Reads the shop's own stocks and ShopService, addressedTo of its key for supplies on their way to it, intent through parkBoxes, and the guest network through parkNetwork and the Network type's queries (decision 0025).

A shop never reads a depot's, a guest's, or another shop's state.

## Principle checks

- Principle 1: criterion 4. Resolved offers derive from intent alone, even when a world's state holds stepped route distance entries. Criterion 7's save and candidate laws hold with offers in the world, and the stepped offers are state that saves hold.
- Principle 2: criterion 7. No run throws, and every shop, supplied or starved, with or without a guest connector, has a defined offer.
- Principles 3 and 6: criteria 3 and 5 compute each offer from the shop's own state, what is addressed to it, route distance, and intent. Review checks that the offer reads nothing else.
- Principle 8: criteria 5 and 6. The wait is exact, and it predicts when the shop takes the guest.
- Principle 10: the food-shop scenario's worlds now hold offers, so the cross-build check compares them.

## Spec changes

- src/sim/operations/SPEC.md: the Registration section registers the field and the resolver; a new Food offer section describes the entry, the anchor, both layers, and the wait. PLAN.md's Task 1 gives the text.
- src/sim/medium/SPEC.md: sampleResolvedField, at the end of the Fields section's sampling paragraph.
- src/sim/SPEC.md: addPark's operations module registers the food offer as well as its flow kinds and systems.

Public interface added to src/sim/operations/operations.h:

```cpp
// A shop's food offer: the hunger a meal relieves, on a scale from 0 to 1, the ticks the next
// guest to arrive waits to be taken, and whether the shop is supplied. A starved shop's offer is
// OfferEntry{}, which says no meals.
struct OfferEntry {
  double Relief = 0.0;
  uint64_t Wait = 0;
  bool Supplied = false;

  bool operator==(const OfferEntry &) const = default;
};

template <typename Visitor> void visitFields(Visitor &visitor, OfferEntry &entry);

// The food offer field: each shop box's offer, at its guest anchor.
struct FoodOffer {
  using Entry = OfferEntry;
  static constexpr std::string_view Name = "food-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;
};

// The hunger a meal relieves, on a scale from 0 to 1.
inline constexpr double MEAL_RELIEF = 0.5;
```

Added to src/sim/medium/field.h:

```cpp
// The field's entries at the place, as sampleField gives them, but choosing each source's resolved
// entries whatever its stepped ones. Resolvers sample with it, since stepped entries are state.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>> sampleResolvedField(const World &world,
                                                                 const Network &network,
                                                                 const Place &place);
```

## Files affected

- Modify: src/sim/operations/operations.h, src/sim/operations/operations.cpp, src/sim/operations/SPEC.md, src/sim/medium/field.h, src/sim/medium/SPEC.md, src/sim/SPEC.md
- Tests (test pass): tests/sim/operations/, tests/sim/medium/, and tests/sim/CMakeLists.txt. The registration test in tests/sim/operations/operations_test.cpp calls addOperations on an empty schema, which criterion 1 now refuses, and is updated by the test pass.

## Dependencies

- meal-service: the queue, FreeAt, and the service rule the wait predicts. Met.
- supply-chain: nearestDepot, supplyRouteLength, shipmentDelay, and ORDER_DELAY. Met.
- shared-medium's entry fields with both layers and the finisher that clears stepped entries. Met.
- navigable-networks' guest anchors on the guest network. Met.

## Out of scope

- The inspection record, the limiting factor, and the starved mark (shop-records).
- Showing offers in the scene or the ghost; legible-simulation's overlay samples the field later.
- Counting orders on their way to or held by a depot as supply units in the wait (MILESTONE.md's deepening candidates).
- Guests sampling offers to choose a shop (believable-guests).

## Open questions

None.
