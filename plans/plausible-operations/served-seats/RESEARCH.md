# Research: Served Seats

## How long do guests sit, and does it depend on why they sit?

People who sit on public seating stay a long time: one survey of park-goers found an average stay of 22 to 28 minutes. Restaurants aim to turn a table in about 45 minutes, and a fast meal takes much less. RollerCoaster Tycoon has guests sit whenever they hold food or drink and a bench is beside them, and it doesn't vary the time by why they sat. served-needs set a bench box's SITTING_TIME to 300 ticks, 10 game minutes, with a deepening candidate for times that scale with the guest's need.

What this means: one sitting time serves both resting and eating. 10 game minutes is within what real sitting takes once game time is compressed. A piece doesn't need to know why a guest sat, so one flow of seat visits is enough, as the slice's medium map has it. The guest knows why it sat and what the seat gave it.

Rejected: a separate visit kind for eating, with its own time. It doubles the flows and the piece's rules for a difference play hasn't asked for. A sitting time that ends when the guest is rested. The piece would have to read the guest's need, which is the guest's internal state (principle 6).

Sources: https://www.prnewswire.co.uk/news-releases/park-bench-etiquette---would-you-stay-or-go-198667041.html — time on public seating, reluctance to share; https://1851franchise.com/nations-restaurant-news-data-the-optimal-table-turnover-time-is-45-minutes-2705367 — table turnover; https://outof.games/realms/rollercoastertycoon/guides/541-guide-to-food-and-drink-stalls-in-rollercoaster-tycoon-1-and-2/ — RCT guests sit to eat when a bench is beside them.

## What should happen when a guest reaches a full piece?

People rarely wait for a seat on a bench. They walk on and look for another, and many won't share a bench with a stranger while another is free. A bench box in served-needs queues guests at its door, which fits a building with a door but not a bench beside a path. The slice's medium map has a full piece return the guest unseated, and the guest remembers the piece and moves on to the next best.

What this means: a piece has no queue. It seats a guest when a seat is free and returns the visit at once otherwise. The ledger's smallest delay is one tick, so an unseated visit comes back the tick after it arrived. With no queue, the piece's only state is when each taken seat frees. Nothing waits on it, so its offer needs no wait, and the offer can be resolved from intent alone, which best-seats needs.

Rejected: a queue at each piece, as a bench box has. Guests would stand beside a bench waiting, which people don't do, and the offer would need a stepped wait that best-seats can't read in resolution. Fewer seats for strangers, so a table of four takes fewer unrelated guests. Play hasn't shown it is needed. It is a deepening candidate.

Sources: https://edblogs.columbia.edu/plana6102-001-2019-1/2019/05/10/when-walking-into-a-park-where-do-you-sit-2 — people avoid occupied benches while others are free; https://www.prnewswire.co.uk/news-releases/park-bench-etiquette---would-you-stay-or-go-198667041.html — half sit only on an empty bench.

## How does a piece's service follow decision 0031?

A full park holds several hundred pieces, and most hold no guest at a time. A tick must not visit every piece. A piece acts only when a seat visit arrives. A seat frees when its time passes, which a piece can read from the tick without stepping. So a tick steps only the pieces holding seat visits, found from the seat-visits ledger's stocks, and its work grows with the guests visiting seats. Seats taken are kept on the piece's own entity as state, so a save holds them, and they go when a resolution no longer lays the piece. The ledger then returns any visit on its way to the piece to its guest. Rests already sent still arrive, as a bench box's do.
