# legible

Legible simulation (plans/legible-simulation). It computes explanations of the simulation for display, from what the park publishes, so the player can see why the park behaves as it does (principle 8).

## Boundary

The library tpj_legible links only tpj_sim, so its values are computed without a window (principle 10). It reads a world only through:

- the medium's fields and networks
- parkNetwork
- park intent
- the owning modules' public queries, such as guestRecord, shopRecord, isAccepted, and nearestDepot

It makes candidate worlds only with makeCandidate (decision 0025). It takes a world by const reference and changes nothing, so nothing it computes enters a world's state, hash, or save (principle 1).

The app calls:

- food.h for foodAvailability
- preview.h for keepPreview and previewedWorld
- inspect.h for pickSubject and inspectSubject
- park_summary.h for summarizePark

views calls availability.h for availabilityValues and availabilityBends. The app and tpj_bench read FOOD_AVAILABILITY from food.h.

## Availability

An availability at a place is how well one need is served by the sources a guest standing there can reach. It is the sum, over the supplied sources the guest can reach, of each source's relief discounted by the time it would take to be served there, walking and waiting (decision 0020).

An AvailabilityDefinition in availability.h says which need. It gives the offer its sources publish, the curve that discounts a relief by time, and Half, the value an overlay of it shades halfway up its ramp. Adding an availability for another need, such as thirst, takes a new definition and no new code.

availabilityAt(world, definition, place) gives the value and each source's contribution: its relief, route distance, wait, effective time, and term. The terms add up to the value exactly, so the value is attributed to its sources (principle 8). Sources' offers are found the same way guests find them.

availabilityValues(world, definition, places) gives the value at each of many places in one call. Each value is exactly what availabilityAt gives at that place, so a shade and the tooltip over it agree. It finds each source's offer once per call, however many of the places reach the source. It keeps nothing after it returns.

availabilityBends(world, definition) gives the places along each guest segment's line where its availability can bend: each stop of the line, and inside each edge each place where a supplied source's route turns from one end to the other, or its effective time reaches a point of the curve. They come in ascending segment key, then ascending distance. Between two consecutive bends on a line, availability is linear in distance, so the values at the bends give it exactly everywhere.

Distance is measured along the paths, so if a source is across a fence, its distance is the length of its route (principle 4). Every reachable supplied source contributes, however far. Its term only falls as its effective time grows (principle 5). If a place can reach no supplied source, or is not on the guest network, its value is 0.

Food availability is FOOD_AVAILABILITY, in food.h. Its sources are shops' food offers. Its Half is a meal's relief with no walk and no wait, so a color means the same availability in every park. foodAvailability(world, place) gives it at a place.

## Previews

A preview shows what a tentative edit would do before it is committed (decision 0025). Its candidate is exactly the world that committing the edit at the next cycle would give. A preview has a candidate only when the world accepts the edit. previewedWorld gives the candidate when there is one, and the world otherwise. Every explanation of the committed park works on the candidate the same way.

keepPreview holds a preview between frames. It makes the preview again only when the world's tick or the edit has changed, and returns whether it did. If a caller replaces the world, the caller must empty the kept preview.

A preview of adding or moving a shop also tells what the shop would find:

- where its guest connector would meet the guest path
- the hungry footfall the world has there now
- the depot that would supply it, with the length of its supply route, or none when the edit would leave the shop starved

## Inspectors

An inspector explains one guest or shop from what it publishes about itself (decision 0025). pickSubject replaces the subject when the player picks a guest or shop. It keeps the subject when they pick anything else or nothing. The subject is kept after its entity is gone, so the inspector can say what it was.

inspectSubject reads the subject's record afresh at each call, so if an inspector is shown every frame, it follows every tick. It says when the subject is gone. For a guest it shows:

- its activity, hunger, target shop, time left in its stay, meals eaten, and last meal
- a table of every option the guest would weigh if it chose now, with each term of its score and its probability (principle 8)

For a shop it shows stock, queue, units on order, what limits it, and whether it is starved.

## Park summary

summarizePark gives what the Debug panel shows about the park: each shop and its record, the number of guests, their mean hunger, how many are waiting, and the meals eaten so far.
