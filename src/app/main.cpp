#include "app/cursor.h"
#include "app/debug_panel.h"
#include "app/food_tooltip.h"
#include "app/frame_clock.h"
#include "app/inspector_window.h"
#include "app/interaction.h"
#include "app/orbit_camera.h"
#include "app/park_dialogs.h"
#include "app/park_session.h"
#include "app/platform_input.h"
#include "app/scene_sync.h"
#include "app/scene_uploads.h"
#include "app/shop_context_tooltip.h"
#include "app/tool_panel.h"
#include "core/profile.h"
#include "legible/inspect.h"
#include "legible/preview.h"
#include "render/graph_overlay.h"
#include "render/guest_mesh.h"
#include "render/park_mesh.h"
#include "render/renderer.h"
#include "sim/command_queue.h"
#include "sim/field_text.h"
#include "sim/guests/guests.h"
#include "sim/medium/flow.h"
#include "sim/operations/operations.h"
#include "sim/park/edits.h"
#include "sim/park/intent.h"
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

ImU32 imColor(tpj::Rgba color) {
  return ImGui::ColorConvertFloat4ToU32(ImVec4(color.R, color.G, color.B, color.A));
}

// Draws the networks over the scene and behind every panel: lines in their kind's graph color, or
// the connector color, then nodes, anchored ones larger in the anchor color.
void drawGraph(const tpj::World &world, const tpj::CameraView &view) {
  const ImVec2 size = ImGui::GetIO().DisplaySize;
  const tpj::GraphOverlay overlay = tpj::buildGraphOverlay(world, view, size.x, size.y);
  ImDrawList *drawList = ImGui::GetBackgroundDrawList();
  for (const tpj::GraphLine &line : overlay.Lines) {
    const tpj::Rgba color =
        line.Connector ? tpj::GRAPH_CONNECTOR_COLOR : tpj::graphColor(line.Kind);
    drawList->AddLine(ImVec2(line.From.X, line.From.Y), ImVec2(line.To.X, line.To.Y),
                      imColor(color), tpj::GRAPH_LINE_THICKNESS);
  }
  for (const tpj::GraphNode &node : overlay.Nodes) {
    const bool anchored = node.Anchor != tpj::NULL_KEY;
    drawList->AddCircleFilled(ImVec2(node.At.X, node.At.Y),
                              anchored ? tpj::GRAPH_ANCHOR_RADIUS : tpj::GRAPH_NODE_RADIUS,
                              imColor(anchored ? tpj::GRAPH_ANCHOR_COLOR : tpj::GRAPH_NODE_COLOR));
  }
}

// The views over the scene that the Debug panel's checkboxes show.
struct ShownViews {
  bool Graph = false;
  bool FoodOverlay = false;
};

// Builds the Debug and Tools panels, setting the shown views from their checkboxes, selecting the
// tool the player chose and starting the park action they pressed.
void drawPanels(tpj::ParkDialogs &dialogs, const tpj::World &world, const tpj::OrbitCamera &camera,
                ShownViews &shown, tpj::Interaction &interaction) {
  tpj::DebugStats stats;
  stats.SimTick = world.Tick;
  stats.Focus = camera.Focus;
  stats.Distance = camera.Distance;
  for (const tpj::ParkBox &box : tpj::parkBoxes(world)) {
    if (const std::optional<tpj::ShopRecord> record = tpj::shopRecord(world, box.Key)) {
      stats.Shops.push_back({box.Key, *record});
    }
  }
  double hunger = 0.0;
  for (const tpj::EntityKey guest : tpj::parkGuests(world)) {
    if (const std::optional<tpj::GuestRecord> record = tpj::guestRecord(world, guest)) {
      ++stats.Guests;
      hunger += record->Hunger;
      if (record->Activity == tpj::GuestActivity::Waiting) {
        ++stats.Waiting;
      }
    }
  }
  stats.MeanHunger = stats.Guests == 0 ? 0.0 : hunger / static_cast<double>(stats.Guests);
  stats.MealsEaten = tpj::unitsConsumed<tpj::Meals>(world, tpj::EATEN_CAUSE);
  tpj::drawDebugPanel(stats, shown.Graph, shown.FoodOverlay);
  const tpj::ToolPanelChoice choice = tpj::drawToolPanel(
      interaction.tool().Kind, !interaction.tool().Drawn.empty(), dialogs.dialogShowing());
  if (choice.Tool) {
    interaction.selectTool(*choice.Tool);
  }
  dialogs.press(choice.Park);
}

// Builds the frame's ImGui draw data: the panels, which set the shown views from their
// checkboxes, the Inspector while it has a subject, forgetting it when closed, the graph over the
// scene while it is shown, the food tooltip at the cursor while the overlay is, on the preview's
// candidate when it has one, and a shop ghost's context.
void buildUi(SDL_Window *window, tpj::ParkDialogs &dialogs, const tpj::World &world,
             const tpj::OrbitCamera &camera, ShownViews &shown, tpj::Interaction &interaction,
             const tpj::Preview &preview) {
  const tpj::CameraView view = tpj::orbitCameraView(camera);
  tpj::beginUiFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawPanels(dialogs, world, camera, shown, interaction);
  if (const std::optional<tpj::InspectorSubject> &subject = interaction.subject();
      subject && !tpj::drawInspector(tpj::inspectSubject(world, *subject))) {
    interaction.forgetSubject();
  }
  if (shown.Graph) {
    drawGraph(world, view);
  }
  if (shown.FoodOverlay) {
    tpj::drawFoodTooltip(tpj::previewedWorld(world, preview),
                         tpj::groundUnderCursor(tpj::readCursor(window), view));
  }
  if (preview.Shop) {
    tpj::drawShopContextTooltip(*preview.Shop);
  }
  ImGui::Render();
}

// Runs the main loop until quit or the frame limit. Returns false if rendering failed.
bool runLoop(tpj::Renderer &renderer, const Options &options, tpj::ParkSession &session) {
  tpj::OrbitCamera camera;
  tpj::SceneSync scene;
  tpj::Interaction interaction(session.generation());
  ShownViews shown{options.ShowGraph, options.ShowFoodOverlay};
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

    buildUi(renderer.Window, dialogs, session.world(), camera, shown, interaction, scene.preview());

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    // The meshes are updated after the panels, so a change of the checkbox shows in this frame.
    const tpj::PreviewLook look{interaction.highlighted(session.world()), interaction.subject(),
                                shown.FoodOverlay};
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
