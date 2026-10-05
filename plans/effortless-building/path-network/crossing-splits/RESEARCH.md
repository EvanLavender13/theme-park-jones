# Research: crossing-splits

## How should two straight segments be tested for meeting, so every case has one stated outcome?

The milestone's research settles the outline: find each crossing of a new segment with segments of its kind, reuse an end that lies within a small tolerance, sort the crossings along each crossed segment, and split each segment once into all its parts (../RESEARCH.md). This question is about the test for one pair of segments.

The textbook test for two segments crossing compares orientations. Each segment's two ends must lie strictly on opposite sides of the line through the other. The crossing's place along the first segment is then the cross product of the second's direction with the gap between their starts, divided by the cross product of the two directions. O'Rourke's SegSegInt uses this, and it splits out the cases where an end lies exactly on the other segment or the two are collinear. Those exact cases matter only with exact arithmetic. With doubles they show up as near misses, so each one needs a tolerance and a rule.

The distance from a point to a segment stays accurate when the line solve does not, as JOSM's code notes. Testing ends first by that distance, and only then testing for a proper crossing, gives three exclusive cases:

- Each end of each segment lies within the tolerance of the line through the other. The segments lie along one line. The slice keeps such overlaps as separate paving, so they don't meet.
- An end of one segment lies within the tolerance of the other segment, away from the other's ends. The other segment splits at that end's point. If ends of both lie within the tolerance of each other, the segments meet end to end, and nothing splits.
- Otherwise, if each segment's ends lie on opposite sides of the other's line, the segments cross. Because no end lies within the tolerance of the other segment, the crossing lies more than the tolerance from every end. Each part a split makes is then at least the tolerance long.

The tolerance must be at least MIN_SEGMENT_LENGTH, so a single split never makes a part too short to have a ground line. It should be small enough that the bend it allows is invisible, because the ghost draws the path straight between its stops. The tools already snap within 2 m, so the tolerance only absorbs rounding in commands that tools and scripts build. MIN_SEGMENT_LENGTH itself, 1 cm, does both, and the tools already treat a place within it of a segment's end as that end.

The simulation's doubles round the same on every build (decision 0022), so no robust predicates or exact arithmetic are needed for the result to be deterministic.

Rejected: Shewchuk's adaptive exact predicates — they give exact signs, but determinism already holds, and the tolerance cases still need rules. Reusing an existing point within iD's 0.75 m — the path would bend visibly away from the straight ghost. Merging crossings closer than the tolerance on one segment — it needs a rule for which point wins and where the other segments go; three segments meeting at one spot without a shared point is rare, and the command is refused there because a part is too short.

Sources: https://www.science.smith.edu/~jorourke/books/compgeom.html — SegSegInt's orientation test and its exact cases; https://www.cs.cmu.edu/~quake/robust.html — exact predicates and why floating-point orientation fails; https://josm.openstreetmap.de/export/HEAD/josm/trunk/src/org/openstreetmap/josm/tools/Geometry.java — near-parallel handling by distance.
