# Principles

These are the constitution. They are general rules for deciding cases nobody has discussed yet. When a request or plan conflicts with a principle, surface the conflict to Evan instead of coding around it. Changing a principle is a deliberate decision recorded in the decision log, never a side effect.

1. The saved park is what the player expressed plus the simulation's changing state. Everything visible is derived from those and is never saved or edited directly.
2. Every intermediate state of the park is a legitimate, working state.
3. Things interact only through two kinds of medium: influences that are sampled without being consumed, and quantities that are conserved and consumed as they move.
4. Distance is measured along the routes things actually travel.
5. Physical validity is the only hard constraint. Everything else affects quality as a gradient.
6. Nothing reads another thing's internals. Complex behavior stays inside an entity and is visible to the world only through the shared medium.
7. The park must plausibly operate. What it looks like is up to the player.
8. Every outcome can be traced to its causes, and every change can be previewed before it is committed.
9. The player directs through standards and policies, not individual instructions.
10. The simulation is deterministic and runs independently of rendering.

Principles 1 through 6, 8, and 10 can be violated by code and should be backed by tests or checks wherever possible. Principles 7 and 9 are game design stances.

The spirit behind these is Unix's "everything is a file": one shared medium that every component speaks, so cross-cutting interactions happen through the medium instead of through direct wiring between systems.

Examples of tests the principles imply: resolvers give identical output for identical inputs; attributed contributions sum to the field value; a preview's predicted context matches the world after placement; regenerating from a save reproduces the world; flows conserve their quantities; no entity accesses another entity's internals.
