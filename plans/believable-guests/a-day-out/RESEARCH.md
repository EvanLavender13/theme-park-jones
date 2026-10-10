# Research: a-day-out

The slice's RESEARCH.md covers how park games model needs, how guests decide to leave, how the genre handles opening hours, and when real guests arrive. This file covers the questions of how to build those findings here.

## How should an arrival curve become arriving guests?

An arrival curve gives a rate, in guests per game hour, that changes over the opening hours. In probability terms this is a non-homogeneous Poisson process. Its usual discrete-time form is a Bernoulli process: in each short step, one arrival happens with a probability equal to the rate times the step's length. A tick is 2 game seconds. At a peak of 15 guests per game hour, the chance in a tick is about 0.008, far below 1, so the Bernoulli form matches the Poisson process closely. One keyed draw for each entrance and tick decides it. That needs no saved state, because the draw is keyed on the world's seed, the tick, and the entrance. It also can't depend on the order systems run in (principle 10).

The other common way is to add the rate into a running total each tick and admit a guest each time the total passes a whole number. That gives the exact expected count, but the arrivals come at perfectly regular gaps. It also needs the total in saved state, which every save, copy, and hash would then carry.

The curve itself is a piecewise-linear table (decision 0020). Its X is the fraction of the opening hours that has passed, so it stretches to whatever hours the player sets. Its Y is the rate. The table reaches 0 a few hours before closing, so no guest arrives late in the day. The rate is 0 whenever the park is closed. Because the curve is a function of the hours alone, the hours panel can preview it from a candidate world with the tentative hours applied (decision 0025).

Rejected: an accumulator — regular gaps between arrivals, and saved state for no gain. A curve over clock time instead of the fraction of the open hours — it wouldn't follow the player's hours. Arrivals while closed at a low rate — the slice says the gate admits no one while closed.

Sources: https://eng.libretexts.org/Bookshelves/Electrical_Engineering/Signal_Processing_and_Modeling/Discrete_Stochastic_Processes_(Gallager)/02:_Poisson_Processes/2.04:_Non-homogeneous_Poisson_Processes — the non-homogeneous Poisson process and its Bernoulli approximation; https://www.colorado.edu/amath/sites/default/files/attached-files/lesson10.pdf — generating non-stationary arrivals in simulation.

## How should going home be scored so that its largest term names the reason?

Going home is the choice's outside option in logit terms. Its score is a sum of terms, as every option's score is (decision 0019). Each reason the slice names gets one term: tiredness, hunger, thirst, the toilet, and closing time. A constant base term keeps going home unlikely while every need is met. The base names no reason, so it is left out when the largest term is taken as the reason.

Each need's term is a weight times the need's curve value, the same curve that shapes the relief an offer gives. So a need that matters to the guest pulls it toward a facility and toward home together. If a facility answering it is in reach, its offer outscores that need's pull home, as long as each need's home weight is below the offer's relief weight times its relief. When several needs are high at once, their home terms add up, and going home can win even with facilities in reach, which is the pressure the slice wants from needs left unmet. If nothing in reach answers it, its home term keeps growing while the need stays unmet, and going home wins. Bladder only rises, with meals and drinks, so a park with no toilet loses its guests to the toilet.

The closing term is 0 until a lead time before closing. It then rises linearly to its full value at closing, and stays at that value while the park is closed. Its full value outscores every other option, as the fixed stay's head-home score does today. A guest also chooses as soon as the park closes, so it doesn't walk on to a far node first.

Utility AI systems such as Dave Mark's Infinite Axis Utility System multiply their considerations, so that any one zero vetoes an action. A product can't be split into parts that add up to the score. Then neither the choice record nor the largest part can say why the guest left.

Rejected: multiplied considerations — no attribution (decision 0019, principle 8). A head-home score that only switches on past a need threshold — a gate (principle 5). A separate discomfort level per need that builds while the need is unmet — the need's own level already stays high while unmet, and a second state per need adds saved state without a new cause. A reason picked by priority instead of the largest term — it hides what decided the choice. A guest that stops choosing once it heads home — it would walk past a toilet it needs on the way out; it keeps choosing as a guest heading to a shop does, with the commitment bonus, and its latest pick's reason is the one tallied.

Sources: https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter09_An_Introduction_to_Utility_Theory.pdf — response curves and scoring; https://tonogameconsultants.com/infinite-axis-utility-systems/ — multiplied considerations in the Infinite Axis Utility System; https://zenn.dev/sanmal/articles/9ed9989c11b7eb?locale=en — why multiplication vetoes and needs a compensation factor.

## How should tiredness rise?

The slice says tiredness rises as the guest walks. Planet Coaster 2's energy falls faster with long walks and queues. RCT's energy falls with walking and is propped up by digestion. Rising with meters walked ties tiredness to the layout: a spread-out park with no benches tires its guests and sends them home sooner. That is the spatial coupling the slice's research credits with making needs into decisions. Guests standing in a queue or sitting don't tire.

Rejected: tiredness rising with time — it ignores the layout. Tiredness rising while queuing — a queue's wait already counts against an offer, so this would count it twice. It is kept as a deepening candidate if long queues look too cheap in play.

Sources: the slice's RESEARCH.md, first question.

## How should guests show four needs on screen?

Guests are drawn as boxes shaded from a sated color to a hungry color by their hunger. With four needs there are three ways to keep this:

- Shade by the most pressing need, on the same single ramp. It reads as how badly off the guest is, and the ramp stays colorblind-safe.
- Give each need a hue, and tint each guest by the hue of its most pressing need. This shows which need, but four hues on boxes 0.6 m wide are hard to tell apart. Two of them would clash with the overlays' viridis ramp.
- Keep shading by hunger alone. That hides three of the four needs.

The Sims' plumbob shows mood with one green-to-red ramp, and leaves each motive to the needs panel. RCT draws no need on its guests and uses thought bubbles. Which need matters is in the guest Inspector and, after explained-needs, in each need's overlay.

This suggests shading by the most pressing need on the existing ramp. It is a presentation choice the user can change in play.

Rejected: a hue per need — hard to read at guest size. Hunger alone — it hides the needs this milestone adds.

Sources: the slice's RESEARCH.md, first question, for RCT's guests and The Sims' motives.
