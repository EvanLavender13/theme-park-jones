#include "app/ui/snap_guides.h"

#include "render/graph_overlay.h"

#include <imgui.h>

#include <math.h>
#include <optional>
#include <span>
#include <stdio.h>

namespace tpj {
namespace {

// In window pixels.
constexpr float RING_RADIUS = 9.0f;
constexpr float THIN_LINE = 2.0f;
constexpr float SEGMENT_LINE = 3.0f;
constexpr float DASH = 8.0f;
constexpr float DASH_GAP = 6.0f;
constexpr float DIRECTION_OVERRUN = 80.0f;
constexpr float TICK_LENGTH = 8.0f;
constexpr float LABEL_RISE = 18.0f;
constexpr float LABEL_PADDING = 3.0f;
// A Length guide draws at most this many ticks.
constexpr int MAX_TICKS = 1000;

const ImU32 POINT_GUIDE_COLOR = IM_COL32(38, 217, 242, 255);
const ImU32 SEGMENT_GUIDE_COLOR = IM_COL32(77, 242, 102, 255);
const ImU32 DIRECTION_GUIDE_COLOR = IM_COL32(255, 89, 217, 255);
const ImU32 LENGTH_GUIDE_COLOR = IM_COL32(255, 217, 51, 255);
const ImU32 LABEL_BACK_COLOR = IM_COL32(0, 0, 0, 153);
const ImU32 LABEL_TEXT_COLOR = IM_COL32(255, 255, 255, 255);

// Where the ground position lies in the window, or none when it is behind the eye.
std::optional<ImVec2> onScreen(const CameraView &view, ParkPoint at) {
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  const std::optional<WindowPoint> point =
      windowPoint(view, size.x, size.y, GroundPoint{at.X, at.Z});
  if (!point) {
    return std::nullopt;
  }
  return ImVec2(point->X, point->Y);
}

// The unit direction from one window position to another, or none when they coincide.
std::optional<ImVec2> screenDirection(ImVec2 from, ImVec2 to) {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float length = sqrtf(dx * dx + dy * dy);
  if (!(length > 0.5f)) {
    return std::nullopt;
  }
  return ImVec2(dx / length, dy / length);
}

// A dashed line from one window position through another and DIRECTION_OVERRUN beyond.
void drawDirection(ImDrawList *list, ImVec2 from, ImVec2 to) {
  const std::optional<ImVec2> direction = screenDirection(from, to);
  if (!direction) {
    return;
  }
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float length = sqrtf(dx * dx + dy * dy) + DIRECTION_OVERRUN;
  const int dashes = static_cast<int>(ceilf(length / (DASH + DASH_GAP)));
  for (int dash = 0; dash < dashes; ++dash) {
    const float start = static_cast<float>(dash) * (DASH + DASH_GAP);
    const float end = fminf(start + DASH, length);
    list->AddLine(ImVec2(from.x + direction->x * start, from.y + direction->y * start),
                  ImVec2(from.x + direction->x * end, from.y + direction->y * end),
                  DIRECTION_GUIDE_COLOR, THIN_LINE);
  }
}

// A tick across the line at each whole meter from the guide's anchor to its stop.
void drawTicks(ImDrawList *list, const CameraView &view, const SnapGuide &guide) {
  const double dx = guide.To.X - guide.From.X;
  const double dz = guide.To.Z - guide.From.Z;
  const double meters = sqrt(dx * dx + dz * dz);
  if (!(meters >= 1.0)) {
    return;
  }
  const int count = static_cast<int>(fmin(floor(meters + 1e-9), MAX_TICKS));
  for (int m = 1; m <= count; ++m) {
    const double t = m / meters;
    const std::optional<ImVec2> at = onScreen(view, {guide.From.X + dx * t, guide.From.Z + dz * t});
    const std::optional<ImVec2> from = onScreen(view, guide.From);
    if (!at || !from) {
      continue;
    }
    const std::optional<ImVec2> along = screenDirection(*from, *at);
    if (!along) {
      continue;
    }
    const ImVec2 across(-along->y * TICK_LENGTH * 0.5f, along->x * TICK_LENGTH * 0.5f);
    list->AddLine(ImVec2(at->x - across.x, at->y - across.y),
                  ImVec2(at->x + across.x, at->y + across.y), LENGTH_GUIDE_COLOR, THIN_LINE);
  }
}

void drawGuide(ImDrawList *list, const CameraView &view, const SnapGuide &guide) {
  const std::optional<ImVec2> from = onScreen(view, guide.From);
  const std::optional<ImVec2> to = onScreen(view, guide.To);
  switch (guide.Kind) {
  case GuideKind::Point:
    if (to) {
      list->AddCircle(*to, RING_RADIUS, POINT_GUIDE_COLOR, 0, THIN_LINE);
    }
    break;
  case GuideKind::Door:
    if (from && to) {
      list->AddLine(*from, *to, POINT_GUIDE_COLOR, THIN_LINE);
    }
    if (to) {
      list->AddCircle(*to, RING_RADIUS, POINT_GUIDE_COLOR, 0, THIN_LINE);
    }
    break;
  case GuideKind::Segment:
    if (from && to) {
      list->AddLine(*from, *to, SEGMENT_GUIDE_COLOR, SEGMENT_LINE);
    }
    break;
  case GuideKind::Direction:
    if (from && to) {
      drawDirection(list, *from, *to);
    }
    break;
  case GuideKind::Length:
    drawTicks(list, view, guide);
    break;
  }
}

// The length between the ends, in meters to two decimals, above the segment's middle.
void drawLength(ImDrawList *list, const CameraView &view, const MeasuredLength &length) {
  const std::optional<ImVec2> middle =
      onScreen(view, {0.5 * (length.From.X + length.To.X), 0.5 * (length.From.Z + length.To.Z)});
  if (!middle) {
    return;
  }
  const double dx = length.To.X - length.From.X;
  const double dz = length.To.Z - length.From.Z;
  char text[32];
  (void)snprintf(text, sizeof(text), "%.2f m", sqrt(dx * dx + dz * dz));
  const ImVec2 size = ImGui::CalcTextSize(text);
  const ImVec2 corner(middle->x - 0.5f * size.x, middle->y - LABEL_RISE - size.y);
  list->AddRectFilled(ImVec2(corner.x - LABEL_PADDING, corner.y - LABEL_PADDING),
                      ImVec2(corner.x + size.x + LABEL_PADDING, corner.y + size.y + LABEL_PADDING),
                      LABEL_BACK_COLOR);
  list->AddText(corner, LABEL_TEXT_COLOR, text);
}

} // namespace

void drawSnapGuides(std::span<const SnapGuide> guides, std::span<const MeasuredLength> lengths,
                    const CameraView &view) {
  ImDrawList *list = ImGui::GetBackgroundDrawList();
  for (const SnapGuide &guide : guides) {
    drawGuide(list, view, guide);
  }
  for (const MeasuredLength &length : lengths) {
    drawLength(list, view, length);
  }
}

} // namespace tpj
