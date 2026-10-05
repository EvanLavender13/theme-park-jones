#ifndef TPJ_SIM_PARK_INTERNAL_MEETINGS_H
#define TPJ_SIM_PARK_INTERNAL_MEETINGS_H

#include "sim/park/intent.h"

#include <optional>
#include <stddef.h>
#include <stdint.h>

namespace tpj {

// How two straight segments, A and B, meet. Each runs from its first end to its second.
enum class MeetingKind : uint8_t {
  // An end of A lies on B, away from B's ends, so B splits at that end's point.
  AEndOnB,
  // An end of B lies on A, away from A's ends, so A splits at that end's point.
  BEndOnA,
  // They cross away from all four ends, so both split at a new point there.
  Crossing
};

struct Meeting {
  MeetingKind Kind = MeetingKind::Crossing;
  // For AEndOnB and BEndOnA, the end that lies on the other segment: 0 for its first, 1 for its
  // second.
  size_t End = 0;
  // Where the segments meet along each, as a fraction from its first end to its second.
  double AlongA = 0.0;
  double AlongB = 0.0;
  // Where they meet: the end's own position, or the crossing.
  ParkPoint At;
};

// How two segments meet, each with two ends at least MIN_SEGMENT_LENGTH apart. None when each end
// of each lies within MIN_SEGMENT_LENGTH of the line through the other, when an end of each lies
// within MIN_SEGMENT_LENGTH of an end of the other, or when they don't meet.
std::optional<Meeting> meetingOf(ParkPoint a0, ParkPoint a1, ParkPoint b0, ParkPoint b1);

} // namespace tpj

#endif
