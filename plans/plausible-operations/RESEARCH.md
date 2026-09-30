# Research: plausible-operations

## How do park games keep shops supplied?

In Parkitect, goods arrive at the main gate. They travel through delivery tubes to depots, and haulers, who are staff, carry them from depots to the shops assigned to each depot. Players zone haulers so they stay close to their shops. A single hauler in a small zone can keep four or more shops stocked, and stock-outs follow from too few haulers, zones that are too large, or tired staff. The system is legible because its failure is visible and local: an empty shop, and a hauler far away.

For this project, decision 0018 turns the haulers into an aggregate flow: supplies move from depot to shop over the backstage network, taking a delay that grows with route length. Staff come later as a capacity pool. What carries over from Parkitect is the structure (a source, a depot, a network, a shop) and the rule that a stock-out must be traceable to its cause. In the slice, that cause is a missing route or a long one.

Rejected:

- Individual hauler agents in the foundation: decision 0018 starts with aggregate flows, and principle 9 has the player direct through standards, not individual workers.

Sources:

- https://parkitect.fandom.com/wiki/Depot: depots, delivery tubes, and haulers restocking assigned shops.
- https://steamsolo.com/guide/zoning-for-park-staff-restocking-shops-parkitect/: hauler zoning, and how many shops one hauler covers.

## How should a shop decide when to reorder?

The (s, S) policy, also called min-max, is the standard continuous-review rule. When the inventory position falls to or below the reorder point s, an order brings it back up to the level S. The inventory position is stock on hand plus stock already ordered. Counting what is already ordered is what stops a policy from ordering the same shortfall twice while a shipment is in transit. The reorder point is normally set to cover expected use during the lead time, plus a safety margin, and S minus s sets the batch size.

For this project, the lead time is the supply route's travel delay, which the shop can sample from route distance. So a sensible s grows with the route: a distant shop has to reorder earlier. In the foundation, s and S are fixed tuning values. Later they can become a standard the player sets (principle 9), or be derived from the route and demand. Because the policy counts orders already placed, it fits the order flow: units in transit to the depot, and supplies in transit back, both count toward the position.

Rejected:

- Periodic review, which checks at fixed intervals: it adds a timing parameter, and on its own gives nothing over continuous review in a simulation that ticks anyway.
- Ordering from stock on hand only: it double-orders while shipments are in transit.

Sources:

- https://en.wikipedia.org/wiki/Reorder_point: the reorder point as use over the lead time plus safety stock.
- https://smartcorp.com/inventory-control/inventory-control-policies-software/: min-max (s, S) and related policies.

## How should a shop estimate a guest's expected wait?

Little's law says that the average number in a system equals the arrival rate times the average time spent in it, whatever the arrival or service pattern. Its practical form for a single queue is simple: the wait is roughly the number ahead divided by the service rate.

For this project, a shop's expected wait is the time to serve the guests already queued at its fixed service rate. The wait is for the next guest to arrive, standing behind the whole queue. It is the later of that guest's turn at the service rate and the time a supply unit for that guest becomes available. Units come from stock, then shipments in transit by arrival tick, which the shop knows from its own orders, then an order it would place now. This is an estimate the shop publishes in its food offer, not a promise. Guests act on it, and legible-simulation can show it. An estimate that can be computed exactly from state is also testable.

Rejected:

- Simulating the queue ahead of time to predict the wait: it costs more, and the slice rules out previews that simulate ahead.

Sources:

- https://en.wikipedia.org/wiki/Little's_law: L = λW and why it holds generally.
- https://eng.libretexts.org/Bookshelves/Civil_Engineering/Fundamentals_of_Transportation/05:_Traffic/5.01:_Queueing: practical queue waiting estimates.

## How does SimCity 4 simulate a city without agents, and what carries over?

SimCity 4 treats a building, not a person, as its unit of travel. A residential lot is a number of residents of a wealth class at a node of the transportation network, and the traffic simulator assigns them as a block: it searches for the nearest job of a suitable kind that can be reached within a maximum trip time, about 150 game minutes, and routes the trip there. Industry generates freight trips the same way. A trip's time is the sum of per-tile travel times along its route, each tile's time set by its mode's speed and raised by multipliers for congestion and busy intersections. Congestion comes from comparing the trips the simulator has assigned to a segment against that segment's capacity, and a congestion-versus-speed curve turns the ratio into slower travel, which pushes later trips onto other routes. The search is A* with a Manhattan heuristic, whose weight trades route quality against speed. The base game minimizes distance; the community's Network Addon Mod retuned it to minimize time. Modes compete on time: buses beat cars but still count as road traffic, while subways take trips off the roads. Trips are recalculated on a cycle of a few game months, except that removing a network piece reroutes immediately.

Every visible outcome is a number on a lot. A residential lot with no job within the maximum trip time shows a "no job" zot over it and its residents do not work, which lowers desirability and can lead to abandonment. Commute time is one desirability factor among pollution, crime, garbage, health and education coverage, traffic noise, slope, and land value, and the three wealth classes weigh the same factors differently, richer residents tolerating long commutes least. Services are coverage rather than movement: a fire station lowers flammability over a radius that its funding scales, and police, health, and education stations likewise cover areas. Power and water are connectivity plus capacity, and garbage without disposal accumulates and lowers desirability.

The moving cars and pedestrians are automata: presentation only. They spawn in rough proportion to a segment's traffic volume and have only a loose correlation to the simulation's numbers; changing them cannot change capacity, speed, or commute times. Players noticed that a followed car goes nowhere, but it did not undermine the game, because the numbers are what the player plays against. Rush Hour's route query answers the tracing question from the aggregate model rather than the cars: clicking a building shows the routes and modes of its trips, and clicking a segment shows where the trips crossing it come from and go to, for the morning and the evening commute. It became one of the expansion's most valued tools.

Its known weaknesses come from the model's choices, not from lacking agents. Only commutes and freight are simulated, with no shopping or leisure trips, so commercial areas see only their workers, and transit is infrastructure with no service frequency, so a bus line placed anywhere is served instantly. Greedy nearest-job assignment produces odd patterns, including commutes that loop between neighboring cities without reaching a job. The approach scaled: cities of hundreds of thousands ran on 2003 hardware. SimCity (2013) replaced it with GlassBox agents that had no memory, going each day to the nearest open job and back to the nearest free home and taking the shortest route into a jam, and it capped city size because the agents did not scale. Cities: Skylines made agents rational but pays for it in simulation cost at scale.

For this project, SimCity 4 is evidence that a beloved simulation can run on aggregate flows assigned over a network, with visible figures derived from the numbers, which is decision 0018's bet for staff and supplies. Its structure maps directly: lots as sources anchored to the network, trips assigned by route cost, capacity raising cost as flows load a segment. It suggests three things for staff. A staff pool's coverage of guest paths is a service like SimCity 4's, but measured along the composed staff network rather than a radius (principle 4). The route query shows how an aggregate model meets principle 8: tracing a place's service back to the source and route it was drawn over, without any individual to follow. And cosmetic figures hold up as long as players play the numbers; they strain when a player follows one figure and expects it to be someone. It also marks where guests differ. SimCity 4's weakest area is exactly the leisure trips a park consists of, which is why guests here are agents choosing by utility (decision 0019), and aggregate guests remain a scale fallback, not the model. Congestion, if the crowding candidate is taken up, needs assignment that SimCity 4 reruns on a cycle, since route distance today resolves only when intent changes.

Rejected:

- Memoryless agents for staff, as in SimCity (2013): they cost more than aggregates and read as less believable, since nothing about a worker persists to explain.
- Service coverage by straight-line radius, as SimCity 4's stations do: principle 4 measures distance along routes, so a staff pool's reach is route distance over its network.
- Greedy nearest-destination assignment: it herds and loops, where decision 0019's scored softmax spreads choices and keeps their reasons.

Sources:

- https://www.sc4nam.com/docs/feature-guides/automata-plugins/: automata as a loose visual approximation that cannot affect capacity, speed, or commute.
- https://www.sc4nam.com/docs/feature-guides/traffic-simulator/: shortest versus fastest commute, the congestion-versus-speed curve, capacities, and desirability feedback.
- https://wiki.sc4devotion.com/index.php?title=Tutorial%3AUnderstanding_the_Traffic_Simulator: the simulator's key properties, Manhattan A*, and the heuristic's accuracy trade-off.
- https://gamefaqs.gamespot.com/pc/561176-simcity-4/faqs/23133: per-tile trip times with congestion and intersection multipliers, the maximum trip time, recalculation cycle, and mode effects.
- https://news.ycombinator.com/item?id=38158537: SimCity 4 as the last statistical city simulator, residents as counts at a network node.
- https://humantransit.org/2010/12/is-sim-city-4-still-making-us-stupid.html: commute-only trips, nearest-job loops, and transit without service frequency.
- https://simcity.fandom.com/wiki/Zots: zots, including the no-job zot, as per-lot failure markers.
- https://en.wikipedia.org/wiki/SimCity_4:_Rush_Hour: the route query tool.
- https://kotaku.com/your-complete-guide-to-the-simcity-disaster-5991077: GlassBox's memoryless agents and the city size cap.
