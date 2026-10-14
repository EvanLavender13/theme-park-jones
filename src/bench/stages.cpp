#include "bench/stages.h"

#include "legible/needs.h"
#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "render/paving_overlay.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "views/availability_overlay.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
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

// A stage that builds a mesh from the resolved park into lists kept across repetitions, as the app
// keeps them, with the mesh's vertex count as its result.
template <typename Lists, typename Build>
StageResult meshStage(std::string name, const World &park, Lists &lists, Build build) {
  const auto mesh = timeRepeated(
      nothing, [&park, &lists, &build](Nothing /*input*/) { return build(park, lists).size(); });
  return StageResult{std::move(name), mesh.Times, ResultKind::Vertices, mesh.Last};
}

// The ghost's vertex count, over its tinted, lit, and marked meshes.
uint64_t ghostVertices(const GhostMesh &ghost) {
  return ghost.Tints.Vertices.size() + ghost.Lit.Vertices.size() + ghost.Marks.Vertices.size();
}

// The guest segment the path stages draw from, and the side they draw toward: the segment's key,
// its line's middle, the unit normal toward the accepted side, and its half width.
struct SpurChoice {
  EntityKey Segment = NULL_KEY;
  ParkPoint Middle;
  ParkPoint Normal;
  double Half = 0.0;
};

// The guest path from the choice's segment to the point the distance beyond its side.
AddPath spurPath(const SpurChoice &spur, double distance) {
  const double reach = spur.Half + distance;
  return AddPath{PathKind::Guest,
                 {OnSegment{spur.Segment, 0.5}, NewPoint{{spur.Middle.X + spur.Normal.X * reach,
                                                          spur.Middle.Z + spur.Normal.Z * reach}}}};
}

// The segment and side of the path edit the path stages preview: a guest path from the middle of a
// guest segment to a place PATH_SPUR_LENGTH beyond its side, the first the world accepts, trying
// segments in ascending key order and, on each, the side a quarter turn counter-clockwise from its
// direction before the other. None when the park has no guest segment with a ground line. Throws
// std::runtime_error when it has one but the world accepts no such path.
std::optional<SpurChoice> spurChoice(const World &park) {
  bool tried = false;
  for (const ParkSegment &segment : parkSegments(park)) {
    if (segment.Kind != PathKind::Guest || segment.Line.size() < 2) {
      continue;
    }
    tried = true;
    const CarrierPoint &first = segment.Line.front();
    const CarrierPoint &last = segment.Line.back();
    const double length = std::hypot(last.X - first.X, last.Z - first.Z);
    const double normalX = -(last.Z - first.Z) / length;
    const double normalZ = (last.X - first.X) / length;
    const ParkPoint middle{0.5 * (first.X + last.X), 0.5 * (first.Z + last.Z)};
    for (const double side : {1.0, -1.0}) {
      const SpurChoice spur{
          segment.Key, middle, {normalX * side, normalZ * side}, 0.5 * segment.Width};
      if (isAccepted(park, ParkEdit{spurPath(spur, PATH_SPUR_LENGTH)})) {
        return spur;
      }
    }
  }
  if (tried) {
    throw std::runtime_error("no guest segment of the park accepts a path from its side");
  }
  return std::nullopt;
}

// The preview of the edit on the park and the edit's ghost with its candidate, timed as two stages
// named for the edit, with the candidate's hash and the ghost's vertex count as their results.
// Throws std::runtime_error with the message when the preview has no candidate.
void previewStages(std::vector<StageResult> &stages, const World &park, const ParkEdit &edit,
                   const std::string &prefix, const std::string &refused) {
  const std::optional<ParkEdit> held = edit;
  const auto preview =
      timeRepeated(nothing, [&park, &held](Nothing /*input*/) { return previewEdit(park, held); });
  const std::optional<World> &candidate = preview.Last.Candidate;
  if (!candidate) {
    throw std::runtime_error(refused);
  }
  stages.push_back(
      StageResult{prefix + "preview", preview.Times, ResultKind::Hash, hashWorld(*candidate)});
  const auto ghost = timeRepeated(nothing, [&park, &edit, &candidate](Nothing /*input*/) {
    return buildGhostMesh(park, edit, candidate);
  });
  stages.push_back(
      StageResult{prefix + "ghost", ghost.Times, ResultKind::Vertices, ghostVertices(ghost.Last)});
}

// Times a drag through a kept preview, on a copy of the park that steps one tick before each of
// WARM_UPS + REPETITIONS frames. Each frame times keepPreview with editAt(frame) and the ghost of
// what it made, and only the frames after the warm-ups count. The result is the last candidate's
// hash. Throws std::runtime_error with the message, naming the frame, when the world refuses a
// frame's edit.
template <typename EditAt>
StageResult timeDrag(std::string name, const World &park, EditAt editAt,
                     const std::string &refused) {
  World stepped = copyWorld(park);
  TickBuffers buffers;
  KeptPreview kept;
  std::vector<int64_t> times;
  times.reserve(REPETITIONS);
  for (size_t frame = 0; frame < WARM_UPS + REPETITIONS; ++frame) {
    stepWorld(stepped, buffers);
    const ParkEdit edit = editAt(frame);
    if (!isAccepted(stepped, edit)) {
      throw std::runtime_error(refused + " at frame " + std::to_string(frame));
    }
    const std::optional<ParkEdit> held = edit;
    const Clock::time_point start = Clock::now();
    keepPreview(kept, stepped, held, false);
    if (kept.Made.Candidate) {
      const GhostMesh ghost = buildGhostMesh(stepped, edit, kept.Made.Candidate);
    }
    const Clock::time_point end = Clock::now();
    if (frame >= WARM_UPS) {
      times.push_back(nanosecondsBetween(start, end));
    }
  }
  if (!kept.Made.Candidate) {
    throw std::runtime_error(refused + ": the last frame has no candidate");
  }
  return StageResult{std::move(name), summarizeTimes(times), ResultKind::Hash,
                     hashWorld(*kept.Made.Candidate)};
}

// The shop drag: the shop moved along its face, its facing kept, DRAG_STEP further each frame out
// to DRAG_SPAN and back. A pose's facing need not be a unit vector, so the direction is
// normalized.
[[gnu::noinline]] StageResult timeShopDrag(const World &park, const ParkBox &shop) {
  const double facing = std::hypot(shop.At.FacingX, shop.At.FacingZ);
  const ParkPoint along{-shop.At.FacingZ / facing, shop.At.FacingX / facing};
  const auto steps = static_cast<size_t>(DRAG_SPAN / DRAG_STEP);
  return timeDrag(
      "shop-drag", park,
      [&shop, along, steps](size_t frame) {
        const size_t phase = (frame + 1) % (2 * steps);
        const size_t out = phase <= steps ? phase : 2 * steps - phase;
        const double offset = DRAG_STEP * static_cast<double>(out);
        Pose pose = shop.At;
        pose.X += along.X * offset;
        pose.Z += along.Z * offset;
        return ParkEdit{MoveBox{shop.Key, pose}};
      },
      "moving shop " + std::to_string(static_cast<uint64_t>(shop.Key)) +
          " along its face is refused");
}

// The path drag: a guest path from the middle of the choice's segment, its end starting
// DRAG_START_REACH beyond the segment's side and moving DRAG_STEP further out each frame.
[[gnu::noinline]] StageResult timePathDrag(const World &park, const SpurChoice &spur) {
  return timeDrag(
      "path-drag", park,
      [&spur](size_t frame) {
        return ParkEdit{spurPath(spur, DRAG_START_REACH + DRAG_STEP * static_cast<double>(frame))};
      },
      "a path from segment " + std::to_string(static_cast<uint64_t>(spur.Segment)) + " is refused");
}

// The stages of edits on the resolved park: the preview and ghost of a box left where it stands,
// the path edit's acceptance check, preview, and ghost, and the two drags.
void editStages(std::vector<StageResult> &stages, const World &park) {
  const std::vector<ParkBox> boxes = parkBoxes(park);
  if (!boxes.empty()) {
    previewStages(stages, park, ParkEdit{MoveBox{boxes.front().Key, boxes.front().At}}, "",
                  "moving box " + std::to_string(static_cast<uint64_t>(boxes.front().Key)) +
                      " to its own pose is refused, so the park has no preview");
  }

  const std::optional<SpurChoice> spur = spurChoice(park);
  if (spur) {
    const ParkEdit edit{spurPath(*spur, PATH_SPUR_LENGTH)};
    const auto acceptance = timeRepeated(nothing, [&park, &edit](Nothing /*input*/) {
      return static_cast<uint64_t>(isAccepted(park, edit) ? 1 : 0);
    });
    stages.push_back(
        StageResult{"path-acceptance", acceptance.Times, ResultKind::Values, acceptance.Last});
    previewStages(stages, park, edit, "path-",
                  "the accepted path from a guest segment's side has no preview");
  }

  const auto shop = std::ranges::find(boxes, BoxKind::Shop, &ParkBox::Kind);
  if (shop != boxes.end()) {
    stages.push_back(timeShopDrag(park, *shop));
  }
  if (spur) {
    stages.push_back(timePathDrag(park, *spur));
  }
}

} // namespace

std::vector<StageResult> benchPark(const World &loaded, uint64_t ticks) {
  if (ticks == 0) {
    throw std::invalid_argument("benchPark: ticks must be positive");
  }
  std::vector<StageResult> stages;

  // A resolution after an edit runs on a park already resolved, so the stage resolves one again,
  // with buffers kept across its repetitions, as the app keeps its own.
  TickBuffers resolveBuffers;
  World resolved = copyWorld(loaded);
  resolveWorld(resolved, resolveBuffers);
  const auto resolution = timeRepeated(nothing, [&resolved, &resolveBuffers](Nothing /*input*/) {
    resolveWorld(resolved, resolveBuffers);
    return Nothing{};
  });
  const World &park = resolved;
  stages.push_back(StageResult{"resolution", resolution.Times, ResultKind::Hash, hashWorld(park)});

  editStages(stages, park);

  OverlayShapeLists shapeLists;
  stages.push_back(meshStage("food-overlay-shape", park, shapeLists,
                             [](const World &world, OverlayShapeLists &lists) -> const auto & {
                               buildOverlayShape(world, lists);
                               return lists.Shape.Vertices;
                             }));
  // What a tick costs while the overlay is shown: the shape is built only when the paths change,
  // and the shades fill lists kept from one tick to the next, as the app's are.
  const OverlayShape &shape = shapeLists.Shape;
  AvailabilityShadeLists shadeLists;
  const auto shades = timeRepeated(nothing, [&park, &shape, &shadeLists](Nothing /*input*/) {
    availabilityShades(park, shape, needAvailability(GuestNeed::Hunger), shadeLists);
    return shadeLists.Shades.Knots.size();
  });
  stages.push_back(StageResult{"food-overlay", shades.Times, ResultKind::Values, shades.Last});
  ParkMeshLists parkLists;
  stages.push_back(meshStage("park-mesh", park, parkLists,
                             [](const World &world, ParkMeshLists &lists) -> const auto & {
                               buildParkMesh(world, lists);
                               return lists.Mesh.Vertices;
                             }));
  // What a tick costs to draw the guests, with the seats and the mesh kept from one tick to the
  // next, as the app keeps them.
  SeatedGuests seated;
  GuestMeshLists guestLists;
  const auto guests = timeRepeated(nothing, [&park, &seated, &guestLists](Nothing /*input*/) {
    seated.update(park);
    buildGuestMesh(park, seated, guestLists);
    return guestLists.Mesh.Vertices.size();
  });
  stages.push_back(StageResult{"guest-mesh", guests.Times, ResultKind::Vertices, guests.Last});

  World stepped = copyWorld(park);
  std::vector<int64_t> tickTimes;
  tickTimes.reserve(ticks);
  TickBuffers buffers;
  for (uint64_t tick = 0; tick < ticks; ++tick) {
    const Clock::time_point start = Clock::now();
    stepWorld(stepped, buffers);
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
  case ResultKind::Values:
    result = " values " + std::to_string(stage.Result);
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
