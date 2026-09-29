# Implementation Plan: Food Offer

## Goal

Each shop box publishes a food offer at its guest anchor, resolved from intent and route distance and stepped from its state, with the exact chained wait for the next guest.

## Approach

field.h gains sampleResolvedField by splitting sampleField into the choice of slots and the sampling of them. operations.cpp's route length helpers take which layers to sample, so the resolver food-offer reads route distance's resolved entries alone and publishes each shop's offer as for a shop that has not stepped. stepShops publishes each shop's stepped offer after it serves, computing the wait by walking its queue against its supply units in order.

## Tasks

### Task 1: Operations spec

Files:
- Modify: `src/sim/operations/SPEC.md`

Step 1: In the Registration section, replace the first sentence, "addOperations registers the flow kinds SupplyOrders, named supply-orders, Supplies, named supplies, GuestVisits, named guest-visits, and Meals, named meals, in that order, then the state component type ShopService, named shop-service, and then the systems stepShops and stepDepots, in that order, so shops step before depots.", with:

```
addOperations registers the flow kinds SupplyOrders, named supply-orders, Supplies, named supplies, GuestVisits, named guest-visits, and Meals, named meals, in that order, then the state component type ShopService, named shop-service, then the field FoodOffer, named food-offer, with addField, then the resolver food-offer, which depends on path-networks, route-distance, and food-offer-field, and then the systems stepShops and stepDepots, in that order, so shops step before depots. So it needs the routes module's registrations before it, and a schema without them refuses the resolver with std::invalid_argument.
```

In the same section, replace "Everything else a shop or depot knows between cycles is in the ledgers, intent, and the fields." with "Everything else a shop or depot knows between cycles is in the ledgers, intent, and the fields, and a shop's offer is in the food-offer field (Food offer)."

Step 2: In the Orders and supplies section, replace "Then the shop serves, as the Service section describes." with "Then the shop serves, as the Service section describes, and publishes its offer, as the Food offer section describes."

Step 3: Append this section at the end of the file:

```
## Food offer

A shop tells guests what it offers through the entry field FoodOffer, named food-offer, whose entries are OfferEntry: Relief, the hunger a meal relieves on a scale from 0 to 1; Wait, the ticks the next guest to arrive waits to be taken; and Supplied. A shop with a nearest depot offers {MEAL_RELIEF, its wait, true}, with MEAL_RELIEF 0.5. A starved shop offers OfferEntry{}: no relief, no wait, and no meals. A shop's guest anchor is the node anchored to it on parkNetwork(world, PathKind::Guest), and each shop box publishes, in both layers, one entry at its guest anchor's nodePlace, or an empty list when it has no guest connector. The meal follows SERVICE_INTERVAL after the shop takes the guest, and a guest adds its own walk.

The wait is counted from the cycle in which a shop first holds the next visit to arrive, behind every guest queued, to the cycle that takes its guest, as if no other visit arrived. Let r be the tick of that first cycle, n the guests queued, and F the shop's FreeAt. The shop's supply units, in order, are its stock under its own handle, each available at r, then the units of the supplies packets addressed to it whose destination is the shop, in ascending arrival, each available at its packet's arrival, and then any number more, each available when an order placed in the offer's cycle would bring it: that cycle's tick plus ORDER_DELAY plus D. D is shipmentDelay of supplyRouteLength(world, depot, shop) for the nearest depot, the length the depot ships by, or of the nearest depot's Distance when that is none. With A_i the tick unit i is available, counting from 0, the queue's first guest is taken at T_0 = max(r, F, A_0), and each later one at T_i = max(T_(i-1) + SERVICE_INTERVAL, A_i), as the Service section's rule takes them. The next arrival is taken at T_n, and the wait is T_n - r.

The resolver food-offer publishes each shop box's resolved offer, as for a shop that has not stepped: nothing queued, held, or shipped, with its first guest waiting for the order it places in its first cycle, so its wait is ORDER_DELAY + D. It samples backstage route distance with sampleResolvedField, since resolvers derive from intent alone, and a candidate or newly placed shop is offered at once. After a shop serves, stepShops publishes its stepped offer from the world between cycles and what the shop did in the cycle, with t the tick being stepped and r = t + 1: n is its queue after serving, F its FreeAt, its stock is what it holds after serving, and the order is placed at t. A sample prefers the stepped offer, and a resolution that changes a shop's resolved offer, as one that cuts or restores its supply route or moves its guest anchor does, clears the stepped one. So an offer says no meals from the tick a route is cut. The offer reads only what serving reads and route distance, so a shop never reads a depot's, a guest's, or another shop's state.
```

Step 4: Check the text.

Run: `grep -c "Food offer" src/sim/operations/SPEC.md`
Expected: `3`

### Task 2: Medium and sim specs

Files:
- Modify: `src/sim/medium/SPEC.md:53`
- Modify: `src/sim/SPEC.md:13`

Step 1: In src/sim/medium/SPEC.md, append to the paragraph that begins "sampleField gives a field's entries at a place on a network" this sentence:

```
sampleResolvedField gives what sampleField gives in the same world with every stepped entry of the field removed: it chooses each source's resolved entries whatever its stepped ones. Resolvers sample with it, since stepped entries are state.
```

Step 2: In src/sim/SPEC.md, replace "and then the operations module's flow kinds and systems, which run its shops and depots (sim/operations/SPEC.md)" with "and then the operations module's flow kinds, food offer, and systems, which run its shops and depots (sim/operations/SPEC.md)".

Run: `grep -c "sampleResolvedField" src/sim/medium/SPEC.md; grep -c "food offer, and systems" src/sim/SPEC.md`
Expected: `1` and `1`

### Task 3: Offer interface

Files:
- Modify: `src/sim/operations/operations.h`

Step 1: Add `#include "sim/medium/field.h"` after `#include "sim/entity_key.h"`.

Step 2: After the Meals struct, add:

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

template <typename Visitor> void visitFields(Visitor &visitor, OfferEntry &entry) {
  visitor.field("relief", entry.Relief);
  visitor.field("wait", entry.Wait);
  visitor.field("supplied", entry.Supplied);
}

// The food offer field: each shop box's offer, at its guest anchor.
struct FoodOffer {
  using Entry = OfferEntry;
  static constexpr std::string_view Name = "food-offer";
  static constexpr FieldKind Kind = FieldKind::Entry;
};
```

Step 3: After RETURN_DELAY, add:

```cpp
// The hunger a meal relieves, on a scale from 0 to 1.
inline constexpr double MEAL_RELIEF = 0.5;
```

Step 4: Replace addOperations' comment with:

```cpp
// Registers the flow kinds supply-orders, supplies, guest-visits, and meals, the shop-service
// state, the food-offer field and its resolver, then the systems that step shops and then depots.
// The routes module's registrations must come first.
```

Run: `cmake --build --preset linux-debug --target tpj_sim 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 4: sampleResolvedField stub

Files:
- Modify: `src/sim/medium/field.h`

Step 1: After sampleField, add the declaration with a stub body:

```cpp
// The field's entries at the place, as sampleField gives them, but choosing each source's resolved
// entries whatever its stepped ones. Resolvers sample with it, since stepped entries are state.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleResolvedField([[maybe_unused]] const World &world, [[maybe_unused]] const Network &network,
                    [[maybe_unused]] const Place &place) {
  return {};
}
```

Run: `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
Expected: no output.

### Task 5: Test pass

Run the test pass as implementing-features describes, with FEATURE.md, the specs src/sim/operations/SPEC.md, src/sim/medium/SPEC.md, src/sim/routes/SPEC.md, and src/sim/SPEC.md, and the public headers src/sim/operations/operations.h and src/sim/medium/field.h.

### Task 6: sampleResolvedField

Files:
- Modify: `src/sim/medium/field.h:273-382`

Step 1: Delete the misplaced comment above sampleSlotAtNode, "// The field's entries at the place on the network, each with its source, sources in ascending key\n// order, by the default rule or the field's sampleEdge." (the two lines before "// Appends the source's entries whose places resolve to the node.").

Step 2: Replace sampleField's definition with a helper that samples given slots, and sampleField on top of it:

```cpp
// The entries of the slots at the place on the network, in the slots' order, by the default rule
// or the field's sampleEdge.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleSlots(const Network &network, const Place &place,
            const std::vector<const FieldSlot<typename F::Entry> *> &slots) {
  std::vector<SampledEntry<typename F::Entry>> sampled;
  const std::optional<NetworkPosition> position = network.resolve(place);
  if (!position) {
    return sampled;
  }
  for (const FieldSlot<typename F::Entry> *slot : slots) {
    if (const auto *node = std::get_if<NodePosition>(&*position)) {
      sampleSlotAtNode(network, *slot, node->Node, sampled);
    } else {
      sampleSlotInEdge<F>(network, *slot, place, std::get<EdgePosition>(*position), sampled);
    }
  }
  return sampled;
}

// The field's entries at the place on the network, each with its source, sources in ascending key
// order, by the default rule or the field's sampleEdge.
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>> sampleField(const World &world, const Network &network,
                                                         const Place &place) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  if (entity == entt::null) {
    return {};
  }
  return sampleSlots<F>(network, place, layeredSlots<F>(world, entity));
}
```

Step 3: Replace the stub body of sampleResolvedField:

```cpp
template <FieldDefinition F>
std::vector<SampledEntry<typename F::Entry>>
sampleResolvedField(const World &world, const Network &network, const Place &place) {
  const entt::entity entity = world.findEntity(fieldKey(F::Name));
  const auto *resolved =
      entity == entt::null ? nullptr : world.Registry.try_get<ResolvedEntries<F>>(entity);
  if (resolved == nullptr) {
    return {};
  }
  std::vector<const FieldSlot<typename F::Entry> *> slots;
  for (const FieldSlot<typename F::Entry> &slot : resolved->Slots) {
    slots.push_back(&slot);
  }
  return sampleSlots<F>(network, place, slots);
}
```

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#field_test]"; build/linux-debug/tpj_sim_tests -# "[#stepped_field_test]"` and then the test pass's file for sampleResolvedField, by its `-#` tag.
Expected: no build output; the field, stepped field, and sampleResolvedField tests pass.

### Task 7: Route lengths by layer

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: In the anonymous namespace, before routeLengthsAt, add:

```cpp
// Which of a field's layers a sample reads: both, by the layer rule, or the resolved alone, as a
// resolver must.
enum class Layers : uint8_t { Both, Resolved };
```

Step 2: Give routeLengthsAt a Layers parameter and sample by it:

```cpp
// Each backstage route distance entry sampled from the layers at the nodes anchored to the entity,
// as its source and distance, in node order and then source order.
std::vector<std::pair<EntityKey, double>> routeLengthsAt(const World &world, EntityKey at,
                                                         Layers layers) {
  using Field = RouteDistance<PathKind::Backstage>;
  const Network &network = parkNetwork(world, PathKind::Backstage);
  std::vector<std::pair<EntityKey, double>> lengths;
  for (const uint32_t node : network.anchoredNodes(at)) {
    const Place place = network.nodePlace(node);
    for (const SampledEntry<RouteEntry> &entry :
         layers == Layers::Both ? sampleField<Field>(world, network, place)
                                : sampleResolvedField<Field>(world, network, place)) {
      lengths.emplace_back(entry.Source, entry.Value.Distance);
    }
  }
  return lengths;
}
```

Step 3: After routeLengthsAt, add routeLengthBy and depotRouteBy, holding the bodies supplyRouteLength and nearestDepot have now, with routeLengthsAt called with layers:

```cpp
std::optional<double> routeLengthBy(const World &world, EntityKey at, EntityKey source,
                                    Layers layers) {
  std::optional<double> least;
  for (const auto &[from, distance] : routeLengthsAt(world, at, layers)) {
    if (from == source && (!least || distance < *least)) {
      least = distance;
    }
  }
  return least;
}

std::optional<DepotRoute> depotRouteBy(const World &world, EntityKey shop, Layers layers) {
  const std::vector<EntityKey> depots = boxKeys(world, BoxKind::Depot);
  std::optional<DepotRoute> nearest;
  for (const auto &[source, distance] : routeLengthsAt(world, shop, layers)) {
    if (!std::ranges::binary_search(depots, source)) {
      continue;
    }
    if (!nearest || distance < nearest->Distance ||
        (distance == nearest->Distance && source < nearest->Depot)) {
      nearest = DepotRoute{source, distance};
    }
  }
  return nearest;
}
```

Step 4: Make the public functions call them:

```cpp
std::optional<double> supplyRouteLength(const World &world, EntityKey at, EntityKey source) {
  return routeLengthBy(world, at, source, Layers::Both);
}

std::optional<DepotRoute> nearestDepot(const World &world, EntityKey shop) {
  return depotRouteBy(world, shop, Layers::Both);
}
```

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no build output; operations_test passes except its registration test, which the test pass updated and which passes after Task 8.

### Task 8: Resolved offers

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: In the anonymous namespace, after serveGuests, add:

```cpp
// The ticks a shipment from the route's depot takes to the shop: by the depot's own supply route
// length, the one it ships by, or by the shop's when the depot has none.
uint64_t deliveryDelay(const World &world, const DepotRoute &route, EntityKey shop, Layers layers) {
  return shipmentDelay(routeLengthBy(world, route.Depot, shop, layers).value_or(route.Distance));
}

// The shop's entries: the offer at each node anchored to it on the guest network.
std::vector<PlacedEntry<OfferEntry>> offerEntries(const World &world, EntityKey shop,
                                                  const OfferEntry &offer) {
  const Network &network = parkNetwork(world, PathKind::Guest);
  std::vector<PlacedEntry<OfferEntry>> entries;
  for (const uint32_t node : network.anchoredNodes(shop)) {
    entries.push_back({network.nodePlace(node), offer});
  }
  return entries;
}

// Publishes each shop box's offer as for a shop that has not stepped: with nothing queued, held,
// or shipped, its first guest waits for the order it places in its first cycle.
void resolveFoodOffers(World &world) {
  for (const EntityKey shop : boxKeys(world, BoxKind::Shop)) {
    OfferEntry offer;
    if (const std::optional<DepotRoute> route = depotRouteBy(world, shop, Layers::Resolved)) {
      offer = OfferEntry{.Relief = MEAL_RELIEF,
                         .Wait = ORDER_DELAY + deliveryDelay(world, *route, shop, Layers::Resolved),
                         .Supplied = true};
    }
    publishResolved<FoodOffer>(world, shop, offerEntries(world, shop, offer));
  }
}
```

Step 2: In addOperations, after the shop-service line, add:

```cpp
  addField<FoodOffer>(schema);
  schema.addResolver("food-offer", &resolveFoodOffers,
                     {"path-networks", "route-distance", "food-offer-field"});
```

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"; build/linux-debug/tpj_sim_tests -# "[#operations_test]"`
Expected: no build output; operations_test passes, its registration test included. The test pass's tests of resolved offers pass; those of stepped offers fail until Task 9.

### Task 9: Stepped offers

Files:
- Modify: `src/sim/operations/operations.cpp`

Step 1: Make serveGuests return the shop's service after serving. Change its signature to `ShopService serveGuests(World &world, EntityKey shop, bool supplied)`, its comment's end to "and serves the guest at the front when it can, giving its service after", and its last statement to:

```cpp
  if (stored != nullptr || !service.Queue.empty() || service.FreeAt != 0) {
    world.Registry.emplace_or_replace<ShopService>(entity, service);
  }
  return service;
```

Step 2: After resolveFoodOffers, add:

```cpp
// A shipment's units and the tick they arrive.
struct Arriving {
  uint64_t Tick = 0;
  int64_t Units = 0;
};

// The ticks the next visit waits, from the cycle stepping from, in which the shop first holds it,
// to the one that takes its guest, behind the queued guests. Supply units come from the stock at
// from, then from the shipments in the order given, then from an order arriving at ordered, and
// each guest is taken at the later of its unit's tick and the previous guest's take plus
// SERVICE_INTERVAL, the first no earlier than from or freeAt.
uint64_t nextWait(uint64_t from, uint64_t freeAt, size_t queued, int64_t stock,
                  const std::vector<Arriving> &shipments, uint64_t ordered) {
  auto shipment = shipments.begin();
  int64_t left = stock;
  uint64_t unitAt = from;
  uint64_t ready = std::max(from, freeAt);
  uint64_t taken = ready;
  for (size_t guest = 0; guest <= queued; ++guest) {
    while (left == 0 && shipment != shipments.end()) {
      unitAt = shipment->Tick;
      left = shipment->Units;
      ++shipment;
    }
    if (left > 0) {
      --left;
    } else {
      unitAt = ordered;
    }
    taken = std::max(ready, unitAt);
    ready = taken + SERVICE_INTERVAL;
  }
  return taken - from;
}

// Publishes the shop's offer for the next visit to arrive, from its service, stock, and shipments
// after serving, and the order it would place in this cycle.
void publishOffer(World &world, EntityKey shop, const std::optional<DepotRoute> &route,
                  const ShopService &service) {
  OfferEntry offer;
  if (route) {
    std::vector<Arriving> shipments;
    for (const FlowPacket &packet : addressedTo<Supplies>(world, shop).Packets) {
      if (packet.To == shop) {
        shipments.push_back({packet.Arrival, packet.Units});
      }
    }
    const uint64_t ordered =
        world.Tick + ORDER_DELAY + deliveryDelay(world, *route, shop, Layers::Both);
    offer = OfferEntry{.Relief = MEAL_RELIEF,
                       .Wait = nextWait(world.Tick + 1, service.FreeAt, service.Queue.size(),
                                        unitsHeld<Supplies>(world, shop, shop), shipments, ordered),
                       .Supplied = true};
  }
  publishStepped<FoodOffer>(world, shop, offerEntries(world, shop, offer));
}
```

Step 3: In stepShops, replace `serveGuests(world, shop, route.has_value());` with:

```cpp
    const ShopService service = serveGuests(world, shop, route.has_value());
    publishOffer(world, shop, route, service);
```

and its comment with "// Each shop, in ascending key order, orders supplies, serves its guests, and publishes its offer."

Run: `cmake --build --preset linux-debug --target tpj_sim_tests 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`, then `build/linux-debug/tpj_sim_tests -# "[#<file>]"` for operations_test and each of the test pass's operations files, one per invocation.
Expected: no build output; all pass.

### Task 10: Confirm the acceptance criteria

Run:
- `git ls-files -m -o --exclude-standard -- '*.h' '*.cpp' | grep -v "^parks" | xargs -r clang-format -i`
- `cmake --build --preset linux-debug 2>&1 | grep -E "^[^ ]+:[0-9]+:[0-9]+: (warning|error):"`
- `ctest --preset linux-debug`
- `cmake.exe --build --preset windows-debug`
- `ctest.exe --preset windows-debug`
- `scripts/cross-build-check.sh`

Expected: no warnings; every test passes on both builds; the cross-build check passes.

### Task 11: Review and commit

Step 1: Review the staged diff via the reviewing skill, as implementing-features step 7 describes, with FEATURE.md, the three changed SPEC.md files, and docs/principles.md as context.

Step 2: Commit once via commit-hygiene, staging `src tests plans` by path:

```
Operations: Publish each shop's food offer with its exact wait
```
