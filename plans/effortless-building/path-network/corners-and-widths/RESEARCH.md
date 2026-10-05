# Research: corners-and-widths

The milestone's RESEARCH.md covers how an arc is built, clamped, and split between its two segments. This file covers the handles that set a corner's radius and a segment's width.

## Where should the radius and width handles sit, how should a drag map to a value, and what happens when the value doesn't fit?

Illustrator's Live Corners puts a widget inside each corner of a selected path. It stands for the center of the corner's curve: dragging it away from the corner makes the radius larger, and dragging it toward the corner makes it smaller. A sharp corner still has its widget, set a little way inside the corner, so it never sits on the anchor point the player drags to move the corner. When the drag reaches the largest radius the neighboring segments allow, the arc preview turns red and the radius stops growing. A preference hides the widget at corners that are nearly straight. Illustrator measures the largest radius against the next anchor along each segment, which matches the milestone's rule of half of each segment.

Figma shows a radius handle inside each corner when the pointer hovers the shape. It hides the handles when the shape is too small on screen for them to stay clear of the resize handles. Inkscape puts its rounding handles on the corner itself when the radius is 0, where they overlap and the player drags one out to reveal the other.

None of the park and city builders found let the player drag a path's side to set its width. Planet Coaster 2 sets the width in the path tool's panel before drawing. Cities: Skylines 2 offers a fixed set of road widths. A side handle has to come from editors instead, where dragging an edge moves that edge. If a drag moves only one side of a path, the center line has to move with it. The milestone keeps the width centered on the ground line, so the drag sets the width to twice the pointer's distance from that line.

These findings suggest the following:

- Put the radius handle inside the corner, on the line halfway between its two segments. Put it at the arc's middle, but never nearer the corner's point than a fixed distance, so it stays clear of the point's own handle at a sharp or tight corner.
- Make the drag relative to where it starts, so pressing the handle doesn't change the radius. Moving the pointer away from the corner along that halfway line makes the radius larger.
- Stop the radius at the largest that fits, as Illustrator does, instead of storing a value the corner can't show.
- Leave out the handle where the two segments run straight on, because there is no corner to round.
- Set the width to the width at the press plus twice how far the pointer has moved away from the ground line, within the milestone's limits.
- If a radius or width would run the path into a box, show the ghost in the invalid tint and refuse the commit, as for every other edit (principle 5).

Rejected: putting the radius handle at the corner's point, as Inkscape does — Edit path already drags the point from there. Hiding handles until hovered, as Figma does — the handle sits at a place the player can't guess, away from the point. Shrinking a radius or width until it clears a box — the game refuses invalid edits instead of changing them, and the ghost shows why. A width set in a panel before drawing, as Planet Coaster 2 does — the milestone asks for a drag, and a panel can't set one segment's width after drawing.

Sources: https://transitmap.net/live-corner-limits/ — Live Corners stops at the next anchor along a segment; https://www.alphr.com/illustrator-how-to-round-corners/ — the red arc at the largest radius; https://help.figma.com/hc/en-us/articles/360050986854-Adjust-corner-radius-and-smoothing — Figma's hover handles; https://forum.figma.com/ask-the-community-7/why-radius-handle-not-appear-14221 — handles hidden on small shapes; https://inkscape.org/doc/tutorials/shapes/tutorial-shapes.html — Inkscape's rounding handles on the corner; https://forums.frontier.co.uk/threads/tutorial-making-smaller-paths-2m-instead-of-4m.630147/ — Planet Coaster 2's width set before drawing; https://www.paradoxinteractive.com/games/cities-skylines-ii/features/road-tools — Cities: Skylines 2's road tools.
