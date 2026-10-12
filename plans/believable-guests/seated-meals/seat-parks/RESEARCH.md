# Research: seat-parks

## Why guests who aren't tired still pick a seat, and which values change that

A guest scores every option and picks by softmax (decision 0019). A seat's relief term is RELIEF_WEIGHT times tiredness's curve times the seat's rest relief. At tiredness 0 the curve gives 0, so the seat scores only its distance term, a little below 0. Carrying on scores CARRY_ON_SCORE, 0.5. With CHOICE_TEMPERATURE at 0.25, a seat 10 m away weighs exp((-0.1 - 0.5) / 0.25), about 0.09 of carrying on. best rest seats gives up to SEAT_CHOICES pieces, 4, so the seats together weigh about a third of carrying on. carried-meals measured that a guest at tiredness 0 picks a seat about 14% of the time in full.park. Facility offers at need 0 behave the same way, but a guest sees few of them.

This is logit's independence of irrelevant alternatives, which the capability's research notes already name: options that are near copies of each other gain share together. Four seats that each score about 0 draw more guests than one would.

Three values change it, and each changes more than seats:

- Lowering CHOICE_TEMPERATURE makes every pick surer. At 0.15, a zero-need seat 10 m away weighs about 0.02 of carrying on, and four of them about 0.07. Where an option scores the same as carrying on, it is still picked as often as carrying on, so the needs at which guests start to want things don't move. Guests pick the best shop more surely, so they spread less across shops.
- Raising CARRY_ON_SCORE makes every option need more relief to win. At 0.8, a zero-need seat weighs about 0.03 of carrying on. A guest then waits until it is hungrier, thirstier, or more tired before it goes to a facility or a seat, which moves when guests leave and how long they stay.
- Lowering DISTANCE_WEIGHT's magnitude or the seat reliefs changes the pull of every seat, not just the idle ones.

Other games handle this differently. The Sims scores every advertisement and picks at random among the few that score best, so a near-zero option is never picked while a better one is near. Utility systems often prune the options to those within some share of the best before a weighted random pick. Both differ from decision 0019's softmax over every option, so adopting either needs a decision record. Within the current decision, the temperature and the carry-on score are the values to tune.

Rejected: a tiredness threshold below which seats aren't offered — the need curve already shapes how much a need matters (decision 0020), and a threshold is a second, hidden curve. Pruning to the best few options — it changes decision 0019 and belongs in its own decision.

Sources: https://en.wikipedia.org/wiki/Utility_system — utility scores feeding a pick of the best or a weighted random pick; https://amara.org/subtitles/cVoJS4OdmVql/en/1/download/The%20Genius%20AI%20Behind%20The%20Sims.en.txt — a Sim picks at random among its top-scoring interactions; https://gamedev.net/forums/topic/720126-i-need-help-with-my-creature-simulation-game — pruning to the top share of scores before a random pick.
