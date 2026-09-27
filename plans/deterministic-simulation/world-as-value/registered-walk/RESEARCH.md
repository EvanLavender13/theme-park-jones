# Research: registered-walk

## How should a module describe a type's fields to the walk?

Serialization libraries converge on one function per type that lists its fields and is run by different visitors. cereal's serialize(Archive&, T&) and Boost.Serialization's serialize(Archive&, T&, version) are the best known. The same list drives saving and loading, so the two cannot disagree about which fields exist. The walk needs five operations (copy, compare, hash, encode, decode), and a single visitFields(Visitor&, T&) serves all of them except copy, which the type's copy constructor handles. The visitor for hashing and equality turns each field into 64-bit words. The later encode and decode visitors in text-saves use the field names. The function takes a mutable reference so that decoding can write through it. Read-only visitors receive the component through a const_cast, which is safe because the stored component is not itself const.

The function is a template found by argument-dependent lookup. It lives beside the type in the owning module, and the walk instantiates it only inside that module's registration call. The walk then holds type-erased function pointers and never sees the layout (principle 6).

Rejected:

- Five hand-written functions per type: they can drift apart, so a field hashed but not saved would be lost silently. Evan chose visitFields.
- Reflection through a macro or code generator: more machinery than a function per type, for no gain at this size.

Sources:

- https://uscilab.github.io/cereal/serialization_functions.html: the single serialize function pattern.
- https://www.boost.org/doc/libs/release/libs/serialization/doc/tutorial.html: the same pattern in Boost.Serialization.

## What does EnTT v4 offer for walking a registry?

The pinned EnTT (v4.0.0, .cpm-cache/entt/1d2c/src/entt/entity/registry.hpp) was read directly:

- registry.storage() iterates the component storages as (id, storage) pairs. It never includes the entity storage, which the registry keeps apart. Each storage reports info(), a type_info with hash() and name(), and whether it is empty().
- registry.storage<entt::entity>() gives the entity storage. Its each() visits only live entities.
- entt::type_id<T>().hash() matches the storage's info().hash() for the default storage of T.
- all_of, get, and emplace work on a const registry for reading, and registry copy construction is deleted, so a copy must be rebuilt through the walk.
- Empty component types are stored without instances, so get is unavailable for them. The walk copies an empty component by emplacing it, and gives it no fields.

A module that views a type creates an empty storage for it. Only non-empty storages of unregistered types therefore count as a violation.

Sources:

- .cpm-cache/entt/1d2c/src/entt/entity/registry.hpp: storage iteration, assure, and the entity storage.
- .cpm-cache/entt/1d2c/src/entt/entity/storage.hpp: each() over live entities.
