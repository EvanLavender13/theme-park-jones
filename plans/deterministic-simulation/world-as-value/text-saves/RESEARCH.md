# Research: text-saves

## How are numbers written and read so both builds agree byte for byte?

std::to_chars for a double with no format argument writes the shortest string that reads back to the same bits, choosing fixed or scientific notation by whichever is shorter, preferring fixed on a tie. libstdc++ has implemented it with Ryu since GCC 11, and both builds use libstdc++ (GCC 13 on Linux, MSYS2 UCRT64 GCC 14 on Windows), so both write the same characters. std::from_chars for double arrived in the same release; it is locale-independent and correctly rounded, so it reads the shortest form back to the original bits. -0.0 is written "-0" and reads back as -0.0. Infinities are written "inf" and "-inf" and read back; NaN reads back too, and must be refused, since NaN in registered state is already an error.

Integers use std::to_chars and std::from_chars in base 10. from_chars reports a value outside the target type as std::errc::result_out_of_range, and it stops at the first character that is not part of the number, so a reader must also check that it consumed the whole token. It accepts a leading minus for signed types only and never a leading plus, which keeps one spelling per value.

Rejected: iostreams and printf — locale-dependent, and precision settings either lose bits or write more digits than the shortest form, which breaks byte-identical re-saves. Hex floats — exact, but unreadable in reviews of checked-in parks.

Sources: https://en.cppreference.com/w/cpp/utility/to_chars — the shortest round-trip guarantee and the fixed-or-scientific rule; https://gcc.gnu.org/pipermail/libstdc++/2020-July/050604.html — libstdc++'s Ryu-based floating-point to_chars; https://gcc.gnu.org/pipermail/libstdc++/2020-July/050633.html — libstdc++'s floating-point from_chars.

## What makes a text format both canonical and strict?

A re-save is byte-identical when the writer has one output for each world: a fixed order of header lines, sections in registration order, entities in key order, fields in visitFields order, one space between tokens, LF line ends, and a final newline. The reader can be strict in the same places and refuse anything the writer would not have written, which makes every accepted file canonical and every error attributable to one line. Strictness costs hand-editing convenience: a hand-edited file must match the writer's field order. Tolerating blank lines, and a carriage return before each line feed, costs nothing in canonicity, since the writer never emits them, and lets files survive editors and Git's line-ending conversion on Windows.

Nested values need a syntax when a field is a vector or a struct. Two families exist. Flattened keys, as in Java properties or INI files, spell every leaf with a dotted path and an index (points.0.x=1), which keeps lines flat but repeats names and needs a separate length. Inline brackets, as in TOML's inline tables and arrays, write a struct as braces around its fields and a vector as brackets around its elements, which keeps a line close to the data's shape and needs a small recursive reader.

## How does a strict reader never crash on malformed input?

Every read goes through a tokenizer that checks bounds before each access and a number parser that checks from_chars's result, so malformed input becomes an error value rather than undefined behavior. The reader never indexes by a value read from the file without checking it, never reserves memory sized by an unchecked count, and never recurses deeper than the field types' own nesting, which is fixed at compile time. Errors carry the line number where they were found.
