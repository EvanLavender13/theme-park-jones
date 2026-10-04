# sim

The simulation: the park's state and the rules that change it. It holds a world of keyed entities whose registered state can be copied, compared, hashed, saved, and replayed exactly (principles 1 and 10, decisions 0016, 0019, 0022, and 0025).

## Boundary

The target tpj_sim links nothing but tpj_core and EnTT, and never depends on rendering, windowing, or input (principle 10). The sim's tests link it alone, so a violation fails the build.

Modules inside the sim register their component types, systems, swap functions, resolvers, finishers, and commands with a WorldSchema. Each kind's internal components are private to its module, and only shared-medium components are public (decision 0016). Code outside the sim reaches the world through sim/world.h, sim/entity_key.h, sim/command_queue.h, sim/save.h, sim/draw.h, sim/sim_math.h, and the park's schema in sim/park_schema.h.

## The tick cycle

The world advances only through stepWorld, one fixed tick of SIM_TICK_SECONDS at a time, so frame rate never changes what a tick does. Each call runs one cycle: a pending resolution first, then the systems step in registration order, World::Tick advances by one, the swap functions run in registration order, the queued commands apply in submission order, and resolution runs again if a command made it pending. stepWorld(world) is the same cycle with no commands.

isStepping, isResolving, and isFinishing are each true exactly while their stage runs, so a function meant for one stage can refuse callers from any other.

## Entity keys

Every entity has a stable EntityKey, and a component refers to another entity by its key, never by entt::entity, because EnTT recycles its identifiers. Keys from createEntity come from a counter the world holds and are never reused. Resolvers create entities with createDerivedEntity, whose key is a pure function of an owner key, a purpose, and an index, and never collides with a counter key. createDerivedEntity reuses an entity that already holds its key.

## Schemas and the park

Component types are registered by name with a DataKind, intent, state, or derived, and a visitFields function that lists the type's fields once. Every walk over the world runs through visitFields. The schema is built once and shared as a const value.

makeParkSchema returns the park's schema, exactly what addPark registers, in dependency order: the medium, park intent and its commands, routes, operations, guests, and last the routes finisher that drops the previous networks after every finisher that reads them. makeNewPark(seed) gives the new-park template (sim/park/SPEC.md) with that schema. Park files load with it, and a scenario that adds synthetic entities calls addPark and registers them after.

## Copies, equality, and hashes

copyWorld, worldsEqual, and hashWorld walk registered types in registration order and entities in key order, never in EnTT storage order. Two worlds are equal exactly when the walk sees the same values in both, doubles compared by their bits, and the hash is a 64-bit function of those values alone. The walk covers the seed, the tick, the key counter, every registered component, and whether resolution is pending. In debug builds the walk refuses, with WorldInvariantError, a world holding state it cannot cover: an unregistered component, an entity with no key, or a NaN.

## Commands

Commands are values of types their owning module registers with addCommand and applies with its applyCommand. They wait in a CommandQueue outside the world, so copies, hashes, and saves never hold them, and stepWorld empties the queue. A queue holding a command of an unregistered type is refused before anything changes.

## Resolution

Resolvers derive the world's derived data from its intent alone, never from state. Registration order is dependency order, so a resolver may depend only on resolvers registered before it. A resolution runs every resolver once and then every finisher once, each in registration order. Resolvers never draw from the key counter.

A finisher completes work that needs every resolver's output, such as comparing it with the previous resolution's. It may change only state its module's spec says a resolution invalidates, such as stepped entries the new resolution no longer matches or places held on a re-derived network, and it changes no state in a world's first resolution. So loading the save of a resolved world and resolving gives the world saved.

Resolution is pending from a world's construction until its first resolution, and again from each applied command until the next. A new or loaded world is resolved in full before its first tick, and a cycle that applies no command runs no resolver.

## Candidates

makeCandidate copies a world, applies commands to the copy, and resolves it, with no stepping, leaving the original unchanged. A candidate made from a world that has just finished a cycle equals the world that queuing the same commands for that cycle would give, so a preview is exact (principle 8, decision 0025).

## Random draws

Draws are keyed, never sequential. A draw is a pure function of its DrawKey: the world's seed, an entity key, a purpose, the tick, and an index. drawKey fills the seed and tick from a world. Draws give the same values in any order, in a copy, and on every build, and drawing changes nothing in the world, so there is no generator state to copy or save.

drawBits gives 64 bits, drawUniform a uniformly distributed double in [0, 1), and drawPick an index with probability proportional to its weight, from one draw, never picking a zero weight. drawPick refuses weights it cannot pick from in proportion. Simulation code draws only through these functions, never through the standard library's distributions, whose results differ between implementations (decision 0019).

## Saves

saveWorld writes a world's intent and state as text, and loadWorld reads it back. A save is canonical: equal worlds give identical text, however they were built. It never holds derived data, commands, or whether resolution is pending. The loaded world has the saved seed, tick, key counter, entities, intent, and state, with resolution pending, so loading the save of a resolved world and resolving gives a world equal to the one saved.

Park files are hand-readable and hand-editable, and the format is a contract with them. The text opens with the line `tpj-park 1`, then seed, tick, and next-key lines. Sections follow, each headed by a registered type's name in brackets, with one line per entity holding the type, in key order: the key, then each field as name=value in visitFields order. An [entities] section lists counter keys whose entities hold no saved component. Integers are decimal, doubles round-trip exactly, bools are true or false, enums are written by name from the type's enumNames, a vector is its elements between [ and ], and a struct is its fields as name=value between { and }.

loadWorld accepts carriage returns, blank lines, a last line with no line feed, and any number std::from_chars reads in full, so a hand-written save loads and may re-save with numbers spelled differently. Anything else it cannot read throws LoadError, whose message and line() name the line. saveWorld throws std::invalid_argument for an enum value with no name.

## Determinism

The same world stepped the same number of times gives the same result, bit for bit, on every supported build (decision 0022). tpj_sim builds with -ffp-contract=off and never calls the C runtime's transcendental functions; it calls simExp and simLog from sim/sim_math.h, which are within one ULP of the correctly rounded result, return IEEE's special values, and give identical bits on every build.

cmake/check_sim_symbols.cmake enforces the rule: it fails when an archive references a C runtime transcendental function, in any of its forms, printing the symbol and the object that references it. Exactly rounded functions such as sqrt, floor, and fmod stay allowed. It runs on tpj_sim's archive.

The results also depend on the floating-point environment, which must be round-to-nearest with subnormals kept. In debug builds, constructing a World checks the constructing thread's environment and throws WorldInvariantError if it is not the default.
