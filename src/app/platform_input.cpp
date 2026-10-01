#include "app/platform_input.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <span>
#include <stddef.h>

namespace tpj {

FrameInput gatherInput() {
  FrameInput input;
  const ImGuiIO &io = ImGui::GetIO();
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    mapEvent(event, io.WantCaptureMouse, input);
  }
  int count = 0;
  const bool *keys = SDL_GetKeyboardState(&count);
  mapKeys(std::span<const bool>(keys, static_cast<size_t>(count)), io.WantCaptureKeyboard,
          input.Camera);
  return input;
}

std::optional<CursorNdc> readCursor(SDL_Window *window) {
  if (ImGui::GetIO().WantCaptureMouse) {
    return std::nullopt;
  }
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSize(window, &width, &height)) {
    return std::nullopt;
  }
  float x = 0.0f;
  float y = 0.0f;
  SDL_GetMouseState(&x, &y);
  return cursorNdc(width, height, x, y);
}

} // namespace tpj
