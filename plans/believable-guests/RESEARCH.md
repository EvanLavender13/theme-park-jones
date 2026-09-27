# Research: believable-guests

## How do games drive agents by needs and advertised offers?

The Sims drives each Sim by motives, such as hunger, energy, and fun. Each motive decays at its own rate, and faster while a related action runs. Objects advertise what they offer: a fridge advertises hunger relief, and a bed advertises energy. The Sim scores the advertisements against its current motives and picks from them. Will Wright took the idea from SimAnt's pheromones, and Robert Zubek describes the general pattern as needs-based AI. The intelligence lives in the offers, and the agent stays simple. Maxis found that Sims which were too smart made the game less interesting, and left room for them to fail.

That is this project's structure almost exactly. A shop's food offer is an advertisement published in the medium, hunger is a private motive that rises over time, and a guest scores the offers it can reach against its own hunger. Nothing is wired from shop to guest (principle 3). The difference is distance: a Sim's house is small, but a guest's offers are discounted by route distance and expected wait (decision 0019).

Rejected:

- Guests reading shop state directly, such as its queue or stock: that breaks principle 6. The offer already carries what a guest needs.

Sources:

- https://gmtk.substack.com/p/the-genius-ai-behind-the-sims: motives, decay, and objects as advertisements.
- https://robert.zubek.net/publications/Needs-based-AI-draft.pdf: needs-based AI as a general pattern.
- https://www.thegamer.com/the-sims-ai-bad-on-purpose-first-game-will-wright/: why Sims were deliberately left imperfect.

## How should scores become choices?

Utility AI, as Dave Mark and Kevin Dill presented it at GDC, maps each input through a response curve to a normalized score. The Infinite Axis Utility System, by Mark and Mike Lewis, multiplies a decision's consideration scores together, and adds a compensation factor, because a plain product sinks toward zero as considerations are added. Decision 0019 instead sums weighted factor terms. That keeps each factor's contribution separable, so a guest can explain a choice by its terms (principle 8), and decision 0020's curves shape each input first.

Choosing by softmax over the scores is the multinomial logit model of McFadden's random utility theory, where a temperature sets how noisy the choice is. At low temperature guests nearly always take the best option, and at high temperature they spread out. Logit has a known property, the independence of irrelevant alternatives: adding an option takes share from all the others in proportion. Two identical shops side by side therefore draw more hungry guests away from carrying on than one shop does, the "red bus, blue bus" effect. For this project that is tolerable, and arguably plausible, since more choice does attract. But it means the "carry on" option's utility should be tuned with several shops present, not one.

Rejected:

- Taking the best option every time: identical guests herd to one shop, which decision 0019 rules out.
- Multiplied considerations: the terms stop being separable, so explanations weaken.

Sources:

- https://en.wikipedia.org/wiki/Utility_system: utility systems in games, response curves, and the history from Mark and Dill.
- https://www.gameai.com/iaus.php: the Infinite Axis Utility System.
- https://cran.r-project.org/web/packages/mlogit/vignettes/c3.rum.html: the random utility model, the logit, and the independence of irrelevant alternatives.

## What do park games' guests teach about wandering and failure?

RollerCoaster Tycoon's guests carry hunger, thirst, tiredness, bladder, and nausea, which drive their mood. A guest with a map heads toward the ride it wants most. Known quirks include guests fixating on food long after eating, guests getting lost or stuck in large parks, and guests unable to turn around on single-lane paths.

For this project, two lessons carry over. Getting lost happens when a guest routes by local heuristics rather than true route distance. Descending navigable-networks' distance field removes that failure by construction. Fixation happens when a need's state and a guest's choice drift apart. Re-scoring at every junction from current hunger, with only a small bonus for staying committed, keeps a choice tied to the need that caused it.

Wandering still needs a rule, because guests without a goal must spread out plausibly. A keyed random choice among a junction's other edges, avoiding the edge just walked, is the minimal version. A curiosity term, such as a preference for less-visited edges, is a natural deepening.

Rejected:

- Heuristic direction-seeking without a distance field: the RollerCoaster Tycoon failure mode, where guests get lost in large parks.

Sources:

- https://rct.fandom.com/wiki/Guests_and_Staff: guest needs, and heading toward a desired ride.
- https://www.gog.com/forum/rollercoaster_tycoon_series/rollercoaster_tycoon_3_guest_ai: player reports of fixation and of getting lost.
