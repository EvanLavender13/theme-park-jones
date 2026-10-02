#ifndef TPJ_APP_INPUT_PLATFORM_INPUT_H
#define TPJ_APP_INPUT_PLATFORM_INPUT_H

#include "app/input/cursor.h"
#include "app/input/input_map.h"

#include <optional>

struct SDL_Window;

namespace tpj {

// Drains SDL's pending events, handing each to ImGui and then to mapEvent, and maps the held keys
// with mapKeys, each given whether ImGui wants the mouse or the keyboard.
FrameInput gatherInput();
// The cursor in the window, or none while ImGui wants the mouse or the window's size cannot be
// read.
std::optional<CursorNdc> readCursor(SDL_Window *window);

} // namespace tpj

#endif
