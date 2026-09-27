# Research: resolved-fields

## How does a field carry its entry type, kind, and sampling rule into the walk?

The milestone's research settles the storage: one table per field on an entity with a derived key, with each source's entries in a slot ordered by source key. What is left is how the owning module names the field's entry type and sampling rule, so that the walk can copy and hash the table and a sample can call the rule.

EnTT registers components by C++ type, and the schema refuses a type registered twice, so two fields with the same entry type, such as two scalar fields of doubles, need two distinct component types. A field is therefore a type of its own. Policy-based design in C++ gives such a type its behaviour as static members that templates read at compile time. The field type names its entry type, its name, its kind, and, when it has one, its sampling rule as a static function. The medium's templates then build the field's component type from it, register it under the field's name, and call the rule directly, with no function pointer or type-erased value stored anywhere the walk would have to cover.

A resolution must start each field empty, so that a removed source has nothing afterwards. The schema already runs resolvers in registration order, and a field has to be registered before any resolver can publish into it, so a resolver registered with the field, which empties its table, runs before every producer that uses it. No new hook in the cycle is needed.

Rejected: A rule held as a function pointer in the field's component — the walk cannot copy or hash code, and every copy would carry it. A registry of fields held outside the schema — it would be global state beside the world, which breaks copies (the milestone's research on Overwatch's singletons). A component per entry type — two fields with one entry type would collide. Clearing a source's entries only when it republishes — a source that is removed never republishes, so its entries would outlive it.

Sources: https://skypjack.github.io/entt/md_docs_2md_2entity.html — components are identified by type; https://en.wikipedia.org/wiki/Modern_C%2B%2B_Design#Policy-based_design — behaviour supplied as a type's static members.
