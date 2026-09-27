# Slice: Boxes and Tubes

Status: planned

## Summary

The first playable loop, drawn in boxes and tubes: the player lays out paths, food shops, and a supply depot, guests get hungry and eat, and when the player changes the park they can see the consequence coming and understand it afterwards. It is the right first goal because it puts every foundational capability to work at once and exercises the principles that make this game different from its references: interaction only through fields and flows (3, 6), distance along routes (4), gradients instead of gates (5), and outcomes that explain themselves and can be previewed (8).

## End-to-end scenario

A new park is flat grass with a fixed entrance at its edge. The player draws a guest path from the entrance as a curve and places a food shop box beside it; the shop connects to the path by itself. Behind the shop the player places a supply depot and draws a backstage path between them, which the shop also connects to. Nothing about the order matters: a shop with no path or no supply route is a legitimate state that simply does poorly (principles 2 and 5).

Guests, drawn as simple shapes, arrive at the entrance at a steady rate and wander the paths. Their hunger rises. When it matters enough, a guest scores the food offers it can reach, weighing relief against route distance and expected wait (decision 0019), walks to the chosen shop, queues, is served a meal, and eats it. Some time later the guest leaves by the entrance. Supplies travel from the depot to the shop along the backstage path, each shipment taking time proportional to the route length, and the shop turns supplies into meals. A shop offers food while it has a live supply route; its expected wait includes any time until stock arrives.

The player turns on the food-availability overlay. The ground is shaded by how well fed a guest standing there could be, measured along the paths, and hovering a spot lists which shops contribute and by how much.

The player picks up a second shop and moves it along the paths. Before committing, the overlay shows the food availability the park would have with the shop there, and the ghost shop shows the context it would see: nearby hungry footfall and the length of its supply route. Where the ghost touches both the guest path and the backstage path, availability rises around it; where it has no supply route, availability does not change and its context says it has no supply route. The player commits it where it is supplied, and the park matches the preview.

Then the player hovers the backstage path to the first shop with the delete tool, and the preview shows the shop starving and the overlay dimming around it. The player deletes the path. Its offer at once tells guests no meals are available, and its box shows that it is starved. It serves the guests already queued from its remaining stock and sends the rest away unserved. Shipments already on their way still arrive. Guests stop choosing it; those near it walk to the second shop or stay hungry, and the overlay dims around it. Clicking the shop shows its missing supply route as its limiting factor. Clicking a hungry guest shows its hunger and its last choice with the factors that decided it.

## Acceptance criteria

Three park files are checked in: tests/parks/fed.park (entrance, guest path, one shop, depot, backstage path, no guests yet), tests/parks/warm.park (fed.park after a warmup, with guests in the park), and tests/parks/cut.park (warm.park with the backstage path removed).

1. Running fed.park for 3000 ticks serves meals, and every served guest's hunger, as its inspection record reports it, fell when it ate. (integration test)
2. Supplies and meals are conserved in every tick of fed.park, warm.park, and cut.park: supplies sent by the depot equal supplies in transit plus in shop stock plus converted to meals, and meals made equal meals in shop stock plus meals in transit to guests plus meals held by guests plus meals eaten. (integration test)
3. Running cut.park and warm.park each for 3000 ticks: in cut.park the shop's offer reports no meals available from the first tick, no guest chooses the shop afterwards, its queue empties with every queued guest either served or returned unserved, and mean guest hunger ends higher than in warm.park. (integration test)
4. Food availability at every sampled place can be reconstructed exactly from its attributed per-shop contributions. (integration test)
5. In warm.park, the preview of placing a second shop touching both paths equals the resolved park immediately after the placement is committed, and food availability rises by a nonzero amount at the places nearest the new shop. (integration test)
6. Loading a saved park and saving it again gives an identical file, and a world regenerated from a save equals the world that was saved, including cut.park's shipments in transit. (integration test)
7. Two runs of fed.park for 3000 ticks give identical state hashes, and the hash from the Windows build equals the hash from the Linux build (decision 0022). (integration test, plus the hash printed by both builds)
8. A capture of warm.park after 3000 ticks with the food overlay on shows the guest path and backstage path as tubes, the shop and depot as boxes, guests on the paths, and the food-availability overlay on the ground. (scripted capture: effortless-building, believable-guests, legible-simulation)
9. A capture of cut.park after 3000 ticks with the food overlay on shows the starved shop marked as starved and the overlay dimmed around it compared with the warm.park capture. (scripted capture: effortless-building, plausible-operations, legible-simulation)
10. In the running app, drawing paths, placing boxes, the preview ghost updating the overlay before commit, the delete tool previewing a deletion, hovering the overlay for attribution, and clicking a guest and a shop for their explanations all work as the scenario describes. (manual)

## Medium

Fields and flows between capabilities. Abbreviations: DS deterministic-simulation, SM shared-medium, NN navigable-networks, BG believable-guests, PO plausible-operations, LS legible-simulation, EB effortless-building.

- Networks: the guest and backstage graphs, with edge lengths, derived from the drawn paths. The network type, its carriers and places, and anchors that tie nodes to entities are defined by SM; the park's networks are produced by NN. Consumed by BG (guests move along them and arrive at the entrance's anchor), by PO (a shop's and depot's anchored nodes), and by every field, which is sampled at network places. A place is a carrier (a drawn path or a connector) and a distance along it, so it survives re-derivation. Flows do not travel edge by edge: a sender takes a packet's delay from route distance. A shared structure rather than a field or flow; it is the ground both kinds of medium live on.
- Route distance: entry field (sampled, not consumed) over the networks: for each source (shop, depot, entrance), the distance from a place to it along the paths and the next edge toward it (principle 4). Produced by NN. Consumed by BG (choice, and movement by following next edges), PO (a shop's supply route length and supply delay), LS (overlay discount, preview context).
- Food offer: field on guest network nodes, one entry per shop: the relief a meal gives, the expected wait including time until stock arrives, and whether the shop is supplied. Produced by PO. Consumed by BG (choice) and LS (overlay, attribution).
- Hungry footfall: field on guest network edges, how many hungry guests pass. Produced by BG. Consumed by LS (preview context). A shop's demand in this slice is the guests actually arriving in its queue, not this field.
- Guest visits: flow of guests into a shop's queue and back out, served or unserved. Produced by BG, consumed by PO; PO returns every guest to BG, unserved when the shop cannot serve it.
- Meals: flow, conserved, from a shop to the guest it served. Produced by PO, consumed by BG, which lowers its own hunger when it eats (hunger stays private to the guest).
- Supply orders: flow, conserved, from a shop to the nearest depot it can reach, placed by an (s, S) policy and consumed when the depot ships. Produced and consumed within PO; listed because it moves through SM's flow transport.
- Supplies: flow, conserved, from depot to shop along the backstage network. Produced and consumed within PO; listed because it moves through SM's flow transport with a delay PO takes from NN's route distance. Each shipment carries its arrival time, so shipments in transit when their route is removed still arrive and remain valid in a save.

Dependencies that are not fields or flows, ruled outside principle 3 by decision 0025:

- Park intent: the drawn paths and placed boxes, which with the simulation state are the saved park (principle 1). Authored by EB. NN, PO, and BG derive their parts of the world from it.
- Candidate resolution: for a preview, EB supplies tentative intent, DS provides a candidate copy of the world, and NN and PO resolve it through the same derivation they use for committed intent. LS samples the candidate world's fields through the normal field interface and never calls another capability's derivation directly.
- Inspection records: what a guest or shop publishes about itself for display, such as a guest's hunger and last choice with its factors, or a shop's limiting factor. Produced by BG and PO, read only by LS and by tests.

## Members

Ordered for building. Milestone slugs are provisional until each capability is planned. Operations and guests exchange medium both ways (offers one way, guest visits and meals the other); operations lands first because guests cannot choose without offers, and its milestone is tested against synthetic guest visits.

1. `deterministic-simulation/world-as-value`: the world as a copyable, hashable, saveable value with the deterministic tick cycle, candidate copies of the world, the state hash, keyed random draws, the simulation's own exp and log (decision 0022), and the cross-build check script. Depends on: none.
2. `shared-medium/first-field-and-flow`: the network type with carriers, places that survive re-derivation, and entity anchors, the field sampling interface with per-source attribution, and conserved flow transport with delay, tested with synthetic networks. Depends on: member 1.
3. `effortless-building/sketch-a-park`: park intent for curves and boxes, tools to draw paths and place and delete boxes with a ghost for tentative placements, rendering paths as tubes and boxes as boxes, saving and loading park files through deterministic-simulation's state walk, and the --park, --ticks, and --hash options for scripted captures and the cross-build check. Depends on: member 1.
4. `navigable-networks/paths-become-routes`: guest and backstage graphs derived from path intent, committed or candidate, shops, the depot, and the entrance connecting to nearby paths by face, and the route distance field with distances and next edges toward every anchored source. Depends on: members 2 and 3.
5. `plausible-operations/supplied-food-shop`: the depot and the generic food shop, resolved from committed or candidate intent, with supplies over the backstage network, meal production limited by the scarcest of demand, supply, and a fixed service rate, the food offer, unserved returns, the shop's inspection record, and a starved state shown on the box. Depends on: members 2, 3, and 4.
6. `believable-guests/hungry-guests`: guests arriving, wandering, getting hungry, choosing offers by scored utility with a seeded softmax, queuing, eating, and leaving, drawn as simple shapes on the paths, plus the hungry footfall field and inspection records with hunger and choice explanations. Depends on: members 1, 2, 4, and 5.
7. `legible-simulation/explained-food`: the food-availability overlay with hover attribution and an --overlay option to turn it on for scripted captures, the guest and shop inspectors, and the placement preview sampling a candidate world. Depends on: members 1 through 6.

## Out of scope

Staff (a service rate stands in for them), money and payment, needs other than hunger, shop forms and procedural building geometry, garbage and litter, smell, noise and other fields, terrain editing, rides and coasters, player-facing UI (tooling panels in ImGui stand in, and the question stays open), and previews that simulate ahead in time.

## Open questions

- How long 3000 ticks takes in the sanitized Linux build decides whether the criteria's run lengths hold. Resolved by measurement once the members step real content, after believable-guests lands, using the harness deterministic-simulation provides.

## Research notes

None at the slice level. Technique research (curves to graphs, route distance fields, flow transport, candidate worlds) belongs to the member capabilities.
