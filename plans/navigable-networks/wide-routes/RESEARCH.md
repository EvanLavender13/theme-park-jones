# Research: wide-routes

## Where should a network's nodes come from once paths are points and segments?

Road-building games that model roads as nodes joined by segments, such as Cities: Skylines, build their road graph straight from those nodes. Two roads join only where they share a node, and a road that passes over or beside another without one doesn't connect. Players read the junctions they placed, and nothing joins behind their back.

good-paths settles the same rule for this game. path-network already splits segments at every crossing it makes and shares a point there, so every intended join is a shared point. routes can then take its nodes from the points the segments name, with no search for where lines meet. Segments of one kind that overlap without sharing a point, as when a path runs along another or a widened segment covers its neighbor, stay apart on the network as they do on the paving, where each one's edge line shows across the other.

A point that a rounded corner uses has no ground position on either segment's line, because both lines end at the middle of the arc. The node sits where the lines meet.

Rejected: keeping the geometric meeting search beside shared points. It would join paths the player kept apart, which is the Planet Coaster fault the slice exists to fix.

Sources: plans/slices/good-paths/RESEARCH.md — Cities: Skylines' nodes and segments, and Planet Coaster 2's joins by nearness; plans/effortless-building/path-network/RESEARCH.md — how splits share points.

## How far should a door reach, and from where?

RollerCoaster Tycoon connects a stall only when its doorway is next to a path tile and faces it. In Planet Coaster the path has to meet the entrance, and players often report shops that look connected but aren't. Neither game measures from a path's center, because their paths have no width the player sets.

Here a door gets a connector, a short walkway the renderer draws, so a small gap between door and paving still reads as a way in. The reach only has to forgive freehand placement. Today it is 4 m from a path's center line. For a default 3 m guest path that is 2.5 m from the edge, and for a default 2 m backstage path it is 3 m. On a 10 m street it is less than nothing, since the door can't stand on the paving.

Measuring from the edge as the center line's distance less half the segment's width matches what the frontage snap measures. A 3 m reach from the edge keeps every connection a door beside a default-width path has today. It also covers the frontage setback of 1 m on every width, so a shop placed by frontage always connects. In main-street.park, the shops' back doors stand 2 m from the backstage path's edge and the depot's door 1 m.

If several segments are within reach, the door connects to the one whose edge is nearest, as frontage faces the nearest edge. The connector still runs to the segment's center line, where its node is, and its length counts in routes.

Rejected:

- Requiring a door to touch the paving. Players placing a box without the frontage snap would rarely land exactly flush, and nothing would show why the box is unconnected.
- Keeping 4 m from the edge. It adds connections that look accidental, with walkways over 4 m of grass.
- Measuring to the drawn surface, with its square ends and junction paving. It is exact, but it differs from what frontage measures. The difference is at most half a width beyond a free end.

Sources: http://cdn.akamai.steamstatic.com/steam/apps/285310/manuals/rollercoaster_tycoon.pdf — a stall's doorway must be next to a footpath and face it; https://steamcommunity.com/app/493340/discussions/0/4859966604397282613/ — Planet Coaster shops that look connected but aren't.
