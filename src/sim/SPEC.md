# sim

The simulation: the park's state and the rules that change it. It holds a world of keyed entities. Their registered state can be copied, compared, hashed, saved, and replayed exactly (principles 1 and 10, decisions 0019, 0022, 0025, and 0039).

## Boundary

The target tpj_sim links only tpj_core. It never depends on rendering, windowing, or input (principle 10). The sim's tests link it alone, so a violation fails the build.

Modules inside the sim register their component types, systems, swap functions, resolvers, finishers, and commands with a WorldSchema. Each kind's internal components are private to its module, and only shared-medium components are public (decision 0039). Outside the sim, code reaches the world through these headers:

- sim/world.h
- sim/entity_key.h
- sim/command_queue.h
- sim/tick_buffers.h
- sim/save.h
- sim/draw.h
- sim/sim_math.h
- sim/clock.h
- the park's schema in sim/park_schema.h

## The tick cycle

The world advances only through stepWorld, one fixed tick of SIM_TICK_SECONDS at a time, so frame rate never changes what a tick does. Each call runs one cycle:

1. A pending resolution runs first.
2. The systems step in registration order.
3. World::Tick advances by one.
4. The swap functions run in registration order.
5. The queued commands apply in submission order.
6. Resolution runs again if a command made it pending.

Systems, swaps, resolvers, and finishers take the world and a TickBuffers, which holds the lists they reuse from one tick or resolution to the next. stepWorld(world, commands, buffers) passes the caller's buffers to every system and swap, and to every resolver and finisher of the resolutions it runs. resolveWorld(world, buffers), commitCommands(world, commands, buffers), and makeCandidate(world, commands, buffers) pass them to every resolver and finisher. stepWorld(world, buffers) is the same cycle with no commands. If you step or resolve a world repeatedly, keep one TickBuffers and pass it every time. A TickBuffers holds one value of each type a module asks for, made on first use and kept. It is never part of a world, so walks, hashes, saves, and copies never see it. Each system, swap, resolver, and finisher clears a list before it reads it, so a world steps and resolves to the same result whatever buffers it is given. stepWorld(world, commands), stepWorld(world), resolveWorld(world), commitCommands(world, commands), and makeCandidate(world, commands) use buffers of their own.

commitCommands(world, commands) applies the queued commands in submission order and resolves, without stepping the systems, advancing World::Tick, or running the swaps. It empties the queue. The world then equals the candidate makeCandidate would have made from it with those commands, so if a cycle has just left it, it equals the world that queuing the commands for that cycle would have given. If the queue is empty and no resolution is pending, nothing changes. If a queued command's type isn't registered, it throws std::invalid_argument and changes nothing. The app uses it to apply the player's edits while the game is paused.

isStepping, isResolving, and isFinishing are each true exactly while their stage runs. If a function is meant for one stage, it can use them to refuse callers from any other stage.

## Units

A module can register unit kinds and indexes with the schema, in its resolver order. A unit kind derives one value for each entity that holds a given intent component. Its derivation reads intent components, earlier kinds' units, and the keys an index files under a key, each by key, and nothing else. An index files each entity that holds a given intent component under the keys it names for that component.

Resolution keeps each unit's value and what its derivation read. A unit derives again only when its entity gains or loses the component, or when something it read has changed since. A command's use of an intent component counts as a change to it. If a unit derives again and its value comes out equal, word for word as the walk compares values, the units that read it don't derive again for it. So a resolution gives the same units as deriving every unit, whatever units the world kept.

An index can also file each unit of an earlier kind under keys it names from the unit's value. When the unit is added, removed, or derives a different value, the index files it again.

A module can also register a kind that updates its own units. At each resolution, its update reads what earlier kinds and indexes changed, reads their units and entries by key, and sets or removes its own units. At its first update, it is given every entity that holds its intent component. Resolution records a change for each unit the update adds, removes, or sets to a value whose words differ, so the units that read it derive again, as for any kind. A module that registers such a kind makes its units equal to what deriving them from nothing gives.

A unit kind can also be kept over earlier kinds instead of over an intent component. It keeps a unit for each key that any of those kinds holds a unit for, and its units are added and removed with theirs. Its module names those kinds with UnitsOf. The module that owns them can declare them in its public header without defining them, so the kind's module learns nothing of their values except what that module's public functions give.

changedUnits gives the keys of a kind's units that the running resolution added, removed, or changed, in ascending order. A resolver that runs after the kind reads it to change only what changed.

derivedUnits gives the keys of the units of the kind registered under a name that the world's last resolution derived, in ascending order, whether their values came out changed or not. For a kind that updates its own units, it gives the keys the update set or removed. A copy gives none until it resolves. It throws std::invalid_argument when no unit kind or index has the name, and an index derives none.

A new world and a loaded world hold no units, so their first resolution derives every unit. copyWorld carries the units and what they read, so a copy resolves only what changed since. copyWorld(world, into) makes into equal to the world, units included, reusing into's memory. It throws std::invalid_argument when into doesn't share the world's schema object. makeCandidate(world, commands, buffers, into) makes into the candidate. makeCandidate(world, change, context, buffers, into) makes into the candidate of a change that isn't a queued command. It refills into from the world, calls change(into, context), and resolves into. A caller that has already planned an edit uses it to apply the plan without planning it again. If the resolving caller's TickBuffers holds a ResolveTrace in its ResolveTraceSlot, resolution tells it each step's name as the step starts and ends: each resolver by its name, and the finishers together as finishers. Both makeCandidate forms that refill into also tell it copy-world around the refill. Units are never part of the walk, so hashes, saves, and comparisons never see them. unitValue gives a unit's value as the last resolution left it. unitsCurrent says whether a kind's units hold every change made to the world.

## The clock

parkTime(tick) gives the park's time at a tick, a ParkTime: its Day, counted from 1, and its Second of that day, from 0 to 86399. Tick 0 is 9:00 on day 1. Each tick adds GAME_SECONDS_PER_TICK to the clock, so a day is DAY_TICKS ticks, and a new day starts at midnight. A park's time is parkTime of its World::Tick. The clock is computed in whole numbers from the tick alone, so it is the same on every build, and it adds nothing to a world's state or save.

## Entity keys

Every entity has a stable EntityKey, and code names an entity only by its key. When a component refers to another entity, it holds the entity's key. Keys from createEntity come from a counter the world holds, and they are never reused.

Resolvers create entities with createDerivedEntity. Its key is a pure function of an owner key, a purpose, and an index, and it never collides with a counter key. If an entity already holds the derived key, createDerivedEntity reuses that entity.

Code reaches an entity's components through World, by key. get, tryGet, has, emplace, emplaceOrReplace, getOrEmplace, and remove each take the entity's key. If the key isn't live, tryGet gives nothing, has is false, and remove does nothing. isLive says whether a key names a live entity.

keysWith<T>() lists the keys of the entities that hold T, in ascending order. If a system visits them in that order, it visits them in the same order in a copy and after a load.

keysWith<T>(keys) puts the same keys in a list the caller keeps, clearing it first. forEachWith<T>(visit) calls visit(key, component) for each entity that holds T, in the same order, and visit must not add or remove a T. Neither allocates once the list has grown.

If code adds a component of a type the world's schema doesn't register, emplace, emplaceOrReplace, and getOrEmplace throw WorldInvariantError in every build, and the world doesn't change. No entity ever holds such a type. In debug builds, they also throw WorldInvariantError if the key isn't live.

## Schemas and the park

Component types are registered by name with a DataKind (intent, state, or derived) and a visitFields function that lists the type's fields once. Every walk over the world runs through visitFields. The schema is built once and shared as a const value.

makeParkSchema returns the park's schema, which is exactly what addPark registers. It registers in dependency order:

1. the medium
2. park intent and its commands
3. routes
4. furniture
5. operations
6. wayfinding
7. guests
8. last, the routes finisher, which drops the previous networks after every finisher that reads them

makeNewPark(seed) gives the new-park template (sim/park/SPEC.md) with that schema. Park files load with that schema. If a scenario adds synthetic entities, it calls addPark and then registers them.

## Copies, equality, and hashes

copyWorld, worldsEqual, and hashWorld walk registered types in registration order and entities in key order. Two worlds are equal exactly when the walk sees the same values in both, with doubles compared by their bits. The hash is a 64-bit function of those values alone. The walk covers the seed, the tick, the key counter, every registered component, and whether resolution is pending.

In debug builds, if a registered component holds a NaN, the walk refuses the world with WorldInvariantError.

## Commands

Commands are values of types that their owning module registers with addCommand. The module applies them with its applyCommand. Commands wait in a CommandQueue outside the world, so copies, hashes, and saves never hold them. stepWorld empties the queue. A queue keeps the memory of the commands it has held. Queueing a command copies it into memory kept for commands of its type, and emptying the queue keeps that memory for later commands. Once the queue has grown to hold its commands, queueing them and emptying the queue allocate nothing. If a queue holds a command of an unregistered type, the queue is refused before anything changes.

applyCommand can also take TickBuffers. When a command type has that form, stepWorld, makeCandidate, and commitCommands call it with their own buffers, so the command's module can keep the lists applying it works with.

## Resolution

Resolvers derive the world's derived data from its intent alone, never from state. Registration order is dependency order, so a resolver may depend only on resolvers registered before it. A resolution runs every resolver once and then every finisher once, each in registration order. Resolvers never draw from the key counter.

A finisher completes work that needs every resolver's output, such as comparing it with the previous resolution's output. It may change only state that its module's spec says a resolution invalidates. Examples are stepped entries the new resolution no longer matches, and places held on a re-derived network. In a world's first resolution, finishers change no state, so loading the save of a resolved world and resolving it gives the world saved.

Resolution is pending from a world's construction until its first resolution. It is pending again from each applied command until the next resolution. A new or loaded world is resolved in full before its first tick. If a cycle applies no command, it runs no resolver.

## Candidates

makeCandidate copies a world, applies commands to the copy, and resolves it without stepping. The original world is unchanged. If a world has just finished a cycle, a candidate made from it equals the world that queuing the same commands for that cycle would give. Previews rely on this to be exact (principle 8, decision 0025).

## Random draws

Draws are keyed, never sequential. A draw is a pure function of its DrawKey: the world's seed, an entity key, a purpose, the tick, and an index. drawKey fills the seed and tick from a world. Draws give the same values in any order, in a copy, and on every build. Drawing changes nothing in the world, so there is no generator state to copy or save.

drawBits gives 64 bits. drawUniform gives a uniformly distributed double in [0, 1). drawPick gives an index with probability proportional to its weight, from one draw, and never picks a zero weight. If drawPick cannot pick from the weights in proportion, it refuses them.

Simulation code draws only through these functions. It never uses the standard library's distributions, whose results differ between implementations (decision 0019).

## Saves

saveWorld writes a world's intent and state as text, and loadWorld reads it back. A save is canonical: equal worlds give identical text, however they were built. A save never holds derived data, commands, or whether resolution is pending. The loaded world has the saved seed, tick, key counter, entities, intent, and state, with resolution pending. If the saved world was resolved, resolving the loaded world gives a world equal to it.

Park files are hand-readable and hand-editable, and the format is a contract with them. The text opens with the line `tpj-park 1`, then seed, tick, and next-key lines. Sections follow. Each section is headed by a registered type's name in brackets. It has one line per entity that holds the type, in key order. Each line holds the key, then each field as name=value in visitFields order. An [entities] section lists counter keys whose entities hold no saved component.

Values are written this way:

- Integers are decimal.
- Doubles round-trip exactly.
- Bools are true or false.
- Enums are written by name, from the type's enumNames.
- A vector is its elements between [ and ].
- A struct is its fields as name=value between { and }.

A component may hold a FixedList, a list with a fixed capacity, wherever it may hold a vector. Walks, saves, and loads treat it exactly as a vector. If a save holds more values than its capacity, loadWorld throws LoadError.

loadWorld accepts carriage returns, blank lines, a last line with no line feed, and any number that std::from_chars reads in full. A hand-written save loads, though it may re-save with numbers spelled differently. If loadWorld meets anything else it cannot read, it throws LoadError, whose message and line() name the line. saveWorld throws std::invalid_argument for an enum value with no name.

## Determinism

The same world stepped the same number of times gives the same result, bit for bit, on every supported build (decision 0022). tpj_sim builds with -ffp-contract=off and never calls the C runtime's transcendental functions. It calls simExp and simLog from sim/sim_math.h instead. These are within one ULP of the correctly rounded result, return IEEE's special values, and give identical bits on every build.

cmake/check_sim_symbols.cmake enforces this rule. It fails when an archive references a C runtime transcendental function, in any of its forms. When it fails, it prints the symbol and the object that references it. Exactly rounded functions such as sqrt, floor, and fmod are still allowed. It runs on tpj_sim's archive.

The results also depend on the floating-point environment, which must be round-to-nearest with subnormals kept. In debug builds, constructing a World checks the constructing thread's environment. It throws WorldInvariantError if the environment is not the default.

## Memory in play

Once its buffers have grown, a cycle with no commands frees no memory it allocated (decision 0041). The world keeps the memory of the entities and components it removes and reuses it for those it adds. Once the park has held as many as it holds, guests arriving and leaving and seat service starting and ending allocate nothing. The allocation check in tests/checks holds a stretch of full.park's ticks to the first sentence.

KeptMap holds values by key in ascending order. It keeps the nodes of removed values for later adds, so adds and removes allocate nothing once it has grown. Copying one copies its values. Assigning one to another reuses the other's nodes and their memory.

Queueing commands and emptying the queue free no memory they allocated once the queue has grown. Applying a command passes the cycle's buffers. The allocation check holds the queue to it over each tool's release and each undo and redo on full.park.
