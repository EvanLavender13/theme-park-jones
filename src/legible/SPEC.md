# legible

Legible simulation (plans/legible-simulation): explanations of the simulation for display, computed from what the park publishes, so the player can see why the park behaves as it does (principle 8).

## Boundary

The library tpj_legible links tpj_sim alone, so its values are computed without a window (principle 10). It reads a world only through the medium's fields and networks, parkNetwork, park intent, and the owning modules' public queries, such as guestRecord, shopRecord, isAccepted, and nearestDepot, and makes candidate worlds only with makeCandidate (decision 0025). It takes a world by const reference and changes nothing, so nothing it computes enters a world's state, hash, or save (principle 1).

The app calls food.h through foodNear, preview.h for keepPreview and previewedWorld, inspect.h for pickSubject and inspectSubject, and park_summary.h for summarizePark. views calls foodAvailability.

## Food availability

Food availability at a place is how well fed a guest standing there could be: the sum, over the supplied shops it can reach, of each shop's relief discounted by the time it would take to get a meal there, walking and waiting (decision 0020). foodAvailability(world, place) gives the value and each shop's contribution, with its relief, route distance, wait, effective time, and term. The terms add up to the value exactly, so the value is attributed to its sources (principle 8). Shops' offers are found as guests find them.

Distance is measured along the paths, so a shop across a fence is as far as its route (principle 4). Every reachable supplied shop contributes, however far, and its term only falls as its effective time grows (principle 5). A place with no reachable supplied shop, or that is not on the guest network, has the value 0.

## Path places

foodNear(world, point, reach) gives food availability at the guest path place nearest a ground point, when that place lies within the reach, and none otherwise. It never picks a place on a connector, and ties between paths go to the lower key. The straight distance only locates the point beside the paths; what is explained there is measured along routes (principle 4).

## Previews

A preview shows what a tentative edit would do before it is committed (decision 0025). Its candidate is exactly the world committing the edit at the next cycle would give, and it has one only when the world accepts the edit. previewedWorld gives the candidate when there is one and the world otherwise, so what explains the committed park explains the candidate the same way.

keepPreview holds a preview between frames and makes it again only when the world's tick or the edit has changed, returning whether it did. A caller that replaces the world empties the kept preview.

A preview of adding or moving a shop also tells what the shop would find: where its guest connector would meet the guest path, the hungry footfall the world has there now, and the depot that would supply it with the length of its supply route, or none when the edit would leave the shop starved.

## Inspectors

An inspector explains one guest or shop from what it publishes about itself (decision 0025). pickSubject replaces the subject when the player picks a guest or shop and keeps it when they pick anything else or nothing. The subject is kept after its entity is gone, so the inspector can say what it was.

inspectSubject reads the subject's record afresh at each call, so an inspector shown every frame follows every tick, and it says when the subject is gone. For a guest it shows its activity, hunger, target shop, time left in its stay, meals eaten, and last meal, and a table of every option the guest would weigh if it chose now, with each term of its score and its probability (principle 8). For a shop it shows stock, queue, units on order, what limits it, and whether it is starved.

## Park summary

summarizePark gives what the Debug panel shows about the park: each shop and its record, the number of guests, their mean hunger, how many are waiting, and the meals eaten so far.
