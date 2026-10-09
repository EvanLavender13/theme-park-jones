#ifndef TPJ_APP_INPUT_INPUT_MAP_H
#define TPJ_APP_INPUT_INPUT_MAP_H

#include "app/input/game_speed.h"
#include "app/input/orbit_camera.h"

#include <span>
#include <stdint.h>

union SDL_Event;

namespace tpj {

// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// What the player asked of the edit history this frame.
enum class HistoryAction : uint8_t { None, Undo, Redo };

// The input gathered over one frame: the camera's, the left button's, and whether the player asked
// to quit.
struct FrameInput {
  CameraInput Camera;
  PointerButtons Buttons;
  // Whether the player holds the key that turns snapping off.
  bool SnapsOff = false;
  // Whether the player pressed Ctrl+Z to undo, or Ctrl+Y or Ctrl+Shift+Z to redo.
  HistoryAction History = HistoryAction::None;
  // What the player asked of the game's speed with Space, 1, 2, or 3.
  SpeedAction Speed = SpeedAction::None;
  bool Quit = false;
};

// Adds one event to the frame's input. A quit event asks to quit, and a left button release is
// always recorded. While ImGui doesn't want the keyboard, a press of Z with Ctrl held asks to
// undo, or to redo with Shift held too, and a press of Y with Ctrl held asks to redo. Key repeats
// count. While ImGui doesn't want the keyboard, a press of Space with no Ctrl held asks to pause
// or unpause, and a press of 1, 2, or 3 asks to run at 1x, 2x, or 4x. Their repeats don't count.
// While ImGui wants the mouse no other mouse event reaches the input. Otherwise a left button press
// is recorded, motion adds its relative pixels to the orbit deltas with the right button held, or
// else to the pan deltas with the middle button held, and a wheel event adds its vertical steps to
// the zoom.
void mapEvent(const SDL_Event &event, bool mouseWanted, bool keyboardWanted, FrameInput &input);
// Sets the camera's move and rotate axes from the keys held, indexed by scancode: W minus S
// forward, D minus A right, and Q minus E rotate, a held key counting 1. A scancode past the end of
// the keys counts as not held. While ImGui wants the keyboard it changes nothing.
void mapKeys(std::span<const bool> keys, bool keyboardWanted, CameraInput &camera);
// Whether the keys held, indexed by scancode, turn snapping off: either Ctrl key held, while ImGui
// doesn't want the keyboard.
bool snapsOff(std::span<const bool> keys, bool keyboardWanted);

} // namespace tpj

#endif
