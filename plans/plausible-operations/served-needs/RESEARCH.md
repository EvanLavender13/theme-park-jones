# Research: Served Needs

## Should the need offer be one field with a need on each entry, or one field per need?

The slice describes one need-offer field that replaces the food offer, with the need named on each entry. Two facts about the code argue against replacing it. A stepped offer is saved state: every park that holds a shop saves a food-offer-stepped section, and the loader refuses a section it doesn't know. Renaming the field would make every such park unloadable. That includes the slice and good-paths parks in tests/parks, the stress park, and the user's three parks in parks/. The flows have the same problem: supplies, supply-orders, guest-visits, and meals are saved ledger sections.

Keeping food-offer as hunger's field, and adding drink-offer, rest-offer, and toilet-offer beside it, keeps every saved park loading. It also leaves guests and the food overlay reading exactly what they read today, so served-needs changes nothing in believable-guests. A per-need overlay samples one field instead of filtering one field by need. Each field can have only one entry per source at a place without a need tag, which is what a shop stocking both goods needs: one entry in each field.

The same holds for flows. Food's kinds keep their saved names: supply-orders and supplies for the good, guest-visits for a visit for food, and meals for its serving. Drink adds drink-orders, drink-supplies, drink-visits, and drinks. Rest and the toilet add rest-visits, rests, toilet-visits, and toilet-uses. Each kind is one need, so a visit still names the need the guest came for, and a shop stocking both serves it without reading the guest.

Rejected: one need-offer field replacing food-offer — breaks loading every saved park with a shop. One field with a need tag added beside food-offer for the other needs — two shapes for the same thing. Renaming food's kinds to hunger names — the same loading break, for a name.

Sources: src/sim/save.cpp — "unknown section" refuses a load; tests/parks/warm.park — food-offer-stepped and the four ledger sections; src/sim/medium/field.h — a field slot holds a source's entries.

## How does a shop stocking both goods serve its queue?

A shop is one counter. It serves its queue in arrival order, one guest per SERVICE_INTERVAL, and each guest takes one supply of the good its visit names. Separate counters for each good would double a both-shop's service rate, which makes both always better than two shops. One counter keeps the choice a trade-off. The expected wait stays exact, because the wait already steps through the queue guest by guest. It now takes each guest's unit from that guest's good.

A guest at the front waiting for a good that hasn't arrived holds up those behind it, as at a real counter. A good the shop no longer orders runs out instead of being awaited: the shop returns the visits for it that its remaining stock and shipments cannot cover, as a starved shop does for every good today.

Rejected: a counter for each good — doubles throughput for free. Serving out of order to skip a guest waiting for stock — breaks arrival order, which the exact wait relies on.

Sources: src/sim/operations/operations.cpp — nextWait, returnUncovered, serveFront.

## How do seats and stalls wait and release?

A bench or toilet block is a counter with several places. It seats queued guests in arrival order whenever a place is free, and keeps each one for a fixed time. The next arrival's wait is exact: give each guest ahead of it the earliest free place in turn, and the arrival takes the earliest place left. With one place this is the shop's rule without supplies. The release time of each place is the only state beyond the queue.

Real benches seat about one person per 0.6 m, so a 2 m bench seats three. A toilet block of about a shop's size holds a few stalls. RollerCoaster Tycoon scales bench and toilet time with how great the need is. Fixed times are simpler and keep the wait exact. Scaling can come later, if a-day-out's play shows fixed times look wrong.

Rejected: times that scale with the guest's need — the facility would read the guest, or the visit would carry a need level, and the wait would depend on guests not yet arrived. Turning guests away when places are full — a gate (principle 5); they wait at the door instead.

Sources: plans/slices/day-at-the-park/RESEARCH.md — RCT benches and toilets.
