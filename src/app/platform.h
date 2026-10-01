#ifndef TPJ_APP_PLATFORM_H
#define TPJ_APP_PLATFORM_H

#include "render/renderer.h"

struct SDL_Window;

namespace tpj {

// SDL's video subsystem, initialized for the owner's lifetime.
class SdlVideo {
public:
  // Initializes it, logging SDL's error when it cannot.
  SdlVideo();
  SdlVideo(const SdlVideo &) = delete;
  SdlVideo &operator=(const SdlVideo &) = delete;
  SdlVideo(SdlVideo &&) = delete;
  SdlVideo &operator=(SdlVideo &&) = delete;
  ~SdlVideo();

  [[nodiscard]] bool ready() const { return Ready; }

private:
  bool Ready = false;
};

// The app's window, open for the owner's lifetime.
class MainWindow {
public:
  // Opens the resizable, high pixel density window titled Theme Park Jones, 1600 by 900, when
  // SDL's video is ready, logging SDL's error when it cannot.
  explicit MainWindow(const SdlVideo &video);
  MainWindow(const MainWindow &) = delete;
  MainWindow &operator=(const MainWindow &) = delete;
  MainWindow(MainWindow &&) = delete;
  MainWindow &operator=(MainWindow &&) = delete;
  ~MainWindow();

  // The window, or null when it is not open.
  [[nodiscard]] SDL_Window *get() const { return Window; }

private:
  SDL_Window *Window = nullptr;
};

// The Dear ImGui context, with docking enabled and the dark style, and its SDL3 platform backend
// for the window, for the owner's lifetime.
class UiContext {
public:
  // Creates them when the window is open.
  explicit UiContext(const MainWindow &window);
  UiContext(const UiContext &) = delete;
  UiContext &operator=(const UiContext &) = delete;
  UiContext(UiContext &&) = delete;
  UiContext &operator=(UiContext &&) = delete;
  ~UiContext();

  [[nodiscard]] bool ready() const { return Ready; }

private:
  bool Ready = false;
};

// The renderer for the window, with a square terrain terrainSize meters across, for the owner's
// lifetime.
class RendererOwner {
public:
  // Creates it when the ImGui context its GPU backend needs is ready, logging through SDL when it
  // cannot.
  RendererOwner(const MainWindow &window, const UiContext &ui, float terrainSize);
  RendererOwner(const RendererOwner &) = delete;
  RendererOwner &operator=(const RendererOwner &) = delete;
  RendererOwner(RendererOwner &&) = delete;
  RendererOwner &operator=(RendererOwner &&) = delete;
  ~RendererOwner();

  [[nodiscard]] bool ready() const { return Ready; }
  [[nodiscard]] Renderer &renderer() { return Gpu; }

private:
  Renderer Gpu;
  bool Ready = false;
};

} // namespace tpj

#endif
