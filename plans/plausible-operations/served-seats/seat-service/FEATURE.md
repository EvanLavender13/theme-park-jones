# Feature: Seat Service

## Summary

Each piece of furniture serves guests from its seats. Each piece with a seat place publishes a seat offer at that place, holding the rest relief and the meal relief a seat there gives, by kind, and its number of seats. Each piece takes seat visits. While one of its seats is free, it seats the guest for SEAT_TIME and then sends the visit back with a rest. When every seat is taken, it sends the visit back the next tick with nothing. A tick steps only the pieces holding visits. The food-shop scenario furnishes a guest path, and its synthetic guests visit a bench there, so the cross-build check runs seat service. This is the only feature of served-seats.

## Acceptance criteria

1. Seat offers is a resolved entry field on the guest network. Each piece with a seat place is the source of one entry at its seat place's place. The entry holds BENCH_REST_RELIEF and BENCH_MEAL_RELIEF for a bench, TABLE_REST_RELIEF and TABLE_MEAL_RELIEF for a table, and the seat place's number of seats. (check: build/seat-service/offercheck.exe on tests/parks/furnished-paths.park prints `pieces 85 offered 85 matching 85`; the park world checks)
2. A piece seats a visiting guest while one of its seats is free, for SEAT_TIME, and then the visit comes back with a rest. The seat is free from the tick the visit comes back. When every seat is taken, the visit comes back after RETURN_DELAY with no rest. (check: build/seat-service/servecheck.exe prints the lines PLAN.md Task 10 gives)
3. Seat visits and rests conserve with the ledger's identity at every tick. If an edit removes a piece, a visit on its way to it, or one that reached it in the same tick, comes back to its guest with no rest, and a guest it seated still gets its rest when its time is up. If a seated guest leaves, its visit and rest are consumed as abandoned, or as undeliverable when the piece is gone as well. (check: the same servecheck run)
4. A tick steps only the pieces that hold seat visits. On full.park, the runtime report's ticks stay within their noise of build/seat-places/after.report, and resolution rises only by the seat offers' resolver. (check: scripts/runtime-report.sh --compare, and review)
5. The food-shop scenario's synthetic guests visit a bench on a furnished guest path, and some are seated and some turned away. (check: build/seat-service/scenariocheck.exe prints seated and turned-away counts above 0; tpj_scenarios output of windows-debug and windows-release is the same by --compare)
6. Otherwise the simulation is unchanged. Each slice park that `tpj_scenarios --slice-parks` remakes differs from the checked-in file only by the empty seat-places-stepped, seat-visits-ledger, and seat-offers-stepped sections. (check: diff, PLAN.md Task 11)
7. The park world checks pass, naming seat-service as unheld until believable-guests' seated-meals. (ctest)

## Tests

None. furnished-paths.park brings seat offers under the park world checks, and the seat state is named unheld until seated-meals' parks hold seated guests (docs/testing.md).

## Medium

- Samples, in resolution: seat places (effortless-building's furnished-paths), every slot, through the new medium read resolvedSlots.
- Publishes, in resolution: seat offers, one entry for each piece with a seat place, sourced by the piece. navigable-networks' best-seats and believable-guests' seated-meals read it.
- Draws: seat visits, sent by guests to a piece's key. Synthetic guests send them in this feature, and believable-guests in seated-meals.
- Supplies: seat visits back to their guests, and rests, one to each guest seated.
- State: seat-service on each piece's entity, the tick each taken seat frees. Saved. No park holds it until seated-meals, so the park world checks name it as unheld.

Operations reads furniture only through the seat places field. Guests learn a piece was full only from a visit that comes back with no rest (principle 6).

## Spec changes

src/sim/operations/SPEC.md:

- In Boundary, after the addFacilities list, add: "addOperations last calls addSeats, which registers the flow kind seat-visits, a piece's seat state, the field seat-offers and the resolver that publishes it, and the system that steps the pieces holding seat visits, after benches and toilet blocks. It needs furniture's registrations before it, and addPark calls addFurniture before addOperations."
- In Boundary, after the paragraph that begins "Shops, depots, and facilities read", add: "Seats read the seat places field, their own seat offers, and their own ledger stocks. They read furniture only through the seat places field."
- In Boundary, after the facilities.h paragraph, add: "Other modules call seats.h for the SeatVisits flow kind, through which guests visit furniture and get rests, the SeatOffers field and its SeatOffer, SEAT_TIME, and the relief of each kind's seat."
- Add a section at the end:

      ## Seats

      Each piece of furniture with a seat place serves guests from its seats. It needs no supplies.

      Each such piece publishes one SeatOffer in the resolved entry field seat-offers, at its seat place's place. The offer holds the tiredness a rest there relieves and the hunger a meal eaten there relieves, each on a scale from 0 to 1, and the piece's number of seats. A table relieves a meal more and tiredness less than a bench. The offers follow from the seat places alone, so a candidate's pieces offer at once, and a piece no longer laid offers nothing. An offer has no wait, because no guest waits at a piece.

      Guests visit a piece through seat-visits. A piece takes the visits that reach it in a tick one at a time. While one of its seats is free, it gives the seat to the guest, who keeps it for SEAT_TIME. Then the visit comes back with a rest, and the seat is free from that tick. If every seat is taken, the visit comes back after RETURN_DELAY with no serving. A piece with no seat place has no seats, so every visit to it comes back that way. A tick's work for seats grows with the visits reaching pieces, never with the number of pieces (decision 0031).

      When a resolution no longer lays a piece, visits on their way to it, and visits that reached it in the same tick, come back to their guests with no serving. Guests it seated still get their rests when their time is up. If a visit or rest is sent to a guest who has left, it is consumed as abandoned, or as undeliverable when the piece is gone as well. Every unit is accounted for, so the rests furniture creates always equal the guests it has seated.

src/sim/medium/SPEC.md, after the sentence "sampleResolvedField samples the resolved layer alone. It is for resolvers, since stepped entries are state.", add: "resolvedSlots gives every source's resolved entries of a field, sources ascending. It is for resolvers that derive entries from all of another field's."

src/scenarios/SPEC.md, in Scenarios, change the last bullet to: "shops selling food and drink, whose supply is cut and restored, a bench and a toilet block, and a furnished guest path whose bench seats guests and turns them away when full".

## Public interface

src/sim/medium/field.h:

    // Every source's resolved entries of the field, sources ascending, or none before the field's
    // first resolution. Resolvers read with it, since stepped entries are state.
    template <FieldDefinition F>
    std::span<const FieldSlot<typename F::Entry>> resolvedSlots(const World &world);

src/sim/operations/seats.h:

    // A guest's visit to a piece of furniture, addressed to the guest, which the piece returns to it.
    struct SeatVisits { static constexpr std::string_view Name = "seat-visits"; };

    // What a seat at a piece offers: the tiredness a rest there relieves and the hunger a meal eaten
    // there relieves, each on a scale from 0 to 1, and the piece's number of seats.
    struct SeatOffer {
      double RestRelief = 0.0;
      double MealRelief = 0.0;
      uint32_t Seats = 0;
      bool operator==(const SeatOffer &) const = default;
    };
    // visitFields: rest-relief, meal-relief, seats

    // The seat offers field: each piece's offer, at its seat place.
    struct SeatOffers {
      using Entry = SeatOffer;
      static constexpr std::string_view Name = "seat-offers";
      static constexpr FieldKind Kind = FieldKind::Entry;
    };

    // Ticks a guest keeps its seat at a piece.
    inline constexpr uint32_t SEAT_TIME = 300;
    // The relief a seat at each kind gives.
    inline constexpr double BENCH_REST_RELIEF = REST_RELIEF;
    inline constexpr double BENCH_MEAL_RELIEF = 0.4;
    inline constexpr double TABLE_REST_RELIEF = 0.2;
    inline constexpr double TABLE_MEAL_RELIEF = MEAL_RELIEF;

    // The offer of a seat at a piece of the kind with the seats.
    constexpr SeatOffer seatOffer(FurnitureKind kind, uint32_t seats);
    // Registers the flow kind seat-visits, the seat-service state, the seat-offers field and its
    // resolver, then the system that steps the pieces holding seat visits. addOperations calls it
    // last.
    void addSeats(WorldSchema &schema);

## Files affected

- Create: src/sim/operations/seats.h, src/sim/operations/seats.cpp, src/sim/operations/internal/seat_service.h
- Modify: src/sim/medium/field.h, src/sim/medium/SPEC.md
- Modify: src/sim/operations/operations.cpp, src/sim/operations/operations.h (addOperations' comment), src/sim/operations/facilities.h (addFacilities' comment), src/sim/operations/SPEC.md, src/sim/CMakeLists.txt
- Modify: src/scenarios/food_shop.cpp, src/scenarios/SPEC.md
- Modify: tests/sim/park_world_checks_test.cpp (UNHELD_TYPES)
- Scratch: build/seat-service/

## Dependencies

- effortless-building/furnished-paths: pieces as entities and the seat places field. Merged.
- plausible-operations/served-needs: the Rests flow, REST_RELIEF, RETURN_DELAY, ABANDONED_CAUSE. Merged.

## Out of scope

- Guests choosing, walking to, and using seats: believable-guests' seated-meals.
- The best-seats fields: navigable-networks' best-seats.
- An inspection record for a piece: a deepening candidate of served-seats.
- Retiring the bench box: the slice's last step.

## Open questions

None.
