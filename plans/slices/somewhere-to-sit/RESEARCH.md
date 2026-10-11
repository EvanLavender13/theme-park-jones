# Research: Somewhere to Sit

## How other park games handle benches, tables, and bins

Planet Coaster 2 calls benches, picnic benches, bins, and lamps path extras. They stand on the path itself. The player places them one at a time from the scenery menu, or the path tool lays them down as the path is drawn, with a spacing and side the player toggles. Frontier's guide suggests about one bench and one bin for every 12 m of path, so a park with a few kilometres of path holds hundreds of each. Benches restore energy. Guests with food or drink sit at picnic benches to eat. Guests drop litter in a bin they pass, or on the ground if they carry it too long. Bins fill and janitors empty them. Update 4 added restaurants: counters, tables, and booths, where guests carry a tray to a table. PC2 dropped PC1's and Planet Zoo's option of standing furniture on the path's outer edge, and players asked for it back.

Planet Coaster moves guests with flow fields computed outward from each goal. For small things such as bins, the developers create a temporary goal when a guest needs one instead of giving every object its own field. That is how they keep thousands of small items affordable.

In both Planet Coaster games, players have complained for years that guests ignore benches, bins, and picnic tables, even ones placed every few metres. Guests seem to notice furniture only when right next to it.

RollerCoaster Tycoon stands benches and bins on path tiles as path additions. Guests sit when tired or holding food and a bench happens to be on their tile. They never seek one out. Parkitect snaps benches and bins into the path. Its janitors empty bins and carry the bags to a delivery area. Guests can vomit into bins, and they are annoyed when staff sit on the benches.

What this means: every game stands seating on the path, and none makes guests seek it at a reasonable cost. RCT has guests notice only what is underfoot. Planet Coaster has guests notice only what is close, and does so unreliably. Neither has a guest walk to a bench on purpose. The day-at-the-park slice made each bench a box with its own offer, so guests do seek benches. The cost is one route search per bench, so that design doesn't scale to the hundreds of benches a real park holds.

Rejected: benches noticed only underfoot, as in RCT. Guests never seek them, so benches feel pointless. Furniture noticed within a short radius, as in Planet Coaster. Players report it as unreliable, and it still can't bring a guest from farther away. Each bench as its own routing source, as in day-at-the-park. Memory and resolve time grow with the square of the bench count.

Sources: https://www.planetcoaster.com/en-US/player-guides/facilities — path extras, auto placement, spacing advice, bin behaviour; https://forums.frontier.co.uk/threads/please-put-back-the-ability-to-place-bins-and-benches-on-the-outer-edge-of-the-path-like-pc1-and-zoo.632362/ — PC2 removed edge placement; https://steamcommunity.com/app/2688950/discussions/0/604152569333352905/ — PC2 guests ignore furniture; https://forums.frontier.co.uk/threads/guests-really-dont-really-use-benches-or-tables-much.530680/ — short awareness range; https://www.planetcoaster.com/news/2025-04-17/update-4-highlights — restaurants with tables; https://www.gamedeveloper.com/audio/game-design-deep-dive-creating-believable-crowds-in-i-planet-coaster-i- — per-goal flow fields, temporary goals for bins; https://rct.fandom.com/wiki/Guests_and_Staff — when RCT guests sit; https://www.coasterforce.com/forums/threads/specialized-studios-parkitect-final-release.41239 — Parkitect furniture and bin emptying.

## What kinds of rest and comfort things real parks have

Real parks' amenities fall into three groups by how many there are and whether guests travel to them.

- Many, used in passing: benches along paths, low walls and planter edges people sit on, bins, water fountains, misting fans, and lamps. Disney is said to keep a bin within about 30 steps of anywhere, because that is about how far people carry litter before dropping it. A litter study found people dropped litter a mean 29 feet from the nearest bin.
- Clusters, which guests travel to as one place: eating areas with tables near food, shaded pavilions and arbors, quiet gardens away from noise, and seating for shows and parades.
- Few, which guests travel to: toilets, first aid, baby care, lockers, and guest services.

Design guides put benches at regular intervals along paths, beside toilets, fountains, shade, and bins, at viewpoints, and outside rides. They group seating so groups of different sizes can sit together.

What this means: the first group is path furniture. It needs no door and no routing source of its own. Seats can serve guests together through one field per need, which is a systematic form of Planet Coaster's temporary goal. Clusters can later be single destinations with many seats. The few stay boxes, as shops and toilet blocks are.

Sources: https://www.apartmenttherapy.com/disney-trash-can-rule-268682 — the 30-step bin rule; https://99percentinvisible.org/episode/service-request-03-trash-cans/transcript/ — litter distance study; https://www.usa-shade.com/blog/how-to-create-relaxation-zones-in-theme-parks/ — kinds of rest area and where benches go; https://www.usa-shade.com/blog/designing-park-seating-area/ — grouped seating.

## What a bench costs in the current design

A bench is a box with a guest front door. Its door becomes a connector carrier, which adds a node where the connector meets the path and a node at the door (src/sim/routes/networks.cpp). Every node anchor is a route distance source. publishRouteDistance runs one search per source over the whole guest network and stores an entry at every reachable node (src/sim/routes/route_distance.cpp). A guest's choice samples those entries and scores every reachable facility (src/sim/guests/guests.cpp). Benches add both sources and nodes, so memory and resolve time grow with the square of the bench count, and each choice grows with it too. At full.park's 45 benches the cost is fine. At a thousand it is not.

What this means: seats must reach guests through a field whose cost depends on the network's size, not on the seat count. One search per field, seeded from every seat at once, does that.
