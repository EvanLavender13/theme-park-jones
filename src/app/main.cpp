#include "app/debug_panel.h"
#include "app/orbit_camera.h"
#include "app/tool_panel.h"
#include "core/profile.h"
#include "render/park_mesh.h"
#include "render/picking.h"
#include "render/renderer.h"
#include "sim/command_queue.h"
#include "sim/field_text.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
#include "sim/park_schema.h"
#include "sim/save.h"
#include "sim/world.h"
#include "tools/tools.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <charconv>
#include <exception>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float PARK_SIZE_METERS = 256.0f;
constexpr double MAX_FRAME_SECONDS = 0.25;

struct Options {
  int FrameLimit = 0;
  bool FramesGiven = false;
  const char *CapturePath = nullptr;
  const char *ParkPath = nullptr;
  uint64_t Ticks = 0;
  bool PrintHash = false;
};

bool parseCount(const char *text, uint64_t &count) {
  const std::string_view value(text);
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(value.data(), end, count);
  return !value.empty() && result.ec == std::errc() && result.ptr == end;
}

// --park PATH starts from a park file, --ticks N steps it N ticks before the first frame, and
// --hash prints the state hash after them and exits. --frames N exits after N frames.
// --capture PATH writes the last frame to PATH as a BMP and implies a short frame limit, which
// makes the app usable for automated visual checks.
bool parseOptions(int argc, char **argv, Options &options) {
  bool valid = true;
  for (int i = 1; i < argc && valid; ++i) {
    const bool hasValue = i + 1 < argc;
    if (strcmp(argv[i], "--frames") == 0 && hasValue) {
      options.FrameLimit = static_cast<int>(strtol(argv[++i], nullptr, 10));
      options.FramesGiven = true;
    } else if (strcmp(argv[i], "--capture") == 0 && hasValue) {
      options.CapturePath = argv[++i];
    } else if (strcmp(argv[i], "--park") == 0 && hasValue) {
      options.ParkPath = argv[++i];
    } else if (strcmp(argv[i], "--ticks") == 0 && hasValue) {
      valid = parseCount(argv[++i], options.Ticks);
    } else if (strcmp(argv[i], "--hash") == 0) {
      options.PrintHash = true;
    } else {
      valid = false;
    }
  }
  if (options.PrintHash && (options.FramesGiven || options.CapturePath != nullptr)) {
    valid = false;
  }
  if (!valid) {
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH]", argv[0]);
    return false;
  }
  if (options.CapturePath != nullptr && options.FrameLimit <= 0) {
    options.FrameLimit = 3;
  }
  return true;
}

// The new park, or the park file, resolved and stepped the requested ticks. None, after a message
// on standard error, when the file cannot be read or loaded.
std::optional<tpj::World> startingWorld(const Options &options) {
  std::optional<tpj::World> world;
  if (options.ParkPath == nullptr) {
    world = tpj::makeNewPark(1);
  } else {
    size_t size = 0;
    void *text = SDL_LoadFile(options.ParkPath, &size);
    if (text == nullptr) {
      (void)fprintf(stderr, "Cannot read %s: %s\n", options.ParkPath, SDL_GetError());
      return std::nullopt;
    }
    try {
      world = tpj::loadWorld(tpj::makeParkSchema(),
                             std::string_view(static_cast<const char *>(text), size));
    } catch (const tpj::LoadError &error) {
      (void)fprintf(stderr, "Cannot load %s: %s\n", options.ParkPath, error.what());
    }
    SDL_free(text);
    if (!world) {
      return std::nullopt;
    }
  }
  tpj::resolveWorld(*world);
  for (uint64_t tick = 0; tick < options.Ticks; ++tick) {
    tpj::stepWorld(*world);
  }
  return world;
}

// The intent a park mesh was built from, so the mesh is rebuilt only when intent changes.
struct DrawnIntent {
  std::vector<tpj::ParkEntrance> Entrances;
  std::vector<tpj::ParkPath> Paths;
  std::vector<tpj::ParkBox> Boxes;

  bool operator==(const DrawnIntent &) const = default;
};

// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// The edit and highlight a ghost mesh was built from.
struct DrawnGhost {
  std::optional<tpj::ParkEdit> Edit;
  std::optional<tpj::EntityKey> Highlight;

  bool operator==(const DrawnGhost &) const = default;
};

// Rebuilds the park mesh when the world's intent differs from what was last drawn, framing the
// camera on the first one. Returns false if the upload failed, and sets rebuilt when it built a
// mesh.
bool updateParkMesh(tpj::Renderer &renderer, const tpj::World &world,
                    std::optional<DrawnIntent> &drawn, tpj::OrbitCamera &camera, bool &rebuilt) {
  rebuilt = false;
  DrawnIntent current{tpj::parkEntrances(world), tpj::parkPaths(world), tpj::parkBoxes(world)};
  if (drawn && *drawn == current) {
    return true;
  }
  const tpj::ParkMesh mesh = tpj::buildParkMesh(world);
  if (!drawn) {
    if (const std::optional<tpj::GroundBounds> bounds = tpj::meshBounds(mesh)) {
      tpj::frameOrbitCamera(camera, *bounds, tpj::CameraView{}.FovY);
    }
  }
  drawn = std::move(current);
  rebuilt = true;
  return tpj::setParkMesh(renderer, mesh);
}

// Rebuilds the ghost mesh when the tool's edit or highlight differs from what was last drawn, or
// the park mesh was rebuilt. Returns false if the upload failed.
bool updateGhostMesh(tpj::Renderer &renderer, const tpj::World &world, const tpj::ToolState &tool,
                     std::optional<DrawnGhost> &drawn, bool parkRebuilt) {
  DrawnGhost current{tpj::tentativeEdit(tool, world), tpj::highlightedEntity(tool, world)};
  if (drawn && *drawn == current && !parkRebuilt) {
    return true;
  }
  tpj::ParkMesh mesh;
  if (current.Edit) {
    mesh = tpj::buildGhostMesh(world, *current.Edit);
  }
  if (current.Highlight) {
    tpj::appendEntity(mesh, world, *current.Highlight, tpj::HIGHLIGHT_TINT);
  }
  drawn = std::move(current);
  return tpj::setGhostMesh(renderer, mesh);
}

// The ground under the cursor, or none while ImGui wants the mouse or the cursor meets no ground.
std::optional<tpj::ParkPoint> groundUnderCursor(SDL_Window *window, const tpj::CameraView &view) {
  if (ImGui::GetIO().WantCaptureMouse) {
    return std::nullopt;
  }
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0) {
    return std::nullopt;
  }
  float x = 0.0f;
  float y = 0.0f;
  SDL_GetMouseState(&x, &y);
  const float ndcX = 2.0f * x / static_cast<float>(width) - 1.0f;
  const float ndcY = 1.0f - 2.0f * y / static_cast<float>(height);
  return tpj::groundAtCursor(view, static_cast<float>(width) / static_cast<float>(height), ndcX,
                             ndcY);
}

float keyAxis(const bool *keys, SDL_Scancode positive, SDL_Scancode negative) {
  return (keys[positive] ? 1.0f : 0.0f) - (keys[negative] ? 1.0f : 0.0f);
}

void addMouseInput(const SDL_Event &event, tpj::CameraInput &input) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
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

// Drains pending events into ImGui, one frame of camera input, and the left button. Input ImGui is
// using does not reach the camera, nor a press the tool; a release always reaches it. Returns false
// when the app should quit.
bool gatherInput(tpj::CameraInput &input, PointerButtons &buttons) {
  bool keepRunning = true;
  const ImGuiIO &io = ImGui::GetIO();
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT) {
      keepRunning = false;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
      buttons.Released = true;
    } else if (!io.WantCaptureMouse) {
      if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        buttons.Pressed = true;
      }
      addMouseInput(event, input);
    }
  }
  if (io.WantCaptureKeyboard) {
    return keepRunning;
  }
  const bool *keys = SDL_GetKeyboardState(nullptr);
  input.MoveForward = keyAxis(keys, SDL_SCANCODE_W, SDL_SCANCODE_S);
  input.MoveRight = keyAxis(keys, SDL_SCANCODE_D, SDL_SCANCODE_A);
  input.Rotate = keyAxis(keys, SDL_SCANCODE_Q, SDL_SCANCODE_E);
  return keepRunning;
}

// Gives the tool the frame's press and release, and queues what a release commits. Called before
// the frame's ticks and pointer move, so the buttons act on the world and pointer the ghost on
// screen was built from.
void useButtons(tpj::ToolState &tool, const tpj::World &world, tpj::CommandQueue &commands,
                const PointerButtons &buttons) {
  if (buttons.Pressed) {
    tpj::pressPointer(tool, world);
  }
  if (buttons.Released) {
    if (const std::optional<tpj::ParkEdit> edit = tpj::releasePointer(tool, world)) {
      tpj::queueEdit(commands, *edit);
    }
  }
}

// Builds the Debug and Tools panels, selecting the tool the player chose.
void drawPanels(const tpj::World &world, const tpj::OrbitCamera &camera, tpj::ToolState &tool) {
  tpj::DebugStats stats;
  stats.SimTick = world.Tick;
  stats.Focus = camera.Focus;
  stats.Distance = camera.Distance;
  tpj::drawDebugPanel(stats);
  if (const std::optional<tpj::ToolKind> kind =
          tpj::drawToolPanel(tool.Kind, !tool.Drawn.empty())) {
    tpj::selectTool(tool, *kind);
  }
}

// Runs the main loop until quit or the frame limit. Returns false if rendering failed.
bool runLoop(tpj::Renderer &renderer, const Options &options, tpj::World &world) {
  tpj::OrbitCamera camera;
  std::optional<DrawnIntent> drawn;
  std::optional<DrawnGhost> drawnGhost;
  tpj::ToolState tool;
  tpj::CommandQueue commands;
  uint64_t lastCounter = SDL_GetPerformanceCounter();
  double simAccumulator = 0.0;

  for (int frame = 1;; ++frame) {
    tpj::CameraInput input;
    PointerButtons buttons;
    if (!gatherInput(input, buttons)) {
      return true;
    }

    const uint64_t counter = SDL_GetPerformanceCounter();
    double dt = static_cast<double>(counter - lastCounter) /
                static_cast<double>(SDL_GetPerformanceFrequency());
    lastCounter = counter;
    if (dt > MAX_FRAME_SECONDS) {
      dt = MAX_FRAME_SECONDS;
    }

    useButtons(tool, world, commands, buttons);

    // The simulation advances in fixed ticks regardless of frame rate, and queued edits apply at
    // the next (principle 10).
    simAccumulator += dt;
    while (simAccumulator >= tpj::SIM_TICK_SECONDS) {
      tpj::stepWorld(world, commands);
      simAccumulator -= tpj::SIM_TICK_SECONDS;
    }
    bool parkRebuilt = false;
    if (!updateParkMesh(renderer, world, drawn, camera, parkRebuilt)) {
      return false;
    }

    tpj::updateOrbitCamera(camera, input, static_cast<float>(dt), 0.5f * PARK_SIZE_METERS);
    tpj::CameraView view;
    view.Eye = tpj::orbitCameraEye(camera);
    view.Target = camera.Focus;
    tpj::movePointer(tool, groundUnderCursor(renderer.Window, view));
    if (!updateGhostMesh(renderer, world, tool, drawnGhost, parkRebuilt)) {
      return false;
    }

    tpj::beginUiFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    drawPanels(world, camera, tool);
    ImGui::Render();

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    if (!tpj::drawFrame(renderer, view, ImGui::GetDrawData(),
                        lastFrame ? options.CapturePath : nullptr)) {
      return false;
    }
    TPJ_PROFILE_FRAME();
    if (lastFrame) {
      return true;
    }
  }
}

// Runs the main loop, logging anything it throws, such as a world invariant the simulation
// checks, so the window and GPU are still released. Returns false if the loop failed or threw.
bool runLoopLogged(tpj::Renderer &renderer, const Options &options, tpj::World &world) {
  try {
    return runLoop(renderer, options, world);
  } catch (const std::exception &error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Main loop: %s", error.what());
    return false;
  }
}

} // namespace

int main(int argc, char **argv) {
  Options options;
  if (!parseOptions(argc, argv, options)) {
    return EXIT_FAILURE;
  }
  std::optional<tpj::World> world = startingWorld(options);
  if (!world) {
    return EXIT_FAILURE;
  }
  // The hash needs no window, so it can be checked where there is no display.
  if (options.PrintHash) {
    printf("tick %llu hash %016llx\n", static_cast<unsigned long long>(world->Tick),
           static_cast<unsigned long long>(tpj::hashWorld(*world)));
    return EXIT_SUCCESS;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
    return EXIT_FAILURE;
  }
  SDL_Window *window = SDL_CreateWindow("Theme Park Jones", 1600, 900,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLGPU(window);

  tpj::Renderer renderer;
  const bool ok = tpj::createRenderer(renderer, window, PARK_SIZE_METERS) &&
                  runLoopLogged(renderer, options, *world);

  tpj::destroyRenderer(renderer);
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
