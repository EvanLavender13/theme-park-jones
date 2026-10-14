#ifndef TPJ_BENCH_STAGES_H
#define TPJ_BENCH_STAGES_H

#include "bench/timing.h"
#include "sim/world.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <vector>

namespace tpj {

// The ticks tpj_bench steps without --ticks: two minutes of game time.
inline constexpr uint64_t DEFAULT_TICKS = 3600;
// The untimed calls a repeated stage makes before its counted ones.
inline constexpr size_t WARM_UPS = 2;
// The counted calls of every stage but ticks.
inline constexpr size_t REPETITIONS = 11;
// How far beyond a guest segment's side the path edit's stages draw their path, in metres.
inline constexpr double PATH_SPUR_LENGTH = 12.0;
// How far the drag stages move their edit each frame, in metres.
inline constexpr double DRAG_STEP = 0.5;
// The farthest the shop drag moves its shop from where it stands, in metres.
inline constexpr double DRAG_SPAN = 2.0;
// How far beyond its segment's side the path drag's end starts, in metres.
inline constexpr double DRAG_START_REACH = 4.0;

// What a stage's result is: a world's hash, a mesh's vertex count, a count of values, or a count
// of frames.
enum class ResultKind { Hash, Vertices, Values, Frames };

// One stage's times and the result of its work.
struct StageResult {
  std::string Name;
  TimeSummary Times;
  ResultKind Kind = ResultKind::Hash;
  uint64_t Result = 0;

  bool operator==(const StageResult &) const = default;
};

// Times resolution, preview and ghost (only when the resolved park holds a box), path-acceptance,
// path-preview, and path-ghost (only when it holds a guest segment with a ground line), shop-drag
// (only when it holds a shop), path-drag (only when it holds a guest segment with a ground line),
// food-overlay-shape, food-overlay, park-mesh, guest-mesh, and ticks, in that order, on a loaded
// world, which it leaves unchanged. Throws std::invalid_argument when ticks is 0,
// std::runtime_error naming the box when the preview of moving the lowest-keyed box to its own pose
// has no candidate, std::runtime_error when the park has a guest segment with a ground line but
// the world accepts no path from any such segment's side, and std::runtime_error naming the shop
// or the segment when the world refuses a drag frame's edit.
std::vector<StageResult> benchPark(const World &loaded, uint64_t ticks);

// stage <name> count <c> median <m> least <l> greatest <g> <hash|vertices|values|frames> <result>,
// with a hash as 16 lowercase hexadecimal digits and a count in decimal, and no line feed.
std::string stageLine(const StageResult &stage);

// park <path> ticks <t> warm-ups <w> repetitions <r>, with no line feed.
std::string parkLine(std::string_view path, uint64_t ticks);

} // namespace tpj

#endif
