#include "app/input/input_map.h"

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_scancode.h>

#include <stddef.h>

namespace tpj {
namespace {

bool isHeld(std::span<const bool> keys, SDL_Scancode code) {
  const auto index = static_cast<size_t>(code);
  return index < keys.size() && keys[index];
}

float keyAxis(std::span<const bool> keys, SDL_Scancode positive, SDL_Scancode negative) {
  return (isHeld(keys, positive) ? 1.0f : 0.0f) - (isHeld(keys, negative) ? 1.0f : 0.0f);
}

void addMouseInput(const SDL_Event &event, CameraInput &camera) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    if ((event.motion.state & SDL_BUTTON_RMASK) != 0) {
      camera.OrbitDx += event.motion.xrel;
      camera.OrbitDy += event.motion.yrel;
    } else if ((event.motion.state & SDL_BUTTON_MMASK) != 0) {
      camera.PanDx += event.motion.xrel;
      camera.PanDy += event.motion.yrel;
    }
  } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    camera.Zoom += event.wheel.y;
  }
}

} // namespace

void mapEvent(const SDL_Event &event, bool mouseWanted, FrameInput &input) {
  if (event.type == SDL_EVENT_QUIT) {
    input.Quit = true;
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
    input.Buttons.Released = true;
  } else if (!mouseWanted) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
      input.Buttons.Pressed = true;
    }
    addMouseInput(event, input.Camera);
  }
}

void mapKeys(std::span<const bool> keys, bool keyboardWanted, CameraInput &camera) {
  if (keyboardWanted) {
    return;
  }
  camera.MoveForward = keyAxis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
  camera.MoveRight = keyAxis(keys, SDL_SCANCODE_D, SDL_SCANCODE_A);
  camera.Rotate = keyAxis(keys, SDL_SCANCODE_Q, SDL_SCANCODE_E);
}

} // namespace tpj
