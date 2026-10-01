#include "app/debug_panel.h"
#include "app/food_tooltip.h"
#include "app/inspector_window.h"
#include "app/interaction.h"
#include "app/orbit_camera.h"
#include "app/park_dialogs.h"
#include "app/park_session.h"
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
#include "render/picking.h"
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
constexpr double MAX_FRAME_SECONDS = 0.25;

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

// Where the cursor lies in normalized device coordinates, and the window's aspect ratio.
struct CursorNdc {
  float X = 0.0f;
  float Y = 0.0f;
  float Aspect = 1.0f;
};

// The cursor, or none while ImGui wants the mouse or the window has no size.
std::optional<CursorNdc> cursorNdc(SDL_Window *window) {
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
  return CursorNdc{2.0f * x / static_cast<float>(width) - 1.0f,
                   1.0f - 2.0f * y / static_cast<float>(height),
                   static_cast<float>(width) / static_cast<float>(height)};
}

// The ground under the cursor, or none while ImGui wants the mouse or the cursor meets no ground.
std::optional<tpj::ParkPoint> groundUnderCursor(SDL_Window *window, const tpj::CameraView &view) {
  const std::optional<CursorNdc> cursor = cursorNdc(window);
  if (!cursor) {
    return std::nullopt;
  }
  return tpj::groundAtCursor(view, cursor->Aspect, cursor->X, cursor->Y);
}

// The view the camera gives.
tpj::CameraView cameraView(const tpj::OrbitCamera &camera) {
  tpj::CameraView view;
  view.Eye = tpj::orbitCameraEye(camera);
  view.Target = camera.Focus;
  return view;
}

// The entity the cursor's ray first meets in the view, or none while ImGui wants the mouse.
std::optional<tpj::EntityKey> entityUnderCursor(SDL_Window *window, const tpj::World &world,
                                                const tpj::CameraView &view) {
  const std::optional<CursorNdc> cursor = cursorNdc(window);
  if (!cursor) {
    return std::nullopt;
  }
  return tpj::entityAtCursor(world, view, cursor->Aspect, cursor->X, cursor->Y);
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
bool gatherInput(tpj::CameraInput &input, tpj::PointerButtons &buttons) {
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
  const tpj::CameraView view = cameraView(camera);
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
    tpj::drawFoodTooltip(tpj::previewedWorld(world, preview), groundUnderCursor(window, view));
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
  uint64_t lastCounter = SDL_GetPerformanceCounter();
  double simAccumulator = 0.0;
  tpj::ParkDialogs dialogs(renderer.Window);

  for (int frame = 1;; ++frame) {
    tpj::CameraInput input;
    tpj::PointerButtons buttons;
    if (!gatherInput(input, buttons)) {
      return true;
    }
    session.useFileRequest(dialogs.take(), dialogs);
    interaction.follow(session.generation());

    const uint64_t counter = SDL_GetPerformanceCounter();
    double dt = static_cast<double>(counter - lastCounter) /
                static_cast<double>(SDL_GetPerformanceFrequency());
    lastCounter = counter;
    if (dt > MAX_FRAME_SECONDS) {
      dt = MAX_FRAME_SECONDS;
    }

    // The buttons act on the world and pointer the ghost on screen was built from, and a Look
    // press picks from the view on screen, before the camera moves.
    interaction.useButtons(session.world(), session.commands(), buttons);
    if (interaction.picks(buttons)) {
      interaction.pick(session.world(),
                       entityUnderCursor(renderer.Window, session.world(), cameraView(camera)));
    }

    // The simulation advances in fixed ticks regardless of frame rate, and queued edits apply at
    // the next (principle 10).
    simAccumulator += dt;
    while (simAccumulator >= tpj::SIM_TICK_SECONDS) {
      session.step();
      simAccumulator -= tpj::SIM_TICK_SECONDS;
    }
    if (!tpj::uploadWorldMeshes(renderer, session.world(),
                                scene.syncWorld(session.world(), session.generation()), camera)) {
      return false;
    }

    tpj::updateOrbitCamera(camera, input, static_cast<float>(dt), 0.5f * PARK_SIZE_METERS);
    const tpj::CameraView view = cameraView(camera);
    interaction.movePointer(groundUnderCursor(renderer.Window, view));
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
