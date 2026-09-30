# Research: eating-guests

## When does a guest choose?

At every node it leaves, when it loses its target, and after its visit comes back. A guest decides when it stands at a node with distance left to walk, which is exactly where wandering-guests already takes its wander pick, so a node reached with the tick's distance used up is decided in the next cycle, once. A guest that loses its target, because its shop can no longer be reached or says no meals, chooses at once, wherever it stands, even inside an edge, so it never walks on toward a shop it cannot use. A served or turned-away guest stands at the shop's anchor, a node, so it chooses there in the cycle its visit comes back. After a choice the guest checks its new target at once, so a guest that picks the shop whose anchor it stands at sends its visit in the same cycle. A guest chooses at most once between steps, which keeps a pick from being redrawn at the same place. Heading home becomes one of the options once the stay is over and route distance gives an entrance entry, instead of a switch the stay flips: at 10 against offers that score at most about 2.3, its probability is within e^-30 of 1, so a guest whose stay is over leaves, and a guest with no route home simply has no such option and wanders.

Rejected: choosing every tick. Probabilities would compound over thirty draws a second, and a lightly hungry guest would flicker between targets. Rejected: choosing only once hunger passes a level. It is a threshold, which principle 5 rules out; the hunger curve and the carry-on option do that work as a gradient. Rejected: heading home as soon as the stay ends, as wandering-guests does. It bypasses the choice the milestone asks for, and leaves no explanation of why the guest left.

Sources: plans/believable-guests/hungry-guests/MILESTONE.md, criteria 3, 4, 5, and 7; src/sim/guests/SPEC.md, Walking.

## How does a guest find the offers it can reach?

Through route distance. Every shop, entrance, and depot anchoring a node of the guest network is a source of guest route distance, so the sources with an entry at the guest's place are exactly the entities it can walk to. For each, the guest samples the food-offer field at the nodePlace of the source's lowest anchored node and takes the source's own entry. A shop publishes one entry there, and an entrance publishes none, so it is never an offer. The guest never lists shops from intent, and reads no shop's state, only the offer the shop publishes (principles 3 and 6).

Rejected: listing the shop boxes with parkBoxes. It is intent a guest has no need of, and it would name shops the guest cannot reach, which route distance already filters. Rejected: sampling the offer at the guest's own place. Offers are published only at shops' anchors, so the guest would see nothing until it stood at one.

Sources: src/sim/operations/SPEC.md, Food offer; src/sim/routes/SPEC.md, Route distance.

## How is the softmax pick made deterministic?

Each option's weight is simExp of its score less the greatest score, divided by the temperature, so the best option weighs exactly 1, no weight overflows, and the total is at least 1, a normal double. The probability of each is its weight over the total, and the pick is drawPick's double overload over the same weights with a keyed draw. Subtracting the greatest score is the standard way to evaluate softmax without overflow, and it leaves the probabilities unchanged. Every step is basic arithmetic or simExp, so every build gives the same bits (decision 0022).

Rejected: adding Gumbel noise to each score and taking the greatest. It picks with the same probabilities, but needs one draw and a simLog per option, and the probabilities it would record are computed separately from the pick. Rejected: exponentiating raw scores. Heading home's 10 at temperature 0.25 gives e^40, and scores added later could overflow.

Sources: https://en.wikipedia.org/wiki/Softmax_function, softmax and its invariance to shifting every score; https://en.wikipedia.org/wiki/LogSumExp, subtracting the greatest argument to avoid overflow; docs/decisions/0019-guest-choice.md.

## How does a guest know it was served?

By its own stocks. The shop returns every visit to its guest, with a meal when it served it, both sent in the same tick with the same delay, so they arrive together. A waiting guest checks each cycle whether it holds a guest-visits unit under its own key; when it does, it consumes it, and a meals unit held with it means it was served, so it consumes that as eaten and its hunger falls. It never reads the shop's queue, stock, or record. A deleted shop's held visits come back with no meal, so the guest reads that as unserved like any other.

Rejected: reading the shop's inspection record for the queue. Records are for tooling and tests (decision 0025), and it would be one entity reading another's internals (principle 6).

Sources: src/sim/operations/SPEC.md, Service; src/sim/medium/SPEC.md, Flows.

## Where does the last choice live?

In the guest's state, saved with it. A choice depends on route distance, offers, hunger, and a draw at the tick it was made, and the world has moved on by the time anyone asks, so it cannot be recomputed from the world. It is a few options of eight numbers each, rewritten at each node, and a loaded world must explain its guests' last choices exactly as the saved one did (principle 1). The record gives it as none until the guest has chosen, and its last meal as none until it has eaten, since saves hold plain values, not optionals.

Rejected: recomputing the last choice for the record. The inputs are gone. Rejected: keeping a history of choices. The milestone asks for the last one, and a history grows without bound.

Sources: src/sim/SPEC.md, the walk and saves; docs/decisions/0025-outside-the-medium.md.
