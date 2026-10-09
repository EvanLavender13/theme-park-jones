# Research: keyed-storage

## What storage should the world use for components keyed by EntityKey?

The world needs four things from its storage. It finds a component by a 64-bit EntityKey. It iterates one type's components in key order, because the walk and the systems already work in key order. A reference to a component stays valid while other entities get or lose that type, because systems hold one component while they add another. Iteration order must not depend on the history of inserts and removals, or a loaded world would iterate differently from the live one.

Keys are sparse. Counter keys start at 1, but derived keys set a high bit and come from a hash. A plain array indexed by key isn't possible, so every option needs a map from key to slot somewhere.

A std::map per registered type meets all four needs with no extra code. Lookup is O(log n). Iteration is in key order by construction. Insert and erase never invalidate references to other elements, which is stronger than EnTT's guarantee today: EnTT's pages keep references stable on insert, but a removal swaps the last element into the hole. The cost is a heap node per component and pointer-chasing on iteration. Today each access already does a std::map lookup in World::findEntity and then EnTT's sparse-set lookup, so a map per type does one map lookup where the current code does two.

Deterministic game projects reach the same conclusion. Egregoria's determinism work replaced hash maps with B-tree maps so that iteration order survives save and load, at a modest runtime cost. Other deterministic ECS designs make ascending entity order part of the storage contract, so no system can depend on insertion history.

This suggests a std::map per type for the first version. Faster stores can replace it behind the same interface later, measured with scripts/runtime-report.sh. The likely next steps are a dense array sorted by key with a key-to-index map, or a B-tree. Both keep key order. The dense array loses reference stability on insert, so the interface would have to forbid holding references across an emplace of the same type first.

Rejected: a sparse set like EnTT's — its dense order follows insertion history, which is the problem this milestone removes, and 64-bit sparse keys need a hash map in front of it anyway. A sorted std::vector per type — insertion moves elements and invalidates references that systems hold today. A slot map — it hands out its own handles instead of using EntityKey, and iterates in slot order, not key order. std::unordered_map — iteration order isn't defined, and it differs between standard libraries.

Sources: https://github.com/uriopass/egregoria/issues/90 — deterministic sim that moved to ordered maps for iteration that survives save and load; https://doc.qu1x.dev/bevy_trackball/src/bevy_ecs/storage/sparse_set.rs.html — sparse-set storage layout as Bevy implements it; https://arxiv.org/pdf/2508.15264 — survey of ECS storage: arrays, sparse sets, maps, archetypes, and their costs.

## How does a component type get an identity without EnTT's type_id?

The schema needs a value that names each registered type, so the world can find that type's store and the schema can refuse a type registered twice. EnTT supplies this with type_id<T>().hash().

The common replacement is the address of a static variable in a class template. Each instantiation gets its own variable, so the address is unique per type within one program. Google Test and Chromium's base/type_id.h use this. The variable must not be const, because some compilers have merged identical const statics. The project links statically and builds with one compiler, so the address is stable for the whole run. It isn't stable between runs, so it must never reach a save or a hash. Nothing saves or hashes TypeId today: saves use the registered name, and the walk follows registration order.

The id only finds the type's position in the schema. The world can then hold one store per registered type, in a vector indexed by that position. If a lookup by id on every access costs too much, a static per-type cache of the index for the current schema avoids it, but that is an optimization to measure first.

Command types need a printable name only for two error messages. The registered name or a fixed text can replace EnTT's type name there.

Rejected: typeid and std::type_index — they work, since RTTI is on, but they compare by name strings on some ABIs and add nothing the static address doesn't. A global counter that hands out ids on first use — the ids depend on the order types are first touched, which varies between programs that link the sim. __PRETTY_FUNCTION__ parsing — it gives a name, not a cheap id, and its format is compiler-specific.

Sources: https://chromium.googlesource.com/chromium/src/+/refs/tags/77.0.3825.0/base/type_id.h — static-address type id without RTTI; https://cobalt.googlesource.com/cobalt/+/refs/heads/master/cobalt/base/type_id.h — same technique, with the note about non-const statics.
