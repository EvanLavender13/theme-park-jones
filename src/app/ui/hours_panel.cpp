#include "app/ui/hours_panel.h"

#include "sim/clock.h"
#include "sim/guests/guests.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <stddef.h>
#include <stdint.h>

namespace tpj {
namespace {

inline constexpr uint64_t QUARTER_HOUR = 900;

// The quarter hour before the second, and the one after, so a second off the steps moves onto them.
uint64_t quarterBefore(uint64_t second) { return (second - 1) / QUARTER_HOUR * QUARTER_HOUR; }
uint64_t quarterAfter(uint64_t second) {
  return second / QUARTER_HOUR * QUARTER_HOUR + QUARTER_HOUR;
}

// A time of day on a 24-hour clock, such as 09:00, or 24:00 for DAY_SECONDS.
void drawTime(uint64_t second) {
  ImGui::Text("%02llu:%02llu", static_cast<unsigned long long>(second / 3600),
              static_cast<unsigned long long>(second % 3600 / 60));
}

// Draws a - button and a + button on the line, each disabled as asked, and gives -1 for a press of
// -, 1 for a press of +, and 0 otherwise.
int drawSteps(const char *id, bool lowerDisabled, bool raiseDisabled) {
  int step = 0;
  ImGui::PushID(id);
  ImGui::SameLine();
  ImGui::BeginDisabled(lowerDisabled);
  if (ImGui::Button("-")) {
    step = -1;
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(raiseDisabled);
  if (ImGui::Button("+")) {
    step = 1;
  }
  ImGui::EndDisabled();
  ImGui::PopID();
  return step;
}

// The arrival curve's area, in pixels.
inline constexpr float CURVE_WIDTH = 240.0f;
inline constexpr float CURVE_HEIGHT = 60.0f;

// The curve from 00:00 to 24:00 in a framed area, scaled so ARRIVAL_CURVE's peak reaches its top,
// with a vertical line at the second of the day, and the times below it.
void drawCurve(const ArrivalDay &curve, uint64_t second) {
  ImGui::TextUnformatted("Arrivals per hour at each entrance");
  ImGui::Dummy(ImVec2(CURVE_WIDTH, CURVE_HEIGHT));
  const ImVec2 low = ImGui::GetItemRectMin();
  const ImVec2 high = ImGui::GetItemRectMax();
  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(low, high, ImGui::GetColorU32(ImGuiCol_FrameBg));
  const double peak = std::ranges::max(ARRIVAL_CURVE, {}, &CurvePoint::Y).Y;
  // The last point, at 24:00, takes the 00:00 sample.
  std::array<ImVec2, ARRIVAL_SAMPLES + 1> points;
  for (size_t i = 0; i < points.size(); ++i) {
    const double rate = curve[i % ARRIVAL_SAMPLES];
    points[i] = ImVec2(low.x + CURVE_WIDTH * static_cast<float>(i) / ARRIVAL_SAMPLES,
                       high.y - CURVE_HEIGHT * static_cast<float>(rate / peak));
  }
  draw->AddPolyline(points.data(), static_cast<int>(points.size()),
                    ImGui::GetColorU32(ImGuiCol_PlotLines), ImDrawFlags_None, 1.5f);
  const float now = low.x + CURVE_WIDTH * static_cast<float>(second) / DAY_SECONDS;
  draw->AddLine(ImVec2(now, low.y), ImVec2(now, high.y),
                ImGui::GetColorU32(ImGuiCol_PlotLinesHovered));
  const float left = ImGui::GetCursorPosX();
  ImGui::TextUnformatted("00:00");
  ImGui::SameLine(left + 0.5f * (CURVE_WIDTH - ImGui::CalcTextSize("12:00").x));
  ImGui::TextUnformatted("12:00");
  ImGui::SameLine(left + CURVE_WIDTH - ImGui::CalcTextSize("24:00").x);
  ImGui::TextUnformatted("24:00");
}

} // namespace

HoursUse drawHoursPanel(const HoursControls &hours, const ArrivalDay &curve, uint64_t second) {
  HoursUse use;
  uint64_t opening = hours.Chosen.Opening;
  // A closing second of 0 is the end of the day while stepping and showing.
  uint64_t closing = hours.Chosen.Closing == 0 ? DAY_SECONDS : hours.Chosen.Closing;
  // At the top center, below the Time panel.
  ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 100.0f),
                          ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.0f));
  if (ImGui::Begin("Hours", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    bool stepped = false;
    ImGui::TextUnformatted("Opens ");
    ImGui::SameLine();
    drawTime(opening);
    const int opens = drawSteps("opens", opening == 0, quarterAfter(opening) >= DAY_SECONDS);
    if (opens < 0) {
      opening = quarterBefore(opening);
      stepped = true;
    } else if (opens > 0) {
      opening = quarterAfter(opening);
      if (closing <= opening) {
        closing = opening + QUARTER_HOUR;
      }
      stepped = true;
    }
    ImGui::TextUnformatted("Closes");
    ImGui::SameLine();
    drawTime(closing);
    const int closes = drawSteps("closes", quarterBefore(closing) < QUARTER_HOUR,
                                 quarterAfter(closing) > DAY_SECONDS);
    if (closes < 0) {
      closing = quarterBefore(closing);
      if (closing <= opening) {
        opening = closing - QUARTER_HOUR;
      }
      stepped = true;
    } else if (closes > 0) {
      closing = quarterAfter(closing);
      stepped = true;
    }
    if (stepped) {
      use.Chosen = OpeningHours{opening, closing % DAY_SECONDS};
    }
    ImGui::BeginDisabled(hours.Chosen == hours.Current);
    if (ImGui::Button("Apply")) {
      use.Apply = true;
    }
    ImGui::EndDisabled();
    drawCurve(curve, second);
  }
  ImGui::End();
  return use;
}

} // namespace tpj
