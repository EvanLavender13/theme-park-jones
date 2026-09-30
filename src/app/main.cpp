#include "app/debug_panel.h"
#include "app/food_tooltip.h"
#include "app/orbit_camera.h"
#include "app/park_file.h"
#include "app/shop_context_tooltip.h"
#include "app/tool_panel.h"
#include "core/profile.h"
#include "legible/food.h"
#include "legible/preview.h"
#include "render/food_overlay.h"
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
#include <mutex>
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

// A new park, resolved.
tpj::World resolvedNewPark() {
  tpj::World world = tpj::makeNewPark(1);
  tpj::resolveWorld(world);
  return world;
}

// The new park, or the park file, resolved and stepped the requested ticks. None, after a message
// on standard error, when the file cannot be read or loaded.
std::optional<tpj::World> startingWorld(const Options &options) {
  std::optional<tpj::World> world;
  if (options.ParkPath == nullptr) {
    world = resolvedNewPark();
  } else {
    tpj::OpenedPark opened = tpj::openParkFile(options.ParkPath);
    if (!opened.Park) {
      (void)fprintf(stderr, "%s\n", opened.Error.c_str());
      return std::nullopt;
    }
    world = std::move(opened.Park);
  }
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

// The highlight and overlay choice the ghost and overlay meshes were last built with.
struct DrawnPreview {
  std::optional<tpj::EntityKey> Highlight;
  bool FoodOverlay = false;

  bool operator==(const DrawnPreview &) const = default;
};

// The left button's presses and releases over one frame.
struct PointerButtons {
  bool Pressed = false;
  bool Released = false;
};

// What the park buttons and the file dialogs ask of the main loop, handed over under a lock, since
// a dialog's callback may run on another thread.
struct FileRequests {
  std::mutex Lock;
  bool DialogShowing = false;
  tpj::ParkAction Action = tpj::ParkAction::None;
  std::string Path;
};

// One request, taken by the main loop.
struct FileRequest {
  tpj::ParkAction Action = tpj::ParkAction::None;
  std::string Path;
};

constexpr SDL_DialogFileFilter PARK_FILTERS[] = {{"Park files", "park"}};

// The parks folder beside the executable, where the dialogs start. The build links it to the
// source tree's. It ends with a separator, since SDL's Windows dialog takes what follows the last
// one as a file name.
const char *parksFolder() {
  static const std::string folder = [] {
    const char *base = SDL_GetBasePath();
    return std::string(base != nullptr ? base : "") + "parks/";
  }();
  return folder.c_str();
}

// Lives for the whole program, since a dialog left open at quit may still call back.
FileRequests &fileRequests() {
  static FileRequests requests;
  return requests;
}

// A dialog's callback: hands the first chosen path to the main loop, and nothing when cancelled.
void handOver(void *userdata, const char *const *filelist, tpj::ParkAction action) {
  auto &requests = *static_cast<FileRequests *>(userdata);
  if (filelist == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "File dialog: %s", SDL_GetError());
  }
  const std::scoped_lock lock(requests.Lock);
  requests.DialogShowing = false;
  if (filelist != nullptr && filelist[0] != nullptr) {
    requests.Action = action;
    requests.Path = filelist[0];
  }
}

void SDLCALL onOpenChosen(void *userdata, const char *const *filelist, int /*filter*/) {
  handOver(userdata, filelist, tpj::ParkAction::Open);
}

void SDLCALL onSaveChosen(void *userdata, const char *const *filelist, int /*filter*/) {
  handOver(userdata, filelist, tpj::ParkAction::Save);
}

bool dialogShowing() {
  FileRequests &requests = fileRequests();
  const std::scoped_lock lock(requests.Lock);
  return requests.DialogShowing;
}

// Acts on a park button: New waits for the next frame, and Open and Save show their dialog. Does
// nothing while a dialog is showing.
void startParkAction(tpj::ParkAction action, SDL_Window *window) {
  if (action == tpj::ParkAction::None) {
    return;
  }
  FileRequests &requests = fileRequests();
  {
    const std::scoped_lock lock(requests.Lock);
    if (requests.DialogShowing) {
      return;
    }
    if (action == tpj::ParkAction::New) {
      requests.Action = action;
      requests.Path.clear();
      return;
    }
    requests.DialogShowing = true;
  }
  // The callback may run before these return, so the lock is not held while they run.
  if (action == tpj::ParkAction::Open) {
    SDL_ShowOpenFileDialog(onOpenChosen, &requests, window, PARK_FILTERS, 1, parksFolder(), false);
  } else {
    SDL_ShowSaveFileDialog(onSaveChosen, &requests, window, PARK_FILTERS, 1, parksFolder());
  }
}

FileRequest takeFileRequest() {
  FileRequests &requests = fileRequests();
  const std::scoped_lock lock(requests.Lock);
  FileRequest request{requests.Action, std::move(requests.Path)};
  requests.Action = tpj::ParkAction::None;
  requests.Path.clear();
  return request;
}

void reportFileError(SDL_Window *window, const std::string &message) {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
  (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Theme Park Jones", message.c_str(), window);
}

// Asks whether to replace the file at the path. True when the player chooses Replace.
bool confirmReplace(SDL_Window *window, const std::string &path) {
  const std::string message = path + " already exists. Replace it?";
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Replace"}};
  const SDL_MessageBoxData data{
      SDL_MESSAGEBOX_WARNING, window, "Theme Park Jones", message.c_str(), 2, buttons, nullptr};
  int chosen = 0;
  return SDL_ShowMessageBox(&data, &chosen) && chosen == 1;
}

// Saves, or replaces the world, as the request asks. Returns true when it replaced the world. A
// save whose extension was added asks before replacing a file, since the dialog asked only about
// the name as typed.
bool useFileRequest(SDL_Window *window, const FileRequest &request, tpj::World &world) {
  if (request.Action == tpj::ParkAction::Save) {
    const std::string path = tpj::withParkExtension(request.Path);
    if (path != request.Path && SDL_GetPathInfo(path.c_str(), nullptr) &&
        !confirmReplace(window, path)) {
      return false;
    }
    const std::string error = tpj::saveParkFile(world, path.c_str());
    if (!error.empty()) {
      reportFileError(window, error);
    }
    return false;
  }
  if (request.Action == tpj::ParkAction::New) {
    world = resolvedNewPark();
    return true;
  }
  if (request.Action == tpj::ParkAction::Open) {
    tpj::OpenedPark opened = tpj::openParkFile(request.Path.c_str());
    if (!opened.Park) {
      reportFileError(window, opened.Error);
      return false;
    }
    world = std::move(*opened.Park);
    return true;
  }
  return false;
}

// Rebuilds the park mesh when the world's intent differs from what was last drawn, framing the
// camera on the first one. Returns false if the upload failed.
bool updateParkMesh(tpj::Renderer &renderer, const tpj::World &world,
                    std::optional<DrawnIntent> &drawn, tpj::OrbitCamera &camera) {
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
  return tpj::setParkMesh(renderer, mesh);
}

// The frame's ghost: the preview's edit with its candidate's walkways and starved marks, then the
// tool's highlight.
tpj::ParkMesh ghostMesh(const tpj::World &world, const tpj::ToolState &tool,
                        const tpj::Preview &preview) {
  tpj::ParkMesh mesh;
  if (preview.Edit) {
    mesh = tpj::buildGhostMesh(world, *preview.Edit, preview.Candidate);
  }
  if (const std::optional<tpj::EntityKey> highlight = tpj::highlightedEntity(tool, world)) {
    tpj::appendEntity(mesh, world, *highlight, tpj::HIGHLIGHT_TINT);
  }
  return mesh;
}

// Rebuilds the guests' mesh when the world's tick differs from the one last drawn. Returns false
// if the upload failed.
bool updateGuestMesh(tpj::Renderer &renderer, const tpj::World &world,
                     std::optional<uint64_t> &drawnTick) {
  if (drawnTick && *drawnTick == world.Tick) {
    return true;
  }
  drawnTick = world.Tick;
  return tpj::setGuestMesh(renderer, tpj::buildGuestMesh(world));
}

// The food overlay while it is shown, shaded by the food availability at each place, and an empty
// mesh while it is not.
tpj::ParkMesh foodOverlayMesh(const tpj::World &world, bool show) {
  if (!show) {
    return {};
  }
  return tpj::buildFoodOverlay(world, [&world](const tpj::Place &place) {
    return tpj::foodAvailability(world, place).Value;
  });
}

// Rebuilds the ghost and food overlay meshes when the preview was made again, or the tool's
// highlight or the overlay's checkbox differs from when they were last built. Returns false if an
// upload failed.
bool updatePreviewMeshes(tpj::Renderer &renderer, const tpj::World &world,
                         const tpj::ToolState &tool, const tpj::Preview &preview, bool remade,
                         bool showOverlay, std::optional<DrawnPreview> &drawn) {
  const DrawnPreview current{tpj::highlightedEntity(tool, world), showOverlay};
  if (!remade && drawn && *drawn == current) {
    return true;
  }
  drawn = current;
  return tpj::setGhostMesh(renderer, ghostMesh(world, tool, preview)) &&
         tpj::setOverlayMesh(renderer,
                             foodOverlayMesh(tpj::previewedWorld(world, preview), showOverlay));
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
void drawPanels(SDL_Window *window, const tpj::World &world, const tpj::OrbitCamera &camera,
                ShownViews &shown, tpj::ToolState &tool) {
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
  const tpj::ToolPanelChoice choice =
      tpj::drawToolPanel(tool.Kind, !tool.Drawn.empty(), dialogShowing());
  if (choice.Tool) {
    tpj::selectTool(tool, *choice.Tool);
  }
  startParkAction(choice.Park, window);
}

// Builds the frame's ImGui draw data: the panels, which set the shown views from their
// checkboxes, the graph over the scene while it is shown, the food tooltip at the cursor while the
// overlay is, on the preview's candidate when it has one, and a shop ghost's context.
void buildUi(SDL_Window *window, const tpj::World &world, const tpj::OrbitCamera &camera,
             const tpj::CameraView &view, ShownViews &shown, tpj::ToolState &tool,
             const tpj::Preview &preview) {
  tpj::beginUiFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawPanels(window, world, camera, shown, tool);
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
bool runLoop(tpj::Renderer &renderer, const Options &options, tpj::World &world) {
  tpj::OrbitCamera camera;
  std::optional<DrawnIntent> drawn;
  std::optional<uint64_t> guestTick;
  tpj::KeptPreview kept;
  std::optional<DrawnPreview> drawnPreview;
  tpj::ToolState tool;
  tpj::CommandQueue commands;
  ShownViews shown{options.ShowGraph, options.ShowFoodOverlay};
  uint64_t lastCounter = SDL_GetPerformanceCounter();
  double simAccumulator = 0.0;

  for (int frame = 1;; ++frame) {
    tpj::CameraInput input;
    PointerButtons buttons;
    if (!gatherInput(input, buttons)) {
      return true;
    }
    if (useFileRequest(renderer.Window, takeFileRequest(), world)) {
      commands.clear();
      tpj::selectTool(tool, tool.Kind);
      drawn.reset();
      guestTick.reset();
      kept = {};
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
    if (!updateParkMesh(renderer, world, drawn, camera)) {
      return false;
    }
    if (!updateGuestMesh(renderer, world, guestTick)) {
      return false;
    }

    tpj::updateOrbitCamera(camera, input, static_cast<float>(dt), 0.5f * PARK_SIZE_METERS);
    tpj::CameraView view;
    view.Eye = tpj::orbitCameraEye(camera);
    view.Target = camera.Focus;
    tpj::movePointer(tool, groundUnderCursor(renderer.Window, view));
    // Made again only when the world ticks or the edit changes, so the ghost, the overlay, and the
    // tooltips show one candidate, and frames between ticks rebuild nothing.
    const bool remade = tpj::keepPreview(kept, world, tpj::tentativeEdit(tool, world));

    buildUi(renderer.Window, world, camera, view, shown, tool, kept.Made);

    const bool lastFrame = options.FrameLimit > 0 && frame >= options.FrameLimit;
    // The meshes are updated after the panels, so a change of the checkbox shows in this frame.
    if (!updatePreviewMeshes(renderer, world, tool, kept.Made, remade, shown.FoodOverlay,
                             drawnPreview) ||
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
