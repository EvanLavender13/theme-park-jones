#ifndef TPJ_APP_FRAME_TIMES_H
#define TPJ_APP_FRAME_TIMES_H

#include <span>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace tpj {

// Each frame's cost, given as each frame starts: a frame's advance gives the time since the
// previous frame's, so it is the previous frame's cost. The first value covers no frame and the
// second the first frame, which follows loading and does one-off work, so both are dropped.
class FrameTimes {
public:
  // Gives the Nanoseconds of the advance at a frame's start.
  void add(int64_t nanoseconds);

  // The values kept: every one given after the first two, in order.
  std::span<const int64_t> kept() const;

  // frame-times, then each kept value in decimal after a single space, with no line feed.
  std::string line() const;

private:
  size_t Given = 0;
  std::vector<int64_t> Kept;
};

} // namespace tpj

#endif
