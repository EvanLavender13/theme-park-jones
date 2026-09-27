# Research: tick-cycle

## Where do queued commands live?

Deterministic lockstep games treat a tick as a function of the state and the inputs applied at that tick. Factorio's clients hold the full game state and receive only input actions, which each client applies at the same tick. Replays in such games are a starting state plus a log of commands per tick. The commands are inputs to the simulation, not part of its state.

For this project, that means the command queue is a separate object passed to the cycle, which empties it. The world value never holds pending commands, so copies, hashes, and saves never meet them, and command types need no visitFields until replays want to record them. Evan chose this.

Rejected: a queue inside the world, covered by copy, equality, and hash. Every command type would need visitFields, and saves would carry commands that have not yet happened.

Sources: https://www.factorio.com/blog/post/fff-302 — input actions proxied and applied on the same tick by every client; https://www.snapnet.dev/blog/netcode-architectures-part-1-lockstep/ — lockstep synchronizes inputs rather than state; https://ruoyusun.com/2019/04/06/game-networking-3.html — replays as stored command lists.

## How are resolvers ordered, and how are dependency cycles rejected at registration?

A resolver may depend only on resolvers already registered. A dependency on an unknown or later resolver, or on itself, is refused. Any cycle would need at least one such forward reference, so none can form. Registration order is then a valid dependency order, and running resolvers in registration order is deterministic without a topological sort. This reads the milestone's "rejected at registration" literally: a check that needed the full set of resolvers could only run once registration had ended.

Rejected: a topological sort over declared dependencies, with ties broken by registration order. It allows registering a resolver before its dependencies, but cycles could then be detected only after all registration, and the order would be harder to read from the registration function.

## How is a command type applied without the cycle naming it?

A command type is registered with addCommand<T>(), which stores the type's id and a captureless function that calls the owning module's applyCommand(World &, const T &), found by argument-dependent lookup. This follows visitFields. A queued command is held in std::any beside its type id. The cycle looks the id up in the schema and calls the stored function, so it never sees the command's layout (principle 6). std::any needs a copyable type, which also lets makeCandidate apply the same queue to a copy without consuming it.

Rejected: std::variant over all command types. It needs a central list naming every module's commands. Commands as std::function closures: a closure can capture state outside the world, which breaks principle 10.
