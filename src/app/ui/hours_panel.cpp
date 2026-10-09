#include "app/ui/hours_panel.h"

#include "sim/clock.h"

#include <imgui.h>

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

} // namespace

HoursUse drawHoursPanel(const HoursControls &hours) {
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
  }
  ImGui::End();
  return use;
}

} // namespace tpj
