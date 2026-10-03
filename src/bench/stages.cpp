#include "bench/stages.h"

#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "views/food_overlay.h"

#include <array>
#include <charconv>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <utility>

namespace tpj {
namespace {

using Clock = std::chrono::steady_clock;

int64_t nanosecondsBetween(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// Exactly 16 lowercase hexadecimal digits.
std::string hex16(uint64_t value) {
  std::array<char, 16> digits{};
  const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value, 16);
  const auto length = static_cast<size_t>(result.ptr - digits.data());
  return std::string(16 - length, '0') + std::string(digits.data(), length);
}

// A repeated stage's times and the output of its last call.
template <typename Output> struct Timed {
  TimeSummary Times;
  Output Last;
};

static_assert(WARM_UPS >= 1, "timeRepeated keeps the first warm-up's output");

// Makes WARM_UPS untimed calls and then REPETITIONS timed ones of work on what prepare gives,
// which is made before the clock is read. The first warm-up's output starts the kept one, and
// each later call's output replaces it after the clock is read, so freeing the one before is
// never timed.
template <typename Prepare, typename Work> auto timeRepeated(Prepare prepare, Work work) {
  auto firstInput = prepare();
  auto last = work(firstInput);
  std::vector<int64_t> times;
  times.reserve(REPETITIONS);
  for (size_t call = 1; call < WARM_UPS + REPETITIONS; ++call) {
    auto input = prepare();
    const Clock::time_point start = Clock::now();
    auto output = work(input);
    const Clock::time_point end = Clock::now();
    if (call >= WARM_UPS) {
      times.push_back(nanosecondsBetween(start, end));
    }
    last = std::move(output);
  }
  return Timed<decltype(last)>{summarizeTimes(times), std::move(last)};
}

struct Nothing {};

Nothing nothing() { return {}; }

// A stage that builds a mesh from the resolved park, with its vertex count as its result.
template <typename Build> StageResult meshStage(std::string name, const World &park, Build build) {
  const auto mesh =
      timeRepeated(nothing, [&park, &build](Nothing /*input*/) { return build(park); });
  return StageResult{std::move(name), mesh.Times, ResultKind::Vertices, mesh.Last.Vertices.size()};
}

} // namespace

std::vector<StageResult> benchPark(const World &loaded, uint64_t ticks) {
  if (ticks == 0) {
    throw std::invalid_argument("benchPark: ticks must be positive");
  }
  std::vector<StageResult> stages;

  const auto resolution = timeRepeated([&loaded] { return copyWorld(loaded); },
                                       [](World &copy) {
                                         resolveWorld(copy);
                                         return std::move(copy);
                                       });
  const World &park = resolution.Last;
  stages.push_back(StageResult{"resolution", resolution.Times, ResultKind::Hash, hashWorld(park)});

  const std::vector<ParkBox> boxes = parkBoxes(park);
  if (!boxes.empty()) {
    const std::optional<ParkEdit> edit = ParkEdit{MoveBox{boxes.front().Key, boxes.front().At}};
    const auto preview = timeRepeated(
        nothing, [&park, &edit](Nothing /*input*/) { return previewEdit(park, edit); });
    const std::optional<World> &candidate = preview.Last.Candidate;
    if (!candidate) {
      throw std::runtime_error("moving box " +
                               std::to_string(static_cast<uint64_t>(boxes.front().Key)) +
                               " to its own pose is refused, so the park has no preview");
    }
    stages.push_back(
        StageResult{"preview", preview.Times, ResultKind::Hash, hashWorld(*candidate)});
  }

  stages.push_back(meshStage("food-overlay", park, buildFoodAvailabilityOverlay));
  stages.push_back(meshStage("park-mesh", park, buildParkMesh));
  stages.push_back(meshStage("guest-mesh", park, buildGuestMesh));

  World stepped = copyWorld(park);
  std::vector<int64_t> tickTimes;
  tickTimes.reserve(ticks);
  for (uint64_t tick = 0; tick < ticks; ++tick) {
    const Clock::time_point start = Clock::now();
    stepWorld(stepped);
    const Clock::time_point end = Clock::now();
    tickTimes.push_back(nanosecondsBetween(start, end));
  }
  stages.push_back(
      StageResult{"ticks", summarizeTimes(tickTimes), ResultKind::Hash, hashWorld(stepped)});
  return stages;
}

std::string stageLine(const StageResult &stage) {
  std::string result;
  switch (stage.Kind) {
  case ResultKind::Hash:
    result = " hash " + hex16(stage.Result);
    break;
  case ResultKind::Vertices:
    result = " vertices " + std::to_string(stage.Result);
    break;
  case ResultKind::Frames:
    result = " frames " + std::to_string(stage.Result);
    break;
  }
  return "stage " + stage.Name + " count " + std::to_string(stage.Times.Count) + " median " +
         std::to_string(stage.Times.Median) + " least " + std::to_string(stage.Times.Least) +
         " greatest " + std::to_string(stage.Times.Greatest) + result;
}

std::string parkLine(std::string_view path, uint64_t ticks) {
  return "park " + std::string(path) + " ticks " + std::to_string(ticks) + " warm-ups " +
         std::to_string(WARM_UPS) + " repetitions " + std::to_string(REPETITIONS);
}

} // namespace tpj
