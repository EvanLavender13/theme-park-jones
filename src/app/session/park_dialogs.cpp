#include "app/session/park_dialogs.h"

#include <string>

namespace tpj {
namespace {

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
ParkFileRequests &requests() {
  static ParkFileRequests instance;
  return instance;
}

// A dialog's callback: logs a failed dialog, and answers the requests with the first chosen path,
// or none when the dialog was cancelled or failed.
void answer(ParkDialog dialog, const char *const *filelist) {
  if (filelist == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "File dialog: %s", SDL_GetError());
  }
  requests().answer(dialog, filelist != nullptr ? filelist[0] : nullptr);
}

void SDLCALL onOpenChosen(void * /*userdata*/, const char *const *filelist, int /*filter*/) {
  answer(ParkDialog::Open, filelist);
}

void SDLCALL onSaveChosen(void * /*userdata*/, const char *const *filelist, int /*filter*/) {
  answer(ParkDialog::Save, filelist);
}

} // namespace

ParkDialogs::ParkDialogs(SDL_Window *window) : Window(window), Requests(requests()) {}

void ParkDialogs::press(ParkAction action) {
  // The callback may run before these return, so no lock is held while they run.
  const ParkDialog dialog = Requests.press(action);
  if (dialog == ParkDialog::Open) {
    SDL_ShowOpenFileDialog(onOpenChosen, nullptr, Window, PARK_FILTERS, 1, parksFolder(), false);
  } else if (dialog == ParkDialog::Save) {
    SDL_ShowSaveFileDialog(onSaveChosen, nullptr, Window, PARK_FILTERS, 1, parksFolder());
  }
}

bool ParkDialogs::dialogShowing() const { return Requests.dialogShowing(); }

FileRequest ParkDialogs::take() { return Requests.take(); }

bool ParkDialogs::confirmReplace(const std::string &path) {
  const std::string message = path + " already exists. Replace it?";
  const SDL_MessageBoxButtonData buttons[] = {
      {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel"},
      {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Replace"}};
  const SDL_MessageBoxData data{
      SDL_MESSAGEBOX_WARNING, Window, "Theme Park Jones", message.c_str(), 2, buttons, nullptr};
  int chosen = 0;
  return SDL_ShowMessageBox(&data, &chosen) && chosen == 1;
}

void ParkDialogs::reportError(const std::string &message) {
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message.c_str());
  (void)SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Theme Park Jones", message.c_str(), Window);
}

} // namespace tpj
