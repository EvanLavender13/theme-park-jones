#include "app/orbit_camera.h"
#include "core/profile.h"
#include "render/renderer.h"
#include "sim/world.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdlib.h>
#include <string.h>

namespace {

constexpr float PARK_SIZE_METERS = 256.0f;
constexpr double MAX_FRAME_SECONDS = 0.25;

struct Options {
  int FrameLimit = 0;
  const char *CapturePath = nullptr;
};

// --frames N exits after N frames. --capture PATH writes the last frame to PATH as a BMP and
// implies a short frame limit, which makes the app usable for automated visual checks.
bool parseOptions(int argc, char **argv, Options &options) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      options.FrameLimit = static_cast<int>(strtol(argv[++i], nullptr, 10));
    } else if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
      options.CapturePath = argv[++i];
    } else {
      SDL_Log("Usage: %s [--frames N] [--capture PATH]", argv[0]);
      return false;
    }
  }
  if (options.CapturePath != nullptr && options.FrameLimit <= 0) {
    options.FrameLimit = 3;
  }
  return true;
}

float keyAxis(const bool *keys, SDL_Scancode positive, SDL_Scancode negative) {
  return (keys[positive] ? 1.0f : 0.0f) - (keys[negative] ? 1.0f : 0.0f);
}

// Drains pending events into one frame of camera input. Returns false when the app should quit.
bool gatherInput(tpj::CameraInput &input) {
  bool keepRunning = true;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      keepRunning = false;
    } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
      if ((event.motion.state & SDL_BUTTON_RMASK) != 0) {
        input.OrbitDx += event.motion.xrel;
        input.OrbitDy += event.motion.yrel;
      } else if ((event.motion.state & SDL_BUTTON_MMASK) != 0) {
        input.PanDx += event.motion.xrel;
        input.PanDy += event.motion.yrel;
      }
    } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
      input.Zoom += event.wheel.y;
    }
  }
  const bool *keys = SDL_GetKeyboardState(nullptr);
  input.MoveForward = keyAxis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
  input.MoveRight = keyAxis(keys, SDL_SCANCODE_D, SDL_SCANCODE_A);
  input.Rotate = keyAxis(keys, SDL_SCANCODE_Q, SDL_SCANCODE_E);
  return keepRunning;
}

// Runs the main loop until quit or the frame limit. Returns false if rendering failed.
bool runLoop(tpj::Renderer &renderer, const Options &options) {
  tpj::World world;
  tpj::OrbitCamera camera;
  uint64_t lastCounter = SDL_GetPerformanceCounter();
  double simAccumulator = 0.0;

  for (int frame = 1;; ++frame) {
    tpj::CameraInput input;
    if (!gatherInput(input)) {
      return true;
    }

    const uint64_t counter = SDL_GetPerformanceCounter();
    double dt = static_cast<double>(counter - lastCounter) /
                static_cast<double>(SDL_GetPerformanceFrequency());
    lastCounter = counter;
    if (dt > MAX_FRAME_SECONDS) {
      dt = MAX_FRAME_SECONDS;
    }

    // The simulation advances in fixed ticks regardless of frame rate (principle 10).
    simAccumulator += dt;
    while (simAccumulator >= tpj::SIM_TICK_SECONDS) {
      tpj::stepWorld(world);
      simAccumulator -= tpj::SIM_TICK_SECONDS;
    }

    tpj::updateOrbitCamera(camera, input, static_cast<float>(dt), 0.5f * PARK_SIZE_METERS);
    tpj::CameraView view;
    view.Eye = tpj::orbitCameraEye(camera);
    view.Target = camera.Focus;

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    if (!tpj::drawFrame(renderer, view, lastFrame ? options.CapturePath : nullptr)) {
      return false;
    }
    TPJ_PROFILE_FRAME();
    if (lastFrame) {
      return true;
    }
  }
}

} // namespace

int main(int argc, char **argv) {
  Options options;
  if (!parseOptions(argc, argv, options)) {
    return EXIT_FAILURE;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
    return EXIT_FAILURE;
  }
  SDL_Window *window = SDL_CreateWindow("Theme Park Jones", 1600, 900,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  tpj::Renderer renderer;
  bool ok = false;
  if (window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
  } else if (tpj::createRenderer(renderer, window, PARK_SIZE_METERS)) {
    ok = runLoop(renderer, options);
  }

  tpj::destroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
