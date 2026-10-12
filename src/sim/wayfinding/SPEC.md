# wayfinding

This module serves the navigable-networks capability (plans/navigable-networks). It derives the fields that lead guests to the best of many places, such as the hundreds of seats along a park's paths, without a route search for each place (principles 4 and 6, decision 0031). It is part of tpj_sim and follows its contract (src/sim/SPEC.md).

## Boundary

wayfinding reads furniture only through the seat places field, operations only through the seat offers field, and the guest network through routes' parkNetwork. It finds the best seats with routes' bestPlaces and sampleBestPlaceEdge. addBestSeats registers the fields best-rest-seats and best-meal-seats and the resolver best-seats, which publishes them. addPark calls it after addOperations. Other modules read the fields by sampling BestRestSeats and BestMealSeats, or through bestSeatsAt, from sim/wayfinding/best_seats.h.

## Best seats

Best rest seats and best meal seats are resolved entry fields on the guest network, named best-rest-seats and best-meal-seats. Each field has one source, its own field key, so a sample's cost doesn't grow with the number of pieces in the park. Every piece of furniture with a seat place and a seat offer seeds both fields. It seeds best rest seats with its rest relief and best meal seats with its meal relief, if that relief is above 0. Its owner is the segment that lays it.

Sampled at any place on the guest network, each field gives the pieces that fewer than SEAT_CHOICES other pieces dominate there, as routes' best places search defines it. Each entry is a BestPlace naming the piece, its segment, its seat place, its relief, its route distance, and the next step toward it. If a guest scores seats by the field's relief and the walk, whatever weight it gives each, its own SEAT_CHOICES best are among them.

The fields follow from intent alone and are derived only in resolution. They don't know which seats are taken.

bestSeatsAt(world, network, place, use, scratch, out) fills out with the entries of best rest seats or best meal seats at the place, by use. They are exactly the values sampleField gives there, in the same order. It reuses out and the scratch's lists, so it allocates only while they grow. A guest can therefore read the field in every tick.
