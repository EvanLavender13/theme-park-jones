# Research: wandering-guests

## How does a guest hold where it is and which way it walks?

A place and a direction along its carrier. The place is the medium's Place, a carrier and a distance, so it names the same ground position however edits cut the carrier into edges, and carried-guests can carry it with carryOver. The direction says whether the guest walks toward higher or lower distances. The edge the guest is on, and the stop ahead, come from resolving the place each tick, so a junction added under a walking guest is simply a node it reaches and decides at. The way back at a node is the step along the guest's own carrier opposite its direction, which is exactly the edge it came in on, even where a carrier stops at one node twice. A new guest stands at its entrance's anchor, distance 0 on the entrance's connector, walking toward higher distances, so it has no way back and takes the connector.

Rejected: holding the edge as two stop distances, as a RouteStep does. An edit that splits the edge leaves a stale stop the guest would walk to, and the stop is already given by resolving the place.

Sources: src/sim/medium/SPEC.md, places and resolve; src/sim/routes/SPEC.md, RouteStep and the mover's walk.

## When in the cycle do guests arrive?

After the guests already in the park step, in the cycle that ends each ARRIVAL_INTERVAL ticks: the one stepping a tick t with t + 1 a multiple of ARRIVAL_INTERVAL. A new guest makes its first walk in the next cycle, so every guest that steps has stood at a resolved place since the last swap. A new park admits its first guest once ARRIVAL_INTERVAL ticks have passed, which leaves every existing test that steps a park only a few cycles unaffected.

Rejected: admitting in the cycle stepping tick 0 and every ARRIVAL_INTERVAL after. It changes the key counter in every one-cycle test of a park with an entrance, for no difference in play.

Sources: src/sim/SPEC.md, the cycle; plans/believable-guests/hungry-guests/MILESTONE.md, tuning values.

## How do guests reach the screen?

The renderer gains a third mesh, the guests', drawn opaque with the park mesh's pipeline. The app rebuilds it from the guests' inspection records whenever the world's tick has changed since it last did, or the world was replaced, which at most 30 times a second uploads a few hundred small boxes. A guest is an upright box 0.6 m square and 1.8 m high at its ground position, its color moving from a sated blue to a hungry pink with its hunger, channel by channel.

Rejected: rebuilding the park mesh with the guests in it. It would rebuild every path and box every tick. Rejected: instanced drawing. It needs a new pipeline and shader inputs, and a few hundred boxes do not need it yet.

Sources: src/render/renderer.cpp, replaceMesh; src/app/SPEC.md, the park mesh's rebuild rule.
