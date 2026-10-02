#ifndef TPJ_APP_INPUT_INPUT_MAP_H
#define TPJ_APP_INPUT_INPUT_MAP_H

#include "app/input/orbit_camera.h"

#include <span>

union SDL_Event;

namespace tpj {

// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// The input gathered over one frame: the camera's, the left button's, and whether the player asked
// to quit.
struct FrameInput {
  CameraInput Camera;
  PointerButtons Buttons;
  bool Quit = false;
};

// Adds one event to the frame's input. A quit event asks to quit, and a left button release is
// always recorded. While ImGui wants the mouse nothing else reaches the input. Otherwise a left
// button press is recorded, motion adds its relative pixels to the orbit deltas with the right
// button held, or else to the pan deltas with the middle button held, and a wheel event adds its
// vertical steps to the zoom.
void mapEvent(const SDL_Event &event, bool mouseWanted, FrameInput &input);
// Sets the camera's move and rotate axes from the keys held, indexed by scancode: W minus S
// forward, D minus A right, and Q minus E rotate, a held key counting 1. A scancode past the end of
// the keys counts as not held. While ImGui wants the keyboard it changes nothing.
void mapKeys(std::span<const bool> keys, bool keyboardWanted, CameraInput &camera);

} // namespace tpj

#endif
