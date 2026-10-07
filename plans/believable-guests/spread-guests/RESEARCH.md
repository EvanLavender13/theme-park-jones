# Research: spread-guests

## What makes guests on a wide path look natural rather than formulaic?

Real walkers differ in speed. Weidmann's review gives a mean free walking speed of 1.34 m/s, with most people between about 0.97 and 1.65 m/s, and later field data fit a normal distribution with a standard deviation of 0.26 to 0.37 m/s. When every guest walks at one speed, guests that arrive together keep their spacing for their whole stay and nobody overtakes, which reads as marching. Varying speed per guest is the cheapest fix with the largest effect.

In two-way flow, walkers sort into lanes by direction, and in right-hand-traffic countries the lanes form on the right. A spread that ignores direction puts both directions everywhere and looks random. A spread biased to one side by the way each guest walks shows two streams on a wide street.

Walkers keep clear of walls and edges, so a path's outer strip is used less than its middle. A per-guest line taken from a distribution that thins out toward the edges, such as the mean of two uniform draws, looks more natural than a flat one.

Walkers cut the inside of corners and cross a junction diagonally toward the way they leave it. A line kept parallel to the center line makes guests on the outside of a bend take the long way round and funnels everyone through the junction's middle, which looks like vehicles on lanes.

A line that never changes looks ruled. A slow wander made from a few slow waves per guest, with unrelated frequencies and phases from its key, avoids both the ruled look and the visible in-step swaying of one wave.

Rejected: one fixed sideways offset per guest with a single sine drift — lanes and swaying in step are both visible on a long street. Offsets spread evenly across the width — no streams by direction, and guests hug the edges as often as the middle. Speed varied only in drawing — the drawn guest would leave its simulated place behind.

Sources: https://www.researchgate.net/publication/303264378_Free_speed_distributions_for_pedestrian_traffic — free speed distributions; https://www.researchgate.net/figure/The-walking-speed-of-pedestrians-is-distributed-normally-with-an-estimated-mean-of-134_fig4_286071735 — mean 1.34 m/s, normal; https://www.sciencedirect.com/science/article/abs/pii/S0378437119302596 — lane formation and overtaking in two-way flow; https://arxiv.org/pdf/1606.03434 — two-way lanes form on the right.

## How can a guest's drawn position stay continuous through junctions, width changes, and turnarounds?

A guest's simulated place is a carrier and a distance. Its drawn position is that place's ground point moved sideways. If the sideways move is worked out only from the guest's current state, it jumps wherever its inputs jump:

- At a node where the path bends without a rounded corner, the direction of travel turns at once, so a sideways offset swings by its length times the turn.
- Where the width changes at a node, an offset kept as a fraction of the half-width changes its length at once.
- When a guest turns back at a dead end, its side by direction flips across the path.

A blend over a short distance before and after each node would hide the first two, but it needs the edge the guest is about to take before the node and the edge it came from after it. A wandering guest picks its next edge only at the node and stores no previous edge. Storing that in the simulation would be state kept only for drawing.

The common game pattern separates the drawn position from the simulated one at the boundary between simulation and drawing. Logic uses the true position, and only what is drawn is smoothed. A follower does this here: the drawing keeps one drawn position per guest and moves it toward the guest's target position, its place's ground point plus its sideways line, by a fixed fraction per tick of simulated time. That covers every jump above with one mechanism. A turnaround becomes a short U-turn, and a follower naturally cuts the inside of corners. Its cost is a lag along the path of the speed times the time constant: about 0.65 m at 1.3 m/s with a 0.5 s constant. Stepping it by simulated ticks, not by frame time, keeps its shape the same at any frame rate, and a paused park holds still.

The follower must not hide real simulation jumps. When a carry-over moves a guest farther in one tick than any guest walks, the drawn position should jump with it, so the player still sees it and the slice's check that nobody jumps along a path stays meaningful.

The follower is drawing state. It isn't saved and the simulation never reads it (principles 1 and 10). A loaded park or a guest seen for the first time starts at its target, so a capture of a loaded park shows every guest on its line.

Rejected: a pure function of the guest's current state with blends at nodes — it can't know a wanderer's next or last edge. Storing the previous edge or the drawn offset in guest state — simulation state that only drawing reads. Smoothing by frame time — the shape of a turn would change with frame rate. Interpolating between ticks alone — it smooths time, not the jumps in space at nodes.

Sources: https://gamedev.net/forums/topic/651736-fixed-timestep-with-smooth-rendering/5120266/ — drawn position kept apart from the simulated one; https://github.com/lawnjelly/smoothing-addon — smoothing only at the boundary to the renderer, logic on true positions.
