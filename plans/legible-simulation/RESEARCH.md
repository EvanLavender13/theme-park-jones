# Research: legible-simulation

## What makes a simulation legible to the player?

Tynan Sylvester's "The Simulation Dream" argues that players cannot hold a complex system in their heads. What they actually play is their mental model of it, built from the hints the game shows. So the design task is to make the inputs and outputs that drive decisions visible, and to keep hidden complexity that the player cannot perceive to a minimum. A player can only choose meaningfully when they can predict how the rules will respond.

Cities: Skylines is this project's warning case (docs/vision.md). It has dozens of info views, one per quantity, and icons for unmet needs, so problems are easy to spot. But the overlays show what is, not why, and they cannot show what a change would do. Players describe the late game as keeping every overlay blue by trial and error.

For this project, that means three things for every overlay:

- It must answer "why here": attribution to sources.
- It must answer "what if": a preview on a candidate world.
- It must keep its value exact, so the explanation is the truth rather than a summary of it (principle 8).

Rejected:

- Overlays without attribution or preview: the Cities: Skylines failure the vision names.

Sources:

- https://www.gamedeveloper.com/design/the-simulation-dream: players play their mental model, so show the inputs and outputs of decisions.
- https://skylines.paradoxwikis.com/Info_views: the scope and form of Cities: Skylines' info views.
- https://www.pcgamesn.com/cities-skylines/cities-skylines-review: the overlay-chasing late game as players experience it.

## How should an overlay's values be colored?

Rainbow color maps mislead, because equal steps in data do not look like equal steps in color, and they fail for colorblind readers. Perceptually uniform sequential maps such as viridis, magma, and cividis make equal data steps look equal. They carry order mainly in lightness, so they stay readable with red-green color deficiency and in grayscale. Cividis is tuned to look nearly the same to colorblind and non-colorblind viewers.

For this project, the food-availability overlay is a sequential, non-negative quantity, so a single-hue or viridis-like ramp fits. Zero should be clearly distinct, as no food reachable. Tooling may use the C runtime freely for color math, since it never feeds back into the simulation (decision 0022).

Rejected:

- Rainbow or red-to-green ramps: they are not perceptually uniform, and not colorblind-safe.

Sources:

- https://sjmgarnier.github.io/viridis/articles/intro-to-viridis.html: viridis design goals, uniformity, colorblind safety, and grayscale readability.
- https://colorcet.holoviz.org/user_guide/Continuous.html: perceptually uniform continuous color maps.
