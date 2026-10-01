#include "app/platform.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

namespace tpj {

SdlVideo::SdlVideo() : Ready(SDL_Init(SDL_INIT_VIDEO)) {
  if (!Ready) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
  }
}

SdlVideo::~SdlVideo() {
  if (Ready) {
    SDL_Quit();
  }
}

MainWindow::MainWindow(const SdlVideo &video) {
  if (!video.ready()) {
    return;
  }
  Window = SDL_CreateWindow("Theme Park Jones", 1600, 900,
                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (Window == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
  }
}

MainWindow::~MainWindow() {
  if (Window != nullptr) {
    SDL_DestroyWindow(Window);
  }
}

UiContext::UiContext(const MainWindow &window) {
  if (window.get() == nullptr) {
    return;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForSDLGPU(window.get());
  Ready = true;
}

UiContext::~UiContext() {
  if (Ready) {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
}

// destroyRenderer releases only what createRenderer made, so it runs even when creation failed.
RendererOwner::RendererOwner(const MainWindow &window, const UiContext &ui, float terrainSize)
    : Ready(ui.ready() && createRenderer(Gpu, window.get(), terrainSize)) {}

RendererOwner::~RendererOwner() { destroyRenderer(Gpu); }

} // namespace tpj
