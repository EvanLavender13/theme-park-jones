# Feature: Text Saves

## Summary

text-saves writes a world's intent and state as canonical text and reads it back. saveWorld writes a version header, the seed, tick, and key counter, then a section listing the counter-keyed entities that hold nothing saved, then a section per registered intent or state type in registration order, with one line per entity in key order. Each line is the key followed by the type's fields as name=value, through the same visitFields that serves the walk: numbers in std::to_chars's shortest round-trip form, enums by the names their type supplies, vectors in brackets, and structs in braces. loadWorld reads that form strictly, leaves the world pending resolution, and names the line of any error. Loading a save of a resolved world and resolving gives the world that was saved, and saving it again gives the same text. This feature does not include reading or writing files, which effortless-building's park-file commands own, or running saves in tpj_scenarios (cross-build-check).

## Acceptance criteria

1. For randomized resolved synthetic worlds, including every field kind, counter-keyed entities that hold no intent or state component, derived-key entities that hold state, and state part-way through a multi-tick process, loading the save and resolving gives a world equal to the one saved, with the same hash, and saving that world again gives the identical text.
2. saveWorld's text is the form src/sim/SPEC.md defines, and equal worlds give identical saves, however their entities and components were created.
3. A loaded world holds the saved seed, tick, and next key, and has resolution pending.
4. A save holds no derived data: no section names a derived type, and changing, adding, or removing derived components leaves a world's save unchanged.
5. Text that is not a save in the form src/sim/SPEC.md defines fails the load with LoadError naming the first line at which it can no longer be read as one, or the line after its last when it ends where a save cannot: inside the header, or after a section with no key lines. No text makes loadWorld crash or trip the sanitizers.
6. Blank lines, and a carriage return before any line feed, do not change what a save loads as.
7. saveWorld throws std::invalid_argument, naming the field, for an enum value its type gives no name. In debug builds it throws WorldInvariantError, naming the type or field, for a world holding a component of an unregistered type or a NaN in registered state, as copy and hash do.
8. Registering a component type named "entities" is refused with std::invalid_argument and no change to the schema, since saves use that name.

## Medium

This feature introduces no fields or flows. It provides:

- saveWorld and loadWorld: effortless-building's park-file commands write and read park files through them, and its new-park template is a save.
- The text form: tests/parks/ holds park files in it, which cross-build-check runs on both builds.
- enumNames: every enum field of every registered type supplies its names, which is how later capabilities' kinds (shop, depot) read in park files.

## Principle checks

- Principle 1: criterion 4. A save holds intent and state only, and criterion 1 shows that resolving regenerates everything else.
- Principle 10: criteria 1 and 2. A save is a canonical function of the world, so equal worlds give identical files and a loaded park simulates as the saved one did. Numbers go through std::to_chars and std::from_chars, which libstdc++ implements identically on both builds.
- Principle 6: saveWorld and loadWorld touch components only through each type's registered write and read functions, built from its owner's visitFields, and never name a component type.
- Principle 2: a world with nothing registered saves as its four header lines and loads back equal once resolved.

## Spec changes

src/sim/SPEC.md: in the paragraph beginning "Every entity has a stable EntityKey", after the sentence ending "never meets the counter's range.", add:

"createDerivedEntity reuses an entity that already holds its key. A loaded entity with a derived key has no origin until resolution, and takes the origin of the first createDerivedEntity call that reaches it."

At the end of the paragraph beginning "Component types are registered with a WorldSchema", add:

"A registered type must be default constructible, so that a load can fill it, and no type may be named entities, which saves reserve."

After the paragraph on keyed draws' distributions (beginning "drawUniform is the draw's top 53 bits"), add:

"saveWorld writes a world's intent and state as text, and loadWorld reads it back. A save is canonical: equal worlds give identical text. Its first four lines are tpj-park 1, then seed, tick, and next-key, each followed by a space and its value. Sections follow, each after a blank line and headed by its name in brackets. [entities] comes first, listing one per line, in ascending order, the counter keys whose entities hold no intent or state component, and it is written only when there are some. A section per intent or state type follows, in registration order, written only when some entity holds the type, with one line per such entity in ascending key order: the key, then each field in visitFields order as a space and name=value. Derived types never appear, and neither does whether resolution is pending. Integers are written in decimal and doubles in std::to_chars's shortest round-trip form; bools are true or false; entity keys are their numbers; enums are written by name; a vector is its elements between [ and ], separated by single spaces; and a struct is its fields as name=value between { and }, separated by single spaces. Every line ends with a line feed.

An enum field's type supplies its names through a constexpr function found by argument-dependent lookup, enumNames(value), which returns a std::array of the names of the values 0, 1, and so on. The names are distinct, and lowercase letters, digits, and hyphens, which a compile-time check enforces. saveWorld throws std::invalid_argument for an enum value with no name, and in debug builds it first checks the world as copy and hash do.

loadWorld reads the text saveWorld writes. It ignores blank lines and a carriage return at the end of a line, and it reads any number std::from_chars reads in full, so a re-save may spell a hand-written number differently. It throws LoadError, whose message and line() name the line, for anything else: a header line missing, out of order, or with the wrong label or version; a section header that is malformed, unknown, repeated, out of order, or names a derived type; a key line before any section; a section with no key lines, found at the next section header or the end of the text; a key that is zero, not above the previous key in its section, a counter key not below next-key, a derived key in [entities], or a key listed in [entities] that appears again in a type's section; a next-key outside 1 to 2^63; a field missing, out of order, or misnamed; a value that is malformed, does not fit its field, is NaN, or is not one of its enum's names; and any text after a line's last field. An error found at the end of the text names the line after the last. The loaded world has the saved seed, tick, next key, entities, intent, and state, and resolution pending. Resolving it recreates the derived data, so loading the save of a resolved world and resolving gives a world equal to the one saved."

## Files affected

- Create: src/sim/field_text.h
- Create: src/sim/save.h
- Create: src/sim/save.cpp
- Modify: src/sim/schema.h
- Modify: src/sim/schema.cpp
- Modify: src/sim/world.h
- Modify: src/sim/world.cpp
- Modify: src/sim/CMakeLists.txt
- Modify: src/sim/SPEC.md
- Test pass: files under tests/, written by the test-writer agent. tests/synthetic_types.h's Mood needs an enumNames function once enum fields require one.

## Dependencies

- registered-walk: keys, the schema, visitFields, copy, equality, and hash. Merged.
- tick-cycle: resolveWorld and pending resolution. Merged.
- std::to_chars and std::from_chars for double, in libstdc++ since GCC 11.

## Out of scope

- Reading and writing files, and the save and load commands: effortless-building's park file.
- Loading saves in tpj_scenarios and comparing them across builds: cross-build-check.
- Save versions and migration when registered types change: the capability's save-versioning candidate.
- Comments in saves.
- A binary encoding: the capability's binary-encoding candidate.

## Open questions

- What happens to a loaded derived-key entity that no resolver reaches. It keeps no origin, so the loaded world differs from one where resolution created it. It cannot arise from the save of a resolved world. Resolved when resolvers remove derived entities their intent no longer derives.
- Whether field names should be checked. A name that breaks the format fails its own type's round trip rather than registration. Resolved if one slips into a registered type.
