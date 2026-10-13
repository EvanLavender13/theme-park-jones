#include "app/application.h"

#include "app/input/cursor.h"
#include "app/input/platform_input.h"
#include "app/scene/scene_uploads.h"
#include "core/profile.h"
#include "legible/preview.h"
#include "render/day_light.h"
#include "render/renderer.h"

#include <SDL3/SDL.h>

#include <exception>
#include <stdint.h>
#include <stdio.h>
#include <utility>

namespace tpj {
namespace {

// The side of the square terrain the renderer draws, centered on the origin, inside which the
// camera's focus stays.
constexpr float PARK_SIZE_METERS = 256.0f;

} // namespace

Application::Application(const Options &options, World start)
    : Window(Video, options.FrameTimes || options.CapturePath != nullptr), Gui(Window),
      Gpu(Window, Gui, PARK_SIZE_METERS), Session(std::move(start)),
      Interactions(Session.world(), Session.generation(), Session.edits()),
      Ui({options.ShowGraph, options.Overlay}, GameSpeed(options.Speed),
         LightPreview{options.LightHour.has_value(), options.LightHour.value_or(0) * 3600}),
      Clock(SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency()), Dialogs(Window.get()),
      FrameLimit(options.FrameLimit), CapturePath(options.CapturePath),
      WriteFrameTimes(options.FrameTimes) {}

// What the loop throws, such as a world invariant the simulation checks, is logged here, inside
// the owners' lifetime, so they are still released.
bool Application::run() {
  if (!Gpu.ready()) {
    return false;
  }
  try {
    if (!runFrames()) {
      return false;
    }
    if (WriteFrameTimes) {
      (void)printf("%s\n", Frames.line().c_str());
    }
    return true;
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
    Ui.speed().use(input.Speed);
    Session.useFileRequest(Dialogs.take(), Dialogs);
    Interactions.follow(Session.world(), Session.generation(), Session.edits());

    const FrameStep step = Clock.advance(SDL_GetPerformanceCounter(), Ui.speed().multiplier());
    // Kept only when asked for, so ordinary play keeps no list that grows every frame.
    if (WriteFrameTimes) {
      Frames.add(step.Nanoseconds);
    }

    // The buttons act on the world and pointer the ghost on screen was built from, and a Look
    // press picks from the view on screen, before the camera moves.
    Interactions.useButtons(Session.world(), Session.commands(), input.Buttons);
    if (Interactions.picks(input.Buttons)) {
      Interactions.pick(Session.world(), Scene.seated(), readCursor(Window.get()),
                        orbitCameraView(Camera));
    }

    // The simulation advances in fixed ticks regardless of frame rate, as many as the speed asks,
    // and queued edits apply at the next, or at once while paused (principle 10).
    // A frame stops stepping once its ticks have taken MAX_STEP_SECONDS, and drops the rest.
    const uint64_t stepsStarted = SDL_GetPerformanceCounter();
    for (uint32_t tick = 0; tick < step.Ticks; ++tick) {
      if (tick > 0 && !Clock.mayStep(stepsStarted, SDL_GetPerformanceCounter())) {
        break;
      }
      Session.step();
    }
    if (Ui.speed().paused()) {
      Session.applyWithoutTick();
    }
    // The tools read the park the ticks left.
    Interactions.follow(Session.world(), Session.generation(), Session.edits());
    const WorldMeshes meshes =
        Scene.syncWorld(Session.world(), Session.generation(), Session.edits());
    if (!uploadWorldMeshes(renderer, Scene, meshes, Camera)) {
      return false;
    }

    updateOrbitCamera(Camera, input.Camera, static_cast<float>(step.Dt), 0.5f * PARK_SIZE_METERS);
    const CameraView view = orbitCameraView(Camera);
    const std::optional<CursorNdc> cursor = readCursor(Window.get());
    Interactions.movePointer(
        Session.world(), PointerSample{groundUnderCursor(cursor, view),
                                       groundPerPixel(cursor, view).value_or(0.0), input.SnapsOff});
    // Made again only when the edit changes, so the ghost, the overlay, and the tooltips show one
    // candidate, and frames rebuild nothing while the pointer rests. The need overlay reads the
    // candidate's state, so while it is shown a preview with a candidate is made again at each tick
    // too.
    const bool remade =
        Scene.syncPreview(Session.world(), Interactions.tentativeEdit(Session.world()),
                          Ui.shown().Overlay.has_value());
    // Kept current before the panels, for the need tooltip, and again after them, so a change of
    // the Overlay row shows in this frame.
    bool shapeChanged = Scene.syncOverlayShape(previewedWorld(Session.world(), Scene.preview()),
                                               Ui.shown().Overlay.has_value());

    ImDrawData *drawData = Ui.build(Window.get(), Dialogs, Session, Camera, Interactions,
                                    Scene.preview(), Scene.overlayShape(), input.History);
    shapeChanged = Scene.syncOverlayShape(previewedWorld(Session.world(), Scene.preview()),
                                          Ui.shown().Overlay.has_value()) ||
                   shapeChanged;

    const bool lastFrame = FrameLimit > 0 && frame >= FrameLimit;
    // While the Light panel previews a time, it lights the frame in place of the clock.
    const LightPreview &preview = Ui.lightPreview();
    const DayLight light = preview.On ? dayLightAt(preview.Second) : dayLightOf(Session.world());
    // The meshes are updated after the panels, so a change of the Overlay row shows in this frame.
    fillLook(Interactions, Session.world(), Ui.shown().Overlay, Look);
    const PreviewMeshes previewMeshes =
        Scene.syncLook(Session.world(), Interactions.park(), remade, Look, shapeChanged);
    if (!uploadPreviewMeshes(renderer, Scene, previewMeshes, shapeChanged) ||
        !drawFrame(renderer, view, light, drawData, lastFrame ? CapturePath : nullptr)) {
      return false;
    }
    TPJ_PROFILE_FRAME();
    if (lastFrame) {
      return true;
    }
  }
}

} // namespace tpj
