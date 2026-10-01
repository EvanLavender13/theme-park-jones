#include "app/cursor.h"
#include "app/frame_clock.h"
#include "app/interaction.h"
#include "app/orbit_camera.h"
#include "app/park_dialogs.h"
#include "app/park_session.h"
#include "app/platform_input.h"
#include "app/scene_sync.h"
#include "app/scene_uploads.h"
#include "app/tooling_ui.h"
#include "core/profile.h"
#include "legible/preview.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "render/renderer.h"
#include "sim/command_queue.h"
#include "sim/park/edits.h"
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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr float PARK_SIZE_METERS = 256.0f;

struct Options {
  int FrameLimit = 0;
  bool FramesGiven = false;
  const char *CapturePath = nullptr;
  const char *ParkPath = nullptr;
  uint64_t Ticks = 0;
  bool PrintHash = false;
  bool ShowGraph = false;
  bool ShowFoodOverlay = false;
};

bool parseCount(const char *text, uint64_t &count) {
  const std::string_view value(text);
  const char *end = value.data() + value.size();
  const auto result = std::from_chars(value.data(), end, count);
  return !value.empty() && result.ec == std::errc() && result.ptr == end;
}

// --park PATH starts from a park file, --ticks N steps it N ticks before the first frame, and
// --hash prints the state hash after them and exits. --frames N exits after N frames. --graph
// starts with the graph view on, and --overlay food with the food overlay on.
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
    } else if (strcmp(argv[i], "--graph") == 0) {
      options.ShowGraph = true;
    } else if (strcmp(argv[i], "--overlay") == 0 && hasValue) {
      // food is the only overlay.
      valid = strcmp(argv[++i], "food") == 0;
      options.ShowFoodOverlay = valid;
    } else {
      valid = false;
    }
  }
  if (options.PrintHash && (options.FramesGiven || options.CapturePath != nullptr ||
                            options.ShowGraph || options.ShowFoodOverlay)) {
    valid = false;
  }
  if (!valid) {
    SDL_Log("Usage: %s [--park PATH] [--ticks N] [--hash] [--frames N] [--capture PATH] [--graph] "
            "[--overlay food]",
            argv[0]);
    return false;
  }
  if (options.CapturePath != nullptr && options.FrameLimit <= 0) {
    options.FrameLimit = 3;
  }
  return true;
}

// Runs the main loop until quit or the frame limit. Returns false if rendering failed.
bool runLoop(tpj::Renderer &renderer, const Options &options, tpj::ParkSession &session) {
  tpj::OrbitCamera camera;
  tpj::SceneSync scene;
  tpj::Interaction interaction(session.generation());
  tpj::ToolingUi ui({options.ShowGraph, options.ShowFoodOverlay});
  tpj::FrameClock clock(SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency());
  tpj::ParkDialogs dialogs(renderer.Window);

  for (int frame = 1;; ++frame) {
    const tpj::FrameInput input = tpj::gatherInput();
    if (input.Quit) {
      return true;
    }
    session.useFileRequest(dialogs.take(), dialogs);
    interaction.follow(session.generation());

    const tpj::FrameStep step = clock.advance(SDL_GetPerformanceCounter());

    // The buttons act on the world and pointer the ghost on screen was built from, and a Look
    // press picks from the view on screen, before the camera moves.
    interaction.useButtons(session.world(), session.commands(), input.Buttons);
    if (interaction.picks(input.Buttons)) {
      interaction.pick(session.world(),
                       tpj::entityUnderCursor(session.world(), tpj::readCursor(renderer.Window),
                                              tpj::orbitCameraView(camera)));
    }

    // The simulation advances in fixed ticks regardless of frame rate, and queued edits apply at
    // the next (principle 10).
    for (uint32_t tick = 0; tick < step.Ticks; ++tick) {
      session.step();
    }
    if (!tpj::uploadWorldMeshes(renderer, session.world(),
                                scene.syncWorld(session.world(), session.generation()), camera)) {
      return false;
    }

    tpj::updateOrbitCamera(camera, input.Camera, static_cast<float>(step.Dt),
                           0.5f * PARK_SIZE_METERS);
    const tpj::CameraView view = tpj::orbitCameraView(camera);
    interaction.movePointer(tpj::groundUnderCursor(tpj::readCursor(renderer.Window), view));
    // Made again only when the world ticks or the edit changes, so the ghost, the overlay, and the
    // tooltips show one candidate, and frames between ticks rebuild nothing.
    const bool remade =
        scene.syncPreview(session.world(), interaction.tentativeEdit(session.world()));

    ui.build(renderer.Window, dialogs, session.world(), camera, interaction, scene.preview());

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    // The meshes are updated after the panels, so a change of the checkbox shows in this frame.
    const tpj::PreviewLook look{interaction.highlighted(session.world()), interaction.subject(),
                                ui.shown().FoodOverlay};
    if ((scene.syncLook(remade, look) &&
         !tpj::uploadPreviewMeshes(renderer, session.world(), scene.preview(), look)) ||
        !tpj::drawFrame(renderer, view, ImGui::GetDrawData(),
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
bool runLoopLogged(tpj::Renderer &renderer, const Options &options, tpj::ParkSession &session) {
  try {
    return runLoop(renderer, options, session);
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
  tpj::OpenedPark start = tpj::startingPark(options.ParkPath, options.Ticks);
  if (!start.Park) {
    (void)fprintf(stderr, "%s\n", start.Error.c_str());
    return EXIT_FAILURE;
  }
  // The hash needs no window, so it can be checked where there is no display.
  if (options.PrintHash) {
    printf("tick %llu hash %016llx\n", static_cast<unsigned long long>(start.Park->Tick),
           static_cast<unsigned long long>(tpj::hashWorld(*start.Park)));
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

  tpj::ParkSession session(std::move(*start.Park));
  tpj::Renderer renderer;
  const bool ok = tpj::createRenderer(renderer, window, PARK_SIZE_METERS) &&
                  runLoopLogged(renderer, options, session);

  tpj::destroyRenderer(renderer);
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
