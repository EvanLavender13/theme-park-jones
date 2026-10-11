# Feature: Best Seats

## Summary

Two resolved entry fields on the guest network, best rest seats and best meal seats, lead guests to the furniture that serves a need best. Sampled anywhere on the guest paving, each gives the pieces that fewer than SEAT_CHOICES other pieces beat on both walk and relief, with each piece's segment, seat place, relief, route distance, and next step. A new search in routes derives them. It starts from every piece at once and keeps the best few at each node, so its work grows with the network, not with the network times the pieces. A rule for places inside an edge gives the same set between nodes. A new module, sim/wayfinding, sits between operations and guests. It seeds the search from seat places and seat offers and publishes the two fields, each under one source. This is the only feature of best-seats.

## Acceptance criteria

1. Best rest seats and best meal seats are resolved entry fields on the guest network, named best-rest-seats and best-meal-seats. Each field's one source is its own field key. Every piece with a seat place and a seat offer seeds a field if its relief for that field's need is above 0. Each entry is a BestPlace naming the piece, its segment, its seat place, its relief, its route distance from the sampled place, and its next step. (check: build/best-seats/bandcheck.exe prints a seeds count of 85 for each field on tests/parks/furnished-paths.park and 644 on tests/parks/stress/full.park)
2. At every node of the guest network, each field gives exactly the pieces that fewer than SEAT_CHOICES others dominate there, in order of distance, then higher relief, then lower key. A brute-force check runs one search per piece. (check: bandcheck prints `nodes N matching N` for each field on both parks)
3. At every edge's midpoint and at every seat place strictly inside an edge, each field gives exactly that place's pieces by the same rule. At a piece's own seat place, its entry has distance 0 and a next step with carrier NULL_KEY. (check: bandcheck prints `places P matching P`)
4. A mover that starts at a node and follows a piece's next steps, sampling again at the end of each, reaches the piece's seat place. Each step's length plus the distance sampled at its end equals the distance sampled at its start, exactly. From an edge's midpoint, the walk also reaches the piece, and its lengths add up to the first distance within 1e-9 m. (check: bandcheck prints `walks W arrived W exact W` from nodes and `walks V arrived V close V` from midpoints)
5. The fields derive from intent alone. A candidate gives what its commit gives, and a loaded park resolves them equal. (ctest: the park world checks, through furnished-paths.park and noon.park's chosen furnishing edit)
6. Ticks are unchanged, and on full.park resolution rises by less than 1.41 ms over build/seat-service/after.report's 14,132.9 µs median. (check: scripts/runtime-report.sh --compare, PLAN.md Task 11)
7. Otherwise the simulation is unchanged, and both builds derive the fields alike. Each slice park that tpj_scenarios --slice-parks remakes differs from the checked-in file only by five empty sections, and tpj_scenarios output of windows-debug and windows-release is the same by --compare. (check: PLAN.md Task 10)
8. sim/wayfinding is its own layer between sim/operations and sim/guests, and the layer check passes. (ctest)

## Tests

None. furnished-paths.park brings the fields under the park world checks (docs/testing.md). The slice adds the integration tests that compare parks with tables near and far.

## Medium

- Samples, in resolution: seat places (effortless-building's furnished-paths), every slot, for each piece's key, seat place, and segment. Seat offers (plausible-operations' served-seats), by piece, for its rest relief and meal relief. Both are read through resolvedSlots.
- Reads: the guest network, through parkNetwork, to walk its edges.
- Publishes, in resolution: best rest seats and best meal seats, one source each. believable-guests' seated-meals samples them to choose a seat and walk to it. legible-simulation's explained-seating samples them for its overlays.
- State: none.

The fields don't know which seats are taken. A guest learns that only by visiting a piece (principle 6).

## Spec changes

src/sim/routes/SPEC.md:

- In Boundary, after "sampleField, and routeEntryAt,", add "the best places search through bestPlaces and sampleBestPlaceEdge,".
- Add a section at the end:

      ## Best places

      bestPlaces finds the best few of many seeds from every node of a network, in one search. A PlaceSeed has a key, an owner, a place on the network, and a value, where a higher value is better. If a seed's place doesn't resolve, it is left out. If two seeds share a key, bestPlaces throws std::invalid_argument.

      A seed's distance from a place is the least length of a route along edges from that place to the seed's place, added from the seed's end, as route distance's are (principle 4). From a place, one seed dominates another when its distance there is no longer, its value is no lower, and it comes first in order of distance, then higher value, then lower key. At each node, bestPlaces gives the seeds that fewer than a given number of other seeds dominate there, in that order. Each comes as a BestPlace: the seed's key, owner, place, and value, its distance, and the next step toward it. A node's entries are at its nodePlace. A seed whose place lies strictly inside an edge also gets one entry at its own place. Where a seed's place lies, its entry has distance 0 and a Next with carrier NULL_KEY, to show the route has arrived.

      A next step follows an edge as route distance's does, except that a step into the edge holding the seed's place ends at that place. Of the steps that achieve a node's distance, the one first by route distance's rule is taken, so the result depends only on the network and the seeds (principle 10).

      Every node a next step leads to also gives that seed. Suppose a mover starts at a node, walks Next along its carrier to Next.To, and samples there. Repeating this, it reaches the seed's place, and the lengths it walks add up exactly to the distance it first sampled. Rounding can make routes of different lengths equal. A node then gives the seeds the search keeps by the order above, which can differ slightly from what a separate search for each seed would find.

      sampleBestPlaceEdge is the rule for a place strictly inside an edge, for a field whose entries bestPlaces made. A seed's distance there is the least of three walks: to the edge's From end plus the seed's distance there, to its To end plus the seed's distance there, and, if the seed's place lies on the edge, along the edge to it. The rule gives the seeds that fewer than the given number of others dominate there, in the same order, each with the next step of its least walk. A mover that follows them reaches the seed, and the lengths it walks add up to the distance it sampled, allowing for rounding. The rule allocates nothing but out's growth, so movers can sample on the tick path.

      bestPlaces' work grows with the network's edges times the number it keeps times the number of different values among the seeds. It doesn't grow with the seeds times the network.

src/sim/wayfinding/SPEC.md (new):

    # wayfinding

    This module serves the navigable-networks capability (plans/navigable-networks). It derives the fields that lead guests to the best of many places, such as the hundreds of seats along a park's paths, without a route search for each place (principles 4 and 6, decision 0031). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

    ## Boundary

    wayfinding reads furniture only through the seat places field, operations only through the seat offers field, and the guest network through routes' parkNetwork. It finds the best seats with routes' bestPlaces and sampleBestPlaceEdge. addBestSeats registers the fields best-rest-seats and best-meal-seats and the resolver best-seats, which publishes them. addPark calls it after addOperations. Other modules read the fields by sampling BestRestSeats and BestMealSeats, from sim/wayfinding/best_seats.h.

    ## Best seats

    Best rest seats and best meal seats are resolved entry fields on the guest network, named best-rest-seats and best-meal-seats. Each field has one source, its own field key, so a sample's cost doesn't grow with the number of pieces in the park. Every piece of furniture with a seat place and a seat offer seeds both fields. It seeds best rest seats with its rest relief and best meal seats with its meal relief, if that relief is above 0. Its owner is the segment that lays it.

    Sampled at any place on the guest network, each field gives the pieces that fewer than SEAT_CHOICES other pieces dominate there, as routes' best places search defines it. Each entry is a BestPlace naming the piece, its segment, its seat place, its relief, its route distance, and the next step toward it. If a guest scores seats by the field's relief and the walk, whatever weight it gives each, its own SEAT_CHOICES best are among them.

    The fields follow from intent alone and are derived only in resolution. They don't know which seats are taken.

src/sim/SPEC.md, in the list under "It registers in dependency order:", add "6. wayfinding" after "5. operations", and renumber guests to 7 and the routes finisher to 8.

## Public interface

src/sim/routes/best_places.h:

    // A place a search starts from, such as a piece of furniture: its key, the key of the entity
    // it belongs to, its place, and its value, higher being better.
    struct PlaceSeed {
      EntityKey Key = NULL_KEY;
      EntityKey Owner = NULL_KEY;
      Place At;
      double Value = 0.0;
    };

    // A seed as found from a place: the seed, its route distance, and the first step toward it.
    struct BestPlace {
      EntityKey Key = NULL_KEY;
      EntityKey Owner = NULL_KEY;
      Place At;
      double Value = 0.0;
      double Distance = 0.0;
      RouteStep Next;
      bool operator==(const BestPlace &) const = default;
    };
    // visitFields: key, owner, at, value, distance, next

    // At each node, the seeds that fewer than keep others dominate there, at the node's nodePlace,
    // nodes in order and each node's seeds in order of distance, then higher value, then lower
    // key. Then one arrival entry for each seed strictly inside an edge, at its own place, in key
    // order. Throws std::invalid_argument when two seeds share a key.
    std::vector<PlacedEntry<BestPlace>> bestPlaces(const Network &network,
                                                   std::span<const PlaceSeed> seeds,
                                                   uint32_t keep);

    // Appends to out the seeds that fewer than keep others dominate at the sample's place, in
    // the order bestPlaces gives a node's. Allocates nothing but out's growth.
    void sampleBestPlaceEdge(const EdgeSample<BestPlace> &sample, uint32_t keep,
                             std::vector<BestPlace> &out);

src/sim/wayfinding/best_seats.h:

    // The pieces each best seats field keeps at a place: those fewer than this many others beat.
    inline constexpr uint32_t SEAT_CHOICES = 4;

    // The best seats for rest: pieces of furniture by rest relief and the walk.
    struct BestRestSeats {
      using Entry = BestPlace;
      static constexpr std::string_view Name = "best-rest-seats";
      static constexpr FieldKind Kind = FieldKind::Entry;
      static void sampleEdge(const EdgeSample<BestPlace> &sample, std::vector<BestPlace> &out);
    };

    // The best seats for a meal: pieces of furniture by meal relief and the walk.
    struct BestMealSeats { /* as BestRestSeats, named "best-meal-seats" */ };

    // Registers the best rest seats and best meal seats fields, then the resolver best-seats.
    // addPark calls it after addOperations.
    void addBestSeats(WorldSchema &schema);

## Files affected

- Create: src/sim/routes/best_places.h, src/sim/routes/best_places.cpp
- Create: src/sim/wayfinding/best_seats.h, src/sim/wayfinding/best_seats.cpp, src/sim/wayfinding/SPEC.md
- Modify: src/sim/routes/SPEC.md, src/sim/SPEC.md, src/sim/CMakeLists.txt, src/sim/park_schema.cpp
- Modify: cmake/layers.txt, tests/checks/layer_check_test.cmake (the repository's expected layers)
- Scratch: build/best-seats/

## Dependencies

- effortless-building/furnished-paths: the seat places field. Merged.
- plausible-operations/served-seats: the seat offers field and resolvedSlots. Merged.

## Out of scope

- Guests sampling the fields, choosing a seat, and walking to it: believable-guests' seated-meals.
- Overlays and hover over the fields: legible-simulation's explained-seating.
- A distance limit and updating the fields only near an edit: deepening candidates of best-seats.

## Open questions

None.
