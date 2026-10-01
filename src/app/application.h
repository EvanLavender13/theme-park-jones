#ifndef TPJ_APP_APPLICATION_H
#define TPJ_APP_APPLICATION_H

#include "app/frame_clock.h"
#include "app/interaction.h"
#include "app/options.h"
#include "app/orbit_camera.h"
#include "app/park_dialogs.h"
#include "app/park_session.h"
#include "app/platform.h"
#include "app/scene_sync.h"
#include "app/tooling_ui.h"
#include "sim/world.h"

namespace tpj {

// The app: the platform owners, acquired first and released last, and the components a frame
// calls, in the order the frame runs.
class Application {
public:
  // Acquires the platform resources in order, stopping at the first that fails, which its owner
  // logs, and starts from the world.
  Application(const Options &options, World start);

  // Runs frames until the player quits or the frame limit is reached. False when a platform
  // resource was not acquired, a frame failed to render, or the loop threw, which it logs as
  // `Main loop: <what>`.
  bool run();

private:
  // The frames, each step a call into a component. False when a frame failed to render.
  bool runFrames();

  // The owners come first, so they are released after every component, in reverse order.
  SdlVideo Video;
  MainWindow Window;
  UiContext Gui;
  RendererOwner Gpu;
  ParkSession Session;
  OrbitCamera Camera;
  SceneSync Scene;
  Interaction Interactions;
  ToolingUi Ui;
  FrameClock Clock;
  ParkDialogs Dialogs;
  int FrameLimit;
  const char *CapturePath;
};

} // namespace tpj

#endif
