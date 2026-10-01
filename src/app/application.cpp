#include "app/application.h"

#include "app/cursor.h"
#include "app/platform_input.h"
#include "app/scene_uploads.h"
#include "core/profile.h"
#include "render/renderer.h"

#include <SDL3/SDL.h>

#include <exception>
#include <stdint.h>
#include <utility>

namespace tpj {
namespace {

// The side of the square terrain the renderer draws, centered on the origin, inside which the
// camera's focus stays.
constexpr float PARK_SIZE_METERS = 256.0f;

} // namespace

Application::Application(const Options &options, World start)
    : Window(Video), Gui(Window), Gpu(Window, Gui, PARK_SIZE_METERS), Session(std::move(start)),
      Interactions(Session.generation()), Ui({options.ShowGraph, options.ShowFoodOverlay}),
      Clock(SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency()), Dialogs(Window.get()),
      FrameLimit(options.FrameLimit), CapturePath(options.CapturePath) {}

// What the loop throws, such as a world invariant the simulation checks, is logged here, inside
// the owners' lifetime, so they are still released.
bool Application::run() {
  if (!Gpu.ready()) {
    return false;
  }
  try {
    return runFrames();
  } catch (const std::exception &error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Main loop: %s", error.what());
    return false;
  }
}

bool Application::runFrames() {
  Renderer &renderer = Gpu.renderer();
  for (int frame = 1;; ++frame) {
    const FrameInput input = gatherInput();
    if (input.Quit) {
      return true;
    }
    Session.useFileRequest(Dialogs.take(), Dialogs);
    Interactions.follow(Session.generation());

    const FrameStep step = Clock.advance(SDL_GetPerformanceCounter());

    // The buttons act on the world and pointer the ghost on screen was built from, and a Look
    // press picks from the view on screen, before the camera moves.
    Interactions.useButtons(Session.world(), Session.commands(), input.Buttons);
    if (Interactions.picks(input.Buttons)) {
      Interactions.pick(
          Session.world(),
          entityUnderCursor(Session.world(), readCursor(Window.get()), orbitCameraView(Camera)));
    }

    // The simulation advances in fixed ticks regardless of frame rate, and queued edits apply at
    // the next (principle 10).
    for (uint32_t tick = 0; tick < step.Ticks; ++tick) {
      Session.step();
    }
    if (!uploadWorldMeshes(renderer, Session.world(),
                           Scene.syncWorld(Session.world(), Session.generation()), Camera)) {
      return false;
    }

    updateOrbitCamera(Camera, input.Camera, static_cast<float>(step.Dt), 0.5f * PARK_SIZE_METERS);
    const CameraView view = orbitCameraView(Camera);
    Interactions.movePointer(groundUnderCursor(readCursor(Window.get()), view));
    // Made again only when the world ticks or the edit changes, so the ghost, the overlay, and the
    // tooltips show one candidate, and frames between ticks rebuild nothing.
    const bool remade =
        Scene.syncPreview(Session.world(), Interactions.tentativeEdit(Session.world()));

    ImDrawData *drawData =
        Ui.build(Window.get(), Dialogs, Session.world(), Camera, Interactions, Scene.preview());

    const bool lastFrame = FrameLimit > 0 && frame >= FrameLimit;
    // The meshes are updated after the panels, so a change of the checkbox shows in this frame.
    const PreviewLook look{Interactions.highlighted(Session.world()), Interactions.subject(),
                           Ui.shown().FoodOverlay};
    if ((Scene.syncLook(remade, look) &&
         !uploadPreviewMeshes(renderer, Session.world(), Scene.preview(), look)) ||
        !drawFrame(renderer, view, drawData, lastFrame ? CapturePath : nullptr)) {
      return false;
    }
    TPJ_PROFILE_FRAME();
    if (lastFrame) {
      return true;
    }
  }
}

} // namespace tpj
