# Feature: Seat Places

## Summary

Furniture publishes where its seats are. A new resolved entry field, seat places, holds one entry for each piece of furniture, at the place on its segment's ground line nearest the piece. Each furnished segment is the source of its pieces' entries. An entry gives the piece's key, its pose, its kind, its number of seats, and its segment. Nothing reads the field yet; served-seats, best-seats, and seated-meals will. The full park gains furniture on every guest segment, benches on the left and tables on the right, so the runtime report measures a park with several hundred pieces. The report is compared with one made before the milestone's first feature. This feature closes the furnished-paths milestone.

## Acceptance criteria

1. Seat places is a resolved entry field on the guest network. Each furnished segment is the source of one entry per piece it lays. The entry lies at the place on the segment's ground line nearest the piece, and holds the piece's key, its pose, its kind, its number of seats, and its segment. A bench has 2 seats and a table 4. (the check in PLAN.md's Task 6, which samples the field at each piece's place in tests/parks/furnished-paths.park and finds that piece's entry, and the park world checks, which hold the field through furnished-paths.park)
2. Nothing reads seat places yet, so the simulation is unchanged. The parks that tpj_scenarios --slice-parks remakes differ from main's only by an empty seat-places-stepped section, which every registered field adds to a save. (the diff in PLAN.md's Task 7)
3. tests/parks/stress/full.park carries furniture on every guest segment, benches on each segment's left side and tables on its right, every 10 m, several hundred pieces in all. Its 2,000 guests, 30 shops, 15 toilet blocks, and 45 benches are as before. (the count in PLAN.md's Task 8)
4. The runtime report on full.park after this feature is compared with one made at the commit before the milestone's first feature. Tick times stay about the same, since nothing on the tick path reads furniture. Resolution grows by the cost of laying furniture, which grows with the pieces and what lies near each furnished segment, never with segments times boxes. (the comparison in PLAN.md's Task 9, and review)
5. The build has no warnings, ctest passes, and scripts/tidy.sh is clean. (PLAN.md's Task 10)

## Tests

None. furnished-paths.park already lays furniture, so the park world checks cover the field's resolved layer and its saved stepped layer.

## Medium

The feature adds the seat places field, a resolved entry field on the guest network. sim/furniture produces it. Each furnished guest segment is the source of the entries for the pieces it lays. plausible-operations' served-seats, navigable-networks' best-seats, and believable-guests' seated-meals will consume it, as the somewhere-to-sit slice's medium map says. This feature adds no consumer.

sim/furniture now also reads the guest network through routes' parkNetwork, to find each entry's place with Network::nearestPlaceOn. It reads nothing private to another module (principle 6).

## Spec changes

src/sim/furniture/SPEC.md:

- Boundary, second sentence: "addFurniture registers the derived types furniture-piece and laid-furniture, the field seat places, and the furniture resolver." Add after the first sentence: "It reads the guest network through routes' parkNetwork." Last sentence: "Other modules read the furniture through parkFurniture and furnitureSize, and its seats through the seat places field and furnitureSeats."
- Add a section after Laying:

      ## Seat places

      Seat places is a resolved entry field on the guest network, named seat-places. Each furnished guest segment is the source of one entry for each piece it lays, in key order of the pieces. The entry lies at the place on the segment's ground line nearest the piece's position. A SeatPlace holds the piece's key, its pose, its kind, its number of seats, and its segment. furnitureSeats gives a kind's number of seats: 2 for a bench and 4 for a table. If the guest network has no carrier for a piece's segment, the piece has no entry. A segment that lays no piece publishes nothing.

src/scenarios/SPEC.md, The full park, a new item after the benches: "furniture on every guest segment, benches on its left side and tables on its right, every 10 m".

src/bench/SPEC.md, the stress parks paragraph: "full.park is the full park: 2,000 guests, 30 shops selling food, drink, or both, 15 toilet blocks, 45 benches, furniture on every guest segment, and 4 km of guest path, a minute into play."

Public interface, src/sim/furniture/furniture.h:

    // A kind's number of seats.
    constexpr uint32_t furnitureSeats(FurnitureKind kind);

    // A piece's seats as the seat places field gives them: the piece's key, its pose, its kind, its
    // number of seats, and its segment.
    struct SeatPlace {
      EntityKey Piece = NULL_KEY;
      Pose At;
      FurnitureKind Kind = FurnitureKind::Bench;
      uint32_t Seats = 0;
      EntityKey Segment = NULL_KEY;

      bool operator==(const SeatPlace &) const = default;
    };

    template <typename Visitor> void visitFields(Visitor &visitor, SeatPlace &place);

    // The seat places field: each furnished segment's pieces, at the places on its ground line
    // nearest them.
    struct SeatPlaces {
      using Entry = SeatPlace;
      static constexpr std::string_view Name = "seat-places";
      static constexpr FieldKind Kind = FieldKind::Entry;
    };

## Files affected

- Modify: src/sim/furniture/SPEC.md, src/sim/furniture/furniture.h, src/sim/furniture/furniture.cpp
- Modify: src/scenarios/SPEC.md, src/scenarios/full_park.cpp
- Modify: src/bench/SPEC.md
- Modify: tests/parks/stress/full.park
- Modify: plans/effortless-building/furnished-paths/MILESTONE.md (Status)

## Dependencies

- yielding-furniture: the pieces, laid along each side's line. Merged.
- The shared medium's entry fields and Network::nearestPlaceOn. Present.

## Out of scope

- Any reader of seat places: served-seats, best-seats, and seated-meals.
- Removing the bench box, and full.park's 45 benches with it: seated-meals, at the slice's end.
- Furnishing the slice parks: seated-meals regenerates the day parks with furniture.

## Open questions

None. The seat counts are tuning values that served-seats and play adjust.
