# Exceptions

List here anything that does not fit fields, flows, or encapsulation (principles 3 and 6), with the reason it cannot. An entry is a problem to remove later, not permission to break the rule. It names the code that breaks the rule and says why nothing cleaner works yet.

If the list keeps growing, a principle is wrong. When entries accumulate, raise it with the user instead of adding another entry.

Each entry gives:

- the location in code;
- the principle it breaks;
- the reason;
- what would remove it.

## Route distance shifts each source's entries on an update

- Location: src/sim/routes/route_distance.cpp, mergeWrites.
- Rule it breaks: docs/conventions.md's growth rule. Work in a changed frame must not grow with a product of counts.
- Reason: a field keeps each source's entries in one list in place order, because saves, hashes, and world comparisons walk them in that order. Writing a source's changed entries moves the rest of its list after the first change. On full.park that is every guest source times the nodes after the first change. Measured with Very Sleepy on windows-release, it is at most 0.6% of a shop-drag frame, about 0.03 ms (route-updates).
- What would remove it: a later changing-previews feature that keeps each source's entries in a fixed slot per node, and puts them in place order only when the world is walked.
